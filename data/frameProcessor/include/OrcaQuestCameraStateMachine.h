#ifndef INCLUDE_ORCAQUESTCAMERASTATEMACHINE_H_
#define INCLUDE_ORCAQUESTCAMERASTATEMACHINE_H_

#include <iostream>
#include <mutex>  // Include the mutex header for std::lock_guard
#include <boost/statechart/event.hpp>
#include <boost/statechart/state_machine.hpp>
#include <boost/statechart/simple_state.hpp>
#include <boost/statechart/state.hpp>
#include <boost/statechart/transition.hpp>
#include <boost/statechart/custom_reaction.hpp>
#include <boost/mpl/list.hpp>
#include <boost/bimap.hpp>

namespace sc = boost::statechart;
namespace mpl = boost::mpl;

namespace FrameProcessor {

    // Forward declaration of the controller class
    class OrcaQuestCameraController;

    // Definition of state machine events
    struct EventConnect : sc::event<EventConnect> {};
    struct EventDisconnect : sc::event<EventDisconnect> {};
    struct EventStartCapture : sc::event<EventStartCapture> {};
    struct EventEndCapture : sc::event<EventEndCapture> {};

    // Forward declaration of states
    struct Off;
    struct Connected;
    struct Capturing;

    // OrcaQuestCameraState - state machine for the OrcaQuest camera controller
    struct OrcaQuestCameraState : sc::state_machine<OrcaQuestCameraState, Off>
    {
        // State transition command type enumeration
        enum CommandType
        {
            CommandUnknown = -1,
            CommandConnect,
            CommandDisconnect,
            CommandStartCapture,
            CommandEndCapture
        };

        // State type enumeration
        enum StateType
        {
            StateUnknown = -1,
            StateOff,
            StateConnected,
            StateCapturing
        };

        // Command type map type definition
        typedef boost::bimap<std::string, CommandType> CommandTypeMap;
        // Command type map entry type definition
        typedef CommandTypeMap::value_type CommandTypeMapEntry;
        // State type map type definition
        typedef boost::bimap<std::string, StateType> StateTypeMap;
        // State type map entry type definition
        typedef StateTypeMap::value_type StateTypeMapEntry;

        // Constructor for the OrcaQuestCameraState class
        OrcaQuestCameraState(OrcaQuestCameraController* controller);

        // Executes a state transition command
        void execute_command(const char* command);
        void execute_command(std::string& command);
        void execute_command(CommandType command);

        // Handles unconsumed, i.e. invalid, state transition events
        void unconsumed_event(const sc::event_base& event);

        // Maps a command name string to a command type value
        CommandType map_command_to_type(std::string& command);

        // Maps a state type to a state name
        std::string map_state_to_name(StateType state_type);

        // Returns the current state name
        std::string current_state_name(void);

        // Returns the current state type
        StateType current_state(void);

        // Initialises the command type map
        void init_command_type_map(void);

        // Initialises the state type map
        void init_state_type_map(void);

        OrcaQuestCameraController* controller_; // Pointer to controller instance
        CommandTypeMap command_type_map_;       // Command type map
        StateTypeMap state_type_map_;           // State type map
        std::mutex state_transition_mutex_;     // State transition mutex
    };

    // IStateInfo - state information interface class
    struct IStateInfo
    {
        // Returns the current state type value
        virtual OrcaQuestCameraState::StateType state_type(void) const = 0;
    };

    // State class definitions
    struct Off : IStateInfo, sc::state<Off, OrcaQuestCameraState>
    {
        // Defines reactions to legal events
        typedef sc::custom_reaction<EventConnect> reactions;

        Off(my_context ctx) : my_base(ctx) {};
        sc::result react(const EventConnect&);
        OrcaQuestCameraState::StateType state_type(void) const { return OrcaQuestCameraState::StateType::StateOff; }
    };

    struct Connected : IStateInfo, sc::state<Connected, OrcaQuestCameraState>
    {
        // Defines reactions to legal events
        typedef mpl::list<
            sc::custom_reaction<EventDisconnect>,
            sc::custom_reaction<EventStartCapture>
        > reactions;

        Connected(my_context ctx) : my_base(ctx) {};
        sc::result react(const EventDisconnect&);
        sc::result react(const EventStartCapture&);
        OrcaQuestCameraState::StateType state_type(void) const { return OrcaQuestCameraState::StateType::StateConnected; }
    };

    struct Capturing : IStateInfo, sc::state<Capturing, OrcaQuestCameraState>
    {
        // Defines reactions to legal events
        typedef sc::custom_reaction<EventEndCapture> reactions;

        Capturing(my_context ctx) : my_base(ctx) {};
        sc::result react(const EventEndCapture&);
        OrcaQuestCameraState::StateType state_type(void) const { return OrcaQuestCameraState::StateType::StateCapturing; }
    };
}
#endif // INCLUDE_ORCAQUESTCAMERASTATEMACHINE_H_