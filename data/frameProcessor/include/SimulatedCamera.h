#ifndef MOCK_CAMERA_H
#define MOCK_CAMERA_H

#include "ICameraInterface.h"
#include "SimulatedImageGenerator.h"
#include <vector>
#include <chrono>

class SimulatedCamera : public ICameraInterface {
public:
    SimulatedCamera();
    ~SimulatedCamera() override = default;

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
    bool set_property(int32_t propertyID, double value) override;
    double get_property(int32_t propertyID) override;
    void close() override;

private:
    std::vector<char> simulated_frame_;
    bool is_connected_;
    bool is_capturing_;
    uint64_t frame_count_;
    double frame_time_;

    std::chrono::steady_clock::time_point last_capture_time_;

    SimulatedImageGenerator imageGenerator;
};

#endif // MOCK_CAMERA_H