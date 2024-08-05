#ifndef INCLUDE_ORCACAPTURECONFIG_H_
#define INCLUDE_ORCACAPTURECONFIG_H_


#include "ParamContainer.h"
#include "DpdkCoreConfiguration.h"
#include <sstream>

namespace FrameProcessor
{
    namespace Defaults
    {
        // Place all default values here
        const unsigned int default_frame_timeout = 1000;
    }


    class OrcaCaptureConfiguration : public OdinData::ParamContainer
    {

        public:

            OrcaCaptureConfiguration() :
                ParamContainer(),
                frame_timeout_(Defaults::default_frame_timeout)
            {
                bind_params();
            }

            void resolve(DpdkCoreConfiguration& core_config_)
            {
                const ParamContainer::Value* value_ptr =
                    core_config_.get_worker_core_config("orca_capture");

                if (value_ptr != nullptr)
                {
                    update(*value_ptr);
                }        
            }

        private:

            virtual void bind_params(void)
            {
                bind_param<std::string>(core_name, "core_name");
                bind_param<std::string>(connect, "connect");
                bind_param<unsigned int>(num_cores, "num_cores");
                bind_param<unsigned int>(num_downstream_cores, "num_downstream_cores");
                bind_param<unsigned int>(frame_timeout_, "frame_timeout");

            }

            std::string core_name;
            std::string connect;
            unsigned int num_cores;
            unsigned int num_downstream_cores;
            // Specfic config
            unsigned int frame_timeout_;

            friend class OrcaCaptureCore;
    };
}

#endif // INCLUDE_ORCACAPTURECONFIG_H_