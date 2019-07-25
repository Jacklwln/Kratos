from __future__ import print_function, absolute_import, division  # makes KratosMultiphysics backward compatible with python 2.6 and 2.7

# Importing the base class
from KratosMultiphysics.CoSimulationApplication.base_classes.co_simulation_io import CoSimulationIO

def Create(model, settings):
    return DummyIO(model, settings)

class DummyIO(CoSimulationIO):
    """This class is used if a Solver directly uses Kratos as a data-structure
    e.g. Kratos itself or simple-solvers written in Python
    """

    def ImportCouplingInterface(self, interface_config):
        pass

    def ExportCouplingInterface(self, interface_config):
        pass

    def ImportCouplingInterfaceData(self, data_config):
        pass

    def ExportCouplingInterfaceData(self, data_config):
        pass

    def PrintInfo(self):
        print("This is the dummy-IO")

    def Check(self):
        pass