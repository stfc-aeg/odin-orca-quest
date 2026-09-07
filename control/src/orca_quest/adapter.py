from odin_control.adapters.adapter import ApiAdapter
from orca_quest.controller import OrcaController, OrcaError

class OrcaAdapter(ApiAdapter):
    """Adapter for the orca camera controller class."""
    controller_cls = OrcaController
    error_cls = OrcaError