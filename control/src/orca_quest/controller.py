"""Class to control camera. Initially, IPC communication as this is how the cameras will be interacted with."""

from odin_control.adapters.base_controller import BaseController, BaseError
from odin_control.adapters.parameter_tree import ParameterTree, ParameterTreeError

from orca_quest.orca_camera import OrcaCamera

class OrcaError(Exception):
    """Simple exception class to wrap lower-level exceptions."""
    pass

class OrcaController(BaseController):
    """Class to consolidate ORCA-Quest camera controls."""

    def __init__(self, options):
        """This constructor initialises the object and builds parameter trees."""
        self.options = options

        # Split on comma, remove whitespace if it exists
        self.endpoints = [
            item.strip() for item in self.options.get('camera_endpoint', '').split(",") if item.strip()
        ]
        self.names = [
            item.strip() for item in self.options.get('camera_name', '').split(",") if item.strip()
        ]

        self.status_bg_task_enable = bool(self.options.get('status_bg_task_enable', 1))
        self.status_bg_task_interval = float(self.options.get('status_bg_task_interval', 1))

        # Internal variables
        self.cameras = []

        # Also builds the tree
        self._connect_cameras()

    def _connect_cameras(self, value=None):
        """Build the parameter tree and attempt to connect the cameras to it."""
        if len(self.cameras) > 0:
            for camera in self.cameras:
                camera._close_connection()
        self.cameras = []
        camtrees = {}
        tree = {}

        for i in range(len(self.endpoints)):
            camera = OrcaCamera(self.endpoints[i], self.names[i], self.status_bg_task_enable, self.status_bg_task_interval)
            self.cameras.append(camera)
            camtrees[self.names[i]] = camera.param_tree

        # Array of camera trees becomes a real Parameter Tree
        tree['cameras'] = camtrees
        self.param_tree = ParameterTree(tree['cameras'])

    def get(self, path, metadata=False):
        """Get the parameter tree.
        This method returns the parameter tree for use by clients via the FurnaceController adapter.
        :param path: path to retrieve from tree
        """
        return self.param_tree.get(path, metadata)

    def get_camera_by_name(self, name):
        """Get a camera object by referencing its name."""
        for cam in self.cameras:
            if name == cam.name:
                return cam
        return None

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
