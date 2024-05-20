import logging

from tornado.escape import json_decode

from odin.adapters.adapter import ApiAdapter, ApiAdapterResponse, request_types, response_types
from odin.adapters.parameter_tree import ParameterTreeError

from orca_quest.controller import OrcaController, OrcaError

class OrcaAdapter(ApiAdapter):
    """Acquisition adapter class."""

    def __init__(self, **kwargs):
        """Initialise AcquisitionAdapter object."""

        super(OrcaAdapter, self).__init__(**kwargs)

        # Split on comma, remove whitespace if it exists
        endpoints = [
            item.strip() for item in self.options.get('camera_endpoint', None).split(",")
        ]
        names = [
            item.strip() for item in self.options.get('camera_name', 'camera_1').split(",")
        ]

        status_bg_task_enable = bool(self.options.get('status_bg_task_enable', 1))
        status_bg_task_interval = int(self.options.get('status_bg_task_interval', 1))

        # Create acquisition controller
        self.camera = OrcaController(endpoints, names,
                                    status_bg_task_enable, status_bg_task_interval
                                    )

    @response_types('application/json', default='application/json')
    def get(self, path, request):
        """Handle an HTTP GET request.

        This method handles an HTTP GET request, returning a JSON response.

        :param path: URI path of request
        :param request: HTTP request object
        :return: an ApiAdapterResponse object containing the appropriate response
        """
        try:
            response = self.camera.get(path)
            status_code = 200
        except ParameterTreeError as e:
            response = {'error': str(e)}
            status_code = 400

        content_type = 'application/json'

        return ApiAdapterResponse(response, content_type=content_type,
                                  status_code=status_code)

    @request_types('application/json')
    @response_types('application/json', default='application/json')
    def put(self, path, request):
        """Handle an HTTP PUT request.

        This method handles an HTTP PUT request, returning a JSON response.

        :param path: URI path of request
        :param request: HTTP request object
        :return: an ApiAdapterResponse object containing the appropriate response
        """

        content_type = 'application/json'

        try:
            data = json_decode(request.body)
            self.camera.set(path, data)
            response = self.camera.get(path)
            status_code = 200
        except OrcaError as e:
            response = {'error': str(e)}
            status_code = 400
        except (TypeError, ValueError) as e:
            response = {'error': 'Failed to decode PUT request body: {}'.format(str(e))}
            status_code = 400

        logging.debug(response)

        return ApiAdapterResponse(response, content_type=content_type,
                                  status_code=status_code)

    def delete(self, path, request):
        """Handle an HTTP DELETE request.

        This method handles an HTTP DELETE request, returning a JSON response.

        :param path: URI path of request
        :param request: HTTP request object
        :return: an ApiAdapterResponse object containing the appropriate response
        """
        response = 'LiveXAdapter: DELETE on path {}'.format(path)
        status_code = 200

        logging.debug(response)

        return ApiAdapterResponse(response, status_code=status_code)

    def cleanup(self):
        """Clean up adapter state at shutdown.

        This method cleans up the adapter state when called by the server at e.g. shutdown.
        It simplied calls the cleanup function of the LiveX instance.
        """
        self.camera.cleanup()

    def initialize(self, adapters):
        """Get list of adapters and call relevant functions for them."""
        self.adapters = dict((k, v) for k, v in adapters.items() if v is not self)
