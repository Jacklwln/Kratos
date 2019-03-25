from __future__ import print_function, absolute_import, division  # makes KratosMultiphysics backward compatible with python 2.6 and 2.7

# Importing Kratos FluidDynamicsApplication
import KratosMultiphysics.CoSimulationApplication.co_simulation_tools as cs_tools
try :
    from KratosMultiphysics.FluidDynamicsApplication.fluid_dynamics_analysis import FluidDynamicsAnalysis
except ModuleNotFoundError:
    raise ModuleNotFoundError(cs_tools.bcolors.FAIL + 'KRATOS_FLUID_DYNAMICS is not available! Please ensure that it is available for usage!'+ cs_tools.bcolors.ENDC)

# Importing the base class
from . import kratos_base_field_solver

def Create(name, cosim_solver_settings):
    return KratosCoSimulationFluidSolver(name, cosim_solver_settings)

class KratosCoSimulationFluidSolver(kratos_base_field_solver.KratosBaseFieldSolver):
    def _CreateAnalysisStage(self):
        return FluidDynamicsAnalysis(self.model, self.project_parameters)

    def _GetParallelType(self):
        return self.project_parameters["problem_data"]["parallel_type"].GetString()

    def _Name(self):
        return self.__class__.__name__