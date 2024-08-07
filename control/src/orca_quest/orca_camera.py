from odin_data.ipc_channel import IpcChannel
from odin_data.ipc_message import IpcMessage

from tornado.ioloop import PeriodicCallback

from functools import partial
import logging

class OrcaCamera():

    def __init__(self, endpoint, name, status_bg_task_enable, status_bg_task_interval):

        self.endpoint = endpoint
        self.name = name
        self.msg_id = 0

        self.status_bg_task_enable = status_bg_task_enable
        self.status_bg_task_interval = status_bg_task_interval
        self.status = {}

        self.config = {}
        self.tree = {}

        self.timeout_ms = 1000

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

        self.request_status()
        self.tree['status'] = {}
        for key in self.status.keys():
            self.tree['status'][key] = (lambda key=key: self.status[key], None)

        self.tree['background_task'] = {
            "interval": (lambda: self.status_bg_task_interval, self.set_task_interval),
            "enable": (lambda: self.status_bg_task_enable, self.set_task_enable)
        }

        if self.status_bg_task_enable:
            self.start_background_tasks()

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
        if value == "connect":
            # Connecting often takes a few seconds
            self.timeout_ms = 5000

        self.send_config_message(self.command_msg)
        self.request_status()

        if value == "connect":
            self.timeout_ms = 1000  # Reset

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

    def send_config_message(self, config, timeout_ms=1000):
        """Send a configuration message to a given camera.
        :param config: command message dictionary
        """
        msg = IpcMessage('cmd', 'configure', id=self._next_msg_id())
        msg.attrs.update(config)

        logging.info(f"Sending configuration: {config} to {self.camera.identity}.")

        self.camera.send(msg.encode())

        if not self.await_response():
            return False
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
            return reply.attrs['params']['camera']

    def request_status(self):
        """Get the current status of a connected camera and update the class status variable."""
        status_msg = IpcMessage('cmd', 'status', id=self._next_msg_id())
        self.camera.send(status_msg.encode())

        try:
            response = self.await_response(silence_reply=True)
            if response:
                self.status = response.attrs['params']['status']
        except:  # If there is an error, do not update the status
            logging.debug(f"Could not fetch status for camera {self.camera.identity}, not updating.")

    def await_response(self, timeout_ms=100, silence_reply=False):
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

    def status_ioloop_callback(self):
        """Periodic callback task to update camera status."""
        self.request_status()

    def start_background_tasks(self):
        logging.debug(f"Launching camera status update task for {self.name} camera with interval {self.status_bg_task_interval}.")
        self.status_ioloop_task = PeriodicCallback(
            self.status_ioloop_callback, (self.status_bg_task_interval * 1000)
        )
        self.status_ioloop_task.start()

    def stop_background_tasks(self):
        """Stop the background tasks."""
        self.status_bg_task_enable = False
        self.status_ioloop_task.stop()

    def set_task_enable(self, enable):
        """Set the background task enable - accordingly enable or disable the task."""
        enable = bool(enable)

        if enable != self.status_bg_task_enable:
            if enable:
                self.start_background_tasks()
            else:
                self.stop_background_tasks()

    def set_task_interval(self, interval):
        """Set the background task interval."""
        logging.debug("Setting background task interval to %f", interval)
        self.status_bg_task_interval = float(interval)
