
#include "OrcaCaptureCore.h"

#include <iostream>
#include <unordered_map>
#include <string>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_udp.h>
#include <rte_malloc.h>
#include "DpdkUtils.h"

namespace FrameProcessor
{
    OrcaCaptureCore::OrcaCaptureCore(
        int proc_idx, int socket_id, DpdkWorkCoreReferences dpdkWorkCoreReferences
    ) :
        DpdkWorkerCore(socket_id),
        proc_idx_(proc_idx),
        decoder_(dpdkWorkCoreReferences.decoder),
        shared_buf_(dpdkWorkCoreReferences.shared_buf),
        logger_(Logger::getLogger("FP.OrcaCaptureCore"))
        
    {

        // Resolve configuration parameters for this core from the config object passed as an
        // argument, and the current port ID
        config_.resolve(dpdkWorkCoreReferences.core_config);

        LOG4CXX_INFO(logger_, "Core " << proc_idx_ << " creation!");

        orca_controller_ = new OrcaQuestCameraController(decoder_);


        decoder_->set_capture_core_ref(reinterpret_cast<void*>(orca_controller_));


        // Check if the downstream ring have already been created by another processing core,
        // otherwise create it with the ring size rounded up to the next power of two
        for (int ring_idx = 0; ring_idx < config_.num_downstream_cores; ring_idx++)
        {
            std::string downstream_ring_name = ring_name_str(config_.core_name, socket_id_, ring_idx);
            struct rte_ring* downstream_ring = rte_ring_lookup(downstream_ring_name.c_str());
            if (downstream_ring == NULL)
            {
                unsigned int downstream_ring_size = nearest_power_two(shared_buf_->get_num_buffers());
                LOG4CXX_INFO(logger_, "Creating ring name "
                    << downstream_ring_name << " of size " << downstream_ring_size
                );
                downstream_ring = rte_ring_create(
                    downstream_ring_name.c_str(), downstream_ring_size, socket_id_, 0
                );
                if (downstream_ring == NULL)
                {
                    LOG4CXX_ERROR(logger_, "Error creating downstream ring " << downstream_ring_name
                        << " : " << rte_strerror(rte_errno)
                    );
                    // TODO - this is fatal and should raise an exception
                }
            }
            else
            {
                LOG4CXX_DEBUG_LEVEL(2, logger_, "downstream ring with name "
                    << downstream_ring_name << " has already been created"
                );
            }
            if (downstream_ring)
            {
                downstream_rings_.push_back(downstream_ring);
            }

        }

        // Check if the clear_frames ring has already been created by another procsssing core,
        // otherwise create it with the ring size rounded up to the next power of two
        std::string clear_frames_ring_name = ring_name_clear_frames(socket_id_);
        clear_frames_ring_ = rte_ring_lookup(clear_frames_ring_name.c_str());
        if (clear_frames_ring_ == NULL)
        {
            unsigned int clear_frames_ring_size = nearest_power_two(shared_buf_->get_num_buffers());
            LOG4CXX_DEBUG_LEVEL(2, logger_, "Creating frame processed ring name "
                << clear_frames_ring_name << " of size " << clear_frames_ring_size
            );
            clear_frames_ring_ = rte_ring_create(
                clear_frames_ring_name.c_str(), clear_frames_ring_size, socket_id_, 0
            );
            if (clear_frames_ring_ == NULL)
            {
                LOG4CXX_ERROR(logger_, "Error creating frame processed ring " << clear_frames_ring_name
                    << " : " << rte_strerror(rte_errno)
                );
                // TODO - this is fatal and should raise an exception
            }
            else
            {
                // Populate the ring with hugepages memory locations to the SMB
                for (int element = 0; element < shared_buf_->get_num_buffers(); element++)
                {

                    rte_ring_enqueue(clear_frames_ring_, shared_buf_->get_buffer_address(element));
                }
            }
        }
    
    }

    OrcaCaptureCore::~OrcaCaptureCore()
    {
        LOG4CXX_DEBUG_LEVEL(2, logger_, "OrcaCaptureCore destructor");

        // Stop the core polling loop so the run method terminates
        stop();
    }

