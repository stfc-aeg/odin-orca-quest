#ifndef INCLUDE_ORCACONTROLCORE_H_
#define INCLUDE_ORCACONTROLCORE_H_

#include <vector>

#include <log4cxx/logger.h>
using namespace log4cxx;
using namespace log4cxx::helpers;
#include <DebugLevelLogger.h>

#include "DpdkWorkerCore.h"
#include "DpdkSharedBuffer.h"
#include "DpdkCoreConfiguration.h"
#include "OrcaControlConfiguration.h"
#include "ProtocolDecoder.h"
#include "OrcaQuestCameraController.h"

#include <rte_ring.h>

#include "IpcChannel.h"
#include "IpcMessage.h"
#include <boost/scoped_ptr.hpp>
#include <boost/thread.hpp>

namespace FrameProcessor
{
    class OrcaControlCore : public DpdkWorkerCore
    {
    public:
        OrcaControlCore(
            int proc_idx, int socket_id, DpdkWorkCoreReferences dpdkWorkCoreReferences
        );
        ~OrcaControlCore();

        bool run(unsigned int lcore_id);
        void stop(void);
        void status(OdinData::IpcMessage& status, const std::string& path);
        bool connect(void);
        void configure(OdinData::IpcMessage& config);

    private:

        int proc_idx_;
        ProtocolDecoder* decoder_;
        DpdkSharedBuffer* shared_buf_;

        OrcaQuestControlCoreConfiguration config_;
        LoggerPtr logger_;

        OdinData::IpcChannel Orca_Ctrl_Channel_;

        OrcaQuestCameraController* OrcaQuestCameraController_;

        struct rte_ring* clear_frames_ring_;
        std::vector<struct rte_ring*> downstream_rings_;
    };
}
#endif // INCLUDE_OrcaControlCore_H_