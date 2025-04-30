// OrcaQuestCamera.h
#ifndef ORCA_QUEST_CAMERA_H
#define ORCA_QUEST_CAMERA_H

#include "ICameraInterface.h"
#include <iostream>
#include <string>
#include <cstring>
#include <unordered_map>

#include <log4cxx/logger.h>
using namespace log4cxx;
using namespace log4cxx::helpers;
#include <DebugLevelLogger.h>

#include "console4.h"
#include "dcamapi4.h"
#include "dcamprop.h"

class OrcaQuestCamera : public ICameraInterface {
public:
    OrcaQuestCamera();
    ~OrcaQuestCamera() override;

    bool api_init() override;
    bool connect(int index = 0) override;
    bool disconnect() override;
    bool disarm() override;
    int get_device_count() override;
    bool attach_buffer(int32_t numFrames) override;
    bool attach_provided_buffer(char* buffer) override;
    bool remove_buffer() override;
    bool prepare_capture(int32_t frameTimeout = 1000) override;
    char* capture_frame() override;
    bool abort_capture() override;
    bool set_property(const std::string& propertyID, double value) override;
    double get_property(int32_t propertyID) override;
    void close() override;

    _DCAMIDPROP camera_properties;
    _DCAMPROPMODEVALUE property_values;

private:

    bool set_dcam_property(int32 propertyID, double value);

    DCAMAPI_INIT api_init_param_;
    DCAMDEV_OPEN device_handle_;   
    DCAMERR err_;
    HDCAM orca_;
    DCAMWAIT_OPEN wait_open_handle_;
    HDCAMWAIT orcaWait_;
    int32_t frame_size_;
    DCAMBUF_FRAME frameBuffer_;
    DCAMWAIT_START frameReady_Waiter_;
    DCAMCAP_TRANSFERINFO captransferinfo_;

    int num_frames_;
    int frame_rate_;
    int camera_number_;

    LoggerPtr logger_;

    bool buffer_type_;

    bool check_dcam_err(DCAMERR err, const std::string& action);
};

#endif // ORCA_QUEST_CAMERA_H