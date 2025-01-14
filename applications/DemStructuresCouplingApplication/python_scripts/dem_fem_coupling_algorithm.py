from __future__ import print_function, absolute_import, division #makes KratosMultiphysics backward compatible with python 2.6 and 2.7

import sys
import time as timer
import os
import weakref
import KratosMultiphysics as Kratos
from Kratos import Logger
import KratosMultiphysics.DEMApplication as Dem
sys.path.insert(0, '')
Logger.Print("Running under OpenMP........", label="DEM")
import DEM_procedures
import DEM_material_test_script
import KratosMultiphysics.StructuralMechanicsApplication as Structural
import KratosMultiphysics.DemStructuresCouplingApplication as DemFem
import dem_structures_coupling_gid_output

class Algorithm(object):

    def __init__(self):
        self.model = Kratos.Model()

        import dem_main_script_ready_for_coupling_with_fem
        self.dem_solution = dem_main_script_ready_for_coupling_with_fem.Solution(self.model)
        self.dem_solution.coupling_algorithm = weakref.proxy(self)

        import structural_mechanics_analysis
        structural_parameters_file_name = "ProjectParameters.json"

        with open(structural_parameters_file_name,'r') as parameter_file:
            parameters = Kratos.Parameters(parameter_file.read())

        # Create structural solver, main_model_part and added variables
        self.structural_solution = structural_mechanics_analysis.StructuralMechanicsAnalysis(self.model, parameters)

        self.AddDEMVariablesToStructural()

    def AddDEMVariablesToStructural(self):
        self.structural_solution._GetSolver().main_model_part.AddNodalSolutionStepVariable(DemFem.DEM_SURFACE_LOAD)
        self.structural_solution._GetSolver().main_model_part.AddNodalSolutionStepVariable(DemFem.BACKUP_LAST_STRUCTURAL_VELOCITY)
        self.structural_solution._GetSolver().main_model_part.AddNodalSolutionStepVariable(DemFem.BACKUP_LAST_STRUCTURAL_DISPLACEMENT)
        self.structural_solution._GetSolver().main_model_part.AddNodalSolutionStepVariable(DemFem.SMOOTHED_STRUCTURAL_VELOCITY)
        self.structural_solution._GetSolver().main_model_part.AddNodalSolutionStepVariable(Dem.DELTA_DISPLACEMENT)
        self.structural_solution._GetSolver().main_model_part.AddNodalSolutionStepVariable(Dem.DEM_PRESSURE)
        self.structural_solution._GetSolver().main_model_part.AddNodalSolutionStepVariable(Dem.DEM_NODAL_AREA)
        self.structural_solution._GetSolver().main_model_part.AddNodalSolutionStepVariable(Dem.ELASTIC_FORCES)
        self.structural_solution._GetSolver().main_model_part.AddNodalSolutionStepVariable(Dem.CONTACT_FORCES)
        self.structural_solution._GetSolver().main_model_part.AddNodalSolutionStepVariable(Dem.TANGENTIAL_ELASTIC_FORCES)
        self.structural_solution._GetSolver().main_model_part.AddNodalSolutionStepVariable(Dem.SHEAR_STRESS)
        self.structural_solution._GetSolver().main_model_part.AddNodalSolutionStepVariable(Dem.NON_DIMENSIONAL_VOLUME_WEAR)
        self.structural_solution._GetSolver().main_model_part.AddNodalSolutionStepVariable(Dem.IMPACT_WEAR)
        self.structural_solution._GetSolver().main_model_part.AddNodalSolutionStepVariable(DemFem.TARGET_STRESS)
        self.structural_solution._GetSolver().main_model_part.AddNodalSolutionStepVariable(DemFem.REACTION_STRESS)
        self.structural_solution._GetSolver().main_model_part.AddNodalSolutionStepVariable(DemFem.LOADING_VELOCITY)

    def Run(self):
        self.Initialize()
        self.RunSolutionLoop()
        self.Finalize()

    def Initialize(self):
        self.structural_solution.Initialize() # Reading mdpa
        self.dem_solution.Initialize() # Adding DEM variables and reading

        self._DetectStructuresSkin()
        self._TransferStructuresSkinToDem()
        self.dem_solution.solver.Initialize()

        self.sandwich_simulation = False
        # Test types (4 different options):
        # Test number 0: no test simulation
        # Test number 1: CTW16 specimen
        # Test number 2: CTW10 specimen
        # Test number 3: Blind test specimen
        self.test_number = 0

        if not self.sandwich_simulation and self.test_number:
            import control_module_fem_dem_utility
            self.control_module_fem_dem_utility = control_module_fem_dem_utility.ControlModuleFemDemUtility(self.model, self.dem_solution.spheres_model_part, self.test_number)
            self.control_module_fem_dem_utility.ExecuteInitialize()

        from KratosMultiphysics.DemStructuresCouplingApplication import stress_failure_check_utility
        self.stress_failure_check_utility = stress_failure_check_utility.StressFailureCheckUtility(self.dem_solution.spheres_model_part, self.test_number)

        mixed_mp = self.model.CreateModelPart('MixedPart')
        filename = os.path.join(self.dem_solution.post_path, self.dem_solution.DEM_parameters["problem_name"].GetString())
        self.gid_output = dem_structures_coupling_gid_output.DemStructuresCouplingGiDOutput(
                            filename,
                            True,
                            "Binary",
                            "Multiples",
                            True,
                            True,
                            self.structural_solution._GetSolver().GetComputingModelPart(),
                            self.dem_solution.spheres_model_part,
                            self.dem_solution.cluster_model_part,
                            self.dem_solution.rigid_face_model_part,
                            self.dem_solution.contact_model_part,
                            mixed_mp
                            )

        structures_nodal_results = ["VOLUME_ACCELERATION","DEM_SURFACE_LOAD","REACTION","TARGET_STRESS","REACTION_STRESS","LOADING_VELOCITY"]
        dem_nodal_results = ["IS_STICKY", "DEM_STRESS_TENSOR"]
        clusters_nodal_results = []
        rigid_faces_nodal_results = []
        contact_model_part_results = ["CONTACT_FAILURE"]
        mixed_nodal_results = ["DISPLACEMENT", "VELOCITY"]
        gauss_points_results = ["CAUCHY_STRESS_TENSOR"]
        self.gid_output.initialize_dem_fem_results(structures_nodal_results,
                                                   dem_nodal_results,
                                                   clusters_nodal_results,
                                                   rigid_faces_nodal_results,
                                                   contact_model_part_results,
                                                   mixed_nodal_results,
                                                   gauss_points_results)

    def _DetectStructuresSkin(self):

        skin_detection_parameters = Kratos.Parameters("""
        {
            "name_auxiliar_model_part"              : "DetectedByProcessSkinModelPart",
            "name_auxiliar_condition"               : "SurfaceLoadFromDEMCondition",
            "list_model_parts_to_assign_conditions" : []
        }
        """)

        computing_model_part = self.structural_solution._GetSolver().GetComputingModelPart()
        if (computing_model_part.ProcessInfo[Kratos.DOMAIN_SIZE] == 2):
            self.structure_skin_detector = Kratos.SkinDetectionProcess2D(computing_model_part, skin_detection_parameters)
        elif (computing_model_part.ProcessInfo[Kratos.DOMAIN_SIZE] == 3):
            self.structure_skin_detector = Kratos.SkinDetectionProcess3D(computing_model_part, skin_detection_parameters)
        else:
            print("No dimensions detected for the structures problem. Exiting.")
            sys.exit()

        self.structure_skin_detector.Execute()

    def _TransferStructuresSkinToDem(self):
        self.structural_mp = self.structural_solution._GetSolver().GetComputingModelPart()
        self.skin_mp = self.structural_mp.GetSubModelPart("DetectedByProcessSkinModelPart")
        dem_walls_mp = self.dem_solution.rigid_face_model_part
        max_prop_id = 0
        for prop in dem_walls_mp.Properties:
            if prop.Id > max_prop_id:
                max_prop_id = prop.Id
        props = Kratos.Properties(max_prop_id + 1)
        props[Dem.FRICTION] = -0.5773502691896257
        props[Dem.WALL_COHESION] = 0.0
        props[Dem.COMPUTE_WEAR] = False
        props[Dem.SEVERITY_OF_WEAR] = 0.001
        props[Dem.IMPACT_WEAR_SEVERITY] = 0.001
        props[Dem.BRINELL_HARDNESS] = 200.0
        props[Kratos.YOUNG_MODULUS] = 7e9
        props[Kratos.POISSON_RATIO] = 0.16
        dem_walls_mp.AddProperties(props)
        DemFem.DemStructuresCouplingUtilities().TransferStructuresSkinToDem(self.skin_mp, dem_walls_mp, props)

    def RunSolutionLoop(self):

        self.dem_solution.step = 0
        self.dem_solution.time = 0.0
        self.dem_solution.time_old_print = 0.0
        self.time_dem = 0.0
        self.Dt_structural = self.structural_solution._GetSolver().settings["time_stepping"]["time_step"].GetDouble()

        while self.structural_solution.time < self.structural_solution.end_time:

            portion_of_the_force_which_is_new = 0.4
            DemFem.DemStructuresCouplingUtilities().SmoothLoadTrasferredToFem(self.dem_solution.rigid_face_model_part, portion_of_the_force_which_is_new)

            self.structural_solution.time = self.structural_solution._GetSolver().AdvanceInTime(self.structural_solution.time)

            self.structural_solution.InitializeSolutionStep()
            if not self.sandwich_simulation and self.test_number:
                self.control_module_fem_dem_utility.ExecuteInitializeSolutionStep()
            self.structural_solution._GetSolver().Predict()
            self.structural_solution._GetSolver().SolveSolutionStep()
            self.structural_solution.FinalizeSolutionStep()
            self.structural_solution.OutputSolutionStep()

            time_final_DEM_substepping = self.structural_solution.time

            self.Dt_DEM = self.dem_solution.spheres_model_part.ProcessInfo.GetValue(Kratos.DELTA_TIME)

            DemFem.InterpolateStructuralSolutionForDEM().SaveStructuralSolution(self.structural_mp)

            DemFem.ComputeDEMFaceLoadUtility().ClearDEMFaceLoads(self.skin_mp)


            if self.test_number == 1 or self.test_number == 2:
                self.outer_walls_model_part = self.model["Structure.SurfacePressure3D_lateral_pressure"]
                DemFem.DemStructuresCouplingUtilities().ComputeSandProductionWithDepthFirstSearch(self.dem_solution.spheres_model_part, self.outer_walls_model_part, self.structural_solution.time)
                DemFem.DemStructuresCouplingUtilities().ComputeSandProduction(self.dem_solution.spheres_model_part, self.outer_walls_model_part, self.structural_solution.time)
            elif self.test_number == 3:
                self.outer_walls_model_part_1 = self.model["Structure.SurfacePressure3D_sigmaXpos"]
                self.outer_walls_model_part_2 = self.model["Structure.SurfacePressure3D_sigmaYpos"]
                DemFem.DemStructuresCouplingUtilities().ComputeTriaxialSandProduction(self.dem_solution.spheres_model_part, self.outer_walls_model_part_1, self.outer_walls_model_part_2, self.structural_solution.time)

            for self.dem_solution.time_dem in self.yield_DEM_time(self.dem_solution.time, time_final_DEM_substepping, self.Dt_DEM):
                self.dem_solution.InitializeTimeStep()
                self.dem_solution.time = self.dem_solution.time + self.dem_solution.solver.dt

                self.dem_solution.step += 1

                self.dem_solution.DEMFEMProcedures.UpdateTimeInModelParts(self.dem_solution.all_model_parts, self.dem_solution.time, self.dem_solution.solver.dt, self.dem_solution.step)

                self.dem_solution._BeforeSolveOperations(self.dem_solution.time)

                DemFem.InterpolateStructuralSolutionForDEM().InterpolateStructuralSolution(self.structural_mp, self.Dt_structural, self.structural_solution.time, self.dem_solution.solver.dt, self.dem_solution.time)

                self.dem_solution.SolverSolve()

                DemFem.DemStructuresCouplingUtilities().MarkBrokenSpheres(self.dem_solution.spheres_model_part)

                center = Kratos.Array3()
                center[0] = 0.0
                center[1] = 0.0
                center[2] = 0.0
                axis = Kratos.Array3()
                axis[0] = 0.0
                axis[1] = 0.0
                axis[2] = 1.0

                radius = 0
                if self.test_number == 1:
                    radius = 0.0036195; #95% of the real hole. CTW16 specimen
                elif self.test_number == 2:
                    radius = 0.012065; #95% of the real hole. CTW10 specimen
                elif self.test_number == 3:
                    radius = 0.036195; #95% of the real hole. Blind Test

                self.dem_solution.creator_destructor.MarkParticlesForErasingGivenCylinder(self.dem_solution.spheres_model_part, center, axis, radius)

                self.dem_solution.AfterSolveOperations()

                DemFem.ComputeDEMFaceLoadUtility().CalculateDEMFaceLoads(self.skin_mp, self.dem_solution.solver.dt, self.Dt_structural)

                self.dem_solution.DEMFEMProcedures.MoveAllMeshes(self.dem_solution.all_model_parts, self.dem_solution.time, self.dem_solution.solver.dt)
                #DEMFEMProcedures.MoveAllMeshesUsingATable(rigid_face_model_part, time, dt)

                ##### adding DEM elements by the inlet ######
                if self.dem_solution.DEM_parameters["dem_inlet_option"].GetBool():
                    self.dem_solution.DEM_inlet.CreateElementsFromInletMesh(self.dem_solution.spheres_model_part, self.dem_solution.cluster_model_part, self.dem_solution.creator_destructor)  # After solving, to make sure that neighbours are already set.

                stepinfo = self.dem_solution.report.StepiReport(timer, self.dem_solution.time, self.dem_solution.step)
                if stepinfo:
                    self.dem_solution.KratosPrintInfo(stepinfo)

                #### PRINTING GRAPHS ####
                os.chdir(self.dem_solution.graphs_path)
                self.dem_solution.post_utils.ComputeMeanVelocitiesInTrap("Average_Velocity.txt", self.dem_solution.time, self.dem_solution.graphs_path)

                self.dem_solution.materialTest.MeasureForcesAndPressure()
                self.dem_solution.materialTest.PrintGraph(self.dem_solution.time)

                self.dem_solution.DEMFEMProcedures.PrintGraph(self.dem_solution.time)
                self.dem_solution.DEMFEMProcedures.PrintBallsGraph(self.dem_solution.time)

                self.dem_solution.DEMEnergyCalculator.CalculateEnergyAndPlot(self.dem_solution.time)

                self.dem_solution.BeforePrintingOperations(self.dem_solution.time)

                #### GiD IO ##########################################
                if self.dem_solution.IsTimeToPrintPostProcess():
                    self.dem_solution.solver.PrepareElementsForPrinting()
                    if self.dem_solution.DEM_parameters["ContactMeshOption"].GetBool():
                        self.dem_solution.solver.PrepareContactElementsForPrinting()
                    self.dem_solution.PrintResultsForGid(self.dem_solution.time)
                    self.dem_solution.demio.PrintMultifileLists(self.dem_solution.time, self.dem_solution.post_path)
                    self.dem_solution.time_old_print = self.dem_solution.time

                    if self.test_number:
                        self.stress_failure_check_utility.ExecuteFinalizeSolutionStep()

                self.dem_solution.FinalizeTimeStep(self.dem_solution.time)

            DemFem.InterpolateStructuralSolutionForDEM().RestoreStructuralSolution(self.structural_mp)
            # TODO: Should control_module_fem_dem_utility.ExecuteFinalizeSolutionStep be done before or after RestoreStructuralSolution ?
            if not self.sandwich_simulation and self.test_number:
                self.control_module_fem_dem_utility.ExecuteFinalizeSolutionStep()

    def ReadDemModelParts(self,
                                    starting_node_Id=0,
                                    starting_elem_Id=0,
                                    starting_cond_Id=0):
        creator_destructor = self.dem_solution.creator_destructor
        structures_model_part = self.structural_solution._GetSolver().GetComputingModelPart()
        max_node_Id = creator_destructor.FindMaxNodeIdInModelPart(structures_model_part)
        max_elem_Id = creator_destructor.FindMaxElementIdInModelPart(structures_model_part)
        max_cond_Id = creator_destructor.FindMaxConditionIdInModelPart(structures_model_part)
        self.dem_solution.BaseReadModelParts(max_node_Id, max_elem_Id, max_cond_Id)
        self.dem_solution.all_model_parts.MaxNodeId = max_node_Id

    def Finalize(self):
        self.dem_solution.Finalize()
        self.dem_solution.CleanUpOperations()
        self.structural_solution.Finalize()

    def yield_DEM_time(self, current_time, current_time_plus_increment, delta_time):

        current_time += delta_time

        tolerance = 0.0001
        while current_time < (current_time_plus_increment - tolerance * delta_time):
            yield current_time
            current_time += delta_time

        current_time = current_time_plus_increment
        yield current_time


if __name__ == "__main__":
    Algorithm().Run()
