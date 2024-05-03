from odin_data.ipc_channel import IpcChannel
from odin_data.ipc_message import IpcMessage

from functools import partial
import logging

class OrcaCamera():

    def __init__(self, endpoint, name):

        self.endpoint = endpoint
        self.name = name
        self.msg_id = 0

        self.config = {}
        self.tree = {}

        self.camera = IpcChannel(IpcChannel.CHANNEL_TYPE_DEALER)
        self.camera.connect(endpoint)

        self.tree['camera_name'] = self.name
        self.tree['endpoint'] = self.endpoint
        self.tree['command'] = (lambda: None, self.send_command)

        initial_config = self.request_config()
        if initial_config:
            self.config = initial_config

            self.tree['config'] = {}

            for key, item in initial_config.items():
                self.tree['config'][key] = (lambda key=key:
                    self.config[key], partial(self.set_config, param=key)
                )  # Function uses key (the parameter) as argument via partial

        # self.status = self.request_status()
        self.tree['status'] = (lambda: self.request_status(), None)

    def send_command(self, value):
        """Compose a command message to be sent to the camera.
        :param value: command to be put into the message, PUT from tree
        """
        logging.debug("command: %s", value)
        self.command_msg = {
            "params": {
                "command": value
            }
        }
        self.send_config_message(self.command_msg)

    def set_config(self, value, param):
        """Update local storage of config values and send a command to the camera to update it.
        :param value: argument passed by PUT request to param tree
        :param param: config parameter to update, e.g.: exposure_time
        """
        self.config[param] = value

        self.command_msg = {
            "params": {
                "camera": {
                    param : value
                }
            }
        }
        self.send_config_message(self.command_msg)

    def send_config_message(self, config):
        """Send a configuration message to a given camera.
        :param config: command message dictionary
        """
        msg = IpcMessage('cmd', 'configure', id=self._next_msg_id())
        msg.attrs.update(config)

        logging.info(f"Sending configuration: {config} to {self.camera.identity}.")

        self.camera.send(msg.encode())

        if not self.await_response():
            return False
        self.request_status()  # Update status
        return True

    def request_config(self):
        """Request configuration values from the camera.
        :return: False, or unnested dict of attributes.
        """
        config_msg = IpcMessage('cmd', 'request_configuration', id=self._next_msg_id())
        logging.info(f"Sending config request to {self.camera.identity}.")
        self.camera.send(config_msg.encode())
        reply = self.await_response()
        if reply:
            self.request_status()  # Update status
            return reply.attrs['params']['camera']

    def request_status(self):
        """Get and return the current status of a connected camera.
        :return status: dictionary of current camera status"""
        status_msg = IpcMessage('cmd', 'status', id=self._next_msg_id())
        self.camera.send(status_msg.encode())

        response = self.await_response(silence_reply=True)
        if response:
            return response.attrs['params']['status']
        else:
            return False

    def await_response(self, timeout_ms=5000, silence_reply=False):
        """Await a response on the given camera.
        :param timeout_ms: timeout in milliseconds
        :param silence_reply: silence positive response logging if true. for frequent requests
        :return: response, or False
        """
        pollevents = self.camera.poll(timeout_ms)

        if pollevents == IpcChannel.POLLIN:
            reply = IpcMessage(from_str=self.camera.recv())

            if not silence_reply:
                logging.info(f"Got response from {self.camera.identity}: {reply}")

            return reply
        else:
            logging.debug(f"No response received, or error occurred, from {self.camera.identity}")

            return False

    def _next_msg_id(self):
        """Return the next (incremented) message id."""
        self.msg_id += 1
        return self.msg_id