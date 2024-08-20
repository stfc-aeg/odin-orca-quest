#include "OrcaQuestCameraController.h"

namespace FrameProcessor{

// Constructor
OrcaQuestCameraController::OrcaQuestCameraController(
    ProtocolDecoder* decoder,
    const rapidjson::Value& camera_config
) :
    decoder_(decoder),
    camera_state_(this),
    logger_(Logger::getLogger("FR.OrcaQuestCameraController")),
    recording_(false)
{
    // Parse the camera configuration to determine if we're using a simulated camera
    bool simulated_camera = false;
    if (camera_config.HasMember("simulated_camera_") && camera_config["simulated_camera_"].IsBool()) {
        simulated_camera = camera_config["simulated_camera_"].GetBool();
    }

    // Create the appropriate camera
    if (simulated_camera) {
        LOG4CXX_INFO(logger_, "Creating Mocked Camera");
        camera_ = CameraFactory::createCamera("mock");
    } else {
        LOG4CXX_INFO(logger_, "Using Real Camera");
        camera_ = CameraFactory::createCamera("real");
    }

    // Create a ParamContainer::Document to pass to update_configuration
    OdinData::ParamContainer::Document config_doc;
    config_doc.CopyFrom(camera_config, config_doc.GetAllocator());

    LOG4CXX_INFO(logger_, "update_configuration");

    // Call update_configuration with the camera config
    update_configuration(config_doc);

    LOG4CXX_INFO(logger_, "initiate");

    camera_state_.initiate();

    LOG4CXX_INFO(logger_, "Finish controller init");
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
    bool connected = camera_->api_init();

    if (connected == false)
    {
        LOG4CXX_ERROR(logger_, "Failed to init API");
        return false;
    }

    connected = camera_->connect(0);
    if (connected == false)
    {
        LOG4CXX_ERROR(logger_, "Failed to connected to camera");
        return false;
    }
    camera_->attach_buffer(10);
    camera_->prepare_capture(camera_config_.image_timeout_);

    // Apply default config if available

    this->apply_configuration();


    // Return the connection status
    return connected;
}

bool OrcaQuestCameraController::disconnect()
{
    camera_->disarm();
    camera_->disconnect();
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
            config_reply.set_nack("Camera configuration update failed");
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

    // unsigned int camera_number_;  //!< Camera number as enumerated by driver
    // double image_timeout_;        //!< Image acquisition timeout in seconds
    // unsigned int num_frames_;     //!< Number of frames to acquire, 0 = no limit
    // unsigned int timestamp_mode_; //!< Camera timestamp mode
    // double exposure_time_;        //!< Exposure time in seconds
    // double frame_rate_;           //!< Frame rate in Hertz

    // Create a copy of the current camera configuration for comparison and then update with
    // the new parameter document

    OrcaQuestCameraConfiguration new_config(camera_config_);
    new_config.update(params);

    LOG4CXX_DEBUG(logger_, "Checking camera_number_ configuration: " << camera_config_.camera_number_ << " to: " << new_config.camera_number_ );
    if (new_config.camera_number_ != camera_config_.camera_number_)
    {
        LOG4CXX_INFO(logger_, "Updating camera number from: " << camera_config_.camera_number_ << " to: " << new_config.camera_number_);
        if (!camera_->set_property("camera_number_", new_config.camera_number_))
        {   
            LOG4CXX_ERROR(logger_, "Failed to update camera number");
            return false;
        }
        camera_config_.camera_number_ = new_config.camera_number_;
    }

    LOG4CXX_DEBUG(logger_, "Checking image_timeout_ configuration");
    if (new_config.image_timeout_ != camera_config_.image_timeout_)
    {
        LOG4CXX_INFO(logger_,  "Updating image timeout from: " << camera_config_.image_timeout_ << " to: " << new_config.image_timeout_);
        camera_config_.image_timeout_ = new_config.image_timeout_;
    }

    LOG4CXX_DEBUG(logger_, "Checking num_frames_ configuration");
    if (new_config.num_frames_ != camera_config_.num_frames_)
    {
        LOG4CXX_INFO(logger_, "Updating number of frames from: " << camera_config_.num_frames_ << " to: " << new_config.num_frames_);
        camera_config_.num_frames_ = new_config.num_frames_;
    }

    LOG4CXX_DEBUG(logger_, "Checking exposure_time_ configuration");
    if (new_config.exposure_time_ != camera_config_.exposure_time_)
    {
        LOG4CXX_INFO(logger_, "Updating exposure time from: " << camera_config_.exposure_time_ << " to: " << new_config.exposure_time_);
        if (!camera_->set_property("exposure_time_", new_config.exposure_time_))
        {
            LOG4CXX_ERROR(logger_, "Failed to update exposure time");
            return false;
        }
        camera_config_.exposure_time_ = new_config.exposure_time_;
    }

    // LOG4CXX_DEBUG(logger_, "Checking frame_rate_ configuration");
    // if (new_config.frame_rate_ != camera_config_.frame_rate_)
    // {
    //     LOG4CXX_INFO(logger_, "Updating frame rate from: " << camera_config_.frame_rate_ << " to: " << new_config.frame_rate_);
    //     if (!camera_->set_property("frame_rate_", new_config.frame_rate_))
    //     {
    //         LOG4CXX_ERROR(logger_, "Failed to update frame rate");
    //         return false;
    //     }
    //     camera_config_.frame_rate_ = new_config.frame_rate_;
    // }

    LOG4CXX_DEBUG(logger_, "Checking trigger_source_ configuration");
    if (new_config.trigger_source_ != camera_config_.trigger_source_)
    {
        LOG4CXX_INFO(logger_, "Updating trigger source from: " << camera_config_.trigger_source_ << " to: " << new_config.trigger_source_);
        if (!camera_->set_property("trigger_source_", new_config.trigger_source_))
        {
            LOG4CXX_ERROR(logger_, "Failed to update trigger source");
            return false;
        }
        camera_config_.trigger_source_ = new_config.trigger_source_;
    }

    // LOG4CXX_DEBUG(logger_, "Checking trigger_active_ configuration");
    // if (new_config.trigger_active_ != camera_config_.trigger_active_)
    // {
    //     LOG4CXX_INFO(logger_, "Updating trigger active from: " << camera_config_.trigger_active_ << " to: " << new_config.trigger_active_);
    //     if (!camera_->set_property("trigger_active_", new_config.trigger_active_))
    //     {
    //         LOG4CXX_ERROR(logger_, "Failed to update trigger active");
    //         return false;
    //     }
    //     camera_config_.trigger_active_ = new_config.trigger_active_;
    // }

    // LOG4CXX_DEBUG(logger_, "Checking trigger_mode_ configuration");
    // if (new_config.trigger_mode_ != camera_config_.trigger_mode_)
    // {
    //     LOG4CXX_INFO(logger_, "Updating trigger mode from: " << camera_config_.trigger_mode_ << " to: " << new_config.trigger_mode_);
    //     if (!camera_->set_property("trigger_mode_", new_config.trigger_mode_))
    //     {
    //         LOG4CXX_ERROR(logger_, "Failed to update trigger mode");
    //         return false;
    //     }
    //     camera_config_.trigger_mode_ = new_config.trigger_mode_;
    // }

    // LOG4CXX_DEBUG(logger_, "Checking trigger_polarity_ configuration");
    // if (new_config.trigger_polarity_ != camera_config_.trigger_polarity_)
    // {
    //     LOG4CXX_INFO(logger_, "Updating trigger polarity from: " << camera_config_.trigger_polarity_ << " to: " << new_config.trigger_polarity_);
    //     if (!camera_->set_property("trigger_polarity_", new_config.trigger_polarity_))
    //     {
    //         LOG4CXX_ERROR(logger_, "Failed to update trigger polarity");
    //         return false;
    //     }
    //     camera_config_.trigger_polarity_ = new_config.trigger_polarity_;
    // }

    // LOG4CXX_DEBUG(logger_, "Checking trigger_connector_ configuration");
    // if (new_config.trigger_connector_ != camera_config_.trigger_connector_)
    // {
    //     LOG4CXX_INFO(logger_, "Updating trigger connector from: " << camera_config_.trigger_connector_ << " to: " << new_config.trigger_connector_);
    //     if (!camera_->set_property("trigger_connector_", new_config.trigger_connector_))
    //     {
    //         LOG4CXX_ERROR(logger_, "Failed to update trigger connector");
    //         return false;
    //     }
    //     camera_config_.trigger_connector_ = new_config.trigger_connector_;
    // }

    LOG4CXX_DEBUG(logger_, "Configuration update completed successfully");
    return true;
}

bool OrcaQuestCameraController::apply_configuration()
{

    // Apply the config to the camera this needs to be done after connection to the camera


    LOG4CXX_DEBUG(logger_, "Applying config to camera");


    LOG4CXX_INFO(logger_, "Updating camera number to: " << camera_config_.camera_number_);
    if (!camera_->set_property("camera_number_", camera_config_.camera_number_))
    {   
        LOG4CXX_ERROR(logger_, "Failed to update camera number");
        return false;
    }


    LOG4CXX_INFO(logger_, "Updating exposure time to: " << camera_config_.exposure_time_);
    if (!camera_->set_property("exposure_time_", camera_config_.exposure_time_))
    {
        LOG4CXX_ERROR(logger_, "Failed to update exposure time");
        return false;
    }


    LOG4CXX_INFO(logger_, "Updating frame rate to: " << camera_config_.frame_rate_);
    if (!camera_->set_property("frame_rate_", camera_config_.frame_rate_))
    {
        LOG4CXX_ERROR(logger_, "Failed to update frame rate");
        return false;
    }


    LOG4CXX_INFO(logger_, "Updating trigger source to: " << camera_config_.trigger_source_);
    if (!camera_->set_property("trigger_source_", camera_config_.trigger_source_))
    {
        LOG4CXX_ERROR(logger_, "Failed to update trigger source");
        return false;
    }



    // LOG4CXX_INFO(logger_, "Updating trigger active to: " << camera_config_.trigger_active_);
    // if (!camera_->set_property("trigger_active_", camera_config_.trigger_active_))
    // {
    //     LOG4CXX_ERROR(logger_, "Failed to update trigger active");
    //     return false;
    // }



    // LOG4CXX_INFO(logger_, "Updating trigger mode to: " << camera_config_.trigger_mode_);
    // if (!camera_->set_property("trigger_mode_", camera_config_.trigger_mode_))
    // {
    //     LOG4CXX_ERROR(logger_, "Failed to update trigger mode");
    //     return false;
    // }



    // LOG4CXX_INFO(logger_, "Updating trigger polarity to: " << camera_config_.trigger_polarity_);
    // if (!camera_->set_property("trigger_polarity_", camera_config_.trigger_polarity_))
    // {
    //     LOG4CXX_ERROR(logger_, "Failed to update trigger polarity");
    //     return false;
    // }



    // LOG4CXX_INFO(logger_, "Updating trigger connector to: " << camera_config_.trigger_connector_);
    // if (!camera_->set_property("trigger_connector_", camera_config_.trigger_connector_))
    // {
    //     LOG4CXX_ERROR(logger_, "Failed to update trigger connector");
    //     return false;
    // }

    LOG4CXX_DEBUG(logger_, "Configuration update completed successfully");
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
    return camera_->capture_frame();
}

} // namespace FrameProcessor