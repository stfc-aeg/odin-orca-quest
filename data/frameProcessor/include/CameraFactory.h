#ifndef CAMERA_FACTORY_H
#define CAMERA_FACTORY_H

#include "ICameraInterface.h"
#include "OrcaQuestCamera.h"
#include "SimulatedCamera.h"
#include <memory>
#include <string>
#include <stdexcept>

class CameraFactory {
public:
    static std::unique_ptr<ICameraInterface> createCamera(const std::string& type) {
        if (type == "real") {
            return std::make_unique<OrcaQuestCamera>();
        } else if (type == "mock") {
            return std::make_unique<SimulatedCamera>();
        }
        throw std::runtime_error("Invalid camera type");
    }
};

#endif // CAMERA_FACTORY_H