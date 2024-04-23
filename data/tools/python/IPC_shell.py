import argparse
import json
import logging
import os
import sys

from odin_data.control.ipc_channel import IpcChannel
from odin_data.control.ipc_message import IpcMessage

try:
    from json.decoder import JSONDecodeError
except ImportError:
    JSONDecodeError = ValueError


class OdinDataClient(object):
    """odin-data client.

    This class implements the odin-data command-line client.
    """

    def __init__(self, instance_count):
        """Initialise the odin-data client."""
        prog_name = os.path.basename(sys.argv[0])
        self.logger = self.setup_logger(prog_name)

        # Parse command line arguments
        self.args = self.parse_arguments(prog_name)

        # Create the appropriate IPC channels
        self.ctrl_channels = []  # Changed to a list to hold multiple channels
        base_port = 9001
        for i in range(instance_count):
            channel = IpcChannel(IpcChannel.CHANNEL_TYPE_DEALER)
            endpoint = f"tcp://192.168.0.31:{base_port + i}"
            channel.connect(endpoint)
            self.ctrl_channels.append(channel)

        self._msg_id = 0
        self._run = True
        self.pending_updates = {}

    def setup_logger(self, prog_name):
        """Set up the logger for the client."""
        logger = logging.getLogger(prog_name)
        logger.setLevel(logging.DEBUG)

        ch = logging.StreamHandler(sys.stdout)
        ch.setLevel(logging.DEBUG)

        formatter = logging.Formatter('%(asctime)s %(levelname)s %(name)s - %(message)s')
        ch.setFormatter(formatter)

        logger.addHandler(ch)

        return logger

    def parse_arguments(self, prog_name):
        """Parse arguments from the command line."""
        parser = argparse.ArgumentParser(prog=prog_name, description='odin-data application client')
        parser.add_argument('--ctrl', type=str, default='tcp://127.0.0.1:5000',
                            dest='ctrl_endpoint',
                            help='Specify the IPC control channel endpoint URL')

        args = parser.parse_args()
        return args

    def _next_msg_id(self):
        """Return the next IPC message ID to use."""
        self._msg_id += 1
        return self._msg_id

    def run(self):
        """Run the odin-data client."""
        self.logger.info("odin-data client starting up")
        for connected_channel in self.ctrl_channels:
            self.logger.debug("Control IPC channel has identity {}".format(connected_channel.identity))

        # Enter interactive shell mode
        try:
            self.interactive_shell()
        except KeyboardInterrupt:
            self.logger.info("Interactive shell closed.")

    def interactive_shell(self):
        """Interactive shell for setting parameters and sending messages."""
        self.logger.info("Enter 'send' to send all updates, 'exit' to quit, 'status' to get status, 'config' to get configuration, or 'help' to show help information.")
        self.logger.info("Use 'set <category> <key> <value>' to set parameters.")
        self.logger.info("Use 'get <json_key_path>' to retrieve values from the status JSON (e.g., 'get hdf frames_written').")

        while self._run:
            try:
                user_input = input("odin-data> ").strip()
                if user_input.lower() == 'exit':
                    self._run = False
                elif user_input.lower() == 'send':
                    self.send_all_updates()
                elif user_input.lower() == 'start':
                    self.start_sequence()
                elif user_input.lower() == 'status':
                    print(self.get_status())
                elif user_input.lower() == 'config':
                    print(self.get_config())
                elif user_input.lower().startswith('set '):
                    _, category, key, value = user_input.split(maxsplit=3)
                    self.set_parameter(category, key, value)
                elif user_input.lower().startswith('command '):
                    _, command = user_input.split(maxsplit=3)
                    self.send_command(command)
                elif user_input.lower().startswith('config '):
                    _, config, value = user_input.split(maxsplit=4)
                    self.send_config(config, value)
                elif user_input.lower().startswith('req_config '):
                    self.request_config()
                elif user_input.lower().startswith('acquisition '):
                    _, path, acquisition_id, frames = user_input.split(maxsplit=3)
                    self.acquisition(path, acquisition_id, frames)
                elif user_input.lower().startswith('get '):
                    # Split the rest of the input into the JSON keys
                    _, *keys = user_input.split()
                    # Retrieve and print the value(s)
                    for output in keys:
                        print(key)
                    value = self.get_value_from_status(*keys)
                    print(f"Value for {' '.join(keys)}: {value}")
                elif user_input.lower() == 'help':
                    self.help_command()
                else:
                    self.logger.error("Unknown command or incomplete command.")
            except ValueError as e:
                self.logger.error(f"Incorrect command format: {e}")
            except Exception as e:
                self.logger.error(f"An error occurred: {e}")



    def set_parameter(self, category, key, value):
        """Set a parameter in the pending updates."""
        if category not in self.pending_updates:
            self.pending_updates[category] = {}

        # Here we need to parse the value type
        if value.lower() in ['true', 'false']:
            value = value.lower() == 'true'
        else:
            try:
                value = int(value)
            except ValueError:
                try:
                    value = float(value)
                except ValueError:
                    pass  # keep as string

        self.pending_updates[category][key] = value
        self.logger.info(f"Set parameter: {category} {key} = {value}")

    def send_command(self, value):
        """send a command."""

        self.command_ = {}

        self.command_["command"] = value

        self.command_msg = {
            "params": self.command_
        }

        self.send_config_message(self.command_msg)


    def request_config(self):
        """Get the camera config from odin-data"""


        all_responses_valid = True
        for channel in self.ctrl_channels:
            config_msg = IpcMessage('cmd', 'request_configuration', id=self._next_msg_id())
            self.logger.info(f"Sending config request to {channel.identity}")
            channel.send(config_msg.encode())
            if not self.await_response(channel):  # pass the channel to await_response
                all_responses_valid = False
        return all_responses_valid


    def send_config(self, config, value):
        """send a config message."""

        # Here we need to parse the value type
        if value.lower() in ['true', 'false']:
            value = value.lower() == 'true'
        else:
            try:
                value = int(value)
            except ValueError:
                try:
                    value = float(value)
                except ValueError:
                    pass  # keep as string

        self.command_msg = {
            "params": {
                "camera": {
                    config : value
                }
            }
        }

        self.send_config_message(self.command_msg)

    def send_all_updates(self):
        """Send all pending updates to odin-data."""
        if not self.pending_updates:
            self.logger.info("No updates to send.")
            return

        # Prepare the update message
        update_msg = {
            "params": self.pending_updates
        }

        self.send_config_message(update_msg)

        # Clear  updates 
        self.pending_updates.clear()

    def receive_reply(self):
        """Receive a reply from odin-data."""
        poll_success = self.ctrl_channel.poll(10000)
        if poll_success:
            reply = IpcMessage(from_str=self.ctrl_channel.recv())
            self.logger.info("Received reply: {}".format(reply))
        else:
            self.logger.warning("No reply received within timeout period")
    
    def start_sequence(self):
        """Start the configuration sequence."""
        self.logger.info("Starting the configuration sequence...")

        # First configuration message
        first_config = {
            "params": {
                "hibirdsdpdk": {
                    "update_config": True,
                    "rx_enable": False,
                    "proc_enable": True
                }
            }
        }
        if self.send_config_message(first_config):
            # If the first config was successful, send the second one
            second_config = {
                "params": {
                    "hibirdsdpdk": {
                        "update_config": True,
                        "rx_enable": True
                    }
                }
            }
            self.send_config_message(second_config)
            self.logger.info("Configuration sequence completed.")
            return True
        else:
            self.logger.error("Failed to send the first configuration message. Aborting sequence.")
            return False

    def send_config_message(self, config):
        """Send a configuration message to all instances."""
        all_responses_valid = True
        for channel in self.ctrl_channels:
            config_msg = IpcMessage('cmd', 'configure', id=self._next_msg_id())
            config_msg.attrs.update(config)
            #config_msg
            self.logger.info(f"Sending configuration: {config} to {channel.identity}")
            channel.send(config_msg.encode())
            if not self.await_response(channel):  # pass the channel to await_response
                all_responses_valid = False
        return all_responses_valid

    def await_response(self, channel, timeout_ms=10000):
        """Await a response to a client command on the given channel."""
        pollevents = channel.poll(timeout_ms)
        if pollevents == IpcChannel.POLLIN:
            reply = IpcMessage(from_str=channel.recv())
            self.logger.info(f"Got response from {channel.identity}: {reply}")
            return reply
        else:
            self.logger.error(f"No response received or error occurred from {channel.identity}.")
            return False


    def help_command(self):
        """Display the help information for the command-line interface."""
        help_text = """
        Interactive OdinData Client Shell Commands:

        exit                            Exits the interactive shell.
        status                          Gets the current status of odin-data.
        config                          Get the current config of odin-data.
        send                            Sends all pending updates to odin-data.
        start                           Send the start sequence to begin a capture.
        set <category> <key> <value>    Sets a parameter for the specified category and key.
                                        Strings do not need quotes. True/False for booleans.
                                        Numbers will be interpreted as integers or floats.

        acquisition <path> <id> <frames> Sets up a new acquisition with given path, acquisition id, and frame count.
                                        The path is where the data will be stored,
                                        the acquisition id is a filename,
                                        and frames is the number of frames to capture.
                                        Use the start command to begin capturing data.

        Examples:
        set hibirdsdpdk rx_enable true  Sets 'rx_enable' to true in hibirdsdpdk.
        set hibirdsdpdk rx_frames 50000 Sets 'rx_frames' to 50000 in hibirdsdpdk.
        acquisition /data test_123 1000 Starts an acquisition with path /data, filename test_123, and 1000 frames.

        If you want to update any hibirdsdpdk config, then you must include set hibirdsdpdk update_config true

        Type 'help' to show this help information.
        """
        print(help_text)

    def get_status(self):
        """Get and display the current status of all connected odin-data instances."""
        status_responses = []
        status_msg = IpcMessage('cmd', 'status', id=self._next_msg_id())

        for ctrl_channel in self.ctrl_channels:
            self.logger.debug(f"Sending status request to {ctrl_channel.identity}")
            ctrl_channel.send(status_msg.encode())

        for ctrl_channel in self.ctrl_channels:
            response = self.await_response(ctrl_channel)
            if response:
                status_responses.append(response)

        return status_responses

    def get_config(self):
        """Get and display the current config of all connected odin-data instances."""
        config_responses = []
        config_msg = IpcMessage('cmd', 'request_configuration', id=self._next_msg_id())

        for ctrl_channel in self.ctrl_channels:
            self.logger.debug(f"Sending config request to {ctrl_channel.identity}")
            ctrl_channel.send(config_msg.encode())

        for ctrl_channel in self.ctrl_channels:
            response = self.await_response(ctrl_channel)
            if response:
                config_responses.append(response)

        return config_responses


    def acquisition(self, path, acquisition_id, frames):
        """Handle the acquisition setup with given parameters."""
        try:
            # Convert frames from string to integer
            frames_count = int(frames)

            # Common settings for both configuration messages
            common_config = {
                "hibirdsdpdk": {
                    "update_config": True,
                    "rx_enable": False,
                    "proc_enable": True,
                    "rx_frames": frames_count
                },
                "hdf": {
                    "write": False
                }
            }

            # First configuration message
            self.logger.info("Sending first configuration for acquisition setup.")
            if not self.send_config_message({"params": common_config}):
                self.logger.error("Failed to send the first configuration message. Aborting acquisition setup.")
                return False

            # Second configuration message
            second_config = {
                "hibirdsdpdk": common_config["hibirdsdpdk"],
                "hdf": {
                    "file": {
                        "path": path
                    },
                    "frames": frames_count,
                    "acquisition_id": acquisition_id,
                    "write": True
                }
            }

            self.logger.info("Sending second configuration for acquisition setup.")
            if not self.send_config_message({"params": second_config}):
                self.logger.error("Failed to send the second configuration message. Aborting acquisition setup.")
                return False

            self.logger.info("Acquisition setup completed successfully.")

        except ValueError as e:
            self.logger.error(f"Invalid frames count provided: {e}")
            return False
        
        return True


    def get_value_from_status(self, *keys):
        # Retrieve the status first
        status_responses = self.get_status()
        
        # Prepare a list to accumulate the results
        results = []

        for reply in status_responses:
            print(reply)

        # Iterate through each status response
        for response in status_responses:
            try:
                data = response.get_params()
                for key in keys:
                    if isinstance(data, dict) and key in data:
                        data = data[key]
                    else:
                        raise KeyError(f"Key {key} not found in the response.")
                
                results.append(data)
            except KeyError as e:
                self.logger.error(f"Key error: {e}")
                return None
            except TypeError as e:
                self.logger.error(f"Type error: {e}")
                return None

        return sum(results) if all(isinstance(x, int) for x in results) else results



if __name__ == '__main__':
    instance_count = int(input("Enter the number of running odin-data instances to connect to: "))
    client = OdinDataClient(instance_count)
    client.run()