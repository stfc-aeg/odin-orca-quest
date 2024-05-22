#include "OrcaQuestCameraController.h"

namespace FrameProcessor{

// Constructor
OrcaQuestCameraController::OrcaQuestCameraController(ProtocolDecoder* decoder) :
    decoder_(decoder),
    camera_state_(this),
    logger_(Logger::getLogger("FR.OrcaQuestCameraController")),
    recording_(false)
{
    // Constructor implementation goes here.
    // Initialize decoder, loggers, or any other necessary components.
    camera_state_.initiate();
    std::string connect = "connect";
    execute_command(connect);
}

// Destructor
OrcaQuestCameraController::~OrcaQuestCameraController()
{
    // Clean up resources if needed.
}

// Execute command
bool OrcaQuestCameraController::execute_command(std::string& command)
{
    bool command_ok = true;

    try
    {
        camera_state_.execute_command(command);
    }
    catch (const std::bad_cast& e)
    {
        LOG4CXX_ERROR(logger_, "std::bad_cast exception caught while executing " << command << " command: " << e.what());
        command_ok = false;
    }
    catch (const std::exception& e)
    {
        LOG4CXX_ERROR(logger_, "Exception caught while executing " << command << " command: " << e.what());
        command_ok = false;
    }
    LOG4CXX_INFO(logger_, "Camera state is now: " << camera_state_.current_state_name());

    return command_ok;
}

//! Connect to the camera
bool OrcaQuestCameraController::connect()
{
    camera_.api_init();
    bool connected = camera_.connect(camera_config_.camera_number_);
    camera_.attach_buffer(10);
    camera_.prepare_capture(camera_config_.image_timeout_);
    // Return the connection status
    return connected;
}

bool OrcaQuestCameraController::disconnect()
{
    camera_.disarm();
    camera_.disconnect();
    // Return the disconnection status
    return true;
}

bool OrcaQuestCameraController::start_capture()
{
    camera_status_.frame_number_ = 0;
    recording_ = true;
    // Return the start capture status
    return true;
}

bool OrcaQuestCameraController::end_capture()
{
    recording_ = false;
    // Return the end capture status
    return true;
}

void OrcaQuestCameraController::configure(OdinData::IpcMessage& config_msg, OdinData::IpcMessage& config_reply)
{

    // Update camera config
    if (config_msg.has_param(CAMERA_CONFIG_PATH))
    {
        LOG4CXX_DEBUG_LEVEL(2, logger_, "Config request has update config: ");
        OdinData::ParamContainer::Document config_params;
        config_msg.encode_params(config_params, CAMERA_CONFIG_PATH);
        if (!this->update_configuration(config_params))
        {
            config_reply.set_nack("Camera configuration udpate failed");
        }
    }

    // Send camera a command
    if (config_msg.has_param(CAMERA_COMMAND_PATH))
    {
        std::string command = config_msg.get_param<std::string>(CAMERA_COMMAND_PATH);
        LOG4CXX_DEBUG_LEVEL(2, logger_, "Config request has command: " << command);
        if (!this->execute_command(command))
        {
            config_reply.set_nack("Camera " + command + " command failed");
        }
        else
        {
            config_reply.set_nack("");  // Set an empty nack to indicate success
        }

        camera_status_.camera_status_ = camera_state_.current_state_name();
    }
    
}

bool OrcaQuestCameraController::update_configuration(OdinData::ParamContainer::Document& params)
{

    // unsigned int camera_number_;     //!< Camera number as enumerated by driver
    // double image_timeout_;        //!< Image acquisition timeout in seconds
    // unsigned int num_frames_;     //!< Number of frames to acquire, 0 = no limit
    // unsigned int timestamp_mode_; //!< Camera timestamp mode
    // double exposure_time_;        //!< Exposure time in seconds
    // double frame_rate_;           //!< Frame rate in Hertz

    // Create a copy of the current camera configuration for comparison and then update with
    // the new parameter document
    OrcaQuestCameraConfiguration new_config(camera_config_);
    new_config.update(params);


    if (new_config.camera_number_ != camera_config_.camera_number_)
    {
        std::cout << "Updated camera number from: " << camera_config_.camera_number_ << " to: " << new_config.camera_number_ << std::endl;
        // Update camera index
        camera_config_.camera_number_ = new_config.camera_number_;
    }

    if (new_config.image_timeout_ != camera_config_.image_timeout_)
    {
        std::cout << "Updated image timeout from: " << camera_config_.image_timeout_ << " to: " << new_config.image_timeout_ << std::endl;
        // Update image timeout
        camera_config_.image_timeout_ = new_config.image_timeout_;
    }

    if (new_config.num_frames_ != camera_config_.num_frames_)
    {
        std::cout << "Updated number of frames from: " << camera_config_.num_frames_ << " to: " << new_config.num_frames_ << std::endl;
        // Update number of frames to capture
        camera_config_.num_frames_ = new_config.num_frames_;
    }

    if (new_config.exposure_time_ != camera_config_.exposure_time_)
    {
        std::cout << "Updated exposure time from: " << camera_config_.exposure_time_ << " to: " << new_config.exposure_time_ << std::endl;
        // Update exposure
        if (!camera_.set_property(0x001F0110, new_config.exposure_time_))
        {
            // Check if there was an error setting the camera property
            return false;
        }
        camera_config_.exposure_time_ = new_config.exposure_time_;
    }

    if (new_config.frame_rate_ != camera_config_.frame_rate_)
    {
        std::cout << "Updated frame rate from: " << camera_config_.frame_rate_ << " to: " << new_config.frame_rate_ << std::endl;
        // Update frame rate
        //camera_.set_property(0x001F0110, new_config.frame_rate_);
    }


    if (new_config.trigger_source_ != camera_config_.trigger_source_)
    {
        std::cout << "Updated trigger source from: " << camera_config_.trigger_source_ << " to: " << new_config.trigger_source_ << std::endl;
        // Update frame rate
        if (!camera_.set_property(0x00100110, new_config.trigger_source_))
        {
            // Check if there was an error setting the camera property
            return false;
        }
        camera_config_.trigger_source_ = new_config.trigger_source_;
    }


    if (new_config.trigger_active_ != camera_config_.trigger_active_)
    {
        std::cout << "Updated trigger active from: " << camera_config_.trigger_active_ << " to: " << new_config.trigger_active_ << std::endl;
        // Update frame rate
        if (!camera_.set_property(0x00100120, new_config.trigger_active_))
        {
            // Check if there was an error setting the camera property
            return false;
        }
        camera_config_.trigger_active_ = new_config.trigger_active_;
    }


    if (new_config.trigger_mode_ != camera_config_.trigger_mode_)
    {
        std::cout << "Updated trigger mode from: " << camera_config_.trigger_mode_ << " to: " << new_config.trigger_mode_ << std::endl;
        // Update frame rate
        
        if (!camera_.set_property(0x00100210, new_config.trigger_mode_))
        {
            // Check if there was an error setting the camera property
            return false;
        }
    }


    if (new_config.trigger_polarity_ != camera_config_.trigger_polarity_)
    {
        std::cout << "Updated trigger polarity from: " << camera_config_.trigger_polarity_ << " to: " << new_config.trigger_polarity_ << std::endl;
        // Update trigger polarity
        
        if (!camera_.set_property(0x00100220, new_config.trigger_polarity_))
        {
            // Check if there was an error setting the camera property
            return false;
        }
    }

    if (new_config.trigger_connector_ != camera_config_.trigger_connector_)
    {
        std::cout << "Updated trigger connector from: " << camera_config_.trigger_connector_ << " to: " << new_config.trigger_connector_ << std::endl;
        // Update trigger connector
        
        if (!camera_.set_property(0x00100230, new_config.trigger_connector_))
        {
            // Check if there was an error setting the camera property
            return false;
        }
    }


    return true;        
}

bool OrcaQuestCameraController::request_configuration(const std::string param_prefix, OdinData::IpcMessage& config_reply)
{

    OdinData::ParamContainer::Document camera_config;

    std::string camera_config_prefix = param_prefix + CAMERA_CONFIG_PATH;

    camera_config_.encode(camera_config, camera_config_prefix);
    config_reply.update(camera_config);

    return true;
}

bool OrcaQuestCameraController::get_status(const std::string param_prefix, OdinData::IpcMessage& config_reply)
{

    camera_status_.camera_status_ = camera_state_.current_state_name();

    OdinData::ParamContainer::Document camera_status;

    std::string camera_status_prefix = "status";

    camera_status_.encode(camera_status, camera_status_prefix);
    config_reply.update(camera_status);

    return true;
}

// Camera state name
std::string OrcaQuestCameraController::camera_state_name()
{
    return camera_state_.current_state_name();
}

bool OrcaQuestCameraController::get_recording()
{
    return recording_;
}

char* OrcaQuestCameraController::get_frame()
{
    return camera_.capture_frame();
}

} // namespace FrameProcessor