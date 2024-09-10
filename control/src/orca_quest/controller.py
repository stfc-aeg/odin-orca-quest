"""Class to control camera. Initially, IPC communication as this is how the cameras will be interacted with."""

from odin.adapters.parameter_tree import ParameterTree, ParameterTreeError
from orca_quest.orca_camera import OrcaCamera

class OrcaError(Exception):
    """Simple exception class to wrap lower-level exceptions."""
    pass

class OrcaController():
    """Class to consolidate ORCA-Quest camera controls."""

    def __init__(self, endpoints, names, status_bg_task_enable, status_bg_task_interval):
        """This constructor initialises the object and builds parameter trees."""

        # Internal variables
        self.cameras = []

        self.endpoints = endpoints
        self.names = names

        self.status_bg_task_enable = status_bg_task_enable
        self.status_bg_task_interval = status_bg_task_interval

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
            camtrees[self.names[i]] = camera.tree
        
        # Array of camera trees becomes a real Parameter Tree
        tree['cameras'] = camtrees
        self.param_tree = ParameterTree(tree)

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
