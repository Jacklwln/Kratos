# ==============================================================================
#  KratosShapeOptimizationApplication
#
#  License:         BSD License
#                   license: ShapeOptimizationApplication/license.txt
#
#  Main authors:    Baumgaertner Daniel, https://github.com/dbaumgaertner
#                   Geiser Armin, https://github.com/armingeiser
#
# ==============================================================================

# Import additional libraries
import os

# ==============================================================================
class ValueLogger():

    # --------------------------------------------------------------------------
    def __init__( self, communicator, optimization_settings ):
        self.communicator = communicator
        self.optimization_settings = optimization_settings

        self.objectives = optimization_settings["objectives"]
        self.constraints = optimization_settings["constraints"]

        self.complete_log_file_name = self.__CreateCompleteLogFileName( optimization_settings )

        self.current_iteration = 0
        self.previos_iteration = 0

        self.obj_reference_values = {}

        self.history = { "value"                         : {},
                         "standardized_value"            : {},
                         "abs_change"                    : {},
                         "rel_change"                    : {} }

        for itr in range(self.objectives.size()):
            objective_id = self.objectives[itr]["identifier"].GetString()
            self.history["value"][objective_id] = {}
            self.history["standardized_value"][objective_id] = {}
            self.history["abs_change"][objective_id] = {}
            self.history["rel_change"][objective_id] = {}

        for itr in range(self.constraints.size()):
            constraint_id = self.constraints[itr]["identifier"].GetString()
            self.history["value"][constraint_id] = {}
            self.history["standardized_value"][constraint_id] = {}
            self.history["abs_change"][constraint_id] = {}
            self.history["rel_change"][constraint_id] = {}

        if self.objectives.size() > 1:
            self.history["combined_objective_value"] = {}
            self.history["combined_objective_abs_change"] = {}
            self.history["combined_objective_rel_change"] = {}

        self.fixed_keys = list(self.history.keys())

    # --------------------------------------------------------------------------
    def InitializeLogging( self ):
        pass

    # --------------------------------------------------------------------------
    def LogCurrentValues( self, current_iteration, additional_values_dictionary ):
        self.current_iteration = current_iteration

        self.__LogValuesToHistory(additional_values_dictionary)
        self._WriteCurrentValuesToConsole()
        self._WriteCurrentValuesToFile()

        self.previos_iteration = current_iteration

    # --------------------------------------------------------------------------
    def FinalizeLogging( self ):
        pass

    # --------------------------------------------------------------------------
    def GetValue( self, key, iteration ):
        return self.history[key][iteration]

    # --------------------------------------------------------------------------
    def GetValueHistory( self, key ):
        return self.history[key]

    # --------------------------------------------------------------------------
    @staticmethod
    def __CreateCompleteLogFileName( optimization_settings ):
        resultsDirectory = optimization_settings["output"]["output_directory"].GetString()
        optimizationLogFilename = optimization_settings["output"]["optimization_log_filename"].GetString() + ".csv"
        return os.path.join( resultsDirectory, optimizationLogFilename )

    # --------------------------------------------------------------------------
    def __LogValuesToHistory( self, additional_values_dictionary ):
        self.__LogObjectiveValuesToHistory()
        self.__LogConstraintValuesToHistory()
        self.__LogAdditionalValuesToHistory( additional_values_dictionary )

    # --------------------------------------------------------------------------
    def __LogObjectiveValuesToHistory( self ):
        for itr in range(self.objectives.size()):
            objective_id = self.objectives[itr]["identifier"].GetString()
            is_first_log = self.history["value"][objective_id] == {}

            self.history["value"][objective_id][self.current_iteration] = self.communicator.getValue( objective_id )
            self.history["standardized_value"][objective_id][self.current_iteration] = self.communicator.getStandardizedValue( objective_id )

            if is_first_log:
                self.obj_reference_values[objective_id] = self.communicator.getValue( objective_id )
                if abs(self.obj_reference_values[objective_id])<1e-12:
                    print("\n> WARNING: Reference value for objective \""+objective_id+"\" < 1e-12!! Therefore, standard reference value of 1 is assumed! ")
                    self.obj_reference_values[objective_id] = 1.0

                self.history["abs_change"][objective_id] = {self.current_iteration: 0.0}
                self.history["rel_change"][objective_id] = {self.current_iteration: 0.0}
            else:
                current_obj_value = self.history["value"][objective_id][self.current_iteration]
                previos_obj_value = self.history["value"][objective_id][self.previos_iteration]
                abs_change = 100*(current_obj_value-self.obj_reference_values[objective_id]) / abs(self.obj_reference_values[objective_id])
                rel_change = 100*(current_obj_value-previos_obj_value) / abs(self.obj_reference_values[objective_id])

                self.history["abs_change"][objective_id][self.current_iteration] = abs_change
                self.history["rel_change"][objective_id][self.current_iteration] = rel_change

    # --------------------------------------------------------------------------
    def __LogConstraintValuesToHistory( self ):
        for itr in range(self.constraints.size()):
            constraint_id = self.constraints[itr]["identifier"].GetString()
            self.history["value"][constraint_id][self.current_iteration] = self.communicator.getValue( constraint_id )
            self.history["standardized_value"][constraint_id][self.current_iteration] = self.communicator.getStandardizedValue( constraint_id )

    # --------------------------------------------------------------------------
    def __LogAdditionalValuesToHistory( self, additional_values_dictionary ):
        for key, value in additional_values_dictionary.items():
            if key in self.fixed_keys:
                raise NameError("ValueLogger: The key \""+key+"\" when logging additional values is prohibited! Prohibitet keys are: "+str(self.fixed_keys))

            if key not in self.history.keys():
                self.history[key] = {}

            self.history[key][self.current_iteration] = value

# ==============================================================================
