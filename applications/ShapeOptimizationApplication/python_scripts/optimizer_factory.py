# ==============================================================================
#  KratosShapeOptimizationApplication
#
#  License:         BSD License
#                   license: ShapeOptimizationApplication/license.txt
#
#  Main authors:    Baumgaertner Daniel, https://github.com/dbaumgaertner
#
# ==============================================================================

# Making KratosMultiphysics backward compatible with python 2.6 and 2.7
from __future__ import print_function, absolute_import, division

# Kratos Core and Apps
from KratosMultiphysics import *
from KratosMultiphysics.ShapeOptimizationApplication import *

# additional imports
from custom_timer import Timer
from analyzer_empty import EmptyAnalyzer
import model_part_controller_factory
import analyzer_factory
import communicator_factory
import algorithm_factory

# ==============================================================================
def CreateOptimizer(optimization_settings, model, external_analyzer=EmptyAnalyzer()):

    ValidateSettings(optimization_settings)

    model_part_controller = model_part_controller_factory.CreateController(optimization_settings["model_settings"], model)

    analyzer = analyzer_factory.CreateAnalyzer(optimization_settings, model_part_controller, external_analyzer)

    communicator = communicator_factory.CreateCommunicator(optimization_settings)

    if optimization_settings["design_variables"]["type"].GetString() == "vertex_morphing":
        return VertexMorphingMethod(optimization_settings, model_part_controller, analyzer, communicator)
    else:
        raise NameError("The following type of design variables is not supported by the optimizer: " + variable_type)

# ------------------------------------------------------------------------------
def ValidateSettings(optimization_settings):
    default_settings = Parameters("""
    {
        "model_settings" : { },
        "objectives" : [ ],
        "constraints" : [ ],
        "design_variables" : { },
        "optimization_algorithm" : { },
        "output" : { }
    }""")

    for key in default_settings.keys():
        if not optimization_settings.Has(key):
            raise RuntimeError("CreateOptimizer: Required setting '{}' missing in 'optimization_settings'!".format(key))

    optimization_settings.ValidateAndAssignDefaults(default_settings)

    for itr in range(optimization_settings["objectives"].size()):
        default_settings = Parameters("""
        {
            "identifier"                          : "NO_IDENTIFIER_SPECIFIED",
            "type"                                : "minimization",
            "scaling_factor"                      : 1.0,
            "weight"                              : 1.0,
            "use_kratos"                          : false,
            "kratos_response_settings"            : {},
            "project_gradient_on_surface_normals" : false
        }""")
        optimization_settings["objectives"][itr].ValidateAndAssignDefaults(default_settings)

    for itr in range(optimization_settings["constraints"].size()):
        default_settings = Parameters("""
        {
            "identifier"                          : "NO_IDENTIFIER_SPECIFIED",
            "type"                                : "<",
            "scaling_factor"                      : 1.0,
            "reference"                           : "initial_value",
            "use_kratos"                          : false,
            "kratos_response_settings"            : {},
            "project_gradient_on_surface_normals" : false
        }""")
        optimization_settings["constraints"][itr].ValidateAndAssignDefaults(default_settings)

# ==============================================================================
class VertexMorphingMethod:
    # --------------------------------------------------------------------------
    def __init__(self, optimization_settings, model_part_controller, analyzer, communicator):
        self.optimization_settings = optimization_settings
        self.model_part_controller = model_part_controller
        self.analyzer = analyzer
        self.communicator = communicator

        self.__AddVariablesToBeUsedByAllAglorithms()

    # --------------------------------------------------------------------------
    def __AddVariablesToBeUsedByAllAglorithms(self):
        model_part = self.model_part_controller.GetOptimizationModelPart()
        number_of_objectives = self.optimization_settings["objectives"].size()
        number_of_constraints = self.optimization_settings["constraints"].size()

        nodal_variable = KratosGlobals.GetVariable("DFDX")
        model_part.AddNodalSolutionStepVariable(nodal_variable)
        nodal_variable = KratosGlobals.GetVariable("DFDX_MAPPED")
        model_part.AddNodalSolutionStepVariable(nodal_variable)

        if number_of_objectives > 1:
            for itr in range(1,number_of_objectives+1):
                nodal_variable = KratosGlobals.GetVariable("DF"+str(itr)+"DX")
                model_part.AddNodalSolutionStepVariable(nodal_variable)

        for itr in range(1,number_of_constraints+1):
            nodal_variable = KratosGlobals.GetVariable("DC"+str(itr)+"DX")
            model_part.AddNodalSolutionStepVariable(nodal_variable)
            nodal_variable = KratosGlobals.GetVariable("DC"+str(itr)+"DX_MAPPED")
            model_part.AddNodalSolutionStepVariable(nodal_variable)

        model_part.AddNodalSolutionStepVariable(CONTROL_POINT_UPDATE)
        model_part.AddNodalSolutionStepVariable(CONTROL_POINT_CHANGE)
        model_part.AddNodalSolutionStepVariable(SHAPE_UPDATE)
        model_part.AddNodalSolutionStepVariable(SHAPE_CHANGE)
        model_part.AddNodalSolutionStepVariable(MESH_CHANGE)
        model_part.AddNodalSolutionStepVariable(NORMAL)
        model_part.AddNodalSolutionStepVariable(NORMALIZED_SURFACE_NORMAL)

    # --------------------------------------------------------------------------
    def Optimize(self):
        algorithm_name = self.optimization_settings["optimization_algorithm"]["name"].GetString()

        print("\n> ==============================================================================================================")
        print("> ", Timer().GetTimeStamp(),": Starting optimization using the following algorithm: ", algorithm_name)
        print("> ==============================================================================================================\n")

        algorithm = algorithm_factory.CreateOptimizationAlgorithm(self.optimization_settings,
                                                                  self.analyzer,
                                                                  self.communicator,
                                                                  self.model_part_controller)
        algorithm.CheckApplicability()
        algorithm.InitializeOptimizationLoop()
        algorithm.RunOptimizationLoop()
        algorithm.FinalizeOptimizationLoop()

        print("\n> ==============================================================================================================")
        print("> Finished optimization                                                                                           ")
        print("> ==============================================================================================================\n")

# ==============================================================================