// ICameraInterface.h
#ifndef I_CAMERA_INTERFACE_H
#define I_CAMERA_INTERFACE_H

#include <cstdint>

class ICameraInterface {
public:
    virtual ~ICameraInterface() = default;

    virtual bool api_init() = 0;
    virtual bool connect(int index = 0) = 0;
    virtual bool disconnect() = 0;
    virtual bool disarm() = 0;
    virtual int get_device_count() = 0;
    virtual bool attach_buffer(int32_t numFrames) = 0;
    virtual bool attach_provided_buffer(char* buffer) = 0;
    virtual bool remove_buffer() = 0;
    virtual bool prepare_capture(int32_t frameTimeout = 1000) = 0;
    virtual char* capture_frame() = 0;
    virtual bool abort_capture() = 0;
    virtual bool set_property(int32_t propertyID, double value) = 0;
    virtual double get_property(int32_t propertyID) = 0;
    virtual void close() = 0;
};

#endif // I_CAMERA_INTERFACE_H