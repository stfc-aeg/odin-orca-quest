#include "OrcaQuestCameraStateMachine.h"
#include "OrcaQuestCameraController.h"

namespace sc = boost::statechart;

namespace FrameProcessor {

OrcaQuestCameraState::OrcaQuestCameraState(OrcaQuestCameraController* controller) :
    controller_(controller)
{
    // State transition events
    EventConnect::custom_static_type_ptr("connect");
    EventDisconnect::custom_static_type_ptr("disconnect");
    EventStartCapture::custom_static_type_ptr("start_capture");
    EventEndCapture::custom_static_type_ptr("end_capture");
}

void OrcaQuestCameraState::execute_command(const char* command)
{
    std::string cmd_str(command);
    this->execute_command(cmd_str);
}

void OrcaQuestCameraState::execute_command(std::string& command)
{
    CommandType command_type = map_command_to_type(command);

    std::cout << command << std::endl;

    if (command_type == CommandUnknown)
    {
        std::stringstream ss;
        ss << "Unknown camera state transition command: " << command;
        throw(std::runtime_error(ss.str()));
    }
    this->execute_command(command_type);
}

void OrcaQuestCameraState::execute_command(OrcaQuestCameraState::CommandType command)
{
    // Use std::lock_guard instead of boost::lock_guard
    std::lock_guard<std::mutex> transition_lock(state_transition_mutex_);

    switch (command)
    {
        case CommandConnect:
            process_event(EventConnect());
            break;
        case CommandDisconnect:
            process_event(EventDisconnect());
            break;
        case CommandStartCapture:
            process_event(EventStartCapture());
            break;
        case CommandEndCapture:
            process_event(EventEndCapture());
            break;
        case CommandUnknown:
        default:
            {
                std::stringstream ss;
                ss << "Unknown camera state transition command type: " << (int)command;
                throw(std::runtime_error(ss.str()));
            }
            break;
    }
}

void OrcaQuestCameraState::unconsumed_event(const sc::event_base& event)
{
    std::stringstream ss;
    ss << event.custom_dynamic_type_ptr<char>() << " is not valid in "
        << current_state_name() << " state";

    throw(std::runtime_error(ss.str()));
}

OrcaQuestCameraState::CommandType OrcaQuestCameraState::map_command_to_type(std::string& command)
{
    CommandType command_type = CommandUnknown;

    if (command_type_map_.size() == 0)
    {
        init_command_type_map();
    }

    if (command_type_map_.left.count(command))
    {
        command_type = command_type_map_.left.at(command);
    }
    return command_type;
}

std::string OrcaQuestCameraState::map_state_to_name(StateType state_type)
{
    std::string state_name = "unknown";

    if (state_type_map_.size() == 0)
    {
        init_state_type_map();
    }

    if (state_type_map_.right.count(state_type))
    {
        state_name = state_type_map_.right.at(state_type);
    }
    return state_name;
}

std::string OrcaQuestCameraState::current_state_name(void)
{
    return map_state_to_name(current_state());
}

OrcaQuestCameraState::StateType OrcaQuestCameraState::current_state(void)
{
    return state_cast<const IStateInfo&>().state_type();
}

void OrcaQuestCameraState::init_command_type_map(void)
{
    command_type_map_.insert(CommandTypeMapEntry("connect",    CommandConnect));
    command_type_map_.insert(CommandTypeMapEntry("disconnect", CommandDisconnect));
    command_type_map_.insert(CommandTypeMapEntry("capture",    CommandStartCapture));
    command_type_map_.insert(CommandTypeMapEntry("end_capture", CommandEndCapture));
}

void OrcaQuestCameraState::init_state_type_map(void)
{
    state_type_map_.insert(StateTypeMapEntry("disconnected", StateOff));
    state_type_map_.insert(StateTypeMapEntry("connected",    StateConnected));
    state_type_map_.insert(StateTypeMapEntry("capturing",    StateCapturing));
}

sc::result Off::react(const EventConnect&)
{
    if (outermost_context().controller_->connect())
    {
        return transit<Connected>();
    }
    else
    {
        // Return discard_event() if the connect operation fails
        return discard_event();
    }
}

sc::result Connected::react(const EventDisconnect&)
{
    if (outermost_context().controller_->disconnect())
    {
        return transit<Off>();
    }
    else
    {
        // Return discard_event() if the disconnect operation fails
        return discard_event();
    }
}

sc::result Connected::react(const EventStartCapture&)
{
    if (outermost_context().controller_->start_capture())
    {
        return transit<Capturing>();
    }
    else
    {
        // Return discard_event() if the start capture operation fails
        return discard_event();
    }
}

sc::result Capturing::react(const EventEndCapture&)
{
    if (outermost_context().controller_->end_capture())
    {
        return transit<Connected>();
    }
    else
    {
        // Return discard_event() if the end capture operation fails
        return discard_event();
    }
}

} // namespace FrameProcessor