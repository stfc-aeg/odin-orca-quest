
#ifndef ORCA_QUEST_CAMERA_H
#define ORCA_QUEST_CAMERA_H

#include <iostream>
#include <string>
#include <cstring>

#include <log4cxx/logger.h>
using namespace log4cxx;
using namespace log4cxx::helpers;
#include <DebugLevelLogger.h>

#include "console4.h"
#include "dcamapi4.h"
#include "dcamprop.h"

class OrcaQuestCamera {
public:
    OrcaQuestCamera();
    ~OrcaQuestCamera();

    bool api_init();
    bool connect(int index = 0);
    bool disconnect();
    bool disarm();
    int get_device_count();
    bool attach_buffer(int32 numFrames);
    bool attach_provided_buffer(char* buffer);
    bool remove_buffer();
    bool prepare_capture(int32 frameTimeout=1000);
    char* capture_frame();
    bool abort_capture();
    bool set_property(int32 propertyID, double value);
    double get_property(int32 propertyID);
    void close();

    _DCAMIDPROP camera_properties;
    _DCAMPROPMODEVALUE property_values;

private:
    DCAMAPI_INIT api_init_param_;
    DCAMDEV_OPEN device_handle_;   
    DCAMERR err_;
    HDCAM orca_;
    DCAMWAIT_OPEN wait_open_handle_;
    HDCAMWAIT orcaWait_;
    int32 frame_size_;
    DCAMBUF_FRAME frameBuffer_;
    DCAMWAIT_START frameReady_Waiter_;
    DCAMCAP_TRANSFERINFO captransferinfo_;

    LoggerPtr logger_;

    bool buffer_type_;

    bool check_dcam_err(DCAMERR err, const std::string& action);

};

#endif // ORCA_QUEST_CAMERA_H




