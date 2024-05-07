#ifndef INCLUDE_LIVEVIEWCORE_H_
#define INCLUDE_LIVEVIEWCORE_H_

#include <vector>

#include <log4cxx/logger.h>
using namespace log4cxx;
using namespace log4cxx::helpers;
#include <DebugLevelLogger.h>
#include "LiveviewCore.h"

#include "IpcChannel.h"
#include "IpcMessage.h"
#include "DpdkWorkerCore.h"
#include "DpdkSharedBuffer.h"
#include "DpdkCoreConfiguration.h"
#include "OrcaCaptureConfiguration.h"
#include "ProtocolDecoder.h"
#include "OrcaQuestCameraController.h"
#include "OrcaQuestCamera.h"

#include <rte_ring.h>

namespace FrameProcessor
{
    class LiveviewCore : public DpdkWorkerCore
    {
    public:
        LiveviewCore(
            int proc_idx, int socket_id, DpdkWorkCoreReferences dpdkWorkCoreReferences
        );
        ~LiveviewCore();

        bool run(unsigned int lcore_id);
        void stop(void);
        void status(OdinData::IpcMessage& status, const std::string& path);
        bool connect(void);
        void configure(OdinData::IpcMessage& config);

    private:

        int proc_idx_;
        ProtocolDecoder* decoder_;
        DpdkSharedBuffer* shared_buf_;

        OrcaCaptureConfiguration config_;
        LoggerPtr logger_;
        
        OdinData::IpcChannel Liveview_ipc_;

        bool camera_property_update_;
        bool in_capture_;


        struct rte_ring* clear_frames_ring_;
        std::vector<struct rte_ring*> downstream_rings_;
    };
}
#endif // INCLUDE_LIVEVIEWCORE_H_