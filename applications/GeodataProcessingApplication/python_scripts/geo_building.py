import KratosMultiphysics
import KratosMultiphysics.MeshingApplication as KratosMesh
import KratosMultiphysics.GeodataProcessingApplication as KratosGeo

from geo_processor import GeoProcessor
import os

class GeoBuilding( GeoProcessor ):

    def __init__( self ):
        super(GeoBuilding, self).__init__()

        self.HasBuildingHull = False
        self.HasDistanceField = False



    def ImportBuildingHullSTL( self, file_name ):

        print("To be done...")


    # function under construction
    def ImportBuilding(self, building_model_part):
        if not self.HasModelPart:
            KratosMultiphysics.Logger.PrintWarning("GeoBuilding", "Model part has to be set, first.")
            return
        
        # we initialize the building_hull_model_part
        self._generate_building_model_part("building")
        self.building_hull_model_part = building_model_part

        self.HasBuildingHull = True


    def ImportBuildingHullMDPA( self, file_name ):

        if not self.HasModelPart:
            KratosMultiphysics.Logger.PrintWarning("GeoBuilding", "Model part has to be set, first.")
            return

        self._generate_building_model_part("ModelPart"+file_name)

        # function to load form MDPA to MODEL_PART
        file_path = os.path.dirname(os.path.realpath(__file__))
        KratosMultiphysics.ModelPartIO( file_path + file_name ).ReadModelPart(self.building_hull_model_part)

        self.HasBuildingHull = True


    def ShiftBuildingHull( self, dx, dy, dz ):

        if not self.HasModelPart:
            KratosMultiphysics.Logger.PrintWarning("GeoBuilding", "Model part has to be set, first.")
            return

        if not self.HasBuildingHull:
            KratosMultiphysics.Logger.PrintWarning("GeoBuilding", "Function SiftBuildingHull requires to import a building hull, first.")
            return

        for node in self.building_hull_model_part.Nodes:
            node.X += dx
            node.Y += dy
            node.Z += dz


    def ComputeDistanceFieldFromHull( self, invert_distance_field = False, size_reduction = 0.0 ):

        if not self.HasModelPart:
            KratosMultiphysics.Logger.PrintWarning("GeoBuilding", "Model part has to be set, first.")
            return

        if not self.HasBuildingHull:
            KratosMultiphysics.Logger.PrintWarning("GeoBuilding", "Function ComputeDistanceFieldFromHull requires to import a building hull, first.")
            return

        aux_model_part_name = "AuxModelPart"
        current_model = self.ModelPart.GetModel()

        if current_model.HasModelPart(aux_model_part_name):
            # clear the existing model part (to be sure)
            aux_model_part = current_model.GetModelPart( aux_model_part_name )
            aux_model_part.Elements.clear()
            aux_model_part.Conditions.clear()
            aux_model_part.Nodes.clear()
        else:
            # create the model part from scratch
            aux_model_part = current_model.CreateModelPart(aux_model_part_name)
            aux_model_part.AddNodalSolutionStepVariable(KratosMultiphysics.DISTANCE)
            aux_model_part.AddNodalSolutionStepVariable(KratosMultiphysics.DISTANCE_GRADIENT)
            aux_model_part.AddNodalSolutionStepVariable(KratosMultiphysics.FLAG_VARIABLE)

        # populating the empty model part (copy, do not just assign!)
        prop = self.ModelPart.Properties[0]
        for node in self.ModelPart.Nodes:
            aux_model_part.CreateNewNode( node.Id, node.X, node.Y, node.Z )
        for elem in self.ModelPart.Elements:
            nodes = elem.GetNodes()
            aux_model_part.CreateNewElement("Element3D4N", elem.Id,  [nodes[0].Id, nodes[1].Id, nodes[2].Id, nodes[3].Id], prop)
        
        KratosMultiphysics.CalculateDistanceToSkinProcess3D(aux_model_part, self.building_hull_model_part ).Execute()

        # check the distance field
        pos = 0; neg = 0
        for node in aux_model_part.Nodes:
            if node.GetSolutionStepValue( KratosMultiphysics.DISTANCE ) > 0.0:
                pos = 1
            if node.GetSolutionStepValue( KratosMultiphysics.DISTANCE ) < 0.0:
                neg = 1

        if ( pos == 0 or neg == 0):
            KratosMultiphysics.Logger.PrintWarning("GeoBuilding", "The distance field from the building hull does not have its zero-level inside the domain.")
            return False

        # inversion of the distance field
        if invert_distance_field:
            for node in aux_model_part.Nodes:
                node.SetSolutionStepValue( KratosMultiphysics.DISTANCE, -node.GetSolutionStepValue( KratosMultiphysics.DISTANCE ) )

        # getting rid of the +/- inf values around the zero level
        for node in aux_model_part.Nodes:
            if ( node.GetSolutionStepValue( KratosMultiphysics.DISTANCE) > 1000.0 ):
                node.SetSolutionStepValue(KratosMultiphysics.DISTANCE, 20.0 )
            elif ( node.GetSolutionStepValue( KratosMultiphysics.DISTANCE) < -1000.0 ):
                node.SetSolutionStepValue(KratosMultiphysics.DISTANCE, -20.0 )

        if (size_reduction != 0.0):
            print("\nvariational_distance_process\n")
            variational_distance_process = self._set_variational_distance_process_serial( aux_model_part, "DistanceFromSkin" )
            variational_distance_process.Execute()
        
        for node in aux_model_part.Nodes:
            source = node
            destination = self.ModelPart.GetNodes()[source.Id]
            destination.SetSolutionStepValue( KratosMultiphysics.DISTANCE, source.GetSolutionStepValue( KratosMultiphysics.DISTANCE ) + size_reduction )

        self.HasDistanceField = True
        return True


    def AddDistanceFieldFromHull( self, invert_distance_field = False, size_reduction = 0.0 ):

        if not self.HasModelPart:
            KratosMultiphysics.Logger.PrintWarning("GeoBuilding", "Model part has to be set, first.")
            return

        if not self.HasBuildingHull:
            KratosMultiphysics.Logger.PrintWarning("GeoBuilding", "Function ComputeDistanceFieldFromHull requires to import a building hull, first.")
            return

        aux_model_part_name = "AuxModelPart"
        current_model = self.ModelPart.GetModel()

        if current_model.HasModelPart(aux_model_part_name):
            # clear the existing model part (to be sure)
            aux_model_part = current_model.GetModelPart( aux_model_part_name )
            aux_model_part.Elements.clear()
            aux_model_part.Conditions.clear()
            aux_model_part.Nodes.clear()
        else:
            # create the model part from scratch
            aux_model_part = current_model.CreateModelPart(aux_model_part_name)
            aux_model_part.AddNodalSolutionStepVariable(KratosMultiphysics.DISTANCE)
            aux_model_part.AddNodalSolutionStepVariable(KratosMultiphysics.DISTANCE_GRADIENT)
            aux_model_part.AddNodalSolutionStepVariable(KratosMultiphysics.FLAG_VARIABLE)

        # populating the empty model part (copy, do not just assign!)
        prop = self.ModelPart.Properties[0]
        for node in self.ModelPart.Nodes:
            n = aux_model_part.CreateNewNode( node.Id, node.X, node.Y, node.Z )
        for elem in self.ModelPart.Elements:
            nodes = elem.GetNodes()
            e = aux_model_part.CreateNewElement("Element3D4N", elem.Id,  [nodes[0].Id, nodes[1].Id, nodes[2].Id, nodes[3].Id], prop)

        KratosMultiphysics.CalculateDistanceToSkinProcess3D(aux_model_part, self.building_hull_model_part ).Execute()

        # check the distance field
        pos = 0; neg = 0
        for node in aux_model_part.Nodes:
            if node.GetSolutionStepValue( KratosMultiphysics.DISTANCE ) > 0.0:
                pos = 1
            if node.GetSolutionStepValue( KratosMultiphysics.DISTANCE ) < 0.0:
                neg = 1

        if ( pos == 0 or neg == 0):
            return

        # inversion of the distance field
        if invert_distance_field:
            for node in aux_model_part.Nodes:
                node.SetSolutionStepValue( KratosMultiphysics.DISTANCE, -node.GetSolutionStepValue( KratosMultiphysics.DISTANCE ) )

        # getting rid of the +/- inf values around the zero level
        for node in aux_model_part.Nodes:
            if ( node.GetSolutionStepValue( KratosMultiphysics.DISTANCE) > 1000.0 ):
                node.SetSolutionStepValue(KratosMultiphysics.DISTANCE, 100.0 )
            elif ( node.GetSolutionStepValue( KratosMultiphysics.DISTANCE) < -1000.0 ):
                node.SetSolutionStepValue(KratosMultiphysics.DISTANCE, -100.0 )

        # full distance field only needed if shifts are necessary
        if (size_reduction != 0):
            variational_distance_process = self._set_variational_distance_process_serial( aux_model_part, "DistanceFromSkin" )
            variational_distance_process.Execute()

        for node in aux_model_part.Nodes:
            source = node
            destination = self.ModelPart.GetNodes()[source.Id]
            # finding the minimum
            distance = min( source.GetSolutionStepValue( KratosMultiphysics.DISTANCE ) + size_reduction, destination.GetSolutionStepValue( KratosMultiphysics.DISTANCE ) )
            destination.SetSolutionStepValue( KratosMultiphysics.DISTANCE, distance )

        self.HasDistanceField = True


    def SetInitialDistanceField( self, distance_value = 1.0 ):

        for node in self.ModelPart.Nodes:
            node.SetSolutionStepValue( KratosMultiphysics.DISTANCE, distance_value )

        self.HasDistanceField = True


    def ShiftGlobalBuildingDistanceField( self, distance_shift_value = 0.0 ):

        if not self.HasDistanceField:
            KratosMultiphysics.Logger.PrintWarning("GeoBuilding", "Refinement around the building not possible. Please compute the distance field from the hull.")
            return

        for node in self.ModelPart.Nodes:
            distance_value = node.GetSolutionStepValue( KratosMultiphysics.DISTANCE ) + distance_shift_value
            node.SetSolutionStepValue( KratosMultiphysics.DISTANCE, distance_value )


    def RefineMeshNearBuilding( self, single_parameter ):

        if not self.HasModelPart:
            KratosMultiphysics.Logger.PrintWarning("GeoBuilding", "Model part has to be set, first.")
            return

        if not self.HasDistanceField:
            KratosMultiphysics.Logger.PrintWarning("GeoBuilding", "Refinement around the building not possible. Please compute the distance field from the hull.")
            return

        find_nodal_h = KratosMultiphysics.FindNodalHNonHistoricalProcess( self.ModelPart )
        find_nodal_h.Execute()

        KratosMultiphysics.VariableUtils().SetNonHistoricalVariable(KratosMultiphysics.NODAL_AREA, 0.0, self.ModelPart.Nodes)
        local_gradient = KratosMultiphysics.ComputeNodalGradientProcess3D(self.ModelPart, KratosMultiphysics.DISTANCE, KratosMultiphysics.DISTANCE_GRADIENT, KratosMultiphysics.NODAL_AREA)
        local_gradient.Execute()

        # We set to zero (or unit) the metric
        ZeroVector = KratosMultiphysics.Vector(6)
        ZeroVector[0] = 1.0; ZeroVector[1] = 1.0; ZeroVector[2] = 1.0
        ZeroVector[3] = 0.0; ZeroVector[4] = 0.0; ZeroVector[5] = 0.0

        for node in self.ModelPart.Nodes:
            node.SetValue(KratosMesh.METRIC_TENSOR_3D, ZeroVector)

        min_size = single_parameter
        # max_dist = 1.25 * single_parameter
        max_dist = 1.00
        # We define a metric using the ComputeLevelSetSolMetricProcess
        level_set_param = KratosMultiphysics.Parameters("""
            {
                "minimal_size"                         : """ + str(min_size) + """,
                "enforce_current"                      : true,
                "anisotropy_remeshing"                 : true,
                "anisotropy_parameters": {
                    "hmin_over_hmax_anisotropic_ratio"      : 0.9,
                    "boundary_layer_max_distance"           : """ + str(max_dist) + """,
                    "interpolation"                         : "Linear" }
            }
            """)
        metric_process = KratosMesh.ComputeLevelSetSolMetricProcess3D(self.ModelPart, KratosMultiphysics.DISTANCE_GRADIENT, level_set_param)
        metric_process.Execute()

        # We create the remeshing process
        remesh_param = KratosMultiphysics.Parameters("""{ }""")
        MmgProcess = KratosMesh.MmgProcess3D(self.ModelPart, remesh_param)
        MmgProcess.Execute()


    # [old version of this function] we create a copy of the model part
    def SubtractBuilding( self, min_size, max_size, hausdorff_value, interpolation="Linear", disc_type="ISOSURFACE", remove_regions="true"):

        if not self.HasModelPart:
            KratosMultiphysics.Logger.PrintWarning("GeoBuilding", "Model part has to be set, first.")
            return

        if not self.HasDistanceField:
            KratosMultiphysics.Logger.PrintWarning("GeoBuilding", "Building subtraction not possible. Please compute the distance field from the hull.")
            return

        ### moving procedure to another model part
        current_model = self.ModelPart.GetModel()
        if current_model.HasModelPart( "MainModelPart" ):
            current_model.DeleteModelPart( "MainModelPart" )

        
        main_model_part = current_model.CreateModelPart("MainModelPart")
        main_model_part.AddNodalSolutionStepVariable(KratosMultiphysics.DISTANCE)
        main_model_part.AddNodalSolutionStepVariable(KratosMultiphysics.DISTANCE_GRADIENT)
        main_model_part.CreateSubModelPart("AuxSubModelPart")
        
        
        # hard copy before surface discretization (we copy the self.ModelPart into the main_model_part)
        # [NG] the conditions are not copied in "HardCopyBeforeSurfaceDiscretization"
        main_model_part = KratosGeo.CleaningUtilities( self.ModelPart ).HardCopyBeforeSurfaceDiscretization( self.ModelPart, main_model_part )
        
        # We calculate the gradient of the distance variable
        find_nodal_h = KratosMultiphysics.FindNodalHNonHistoricalProcess(main_model_part)
        find_nodal_h.Execute()
        KratosMultiphysics.VariableUtils().SetNonHistoricalVariable(KratosMultiphysics.NODAL_AREA, 0.0, main_model_part.Nodes)
        local_gradient = KratosMultiphysics.ComputeNodalGradientProcess3D(main_model_part, KratosMultiphysics.DISTANCE, KratosMultiphysics.DISTANCE_GRADIENT, KratosMultiphysics.NODAL_AREA)
        local_gradient.Execute()
        
        ZeroVector = KratosMultiphysics.Vector(6)
        ZeroVector[0] = 0.0; ZeroVector[1] = 0.0; ZeroVector[2] = 0.0
        ZeroVector[3] = 0.0; ZeroVector[4] = 0.0; ZeroVector[5] = 0.0
        
        for node in main_model_part.Nodes:
            node.SetValue(KratosMesh.METRIC_TENSOR_3D, ZeroVector)

        # We define a metric using the ComputeLevelSetSolMetricProcess
        level_set_param = KratosMultiphysics.Parameters("""
            {
                "minimal_size"                         : """ + str(min_size) + """,
                "maximal_size"                         : """ + str(max_size) + """,
                "sizing_parameters":
                {
                    "reference_variable_name"          : "DISTANCE",
                    "boundary_layer_max_distance"      : 2.0
                },
                "enforce_current"                      : false,
                "anisotropy_remeshing"                 : false
            }
            """)
        
        # we set "interpolation"
        level_set_param["sizing_parameters"].AddEmptyValue("interpolation")
        level_set_param["sizing_parameters"]["interpolation"].SetString(interpolation)      # the options are: constant / linear / exponential

        metric_process = KratosMesh.ComputeLevelSetSolMetricProcess3D(main_model_part, KratosMultiphysics.DISTANCE_GRADIENT, level_set_param)
        metric_process.Execute()

        # The Hausdorff parameter is an important parameter to decrease the mesh size
        remesh_param = KratosMultiphysics.Parameters("""{
            "filename"                              : "out",
            "isosurface_parameters"                 :
            {
                "isosurface_variable"               : "DISTANCE",
                "nonhistorical_variable"            : false,
                "remove_regions"                    : """ + remove_regions + """
            },
            "framework"                             : "Eulerian",
            "internal_variables_parameters"         :
            {
                "allocation_size"                       : 1000,
                "bucket_size"                           : 4,
                "search_factor"                         : 2,
                "interpolation_type"                    : "LST",
                "internal_variable_interpolation_list"  : []
            },
            "force_sizes"                           :
            {
                "force_min"                             : true,
                "minimal_size"                          : """ + str(min_size) + """,
                "force_max"                             : true,
                "maximal_size"                          : """ + str(max_size) + """
            },
            "advanced_parameters"                   :
            {
                "force_hausdorff_value"                 : true,
                "hausdorff_value"                       : """ + str(hausdorff_value) + """,
                "no_move_mesh"                          : false,
                "no_surf_mesh"                          : false,
                "no_insert_mesh"                        : false,
                "no_swap_mesh"                          : false,
                "normal_regularization_mesh"            : false,
                "deactivate_detect_angle"               : false,
                "force_gradation_value"                 : true,
                "gradation_value"                       : 1.2
            },
            "save_external_files"                   : false,
            "save_mdpa_file"                        : false,
            "max_number_of_searchs"                 : 1000,
            "interpolate_non_historical"            : false,
            "extrapolate_contour_values"            : true,
            "surface_elements"                      : false,
            "search_parameters"                     :
            {
                "allocation_size"                       : 1000,
                "bucket_size"                           : 4,
                "search_factor"                         : 2.0
            },
            "echo_level"                            : 3,
            "debug_result_mesh"                     : false,
            "step_data_size"                        : 0,
            "initialize_entities"                   : true,
            "remesh_at_non_linear_iteration"        : false,
            "buffer_size"                           : 0
        }""")

        # we set "discretization_type"
        remesh_param.AddEmptyValue("discretization_type")
        remesh_param["discretization_type"].SetString(disc_type)    # the options are: STANDARD / ISOSURFACE

        MmgProcess = KratosMesh.MmgProcess3D(main_model_part, remesh_param)
        MmgProcess.Execute()

        # [NG] check if it is correct... must be all conditions equal to "-1.0e-7"? probably only the bottom conditions. CHECK IT!
        for node in main_model_part.Nodes:
            node.SetSolutionStepValue( KratosMultiphysics.DISTANCE, 1.0 )
        # [NG] in "main_model_part" there are no Conditions... They have been deleted in MMG process with "remove_regions"=true. CHECK IT!
        for cond in main_model_part.Conditions:
            nodes = cond.GetNodes()
            for node in nodes:
                if ( node in main_model_part.Nodes ):
                    node.SetSolutionStepValue( KratosMultiphysics.DISTANCE, -1.0e-7 )

        self.ModelPart.Nodes.clear()
        self.ModelPart.Conditions.clear()
        self.ModelPart.Elements.clear()
        self.ModelPart.RemoveSubModelPart("Parts_Fluid")
        self.ModelPart.CreateSubModelPart("Parts_Fluid")
        self.ModelPart.RemoveSubModelPart("Complete_Boundary")
        self.ModelPart.CreateSubModelPart("Complete_Boundary")


        # hard copy after surface discretization
        self.ModelPart = KratosGeo.CleaningUtilities( self.ModelPart ).HardCopyAfterSurfaceDiscretization( main_model_part, self.ModelPart )


    # [NG] self.ModelPart is used directly (without a copy)
    def SubtractBuildingMOD( self, min_size, max_size, hausdorff_value, interpolation="Linear", disc_type="ISOSURFACE", remove_regions="true"):

        if not self.HasModelPart:
            KratosMultiphysics.Logger.PrintWarning("GeoBuilding", "Model part has to be set, first.")
            return

        if not self.HasDistanceField:
            KratosMultiphysics.Logger.PrintWarning("GeoBuilding", "Building subtraction not possible. Please compute the distance field from the hull.")
            return
        
        # a new sub model part where only nodes and elements are stored
        if not self.ModelPart.HasSubModelPart("ElementSubModelPart"):
            sub_elem = self.ModelPart.CreateSubModelPart("ElementSubModelPart")
            for node in self.ModelPart.Nodes:
                sub_elem.AddNode(node, 0)
            for elem in self.ModelPart.Elements:
                sub_elem.AddElement(elem, 0)

        # We calculate the gradient of the distance variable
        find_nodal_h = KratosMultiphysics.FindNodalHNonHistoricalProcess(self.ModelPart)
        find_nodal_h.Execute()
        KratosMultiphysics.VariableUtils().SetNonHistoricalVariable(KratosMultiphysics.NODAL_AREA, 0.0, self.ModelPart.Nodes)
        local_gradient = KratosMultiphysics.ComputeNodalGradientProcess3D(self.ModelPart, KratosMultiphysics.DISTANCE, KratosMultiphysics.DISTANCE_GRADIENT, KratosMultiphysics.NODAL_AREA)
        local_gradient.Execute()

        
        ZeroVector = KratosMultiphysics.Vector(6)
        ZeroVector[0] = 0.0; ZeroVector[1] = 0.0; ZeroVector[2] = 0.0
        ZeroVector[3] = 0.0; ZeroVector[4] = 0.0; ZeroVector[5] = 0.0
        
        for node in self.ModelPart.Nodes:
            node.SetValue(KratosMesh.METRIC_TENSOR_3D, ZeroVector)

        # We define a metric using the ComputeLevelSetSolMetricProcess
        level_set_param = KratosMultiphysics.Parameters("""
            {
                "minimal_size"                         : """ + str(min_size) + """,
                "maximal_size"                         : """ + str(max_size) + """,
                "sizing_parameters":
                {
                    "reference_variable_name"          : "DISTANCE",
                    "boundary_layer_max_distance"      : 2.0
                },
                "enforce_current"                      : false,
                "anisotropy_remeshing"                 : false
            }
            """)
        
        # we set "interpolation"
        level_set_param["sizing_parameters"].AddEmptyValue("interpolation")
        level_set_param["sizing_parameters"]["interpolation"].SetString(interpolation)      # the options are: constant / linear / exponential

        metric_process = KratosMesh.ComputeLevelSetSolMetricProcess3D(self.ModelPart, KratosMultiphysics.DISTANCE_GRADIENT, level_set_param)
        metric_process.Execute()

        # The Hausdorff parameter is an important parameter to decrease the mesh size
        remesh_param = KratosMultiphysics.Parameters("""{
            "filename"                              : "out",
            "isosurface_parameters"                 :
            {
                "isosurface_variable"               : "DISTANCE",
                "nonhistorical_variable"            : false,
                "remove_regions"                    : """ + remove_regions + """
            },
            "framework"                             : "Eulerian",
            "internal_variables_parameters"         :
            {
                "allocation_size"                       : 1000,
                "bucket_size"                           : 4,
                "search_factor"                         : 2,
                "interpolation_type"                    : "LST",
                "internal_variable_interpolation_list"  : []
            },
            "force_sizes"                           :
            {
                "force_min"                             : true,
                "minimal_size"                          : """ + str(min_size) + """,
                "force_max"                             : true,
                "maximal_size"                          : """ + str(max_size) + """
            },
            "advanced_parameters"                   :
            {
                "force_hausdorff_value"                 : true,
                "hausdorff_value"                       : """ + str(hausdorff_value) + """,
                "no_move_mesh"                          : false,
                "no_surf_mesh"                          : false,
                "no_insert_mesh"                        : false,
                "no_swap_mesh"                          : false,
                "normal_regularization_mesh"            : false,
                "deactivate_detect_angle"               : false,
                "force_gradation_value"                 : true,
                "gradation_value"                       : 1.2
            },
            "save_external_files"                   : false,
            "save_mdpa_file"                        : false,
            "max_number_of_searchs"                 : 1000,
            "interpolate_non_historical"            : false,
            "extrapolate_contour_values"            : true,
            "surface_elements"                      : false,
            "search_parameters"                     :
            {
                "allocation_size"                       : 1000,
                "bucket_size"                           : 4,
                "search_factor"                         : 2.0
            },
            "echo_level"                            : 3,
            "debug_result_mesh"                     : false,
            "step_data_size"                        : 0,
            "initialize_entities"                   : true,
            "remesh_at_non_linear_iteration"        : false,
            "buffer_size"                           : 0
        }""")

        # we set "discretization_type"
        remesh_param.AddEmptyValue("discretization_type")
        remesh_param["discretization_type"].SetString(disc_type)    # the options are: STANDARD / ISOSURFACE

        MmgProcess = KratosMesh.MmgProcess3D(self.ModelPart, remesh_param)
        MmgProcess.Execute()

        self.ModelPart.RemoveSubModelPart("ElementSubModelPart")        # we need this sub model part only for MMG process; so now we can delete it

        # reset distance
        for node in self.ModelPart.Nodes:
            node.SetSolutionStepValue(KratosMultiphysics.DISTANCE, 0.0)
        

        # #################################################
        # # MMGIO check
        # # add MMGIO to WriteModelPart and after ReadModelPart
        # if (disc_type == "ISOSURFACE"):
        #     print("\n\n****** MMGIO START ******\n")
        #     # we write the info from the model part
        #     mmgio = KratosMesh.MmgIO3D("MMGIO/test_02/WriteModelPart_1")
        #     mmgio.WriteModelPart(self.ModelPart)

        #     # print(self.ModelPart)
        #     # input("self.ModelPart")
            
        #     # we read the model part
        #     current_model = self.ModelPart.GetModel()
        #     read_model_part = current_model.CreateModelPart("ReadModelPart")
        #     mmgio.ReadModelPart(read_model_part)

        #     # print(read_model_part)
        #     # input("PAUSE")

        #     # we write the new info from the read_model_part
        #     mmgio = KratosMesh.MmgIO3D("MMGIO/test_02/WriteModelPart_2")
        #     mmgio.WriteModelPart(read_model_part)

        #     print("MMGIO process done!")
        # #################################################
    

    def ShiftBuildingOnTerrain(self, buildings_model_part, terrain_model_part):
        # function to shift Buildings on terrain
        import numpy as np      # TODO: REMOVE NUMPY

        N = KratosMultiphysics.Vector(3)
        coords =  KratosMultiphysics.Array3()

        locate_on_background = KratosMultiphysics.BinBasedFastPointLocator2D(terrain_model_part)
        locate_on_background.UpdateSearchDatabase()

        already_moved = []            # list with all nodes already moved
        height = []

        self.ModelPart = buildings_model_part
        # KratosMultiphysics.ModelPartIO("data/building_model_part_ORG", KratosMultiphysics.IO.WRITE).WriteModelPart(self.ModelPart)
        ID_vertex = self.ModelPart.NumberOfNodes() + 1

        for num_building in range(self.ModelPart.NumberOfSubModelParts()):
            current_sub_model = self.ModelPart.GetSubModelPart("Building_{}".format(num_building+1))
            
            displacement = []        # the vector where we save the Z coordinate to evaluate the minimum
            for node_building in current_sub_model.Nodes:
                # take only nodes with z = 0.0 which are the nodes at the base of the Building
                if node_building.Z != 0.0:
                    continue

                # fill coords array
                coords[0] = node_building.X
                coords[1] = node_building.Y
                coords[2] = node_building.Z
                
                # "found" is a boolean variable (True if "node" is inside the mesh element)
                # "pelem" is a pointer to element that contain the node of the Building
                [found, N, pelem] = locate_on_background.FindPointOnMesh(coords)        # here we have the terrain element (but it is on xy plane)

                if not isinstance(pelem, KratosMultiphysics.Element):            # if "pelem" is not a "Kratos.Element", we go to the next node
                    continue
                
                # calculation of the intersection point between Building and terrain
                Norm = self._normal_triangle(pelem)

                # define plane
                planeNormal = np.array(Norm)
                planePoint = np.array(pelem.GetNode(0))

                # define ray
                rayDirection = np.array([0, 0, 1])
                rayPoint = np.array(coords)

                ndotu = planeNormal.dot(rayDirection)

                w = rayPoint - planePoint
                si = -planeNormal.dot(w) / ndotu
                Psi = w + si *rayDirection + planePoint
                
                displacement.append(Psi[2])                # append just coordinate Z

            # check if "displacement" is empty
            if displacement:
                height.append(min(displacement))                # quantity to move the n-th Building
            else:
                height.append(0)
            
            # check on the nodes shared by multiple elements
            node_mod = {}    # dictionary where the key is the old node Id and the "value" is the new node Id
            for node in current_sub_model.Nodes:
                if node.Id in already_moved:                    # if it is one of the already moved nodes
                    self.ModelPart.CreateNewNode(ID_vertex, node.X, node.Y, (node.Z + height[num_building]))    # a new node with different Id and Z coordinate moved
                    node_mod[node.Id] = ID_vertex                # fill the dictionary with the node to be removed (the key) and the node to be added (the value) in this current_sub_model
                    node.Id = ID_vertex                            # update the node of the element
                    ID_vertex += 1
                else:
                    already_moved.append(node.Id)                # fill "already_moved" with current Id node
                    node.Z = node.Z + height[num_building]            # move current node of the quantity in list "height"
            
            for old_node, new_node in node_mod.items():
                current_sub_model.AddNodes([new_node])            # add the new nodes in the current sub model part
                current_sub_model.RemoveNode(old_node)            # remove the old nodes that are now replaced

        for node in self.ModelPart.Nodes:
            node.Set(KratosMultiphysics.TO_ERASE,True)

        for sub_model in self.ModelPart.SubModelParts:
            for node in sub_model.Nodes:
                node.Set(KratosMultiphysics.TO_ERASE,False)

        # to erase unused nodes
        self.ModelPart.RemoveNodesFromAllLevels(KratosMultiphysics.TO_ERASE)

        # KratosMultiphysics.ModelPartIO("data/building_model_part_MOD", KratosMultiphysics.IO.WRITE).WriteModelPart(self.ModelPart)

        """ CHECK THIS """
        self.HasBuildingHull = True


    def DeleteBuildingsUnderValue(self, buildings_model_part, z_value=1e-7):
        # this function delete the buildings that are under a certain Z value

        self.ModelPart = buildings_model_part
        del_buildings = []
        for sub_model in self.ModelPart.SubModelParts:
            for elem in sub_model.Elements:
                for node in elem.GetNodes():
                    if (node.Z <= z_value):
                        del_buildings.append(sub_model.Name)
                        break
                else:
                    continue    # we continue with the other nodes if we haven't left previously loop
                break           # if we left the previously loop, we break also this loop and we go in the next SubModelPart
        
        del_buildings = list(set(del_buildings))
        
        for sub_model_to_del in del_buildings:
            sub_to_delete = self.ModelPart.GetSubModelPart(sub_model_to_del)
            # self.ModelPart.RemoveSubModelPart(sub_model_to_del)
            
            # we delete from the ModelPart the elements that are inside of this SubModelPart
            for elem in sub_to_delete.Elements:
                elem.Set(KratosMultiphysics.TO_ERASE, True)
            self.ModelPart.RemoveElementsFromAllLevels(KratosMultiphysics.TO_ERASE)

            # we delete from the ModelPart the nodes that are inside of this SubModelPart
            for node in sub_to_delete.Nodes:
                node.Set(KratosMultiphysics.TO_ERASE, True)
            self.ModelPart.RemoveNodesFromAllLevels(KratosMultiphysics.TO_ERASE)
            
            self.ModelPart.RemoveSubModelPart(sub_to_delete)
        
        # return self.ModelPart


