import sys
import json
from PyQt5.QtWidgets import (
    QApplication, QMainWindow, QVBoxLayout, QHBoxLayout, QLabel, QLineEdit, QPushButton,
    QTextEdit, QWidget, QTreeWidget, QTreeWidgetItem, QSplitter
)
from PyQt5.QtCore import Qt
from PyQt5.QtWidgets import QHeaderView
from odin_data.control.ipc_channel import IpcChannel
from odin_data.control.ipc_message import IpcMessage

class OdinDataClientGUI(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Odin Data Client GUI")
        self.resize(800, 600)
        self._msg_id = 0

        # Create the main layout
        main_layout = QVBoxLayout()

        # Endpoints setup
        self.base_address = "tcp://localhost"
        self.ports = [5000, 9001]
        self.ctrl_channels = {}

        # Add endpoint buttons and setup UI for each endpoint
        for port in self.ports:
            endpoint = f"{self.base_address}:{port}"
            self.setup_endpoint_ui(endpoint, main_layout)

        # Set central widget
        central_widget = QWidget()
        central_widget.setLayout(main_layout)
        self.setCentralWidget(central_widget)

    def setup_endpoint_ui(self, endpoint, layout):
        try:
            ctrl_channel = IpcChannel(IpcChannel.CHANNEL_TYPE_DEALER)
            ctrl_channel.connect(endpoint)
            self.ctrl_channels[endpoint] = ctrl_channel

            # Endpoint UI setup
            instance_widget = QWidget()
            instance_layout = QVBoxLayout()
            refresh_button = QPushButton(f"Refresh Status and Config ({endpoint})")
            refresh_button.clicked.connect(lambda _, ep=endpoint: self.refresh_data(ep))
            instance_layout.addWidget(refresh_button)

            # Status and config tree setup
            for label_text, tree_name in [("Current Status:", "status"), ("Current Config:", "config")]:
                tree_layout = QVBoxLayout()
                label = QLabel(label_text)
                tree = QTreeWidget()
                tree.setHeaderLabels(["Key", "Value"])
                tree.setObjectName(f"{tree_name}_tree_{endpoint}")
                if tree_name == "config":
                    tree.itemDoubleClicked.connect(lambda item, column, ep=endpoint: self.edit_config_item(item, column, ep))
                tree_layout.addWidget(label)
                tree_layout.addWidget(tree)
                instance_layout.addLayout(tree_layout)
            
            instance_widget.setLayout(instance_layout)
            layout.addWidget(instance_widget)
            self.refresh_data(endpoint)
        except Exception as e:
            print(f"Error setting up endpoint {endpoint}: {str(e)}")

    def refresh_data(self, endpoint):
        """Refresh both status and config data."""
        for data_type in ["status", "config"]:
            getattr(self, f"get_{data_type}")(endpoint)

    def populate_tree(self, data, tree_widget, expand=True):
        """Fill a tree widget with the provided data, expanding all nodes."""
        tree_widget.clear()
        parent = tree_widget.invisibleRootItem()
        self.populate_tree_items(data, parent)
        if expand:
            tree_widget.expandAll()

    def populate_tree_items(self, data, parent):
        """Recursively add tree items."""
        if isinstance(data, dict):
            for key, value in data.items():
                child = QTreeWidgetItem(parent, [key])
                self.populate_tree_items(value, child)
        else:
            QTreeWidgetItem(parent, ["", str(data)])

    def get_status(self, endpoint):
        """Request and display status data."""
        msg = IpcMessage('cmd', 'status', id=self._next_msg_id())
        self.send_message(endpoint, msg, "status")

    def get_config(self, endpoint):
        """Request and display configuration data."""
        msg = IpcMessage('cmd', 'request_configuration', id=self._next_msg_id())
        self.send_message(endpoint, msg, "config")

    def send_message(self, endpoint, message, tree_name):
        """Send IPC message and handle the response to update the UI."""
        self.ctrl_channels[endpoint].send(message.encode())
        response = self.await_response(endpoint)
        if response:
            data = response.get_params()
            widget = self.findChild(QTreeWidget, f"{tree_name}_tree_{endpoint}")
            if widget:
                self.populate_tree(data, widget)
        else:
            print(f"No {tree_name} response received from {endpoint}")

    def _next_msg_id(self):
        self._msg_id += 1
        return self._msg_id

    def await_response(self, endpoint, timeout_ms=1000):
        """Wait for a reply from the endpoint."""
        if self.ctrl_channels[endpoint].poll(timeout_ms) == IpcChannel.POLLIN:
            reply = IpcMessage(from_str=self.ctrl_channels[endpoint].recv())
            return reply
        print(f"No response received within {timeout_ms} ms from {endpoint}")
        return None

    def edit_config_item(self, item, column, endpoint):
        """Allow editing of the config value directly in the tree."""
        if column == 1:
            item.setFlags(item.flags() | Qt.ItemIsEditable)

    def update_config(self, item, column, endpoint):
        """Update configuration when an item's value is changed."""
        if column == 1:
            key = item.parent().text(0)
            value = item.text(1)
            new_value = self.parse_new_value(value)
            msg = IpcMessage('cmd', 'configure', id=self._next_msg_id())
            msg.attrs['params'] = {key: new_value}
            self.ctrl_channels[endpoint].send(msg.encode())

    def parse_new_value(self, value):
        """Attempt to convert value string to a boolean or number."""
        if value.lower() in ['true', 'false']:
            return value.lower() == 'true'
        try:
            return int(value)
        except ValueError:
            try:
                return float(value)
            except ValueError:
                return value  # Retain as string if not a number

if __name__ == '__main__':
    app = QApplication(sys.argv)
    window = OdinDataClientGUI()
    window.show()
    sys.exit(app.exec_())
