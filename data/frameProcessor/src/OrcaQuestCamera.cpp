#include "OrcaQuestCamera.h"


OrcaQuestCamera::OrcaQuestCamera():
logger_(Logger::getLogger("FP.OrcaQuestCamera"))
{}


OrcaQuestCamera::~OrcaQuestCamera() {
    close();
    if (dcamapi_uninit() != DCAMERR_SUCCESS) {
        LOG4CXX_WARN(logger_,"Failed to uninitialize DCAM API.");
    }
}


int OrcaQuestCamera::get_device_count() {

    return api_init_param_.iDeviceCount;
}


bool OrcaQuestCamera::api_init() {
    memset(&api_init_param_, 0, sizeof(api_init_param_));
    api_init_param_.size = sizeof(api_init_param_);

    err_ = dcamapi_init(&api_init_param_);
    if (!failed(err_)) {
        LOG4CXX_INFO(logger_, "DCAM API initialized successfully.");
        return true;
    }
    else
    {
        LOG4CXX_WARN(logger_, "DCAM API failed to initialize.");

        char errtext[ 256 ];

        // err, hdcam, errid, errtext, sizeof(errtext)
        DCAMDEV_STRING	param;
        memset( &param, 0, sizeof(param) );
        param.size		= sizeof(param);
        param.text		= errtext;
        param.textbytes	= sizeof(errtext);
        param.iString	= err_;
        
        err_ = dcamdev_getstring( orca_, &param );

        LOG4CXX_WARN(logger_, errtext);
        return false;
    }
}

bool OrcaQuestCamera::connect(int index) {
    
    memset(&device_handle_, 0, sizeof(device_handle_));
    device_handle_.size = sizeof(device_handle_);
    device_handle_.index = index; // only have single camera connected

    err_ = dcamdev_open(&device_handle_);
    if (failed(err_)) {
        LOG4CXX_WARN(logger_, "Failed to open connection to camera. Stopping.");
        dcamapi_uninit();
        return false;
    }
    orca_ = device_handle_.hdcam;

    memset(&wait_open_handle_, 0, sizeof(wait_open_handle_));
    wait_open_handle_.size = sizeof(wait_open_handle_);
    wait_open_handle_.hdcam = orca_;

    err_ = dcamwait_open(&wait_open_handle_);
    if (failed(err_)) {
        LOG4CXX_WARN(logger_, "Failed to open wait handle. Stopping.");
        dcamapi_uninit();
        return false;
    }

    orcaWait_ = wait_open_handle_.hwait;
    LOG4CXX_INFO(logger_, "Connected to camera.");
    return true;
}

bool OrcaQuestCamera::disconnect() {
    
    dcamwait_close( orcaWait_ );

    dcamdev_close( orca_ );

    dcamapi_uninit();
    return true;
}

bool OrcaQuestCamera::disarm() {

    err_ = dcamcap_stop(orca_);
    if (failed(err_)) {
        LOG4CXX_WARN(logger_, "Failed to stop capture capture.");
        return false;
    }
    
    err_ = dcambuf_release(orca_);
    if (failed(err_)) {
        LOG4CXX_WARN(logger_, "Failed to release buffers capture capture.");
        return false;
    }
    return true;
}

bool OrcaQuestCamera::attach_buffer(int32 numFrames) {
    if (orca_ == nullptr) return false;
    
    err_ = dcambuf_alloc(orca_, numFrames);
    if (failed(err_)) {
        LOG4CXX_WARN(logger_, "Failed to alloc buffer for " << numFrames << " frames.");
        return false;
    }
    LOG4CXX_INFO(logger_, "Attached " << numFrames << " buffers to framegrabber.");
    return true;
}

bool OrcaQuestCamera::remove_buffer() {
    if (orca_ == nullptr) return false;

    if(buffer_type_)
    {
        err_ = dcambuf_release(orca_);
        return !failed(err_);
    }
    else
    {
        
    }

    return false;
}

bool OrcaQuestCamera::prepare_capture(int32 frameTimeout) {
    if (orca_ == nullptr) return false;

    memset(&frameReady_Waiter_, 0, sizeof(frameReady_Waiter_));
    frameReady_Waiter_.size = sizeof(frameReady_Waiter_);
    frameReady_Waiter_.eventmask = DCAMWAIT_CAPEVENT_FRAMEREADY;
    frameReady_Waiter_.timeout = frameTimeout;

    memset(&frameBuffer_, 0, sizeof(frameBuffer_));
    frameBuffer_.size = sizeof(frameBuffer_);
    frameBuffer_.iFrame = -1; // latest frame

    err_ = dcamcap_start(orca_, DCAMCAP_START_SEQUENCE);
    if (failed(err_)) {
        LOG4CXX_WARN(logger_,"Failed to start capture.");
        return false;
    }
    LOG4CXX_INFO(logger_, "Camera armed for capture.");
    return true;
}

char* OrcaQuestCamera::capture_frame() {
    if (orca_ == nullptr) return NULL;

    err_ = dcamwait_start(orcaWait_, &frameReady_Waiter_);
    if (failed(err_)) {
        //LOG4CXX_DEBUG(logger_,"Error capturing frame");
        return NULL;
    }

    err_ = dcambuf_lockframe(orca_, &frameBuffer_);
    if (failed(err_)) {
        LOG4CXX_WARN(logger_,"Error locking frame");
        return NULL;
    }

    return reinterpret_cast<char*>(frameBuffer_.buf);
}

bool OrcaQuestCamera::abort_capture() {
    if (orcaWait_ == nullptr) return false;
    err_ = dcamwait_abort(orcaWait_);
    return check_dcam_err(err_, "Abort capture");
}

bool OrcaQuestCamera::set_property(int32 propertyID, double value) {
    if (orca_ == nullptr) return false;
    err_ = dcamprop_setvalue(orca_, propertyID, value);
    return check_dcam_err(err_, "Set property");
}

double OrcaQuestCamera::get_property(int32 propertyID) {
    if (orca_ == nullptr) return 0.0;
    double value;
    err_ = dcamprop_getvalue(orca_, propertyID, &value);
    if (!check_dcam_err(err_, "Get property")) return 0.0;
    return value;
}

void OrcaQuestCamera::close() {
    if (orca_ != nullptr) {
        remove_buffer();
        dcamdev_close(orca_);
        orca_ = nullptr;
    }
}

bool OrcaQuestCamera::check_dcam_err(DCAMERR err, const std::string& action) {
    if (err != DCAMERR_SUCCESS) {
        LOG4CXX_WARN(logger_,"Error during " << action << ": " << err);
        return false;
    }
    return true;
}