### --- auxiliary functions --- ### -------------------------------------

    def _generate_building_model_part( self, name ):

        current_model = self.ModelPart.GetModel()

        if current_model.HasModelPart( name ):
            current_model.DeleteModelPart( name )

        self.building_hull_model_part = current_model.CreateModelPart( name )
        self.building_hull_model_part.AddNodalSolutionStepVariable(KratosMultiphysics.DISTANCE)
        self.building_hull_model_part.AddNodalSolutionStepVariable(KratosMultiphysics.DISTANCE_GRADIENT)


    def _set_variational_distance_process_serial(self, complete_model, aux_name):
        # Construct the variational distance calculation process
        serial_settings = KratosMultiphysics.Parameters("""
            {
                "linear_solver_settings"   : {
                    "solver_type" : "amgcl"
                }
            }
        """)
        import linear_solver_factory
        linear_solver = linear_solver_factory.ConstructSolver(serial_settings["linear_solver_settings"])
        maximum_iterations = 2
        if complete_model.ProcessInfo[KratosMultiphysics.DOMAIN_SIZE] == 2:
            variational_distance_process = KratosMultiphysics.VariationalDistanceCalculationProcess2D(
                complete_model,
                linear_solver,
                maximum_iterations,
                KratosMultiphysics.VariationalDistanceCalculationProcess2D.NOT_CALCULATE_EXACT_DISTANCES_TO_PLANE,
                aux_name )
        else:
            variational_distance_process = KratosMultiphysics.VariationalDistanceCalculationProcess3D(
                complete_model,
                linear_solver,
                maximum_iterations,
                KratosMultiphysics.VariationalDistanceCalculationProcess3D.NOT_CALCULATE_EXACT_DISTANCES_TO_PLANE,
                aux_name )

        return variational_distance_process


    def _normal_triangle(self, elem):
        P1 = elem.GetNode(0)
        P2 = elem.GetNode(1)
        P3 = elem.GetNode(2)

        Nx = (P2.Y-P1.Y)*(P3.Z-P1.Z) - (P3.Y-P1.Y)*(P2.Z-P1.Z)
        Ny = (P2.Z-P1.Z)*(P3.X-P1.X) - (P2.X-P1.X)*(P3.Z-P1.Z)
        Nz = (P2.X-P1.X)*(P3.Y-P1.Y) - (P3.X-P1.X)*(P2.Y-P1.Y)

        return [Nx, Ny, Nz]
