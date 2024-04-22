"""Class to control camera. Initially, IPC communication as this is how the cameras will be interacted with."""

from odin_data.ipc_channel import IpcChannel
from odin_data.ipc_message import IpcMessage
from odin.adapters.parameter_tree import ParameterTree, ParameterTreeError

import logging

class OrcaError(Exception):
    """Simple exception class to wrap lower-level exceptions."""
    pass

class OrcaController():
    """Class to send commands via IPC to ORCA-Quest camera."""

    def __init__(self, instance_count):
        """This constructor initialises the object and builds parameter trees."""

        self.port = 9001
        self.ctrl_channels = []

        for i in range(instance_count):
            channel = IpcChannel(IpcChannel.CHANNEL_TYPE_DEALER)
            endpoint = f"tcp://192.168.0.31:{self.port + i}"  # Different port for each
            channel.connect(endpoint)
            self.ctrl_channels.append(channel)

        logging.debug("instance_count: %s", instance_count)
        logging.debug("ctrl_channels: %s", self.ctrl_channels)

        # tree
        self.latest_command = None
        self.param_tree = ParameterTree({
            'command': (lambda: self.latest_command, self.send_command)
        })

        self.msg_id = 0

    def next_msg_id(self):
        """Return the next message id."""

        self.msg_id += 1
        return self.msg_id

    def send_command(self, value):
        """Compose a command message to be sent."""

        self.command = {}
        self.command["command"] = value

        self.command_msg = {
            "params": self.command
        }
        self.send_config_message(self.command_msg)
    
    def send_config_message(self, config):
        """Send a configuration message."""

        all_responses_valid = True

        for channel in self.ctrl_channels:
            msg = IpcMessage('cmd', 'configure', id=self.next_msg_id())
            msg.attrs.update(config)
            # logging
            logging.debug(f"Sending configuration: {config} to {channel.identity}")

            channel.send(msg.encode())
            if not self.await_response(channel):
                # await response from channel
                all_responses_valid = False

        return all_responses_valid

    def await_response(self, channel, timeout_ms=10000):
        """Await a response on the given channel."""

        pollevents = channel.poll(timeout_ms)

        if pollevents == IpcChannel.POLLIN:
            reply = IpcMessage(from_str=channel.recv())

            logging.debug(f"Got response from {channel.identity}: {reply}")

            return reply
        else:
            logging.debug(f"No response received or error occurred from {channel.identity}.")

            return False

    def get(self, path):
        """Get the parameter tree.
        This method returns the parameter tree for use by clients via the FurnaceController adapter.
        :param path: path to retrieve from tree
        """
        return self.param_tree.get(path)

    def set(self, path, data):
        """Set parameters in the parameter tree.
        This method simply wraps underlying ParameterTree method so that an exceptions can be
        re-raised with an appropriate LiveXError.
        :param path: path of parameter tree to set values for
        :param data: dictionary of new data values to set in the parameter tree
        """
        try:
            self.param_tree.set(path, data)
        except ParameterTreeError as e:
            raise LiveXError(e)
