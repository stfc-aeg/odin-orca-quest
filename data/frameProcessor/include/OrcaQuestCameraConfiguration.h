#ifndef INCLUDE_ORCAQUESTCAMERACONFIGURATION_H_
#define INCLUDE_ORCAQUESTCAMERACONFIGURATION_H_

#include "ParamContainer.h"

namespace FrameProcessor
{
    namespace Defaults
    {
        const unsigned int default_camera_number = 0; 
        const double default_image_timeout = 10; 
        const unsigned int default_number_frames = 0; 
        const unsigned int default_timestamp_mode = 2;

        const int default_trigger_source = 1;
        const int default_trigger_mode = 1;
        const int default_trigger_active = 1;
        const int default_trigger_polarity = 1;
        const int default_trigger_connector = 2;

        const bool default_simulated_camera = true;
    }

    class OrcaQuestCameraConfiguration : public OdinData::ParamContainer
    {

        public:

            //! Default constructor
            //!
            //! This constructor initialises all parameters to default values and binds the
            //! parameters in the container, allowing them to accessed via the path-like set/get
            //! mechanism implemented in ParamContainer.

            OrcaQuestCameraConfiguration() :
                ParamContainer(),
                camera_number_(Defaults::default_camera_number),
                image_timeout_(Defaults::default_image_timeout),
                num_frames_(Defaults::default_number_frames),
                timestamp_mode_(Defaults::default_timestamp_mode),
                exposure_time_(0.0),
                frame_rate_(0.0),
                trigger_source_(Defaults::default_trigger_source),
                trigger_active_(Defaults::default_trigger_mode),
                trigger_mode_(Defaults::default_trigger_active),
                trigger_polarity_(Defaults::default_trigger_polarity),
                trigger_connector_(Defaults::default_trigger_connector),
                camera_simulated_mode_(Defaults::default_simulated_camera)
            {
                // Bind the parameters in the container
                bind_params();
            }

            //! Copy constructor
            //!
            //! This constructor creates a copy of an existing configuration object. All parameters
            //! are first bound and then the underlying parameter container is updated from the
            //! existing object. This mechansim is necessary (rather than relying on a default copy
            //! constructor) since it is not possible for a parameter container to automatically
            //! rebind parameters defined in a derived class.
            //!
            //! \param config - existing config object to copy

            OrcaQuestCameraConfiguration(const OrcaQuestCameraConfiguration& config) :
                ParamContainer(config)
            {
                // Bind the parameters in the container
                bind_params();

                // Update the container from the existing config object
                update(config);
            }

        private:

            //! Bind parameters in the container
            //!
            //! This method binds all the parameters in the container to named paths. This method
            //! is separated out so that it can be used in both default and copy constructors.

            virtual void bind_params(void)
            {
                bind_param<unsigned int>(camera_number_, "camera_number");
                bind_param<double>(image_timeout_, "image_timeout");
                bind_param<unsigned int>(num_frames_, "num_frames");
                bind_param<unsigned int>(timestamp_mode_, "timestamp_mode");
                bind_param<double>(exposure_time_, "exposure_time");
                bind_param<double>(frame_rate_, "frame_rate");
                bind_param<int>(trigger_source_, "trigger_source");
                bind_param<int>(trigger_active_, "trigger_active");
                bind_param<int>(trigger_mode_, "trigger_mode");
                bind_param<int>(trigger_polarity_, "trigger_polarity");
                bind_param<int>(trigger_connector_, "trigger_connector");
                bind_param<bool>(camera_simulated_mode_, "simulated_camera");
            }

            unsigned int camera_number_;     //!< Camera number as enumerated by driver
            unsigned int frame_number_;
            double image_timeout_;        //!< Image acquisition timeout in seconds
            unsigned int num_frames_;     //!< Number of frames to acquire, 0 = no limit
            unsigned int timestamp_mode_; //!< Camera timestamp mode
            double exposure_time_;        //!< Exposure time in seconds
            double frame_rate_;           //!< Frame rate in Hertz
            bool camera_simulated_mode_;     //!< Enabled simulated camera for testing


            // TRIGGER SOURCE	            Internal	|        Software	            External
            // TRIGGER ACTIVE	            -	        |   Edge	Level	Pulse	|   Edge	Level	Pulse
            //             	   NORMAL	    OK	        |   OK	    OK	    Few	    |   OK	    OK	    Few
            // TRIGGER MODE    PIV	        NG	        |   OK	    None	None	|   OK	    None   None
            //                 START	    NG	        |   OK	    NG	    NG	    |   OK	    NG	    NG
            //
            // OK:	    The combination is supported.
            // NG:	    The combination is not defined in DCAM-API
            // Few:	    The combination is defined but a few cameras support.
            // None:    The combination can be defined but no cameras support it.


            // Setting for triggering source for frame capture\my
            // 1 = internal
            // 2 = External
            // 3 = Software
            // 4 = Master Pulse
            int trigger_source_;

            // Setting for when trigger activates
            // 1 = Edge
            // 2 = Level
            // 3 = Pulse? /* reserved */
            int trigger_active_;

            // Setting for triggering mode
            // 1 = Normal
            // 2 = PIV
            // 3 = START
            int trigger_mode_;

            // Setting for external trigger polarity
            // 1 = Falling edge or LOW level
            // 2 = Rising edge or HIGH level
            int trigger_polarity_;

            // Setting for external trigger connector
            // 1 = BNC
            // 2 = INTERFACE
            // 3 = MULTI
            int trigger_connector_;


            // Allow the OrcaQuestCameraController class direct access to config parameters
            friend class OrcaQuestCameraController;
            friend class OrcaQuestCamera;
            friend class OrcaCaptureCore;

    };
}

#endif // INCLUDE_ORCAQUESTCAMERACONFIGURATION_H_