    bool OrcaCaptureCore::run(unsigned int lcore_id)
    {

        lcore_id_ = lcore_id;
        run_lcore_ = true;

        LOG4CXX_INFO(logger_, "Core " << lcore_id_ << " starting up");

        // Declares

        struct RawFrameHeader *current_frame_header_;
        struct SuperFrameHeader *current_super_frame_buffer_;

        char* frame_src;

        uint64_t frame_size = decoder_->get_frame_bit_depth() * decoder_->get_frame_x_resolution() * decoder_->get_frame_y_resolution();


        uint64_t frame_counter = 0;
        uint64_t dropped_frames_ = 0;

        // Create an io_service object
        boost::asio::io_service io_service;

        // Create an instance of MjpegStreamer with a frame size of 640x480
        auto streamer = std::make_shared<MjpegStreamer>(io_service, 4096, 2304, 100);

        // Start the streamer on port 8080
        streamer->start(9002);

        // Create a separate thread for running the io_service
        std::thread io_thread([&io_service]() {
            io_service.run();
        });

        LOG4CXX_INFO(logger_, "Core " << lcore_id_ << " Connecting to camera\n");

        bool passed = false;


        camera_property_update_ = false;
        in_capture_ = false;

        while (likely(run_lcore_))
        {
            if(camera_property_update_)
            {  
                in_capture_ = false;
                continue;
            }

            

            if(orca_controller_->get_recording())
            {
                // Recording is set to true, capture images
                in_capture_ = true;


                frame_src = orca_controller_->get_frame();

                if(frame_src == nullptr)
                {
                    LOG4CXX_WARN(logger_,
                            "Got NULLPTR for frame");
                    break;
                }
                
                if (unlikely(rte_ring_dequeue(
                    clear_frames_ring_, (void **) &current_super_frame_buffer_
                )) != 0)
                {
                    // If a memory location cannot be found start dropping this frame
                    dropped_frames_++;
                    LOG4CXX_WARN(logger_,
                            "dropping frame: " << frame_counter);
                }
                else
                {
                    // Zero out the frame header to clear old data
                    memset(current_super_frame_buffer_, 0, decoder_->get_frame_buffer_size());

                    // Set the frame number and start time in the header
                    decoder_->set_super_frame_number(current_super_frame_buffer_, frame_counter);
                    decoder_->set_super_frame_start_time(
                        current_super_frame_buffer_, rte_get_tsc_cycles()
                    );

                    current_frame_header_ = decoder_->get_frame_header(current_super_frame_buffer_, 0);

                    // Copy the frame data to the frame struct
                    rte_memcpy(decoder_->get_image_data_start(current_super_frame_buffer_), frame_src, frame_size);

                    if(frame_counter % 10)
                    {
                        streamer->push((uint16_t*)frame_src, frame_size);
                    }

                    rte_ring_enqueue(
                                downstream_rings_[
                                    decoder_->get_super_frame_number(current_super_frame_buffer_) % 
                                    config_.num_downstream_cores
                                ], current_super_frame_buffer_
                            );
                }


                frame_counter++;
            }
            in_capture_ = false;

            rte_delay_ms(100);


        }
        return true;
    }

    void OrcaCaptureCore::stop(void)
    {
        if (run_lcore_)
        {
            LOG4CXX_INFO(logger_, "Core " << lcore_id_ << " stopping");

            run_lcore_ = false;
        }
        else
        {
            LOG4CXX_DEBUG_LEVEL(2, logger_, "Core " << lcore_id_ << " already stopped");
        }
    }

    void OrcaCaptureCore::status(OdinData::IpcMessage& status, const std::string& path)
    {
        LOG4CXX_DEBUG(logger_, "Status requested for Orca_Capture_Core_" << proc_idx_
            << " from the DPDK plugin");

        std::string status_path = path + "/OrcaCaptureCore_" + std::to_string(proc_idx_) + "/";
    }

    bool OrcaCaptureCore::connect(void)
    {


        LOG4CXX_INFO(logger_, "Core " << lcore_id_ << " connecting...");
 
        return true;
    }


    void OrcaCaptureCore::configure(OdinData::IpcMessage& config)
    {
        // Update the config based from the passed IPCmessage

        LOG4CXX_INFO(logger_, config_.core_name << " : " << proc_idx_ << " Got update config: \n" << config);
        

        // while(in_capture_)
        //     continue;

        // camera_.set_property(DCAM_IDPROP_EXPOSURETIME, 0.02);


        // camera_property_update_ = false;
    }

    DPDKREGISTER(DpdkWorkerCore, OrcaCaptureCore, "OrcaCaptureCore");
}