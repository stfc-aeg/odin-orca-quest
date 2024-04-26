"""Class to control camera. Initially, IPC communication as this is how the cameras will be interacted with."""

from odin_data.ipc_channel import IpcChannel
from odin_data.ipc_message import IpcMessage
from odin.adapters.parameter_tree import ParameterTree, ParameterTreeError

from functools import partial

import logging

class OrcaError(Exception):
    """Simple exception class to wrap lower-level exceptions."""
    pass

class OrcaController():
    """Class to send commands via IPC to ORCA-Quest camera."""

    def __init__(self, num_cameras, endpoints, names):
        """This constructor initialises the object and builds parameter trees."""

        self.cameras = []
        self.msg_id = 0

        tree = {}
        camtrees = []

        if len(endpoints) < num_cameras:
            raise OrcaError("Fewer endpoints defined than number of cameras in config.")

        for i in range(num_cameras):
            camera = IpcChannel(IpcChannel.CHANNEL_TYPE_DEALER)
            endpoint = endpoints[i]
            camera.connect(endpoint)
            self.cameras.append(camera)

            # Each camera has its own part of the tree kept in an array
            camera_subtree = {}
            camera_subtree["camera_name"] = names[i]
            camera_subtree["endpoint"] = endpoint
            camera_subtree["command"] = (lambda: None, partial(self.send_command, camera=camera))
            camera_subtree["config"] = self.request_config(camera)
            camtrees.append(camera_subtree)

        tree['cameras'] = camtrees  # Array of cameras here

        self.param_tree = ParameterTree(tree)

        logging.debug("instance_count: %s", num_cameras)
        logging.debug("cameras: %s", self.cameras)


    def next_msg_id(self):
        """Return the next message id."""
        self.msg_id += 1
        return self.msg_id

    def request_config(self, camera):

        config_msg = IpcMessage('cmd', 'request_configuration', id=self.next_msg_id())
        logging.info(f"Sending config request to {camera.identity}")
        camera.send(config_msg.encode())
        reply = self.await_response(camera)
        if reply:
            return reply.attrs['params']['camera']
        else:
            return False

    def send_command(self, camera, value):
        """Compose a command message to be sent to a given camera."""

        self.command = {}
        self.command["command"] = value

        self.command_msg = {
            "params": self.command
        }
        self.send_config_message(camera, self.command_msg)
    
    def send_config_message(self, camera, config):
        """Send a configuration message to a given camera."""

        msg = IpcMessage('cmd', 'configure', id=self.next_msg_id())
        msg.attrs.update(config)
        # logging
        logging.debug(f"Sending configuration: {config} to {camera.identity}")

        camera.send(msg.encode())
        if not self.await_response(camera):
            # await response from camera
            return False
        return True


    def send_config_message_all(self, config):
        """Send a configuration message to all cameras."""

        all_responses_valid = True

        for camera in self.cameras:
            msg = IpcMessage('cmd', 'configure', id=self.next_msg_id())
            msg.attrs.update(config)
            # logging
            logging.debug(f"Sending configuration: {config} to {camera.identity}")

            camera.send(msg.encode())
            if not self.await_response(camera):
                # await response from camera
                all_responses_valid = False

        return all_responses_valid

    def await_response(self, camera, timeout_ms=5000):
        """Await a response on the given camera."""

        pollevents = camera.poll(timeout_ms)

        if pollevents == IpcChannel.POLLIN:
            reply = IpcMessage(from_str=camera.recv())

            logging.debug(f"Got response from {camera.identity}: {reply}")

            return reply
        else:
            logging.debug(f"No response received or error occurred from {camera.identity}.")

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
            raise OrcaError(e)
