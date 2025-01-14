from __future__ import print_function, absolute_import, division #makes KratosMultiphysics backward compatible with python 2.6 and 2.7
import KratosMultiphysics.DEMApplication as DEM
import KratosMultiphysics.SwimmingDEMApplication as SDEM
import DEM_analysis_stage

BaseAnalysis = DEM_analysis_stage.DEMAnalysisStage

class FluidCoupledDEMAnalysisStage(BaseAnalysis):

    def __init__(self, model, project_parameters):
        self.sdem_parameters = project_parameters
        super(FluidCoupledDEMAnalysisStage, self).__init__(model, project_parameters['dem_parameters'])

    def SetSolverStrategy(self):
        import swimming_sphere_strategy as SolverStrategy
        return SolverStrategy

    def _CreateSolver(self):

        def SetSolverStrategy():
            strategy = self.sdem_parameters['dem_parameters']['solver_settings']['strategy'].GetString()
            filename = __import__(strategy)
            return filename

        return SetSolverStrategy().SwimmingStrategy(self.all_model_parts,
                                                    self.creator_destructor,
                                                    self.dem_fem_search,
                                                    self.sdem_parameters,
                                                    self.procedures)

    def SelectTranslationalScheme(self):
        translational_scheme = BaseAnalysis.SelectTranslationalScheme(self)
        translational_scheme_name = self.project_parameters["TranslationalIntegrationScheme"].GetString()

        if translational_scheme is None:
            if translational_scheme_name == 'Hybrid_Bashforth':
                return SDEM.HybridBashforthScheme()
            elif translational_scheme_name == "TerminalVelocityScheme":
                return SDEM.TerminalVelocityScheme()
            else:
                return None
        else:
            return translational_scheme

    def SelectRotationalScheme(self):
        rotational_scheme = BaseAnalysis.SelectRotationalScheme(self)
        translational_scheme_name = self.project_parameters["TranslationalIntegrationScheme"].GetString()
        rotational_scheme_name = self.project_parameters["RotationalIntegrationScheme"].GetString()

        if rotational_scheme is None:
            if rotational_scheme_name == 'Direct_Integration':
                if translational_scheme_name == 'Hybrid_Bashforth':
                    return SDEM.HybridBashforthScheme()
                elif translational_scheme_name == 'TerminalVelocityScheme':
                    return SDEM.TerminalVelocityScheme()
            elif rotational_scheme_name == 'Runge_Kutta':
                return SDEM.RungeKuttaScheme()
            elif rotational_scheme_name == 'Quaternion_Integration':
                return SDEM.QuaternionIntegrationScheme()
            else:
                return None
        else:
            return rotational_scheme

    def BaseReadModelParts(self, max_node_Id = 0, max_elem_Id = 0, max_cond_Id = 0):
        super(FluidCoupledDEMAnalysisStage, self).ReadModelParts(max_node_Id, max_elem_Id, max_cond_Id)

    def ReadModelParts(self, max_node_Id = 0, max_elem_Id = 0, max_cond_Id = 0):
        self.coupling_analysis.ReadDispersePhaseModelParts()

    def GetParticleHistoryWatcher(self):
        watcher_type = self.sdem_parameters["full_particle_history_watcher"].GetString()

        if watcher_type == 'Empty':
            return None
        elif watcher_type == 'ParticlesHistoryWatcher':
            return DEM.ParticlesHistoryWatcher()

    def IsTimeToPrintPostProcess(self):
        return self.analytic_data_counter.Tick()

    def SetGraphicalOutput(self):
        pass

    def GraphicalOutputInitialize(self):
        pass

    def PrintResultsForGid(self, time):
        pass

    def GraphicalOutputFinalize(self):
        pass