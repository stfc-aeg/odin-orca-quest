#ifndef INCLUDE_ORCAQUESTCAMERACONTROLLER_H_
#define INCLUDE_ORCAQUESTCAMERACONTROLLER_H_

#include <vector>

#include <log4cxx/logger.h>
using namespace log4cxx;
using namespace log4cxx::helpers;
#include <DebugLevelLogger.h>
#include <IpcMessage.h>
#include "OrcaQuestCameraConfiguration.h"
#include <ParamContainer.h>

#include "OrcaQuestCamera.h"
#include "ICameraInterface.h"
#include "CameraFactory.h"
#include "OrcaCaptureCoreConfiguration.h"
#include "OrcaQuestCameraStatus.h"

#include "DpdkSharedBuffer.h"
#include "DpdkCoreConfiguration.h"
#include "DpdkWorkerCore.h"
#include "ProtocolDecoder.h"

#include "OrcaQuestCameraStateMachine.h"

namespace FrameProcessor
{
    const std::string CAMERA_CONFIG_PATH = "camera";
    const std::string CAMERA_COMMAND_PATH = "command";

    class OrcaQuestCameraController
    {
    public:
        //! Constructor for the controller taking a pointer to the decoder as an argument
        OrcaQuestCameraController(ProtocolDecoder* decoder, const rapidjson::Value& camera_config);

        //! Destructor for the controller class
        ~OrcaQuestCameraController();

        //! Run the specified command
        bool execute_command(std::string& command);
        
        //! Connect to the camera
        bool connect();

        //! Disconnect from the camera
        bool disconnect();

        //! Arm the camera, ready for capture
        bool arm_camera();

        //! Disarm camera
        bool disarm_camera();

        //! Start capturing frames
        bool start_capture();

        //! End current capture
        bool end_capture();

        //! Returns the name of the current camera state
        std::string camera_state_name(void);

        void configure(OdinData::IpcMessage& config_msg, OdinData::IpcMessage& config_reply);

        bool get_recording(void);

        char* get_frame(void);

        bool update_configuration(OdinData::ParamContainer::Document& params);

        bool apply_configuration();

        bool request_configuration(const std::string param_prefix, OdinData::IpcMessage& config_reply);

        bool get_status(const std::string param_prefix, OdinData::IpcMessage& config_reply);



    private:
        char* last_frame_;
        bool recording_;

        OrcaQuestCameraConfiguration camera_config_;
        OrcaQuestCameraStatus camera_status_;

        ProtocolDecoder* decoder_;

        std::unique_ptr<ICameraInterface> camera_;
        OrcaQuestCameraState camera_state_;

        LoggerPtr logger_;

        friend class OrcaCaptureCore;
    };
}
#endif