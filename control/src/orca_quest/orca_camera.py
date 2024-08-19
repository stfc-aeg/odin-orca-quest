from odin_data.control.ipc_channel import IpcChannel
from odin_data.control.ipc_message import IpcMessage

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
        self.error_consecutive = 0

        self.camera = IpcChannel(IpcChannel.CHANNEL_TYPE_DEALER)
        self.camera.connect(endpoint)

        self.tree['camera_name'] = self.name
        self.tree['endpoint'] = self.endpoint
        self.tree['command'] = (lambda: None, self.send_command)

        self.get_status_config(msg='request_configuration')  # Goes to self.config
        if self.config:
            self.tree['config'] = {}

            for key, item in self.config.items():
                self.tree['config'][key] = (lambda key=key:
                    self.config[key], partial(self.set_config, param=key)
                )  # Function uses key (the parameter) as argument via partial

        self.get_status_config(msg='status')
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
        self.get_status_config(msg='status', silence_reply=True)

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

    def get_status_config(self, msg, silence_reply=False):
        """Identify if the response is for setting the status or config."""
        cmd_msg = IpcMessage('cmd', msg, id=self._next_msg_id())
        self.camera.send(cmd_msg.encode())

        try:
            response = self.await_response(silence_reply=silence_reply)
            if response:
                if ('camera' in response.attrs['params'].keys()):
                    self.config = response.attrs['params']['camera']
                elif ('status' in response.attrs['params'].keys()):
                    self.status = response.attrs['params']['status']
                else:
                    logging.debug(f"Got unexpected response structure from {self.camera.identity}.")
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

            self.error_consecutive = 0
            return reply
        else:
            self.error_consecutive += 1
            logging.debug(f"No response received, or error occurred, from {self.camera.identity}")
            return False

    def _next_msg_id(self):
        """Return the next (incremented) message id."""
        self.msg_id += 1
        return self.msg_id

    def status_ioloop_callback(self):
        """Periodic callback task to update camera status."""
        if self.error_consecutive >= 10:
            # After 10 consecutive errors, halt the background task
            logging.debug("Multiple consecutive errors in camera response. Halting periodic request task.")
            self.stop_background_tasks()
        self.get_status_config(msg='status', silence_reply=True)
        self.get_status_config(msg='request_configuration', silence_reply=True)

    def start_background_tasks(self):
        """Start the background tasks and reset the continuous error counter."""
        self.error_consecutive = 0

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
