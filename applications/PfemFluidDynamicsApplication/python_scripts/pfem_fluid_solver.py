from __future__ import print_function, absolute_import, division #makes KratosMultiphysics backward compatible with python 2.6 and 2.7
import os
#import kratos core and applications
import KratosMultiphysics
import KratosMultiphysics.PfemFluidDynamicsApplication as KratosPfemFluid
from python_solver import PythonSolver


def CreateSolver(model, parameters):
    return PfemFluidSolver(model, parameters)

class PfemFluidSolver(PythonSolver):

    def __init__(self, model, parameters):

        ##settings string in json format
        default_settings = KratosMultiphysics.Parameters("""
        {
            "solver_type": "pfem_fluid_solver",
            "model_part_name": "PfemFluidModelPart",
            "domain_size": 2,
            "time_stepping"               : {
                "automatic_time_step" : false,
                "time_step"           : 0.001
            },
            "model_import_settings":{
                "input_type": "mdpa",
                "input_filename": "unknown_name"
            },
            "buffer_size": 3,
            "echo_level": 1,
            "reform_dofs_at_each_step": false,
            "clear_storage": false,
            "compute_reactions": true,
            "move_mesh_flag": true,
            "dofs"                : [],
            "stabilization_factor": 1.0,
            "line_search": false,
            "compute_contact_forces": false,
            "block_builder": false,
            "component_wise": false,
            "predictor_corrector": true,
            "time_order": 2,
            "maximum_velocity_iterations": 1,
            "maximum_pressure_iterations": 7,
            "velocity_tolerance": 1e-5,
            "pressure_tolerance": 1e-5,
            "pressure_linear_solver_settings":  {
                "solver_type"                    : "amgcl",
                "max_iteration"                  : 5000,
                "tolerance"                      : 1e-9,
                "provide_coordinates"            : false,
                "scaling"                        : false,
                "smoother_type"                  : "damped_jacobi",
                "krylov_type"                    : "cg",
                "coarsening_type"                : "aggregation",
                "verbosity"                      : 0
            },
            "velocity_linear_solver_settings": {
                "solver_type"                    : "bicgstab",
                "max_iteration"                  : 5000,
                "tolerance"                      : 1e-9,
                "preconditioner_type"            : "none",
                "scaling"                        : false
            },
            "solving_strategy_settings":{
               "time_step_prediction_level": 0,
               "max_delta_time": 1.0e-5,
               "fraction_delta_time": 0.9,
               "rayleigh_damping": false,
               "rayleigh_alpha": 0.0,
               "rayleigh_beta" : 0.0
            },
        "bodies_list": [],
        "problem_domain_sub_model_part_list": [],
        "processes_sub_model_part_list": [],
        "constraints_process_list": [],
        "loads_process_list"       : [],
        "output_process_list"      : [],
        "output_configuration"     : {},
        "problem_process_list"     : [],
        "processes"                : {},
        "output_processes"         : {},
        "check_process_list": []
        }
        """)

        ##overwrite the default settings with user-provided parameters
        self.settings = parameters
        self.settings.ValidateAndAssignDefaults(default_settings)

        #construct the linear solver
        import python_linear_solver_factory
        self.pressure_linear_solver = python_linear_solver_factory.ConstructSolver(self.settings["pressure_linear_solver_settings"])
        self.velocity_linear_solver = python_linear_solver_factory.ConstructSolver(self.settings["velocity_linear_solver_settings"])

        self.compute_reactions = self.settings["compute_reactions"].GetBool()
        print("Construction of 2-step Pfem Fluid Solver finished.")
        super(PfemFluidSolver, self).__init__(model, parameters)

        model_part_name = self.settings["model_part_name"].GetString()
        if model_part_name == "":
            raise Exception('Please specify a model_part name!')

        if self.model.HasModelPart(model_part_name):
            self.main_model_part = self.model.GetModelPart(model_part_name)
        else:
            self.main_model_part = self.model.CreateModelPart(model_part_name)

    def GetMinimumBufferSize(self):
        return 3

    def Initialize(self):

        print("::[Pfem Fluid Solver]:: -START-")

        print(self.main_model_part.SetBufferSize(self.settings["buffer_size"].GetInt()))

        # Get the computing model part
        self.computing_model_part = self.GetComputingModelPart()

        self.fluid_solver = KratosPfemFluid.TwoStepVPStrategy(self.computing_model_part,
                                                              self.velocity_linear_solver,
                                                              self.pressure_linear_solver,
                                                              self.settings["reform_dofs_at_each_step"].GetBool(),
                                                              self.settings["velocity_tolerance"].GetDouble(),
                                                              self.settings["pressure_tolerance"].GetDouble(),
                                                              self.settings["maximum_pressure_iterations"].GetInt(),
                                                              self.settings["time_order"].GetInt(),
                                                              self.main_model_part.ProcessInfo[KratosMultiphysics.SPACE_DIMENSION])

        echo_level = self.settings["echo_level"].GetInt()

        # Set echo_level
        self.fluid_solver.SetEchoLevel(echo_level)

        # Set initialize flag
        if( self.main_model_part.ProcessInfo[KratosMultiphysics.IS_RESTARTED] == True ):
            self.mechanical_solver.SetInitializePerformedFlag(True)


        # Check if everything is assigned correctly
        self.fluid_solver.Check()


        print("::[Pfem Fluid Solver]:: -END- ")


    def AddVariables(self):
        self.main_model_part.AddNodalSolutionStepVariable(KratosMultiphysics.VELOCITY)
        self.main_model_part.AddNodalSolutionStepVariable(KratosMultiphysics.ACCELERATION)
        self.main_model_part.AddNodalSolutionStepVariable(KratosMultiphysics.DISPLACEMENT)
        self.main_model_part.AddNodalSolutionStepVariable(KratosMultiphysics.PRESSURE)
        self.main_model_part.AddNodalSolutionStepVariable(KratosMultiphysics.VISCOSITY)

        self.main_model_part.AddNodalSolutionStepVariable(KratosMultiphysics.MESH_VELOCITY)

        self.main_model_part.AddNodalSolutionStepVariable(KratosMultiphysics.NODAL_AREA)
        self.main_model_part.AddNodalSolutionStepVariable(KratosMultiphysics.BODY_FORCE)
        self.main_model_part.AddNodalSolutionStepVariable(KratosMultiphysics.DENSITY)
        self.main_model_part.AddNodalSolutionStepVariable(KratosMultiphysics.BULK_MODULUS)
        self.main_model_part.AddNodalSolutionStepVariable(KratosMultiphysics.DYNAMIC_VISCOSITY)
        self.main_model_part.AddNodalSolutionStepVariable(KratosMultiphysics.POISSON_RATIO)
        self.main_model_part.AddNodalSolutionStepVariable(KratosMultiphysics.YOUNG_MODULUS)

        self.main_model_part.AddNodalSolutionStepVariable(KratosMultiphysics.NODAL_MASS)
        self.main_model_part.AddNodalSolutionStepVariable(KratosMultiphysics.NODAL_ERROR)
        self.main_model_part.AddNodalSolutionStepVariable(KratosMultiphysics.FORCE_RESIDUAL)


        #VARIABLES FOR PAPANASTASIOU MODEL
        self.main_model_part.AddNodalSolutionStepVariable(KratosPfemFluid.FLOW_INDEX)
        self.main_model_part.AddNodalSolutionStepVariable(KratosPfemFluid.YIELD_SHEAR)
        self.main_model_part.AddNodalSolutionStepVariable(KratosPfemFluid.ADAPTIVE_EXPONENT)

        #VARIABLES FOR MU-I RHEOLOGY MODEL
        self.main_model_part.AddNodalSolutionStepVariable(KratosPfemFluid.STATIC_FRICTION)
        self.main_model_part.AddNodalSolutionStepVariable(KratosPfemFluid.DYNAMIC_FRICTION)
        self.main_model_part.AddNodalSolutionStepVariable(KratosPfemFluid.INERTIAL_NUMBER_ZERO)
        self.main_model_part.AddNodalSolutionStepVariable(KratosPfemFluid.GRAIN_DIAMETER)
        self.main_model_part.AddNodalSolutionStepVariable(KratosPfemFluid.GRAIN_DENSITY)
        self.main_model_part.AddNodalSolutionStepVariable(KratosPfemFluid.REGULARIZATION_COEFFICIENT)
        self.main_model_part.AddNodalSolutionStepVariable(KratosPfemFluid.INFINITE_FRICTION)
        self.main_model_part.AddNodalSolutionStepVariable(KratosPfemFluid.INERTIAL_NUMBER_ONE)
        self.main_model_part.AddNodalSolutionStepVariable(KratosPfemFluid.ALPHA_PARAMETER)

        self.main_model_part.AddNodalSolutionStepVariable(KratosMultiphysics.REACTION)
        self.main_model_part.AddNodalSolutionStepVariable(KratosMultiphysics.NORMAL)
        self.main_model_part.AddNodalSolutionStepVariable(KratosMultiphysics.VOLUME_ACCELERATION)
        self.main_model_part.AddNodalSolutionStepVariable(KratosMultiphysics.FLUID_FRACTION)
        self.main_model_part.AddNodalSolutionStepVariable(KratosMultiphysics.FLUID_FRACTION_OLD)
        self.main_model_part.AddNodalSolutionStepVariable(KratosMultiphysics.FLUID_FRACTION_RATE)

        # PFEM fluid variables
        # self.main_model_part.AddNodalSolutionStepVariable(KratosPfemFluid.NORMVELOCITY)
        self.main_model_part.AddNodalSolutionStepVariable(KratosPfemFluid.YIELDED)
        self.main_model_part.AddNodalSolutionStepVariable(KratosPfemFluid.FREESURFACE)
        self.main_model_part.AddNodalSolutionStepVariable(KratosPfemFluid.PRESSURE_VELOCITY)
        self.main_model_part.AddNodalSolutionStepVariable(KratosPfemFluid.PRESSURE_REACTION)
        self.main_model_part.AddNodalSolutionStepVariable(KratosPfemFluid.PRESSURE_ACCELERATION)

        print("::[Pfem Fluid Solver]:: Variables ADDED")


    def AddDofs(self):

        for node in self.main_model_part.Nodes:
            # adding dofs
            node.AddDof(KratosMultiphysics.PRESSURE)
            node.AddDof(KratosMultiphysics.DENSITY)
            node.AddDof(KratosMultiphysics.VELOCITY_X)
            node.AddDof(KratosMultiphysics.VELOCITY_Y)
            node.AddDof(KratosMultiphysics.VELOCITY_Z)
            node.AddDof(KratosMultiphysics.DISPLACEMENT_X)
            node.AddDof(KratosMultiphysics.DISPLACEMENT_Y)
            node.AddDof(KratosMultiphysics.DISPLACEMENT_Z)
        print("::[Pfem Fluid Solver]:: DOF's ADDED")


    def ImportModelPart(self):

        print("::[Pfem Fluid Solver]:: Model reading starts.")

        self.computing_model_part_name = "fluid_computing_domain"

        # we can use the default implementation in the base class
        self._ImportModelPart(self.main_model_part,self.settings["model_import_settings"])

        # Auxiliary Kratos parameters object to be called by the CheckAndPepareModelProcess
        params = KratosMultiphysics.Parameters("{}")
        params.AddEmptyValue("computing_model_part_name").SetString(self.computing_model_part_name)
        params.AddValue("problem_domain_sub_model_part_list",self.settings["problem_domain_sub_model_part_list"])
        params.AddValue("processes_sub_model_part_list",self.settings["processes_sub_model_part_list"])
        if( self.settings.Has("bodies_list") ):
            params.AddValue("bodies_list",self.settings["bodies_list"])

        # CheckAndPrepareModelProcess creates the fluid_computational model part
        import pfem_check_and_prepare_model_process_fluid
        pfem_check_and_prepare_model_process_fluid.CheckAndPrepareModelProcess(self.main_model_part, params).Execute()

        self.main_model_part.SetBufferSize( self.settings["buffer_size"].GetInt() )

        current_buffer_size = self.main_model_part.GetBufferSize()
        if(self.GetMinimumBufferSize() > current_buffer_size):
            current_buffer_size = self.GetMinimumBufferSize()

        self.main_model_part.SetBufferSize( current_buffer_size )

        # Fill buffer
        delta_time = self.main_model_part.ProcessInfo[KratosMultiphysics.DELTA_TIME]
        time = self.main_model_part.ProcessInfo[KratosMultiphysics.TIME]
        time = time - delta_time * (current_buffer_size)
        self.main_model_part.ProcessInfo.SetValue(KratosMultiphysics.TIME, time)
        for size in range(0, current_buffer_size):
            step = size - (current_buffer_size -1)
            self.main_model_part.ProcessInfo.SetValue(KratosMultiphysics.STEP, step)
            time = time + delta_time
            #delta_time is computed from previous time in process_info
            self.main_model_part.CloneTimeStep(time)

        self.main_model_part.ProcessInfo[KratosMultiphysics.IS_RESTARTED] = False

        print ("::[Pfem Fluid Solver]:: Model reading finished.")

    def _ComputeDeltaTime(self):

        delta_time = self.main_model_part.ProcessInfo[KratosMultiphysics.DELTA_TIME]

        return delta_time

    def GetComputingModelPart(self):
        return self.main_model_part.GetSubModelPart(self.computing_model_part_name)

    def Solve(self):
        if self.settings["clear_storage"].GetBool():
            self.Clear()

        self.fluid_solver.Solve()

        #self.fluid_solver.CalculateAccelerations()  # ACCELERATION
        #self.fluid_solver.CalculateDisplacements()  # DISPLACEMENTS

    # solve :: sequencial calls

    def AdvanceInTime(self, current_time):
        dt = self._ComputeDeltaTime()
        new_time = current_time + dt

        self.main_model_part.CloneTimeStep(new_time)
        self.main_model_part.ProcessInfo[KratosMultiphysics.STEP] += 1

        return new_time

    def InitializeStrategy(self):
        if self.settings["clear_storage"].GetBool():
            self.Clear()

        self.fluid_solver.Initialize()

    def InitializeSolutionStep(self):
        #self.fluid_solver.InitializeSolutionStep()
        if self._TimeBufferIsInitialized():
            self.fluid_solver.InitializeSolutionStep()

        ## Automatic time step computation according to user defined CFL number
        if (self.settings["time_stepping"]["automatic_time_step"].GetBool()):
            adaptive_time_interval = KratosPfemFluid.AdaptiveTimeIntervalProcess(self.main_model_part,self.settings["echo_level"].GetInt())
            adaptive_time_interval.Execute()

        #pass
        #unactive_peak_elements = False
        #unactive_sliver_elements = False
        #set_active_flag = KratosPfemFluid.SetActiveFlagProcess(self.main_model_part,unactive_peak_elements,unactive_sliver_elements,self.settings["echo_level"].GetInt())
        #set_active_flag.Execute()

        # split_elements = KratosPfemFluid.SplitElementsProcess(self.main_model_part,self.settings["echo_level"].GetInt())
        # split_elements.ExecuteInitialize()

    def Predict(self):
        pass
        #self.fluid_solver.Predict()

    def SolveSolutionStep(self):
        #self.fluid_solver.SolveSolutionStep()
        self.fluid_solver.Solve()

    def FinalizeSolutionStep(self):
        #pass
        self.fluid_solver.FinalizeSolutionStep()

        #print("set_active_flag.ExecuteFinalize()")
        unactive_peak_elements = False
        unactive_sliver_elements = False
        if(unactive_peak_elements == True or unactive_sliver_elements == True):
            set_active_flag = KratosPfemFluid.SetActiveFlagProcess(self.main_model_part,unactive_peak_elements,unactive_sliver_elements,self.settings["echo_level"].GetInt())
            set_active_flag.Execute()

        #split_elements = KratosPfemFluid.SplitElementsProcess(self.main_model_part,self.settings["echo_level"].GetInt())
        #split_elements.ExecuteFinalize()


    # solve :: sequencial calls


    def SetEchoLevel(self, level):
        self.fluid_solver.SetEchoLevel(level)

    def Clear(self):
        self.fluid_solver.Clear()

    def Check(self):
        self.fluid_solver.Check()
#
    def _TimeBufferIsInitialized(self):
        # We always have one extra old step (step 0, read from input)
        return self.main_model_part.ProcessInfo[KratosMultiphysics.STEP] + 1 >= self.GetMinimumBufferSize()

#


