#ifndef INCLUDE_ORCAQUESTCAMERASTATUS_H_
#define INCLUDE_ORCAQUESTCAMERASTATUS_H_

#include "ParamContainer.h"

namespace FrameProcessor
{
    namespace Defaults
    {
        const unsigned int frame_number = 0; 
        const std::string camera_status = "disconnected"; 
    }

    class OrcaQuestCameraStatus : public OdinData::ParamContainer
    {

        public:

            //! Default constructor
            //!
            //! This constructor initialises all parameters to default values and binds the
            //! parameters in the container, allowing them to accessed via the path-like set/get
            //! mechanism implemented in ParamContainer.

            OrcaQuestCameraStatus() :
                ParamContainer(),
                frame_number_(Defaults::frame_number),
                camera_status_(Defaults::camera_status)
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

            OrcaQuestCameraStatus(const OrcaQuestCameraStatus& config) :
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
                bind_param<std::string>(camera_status_, "camera_status");
                bind_param<unsigned int>(frame_number_, "frame_number");
            }

            std::string camera_status_;     //!< Camera status as reported by the state machine
            unsigned int frame_number_;        //!< Latest captured frame number 

            //! Allow the OrcaQuestCameraController class direct access to config parameters
            friend class OrcaQuestCameraController;
            friend class OrcaQuestCamera;
            friend class OrcaCaptureCore;

    };
}

#endif // INCLUDE_ORCAQUESTCAMERASTATUS_H_