"""Class to control camera. Initially, IPC communication as this is how the cameras will be interacted with."""

from odin.adapters.parameter_tree import ParameterTree, ParameterTreeError
from orca_quest.orca_camera import OrcaCamera

import logging

class OrcaError(Exception):
    """Simple exception class to wrap lower-level exceptions."""
    pass

class OrcaController():
    """Class to consolidate ORCA-Quest camera controls"""

    def __init__(self, num_cameras, endpoints, names, status_bg_task_enable, status_bg_task_interval):
        """This constructor initialises the object and builds parameter trees."""

        # Internal variables
        self.cameras = []
        camtrees = []
        tree = {}

        self.status_bg_task_enable = status_bg_task_enable
        self.status_bg_task_interval = status_bg_task_interval

        # Raise error if insufficient endpoints for cameras
        if len(endpoints) < num_cameras:
            raise OrcaError("Fewer endpoints defined than number of cameras in config.")

        for i in range(num_cameras):
            # For each desired camera, create OrcaCamera with name and endpoint
            camera = OrcaCamera(endpoints[i], names[i], self.status_bg_task_enable, self.status_bg_task_interval)

            # Store in list of cameras and put tree in list of trees
            self.cameras.append(camera)
            camtrees.append(camera.tree)

        # Array of camera trees becomes real parameter tree
        tree['cameras'] = camtrees
        self.param_tree = ParameterTree(tree)

        logging.debug("number of cams: %s", num_cameras)
        logging.debug("cameras: %s", self.cameras)

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
