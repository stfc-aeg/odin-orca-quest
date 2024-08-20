#include "SimulatedCamera.h"
#include <algorithm>
#include <cstring>
#include <iostream>


SimulatedCamera::SimulatedCamera() : 
        is_connected_(false), 
        is_capturing_(false), 
        frame_count_(0),
        frame_time_(1.0 / 120.0), // 120fps
        last_capture_time_(std::chrono::steady_clock::now()),
        imageGenerator(4096, 2304, 16, ImageFormat::MONO)
        
{
    // Initialize simulated_frame_ with some test data
    simulated_frame_.resize(2304 * 4096 * 2, 0);

}

bool SimulatedCamera::api_init() 
{
    std::cout << "USING SIMULATED CAMERA" << std::endl;
    return true;
}

bool SimulatedCamera::connect(int index) 
{
    is_connected_ = true;
    return true;
}

bool SimulatedCamera::disconnect() 
{
    is_connected_ = false;
    return true;
}

bool SimulatedCamera::disarm() 
{
    is_capturing_ = false;
    return true;
}

int SimulatedCamera::get_device_count() 
{
    return 1;
}

bool SimulatedCamera::attach_buffer(int32_t numFrames) 
{
    return true;
}

bool SimulatedCamera::attach_provided_buffer(char* buffer) 
{
    return true;
}

bool SimulatedCamera::remove_buffer() 
{
    return true;
}

bool SimulatedCamera::prepare_capture(int32_t frameTimeout) 
{
    is_capturing_ = true;
    return true;
}

char* SimulatedCamera::capture_frame() 
{
    if (!is_capturing_) {
        return nullptr;
    }

    // Timer to gate valid frames to a given framerate
    auto current_time = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration<double>(current_time - last_capture_time_).count();

    if (elapsed >= frame_time_) {
            last_capture_time_ = current_time;

            imageGenerator.generateImage(camera_number_, frame_count_);
            frame_count_++;

            return (char*) imageGenerator.getImageData().data();
        }

        return nullptr;
    }

bool SimulatedCamera::abort_capture() {
    is_capturing_ = false;
    return true;
}

bool SimulatedCamera::set_property(const std::string& propertyID, double value) {

    if (propertyID == "exposure_time_")
    {
        frame_time_ = value;
    }

    if (propertyID == "frame_rate_")
    {
        frame_time_ = 1 / value;
    }

    if (propertyID == "camera_number")
    {
        camera_number_ = (int) value;
    }
    return true;
}

double SimulatedCamera::get_property(int32_t propertyID) {
    return 0.0;
}

void SimulatedCamera::close() {
    is_connected_ = false;
    is_capturing_ = false;
}