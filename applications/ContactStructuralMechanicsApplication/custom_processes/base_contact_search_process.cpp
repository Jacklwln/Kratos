// KRATOS  ___|  |                   |                   |
//       \___ \  __|  __| |   |  __| __| |   |  __| _` | |
//             | |   |    |   | (    |   |   | |   (   | |
//       _____/ \__|_|   \__,_|\___|\__|\__,_|_|  \__,_|_| MECHANICS
//
//  License:		 BSD License
//					 license: StructuralMechanicsApplication/license.txt
//
//  Main authors:    Vicente Mataix Ferrandiz
//

// System includes
#include <algorithm>

// External includes

// Project includes
#include "input_output/logger.h"
#include "utilities/geometrical_projection_utilities.h"
#include "utilities/mortar_utilities.h"
#include "utilities/variable_utils.h"

/* Custom utilities */
#include "custom_utilities/contact_utilities.h"
#include "custom_processes/base_contact_search_process.h"
#include "custom_processes/find_intersected_geometrical_objects_with_obb_for_contact_search_process.h"

namespace Kratos
{
/// Local Flags
template<SizeType TDim, SizeType TNumNodes, SizeType TNumNodesMaster>
const Kratos::Flags BaseContactSearchProcess<TDim, TNumNodes, TNumNodesMaster>::INVERTED_SEARCH(Kratos::Flags::Create(0));
template<SizeType TDim, SizeType TNumNodes, SizeType TNumNodesMaster>
const Kratos::Flags BaseContactSearchProcess<TDim, TNumNodes, TNumNodesMaster>::NOT_INVERTED_SEARCH(Kratos::Flags::Create(0, false));
template<SizeType TDim, SizeType TNumNodes, SizeType TNumNodesMaster>
const Kratos::Flags BaseContactSearchProcess<TDim, TNumNodes, TNumNodesMaster>::CREATE_AUXILIAR_CONDITIONS(Kratos::Flags::Create(1));
template<SizeType TDim, SizeType TNumNodes, SizeType TNumNodesMaster>
const Kratos::Flags BaseContactSearchProcess<TDim, TNumNodes, TNumNodesMaster>::NOT_CREATE_AUXILIAR_CONDITIONS(Kratos::Flags::Create(1, false));
template<SizeType TDim, SizeType TNumNodes, SizeType TNumNodesMaster>
const Kratos::Flags BaseContactSearchProcess<TDim, TNumNodes, TNumNodesMaster>::MULTIPLE_SEARCHS(Kratos::Flags::Create(2));
template<SizeType TDim, SizeType TNumNodes, SizeType TNumNodesMaster>
const Kratos::Flags BaseContactSearchProcess<TDim, TNumNodes, TNumNodesMaster>::NOT_MULTIPLE_SEARCHS(Kratos::Flags::Create(2, false));
template<SizeType TDim, SizeType TNumNodes, SizeType TNumNodesMaster>
const Kratos::Flags BaseContactSearchProcess<TDim, TNumNodes, TNumNodesMaster>::PREDEFINE_MASTER_SLAVE(Kratos::Flags::Create(3));
template<SizeType TDim, SizeType TNumNodes, SizeType TNumNodesMaster>
const Kratos::Flags BaseContactSearchProcess<TDim, TNumNodes, TNumNodesMaster>::NOT_PREDEFINE_MASTER_SLAVE(Kratos::Flags::Create(3, false));
template<SizeType TDim, SizeType TNumNodes, SizeType TNumNodesMaster>
const Kratos::Flags BaseContactSearchProcess<TDim, TNumNodes, TNumNodesMaster>::PURE_SLIP(Kratos::Flags::Create(4));
template<SizeType TDim, SizeType TNumNodes, SizeType TNumNodesMaster>
const Kratos::Flags BaseContactSearchProcess<TDim, TNumNodes, TNumNodesMaster>::NOT_PURE_SLIP(Kratos::Flags::Create(4, false));

/***********************************************************************************/
/***********************************************************************************/

template<SizeType TDim, SizeType TNumNodes, SizeType TNumNodesMaster>
BaseContactSearchProcess<TDim, TNumNodes, TNumNodesMaster>::BaseContactSearchProcess(
    ModelPart& rMainModelPart,
    Parameters ThisParameters
    ):mrMainModelPart(rMainModelPart),
      mThisParameters(ThisParameters)
{
    KRATOS_TRY

    KRATOS_ERROR_IF(mrMainModelPart.HasSubModelPart("Contact") == false) << "AdvancedContactSearch:: Please add the Contact submodelpart to your modelpart list" << std::endl;

    Parameters default_parameters = GetDefaultParameters();

    mThisParameters.ValidateAndAssignDefaults(default_parameters);

    mCheckGap = this->ConvertCheckGap(mThisParameters["check_gap"].GetString());
    mOptions.Set(BaseContactSearchProcess::INVERTED_SEARCH, mThisParameters["inverted_search"].GetBool());
    mOptions.Set(BaseContactSearchProcess::PREDEFINE_MASTER_SLAVE, mThisParameters["predefined_master_slave"].GetBool());
    mOptions.Set(BaseContactSearchProcess::PURE_SLIP, mThisParameters["pure_slip"].GetBool());

    // The search tree considered
    const SearchTreeType type_search = ConvertSearchTree(mThisParameters["type_search"].GetString());
    if (mOptions.IsNot(BaseContactSearchProcess::PREDEFINE_MASTER_SLAVE) && type_search == SearchTreeType::OctreeWithOBB)
        mThisParameters["type_search"].SetString("KdtreeInRadius");

    // If we are going to consider multple searchs
    const std::string& id_name = mThisParameters["id_name"].GetString();
    const bool multiple_searchs = id_name == "" ? false : true;
    mOptions.Set(BaseContactSearchProcess::MULTIPLE_SEARCHS, multiple_searchs);

    // Check if the computing contact submodelpart
    const std::string sub_computing_model_part_name = "ComputingContactSub" + id_name;
    if (!(mrMainModelPart.HasSubModelPart("ComputingContact"))) { // We check if the submodelpart where the actual conditions used to compute contact are going to be computed
        ModelPart* p_computing_model_part = &mrMainModelPart.CreateSubModelPart("ComputingContact");
        p_computing_model_part->CreateSubModelPart(sub_computing_model_part_name);
    } else {
        ModelPart& r_computing_contact_model_part = mrMainModelPart.GetSubModelPart("ComputingContact");
        if (!(r_computing_contact_model_part.HasSubModelPart(sub_computing_model_part_name)) && mOptions.Is(BaseContactSearchProcess::MULTIPLE_SEARCHS)) {
            r_computing_contact_model_part.CreateSubModelPart(sub_computing_model_part_name);
        } else { // We clean the existing modelpart
            ModelPart& r_sub_computing_contact_model_part = mOptions.IsNot(BaseContactSearchProcess::MULTIPLE_SEARCHS) ? r_computing_contact_model_part : r_computing_contact_model_part.GetSubModelPart(sub_computing_model_part_name);
            CleanModelPart(r_sub_computing_contact_model_part);
        }
    }

    // Updating the base condition
    mConditionName = mThisParameters["condition_name"].GetString();
    if (mConditionName == "") {
        mOptions.Set(BaseContactSearchProcess::CREATE_AUXILIAR_CONDITIONS, false);
    } else {
        mOptions.Set(BaseContactSearchProcess::CREATE_AUXILIAR_CONDITIONS, true);
        std::ostringstream condition_name;
        condition_name << mConditionName << "Condition" << TDim << "D" << TNumNodes << "N" << mThisParameters["final_string"].GetString();
        mConditionName = condition_name.str();
    }

//     KRATOS_DEBUG_ERROR_IF_NOT(KratosComponents<Condition>::Has(mConditionName)) << "Condition " << mConditionName << " not registered" << std::endl;

    // We get the contact model part
    ModelPart& r_contact_model_part = mrMainModelPart.GetSubModelPart("Contact");
    ModelPart& r_sub_contact_model_part = mOptions.IsNot(BaseContactSearchProcess::MULTIPLE_SEARCHS) ? r_contact_model_part : r_contact_model_part.GetSubModelPart("ContactSub" + id_name);

    // We set to zero the NORMAL_GAP
    if (mCheckGap == CheckGap::MappingCheck) {
        VariableUtils().SetNonHistoricalVariable(NORMAL_GAP, 0.0, r_sub_contact_model_part.Nodes());
    }

    // Iterate in the conditions
    ConditionsArrayType& r_conditions_array = r_sub_contact_model_part.Conditions();
    VariableUtils().SetFlag(ACTIVE, false, r_conditions_array);

    // We identify the type of solution
    mTypeSolution =  TypeSolution::VectorLagrangeMultiplier;
    if (mrMainModelPart.HasNodalSolutionStepVariable(VECTOR_LAGRANGE_MULTIPLIER) == false) {
        if (mrMainModelPart.HasNodalSolutionStepVariable(LAGRANGE_MULTIPLIER_CONTACT_PRESSURE)) {
            mTypeSolution = TypeSolution::NormalContactStress;
        } else {
            const bool is_frictional = mrMainModelPart.Is(SLIP);
            if (mrMainModelPart.HasNodalSolutionStepVariable(WEIGHTED_GAP)) {
                if (is_frictional) {
                    mTypeSolution = TypeSolution::FrictionalPenaltyMethod;
                } else {
                    mTypeSolution = TypeSolution::FrictionlessPenaltyMethod;
                }
            } else if (mrMainModelPart.HasNodalSolutionStepVariable(SCALAR_LAGRANGE_MULTIPLIER)) {
                mTypeSolution = TypeSolution::ScalarLagrangeMultiplier;
            } else {
                if (is_frictional) {
                    mTypeSolution = TypeSolution::OtherFrictional;
                } else {
                    mTypeSolution = TypeSolution::OtherFrictionless;
                }
            }
        }
    }

    KRATOS_CATCH("")
}

/***********************************************************************************/
/***********************************************************************************/

template<SizeType TDim, SizeType TNumNodes, SizeType TNumNodesMaster>
void BaseContactSearchProcess<TDim, TNumNodes, TNumNodesMaster>::Execute()
{
    KRATOS_TRY

    // We execute the different phases of the process all together
    this->ExecuteInitialize();
    this->ExecuteInitializeSolutionStep();
    this->ExecuteFinalizeSolutionStep();

    KRATOS_CATCH("")
}

/***********************************************************************************/
/***********************************************************************************/

template<SizeType TDim, SizeType TNumNodes, SizeType TNumNodesMaster>
void BaseContactSearchProcess<TDim, TNumNodes, TNumNodesMaster>::ExecuteInitialize()
{
    KRATOS_TRY

    // We initialize the search utility
    this->CheckContactModelParts();
    this->CreatePointListMortar();
    this->InitializeMortarConditions();

    KRATOS_CATCH("")
}

/***********************************************************************************/
/***********************************************************************************/

template<SizeType TDim, SizeType TNumNodes, SizeType TNumNodesMaster>
void BaseContactSearchProcess<TDim, TNumNodes, TNumNodesMaster>::ExecuteInitializeSolutionStep()
{
    KRATOS_TRY

    // We compute the search pairs
    this->ClearMortarConditions();
    this->UpdateMortarConditions();
//     this->CheckMortarConditions();

    KRATOS_CATCH("")
}

/***********************************************************************************/
/***********************************************************************************/

template<SizeType TDim, SizeType TNumNodes, SizeType TNumNodesMaster>
void BaseContactSearchProcess<TDim, TNumNodes, TNumNodesMaster>::ExecuteFinalizeSolutionStep()
{
    KRATOS_TRY

    // We clear the pairs
    this->ClearMortarConditions();

    KRATOS_CATCH("")
}

/***********************************************************************************/
/***********************************************************************************/

template<SizeType TDim, SizeType TNumNodes, SizeType TNumNodesMaster>
void BaseContactSearchProcess<TDim, TNumNodes, TNumNodesMaster>::InitializeMortarConditions()
{
    KRATOS_TRY

    // Iterate in the conditions
    ModelPart& r_contact_model_part = mrMainModelPart.GetSubModelPart("Contact");
    ModelPart& r_sub_contact_model_part = mOptions.IsNot(BaseContactSearchProcess::MULTIPLE_SEARCHS) ? r_contact_model_part : r_contact_model_part.GetSubModelPart("ContactSub"+mThisParameters["id_name"].GetString());
    ConditionsArrayType& r_conditions_array = r_sub_contact_model_part.Conditions();
    const auto it_cond_begin = r_conditions_array.begin();
    const int num_conditions = static_cast<int>(r_conditions_array.size());

    #pragma omp parallel for
    for(int i = 0; i < num_conditions; ++i) {
        auto it_cond = it_cond_begin + i;
        if (!(it_cond->Has(INDEX_MAP))) {
            it_cond->SetValue(INDEX_MAP, Kratos::make_shared<IndexMap>());
//             it_cond->GetValue(INDEX_MAP)->reserve(mThisParameters["allocation_size"].GetInt());
        }
    }

    KRATOS_CATCH("")
}

/***********************************************************************************/
/***********************************************************************************/

template<SizeType TDim, SizeType TNumNodes, SizeType TNumNodesMaster>
void BaseContactSearchProcess<TDim, TNumNodes, TNumNodesMaster>::SetOriginDestinationModelParts(ModelPart& rModelPart)
{
    KRATOS_TRY

    // We check if the MasterSubModelPart already exists
    const std::string& id_name = mThisParameters["id_name"].GetString();
    if (rModelPart.HasSubModelPart("MasterSubModelPart" + id_name) == false) {
        rModelPart.CreateSubModelPart("MasterSubModelPart" + id_name);
    } else {
        rModelPart.RemoveSubModelPart("MasterSubModelPart" + id_name);
        rModelPart.CreateSubModelPart("MasterSubModelPart" + id_name);
    }
    // We check if the SlaveSubModelPart already exists
    if (rModelPart.HasSubModelPart("SlaveSubModelPart" + id_name) == false) {
        rModelPart.CreateSubModelPart("SlaveSubModelPart" + id_name);
    } else {
        rModelPart.RemoveSubModelPart("SlaveSubModelPart" + id_name);
        rModelPart.CreateSubModelPart("SlaveSubModelPart" + id_name);
    }

    ModelPart& r_master_model_part = rModelPart.GetSubModelPart("MasterSubModelPart" + id_name);
    ModelPart& r_slave_model_part = rModelPart.GetSubModelPart("SlaveSubModelPart" + id_name);

    // The vectors containing the ids
    std::vector<IndexType> slave_nodes_ids,  master_nodes_ids;
    std::vector<IndexType> slave_conditions_ids, master_conditions_ids;

    // Begin iterators
    const auto it_node_begin = rModelPart.NodesBegin();
    const auto it_cond_begin = rModelPart.ConditionsBegin();

    #pragma omp parallel
    {
        // Creating a buffer for parallel vector fill
        std::vector<IndexType> slave_nodes_ids_buffer, master_nodes_ids_buffer;
        std::vector<IndexType> slave_conditions_ids_buffer, master_conditions_ids_buffer;

        #pragma omp for
        for(int i=0; i<static_cast<int>(rModelPart.Nodes().size()); ++i) {
            auto it_node = it_node_begin + i;

            if (it_node->Is(SLAVE) == !mOptions.Is(BaseContactSearchProcess::INVERTED_SEARCH)) {
                slave_nodes_ids_buffer.push_back(it_node->Id());
            }
            if (it_node->Is(MASTER) == !mOptions.Is(BaseContactSearchProcess::INVERTED_SEARCH)) {
                master_nodes_ids_buffer.push_back(it_node->Id());
            }
        }

        #pragma omp for
        for(int i=0; i<static_cast<int>(rModelPart.Conditions().size()); ++i) {
            auto it_cond = it_cond_begin + i;

            if (it_cond->Is(SLAVE) == !mOptions.Is(BaseContactSearchProcess::INVERTED_SEARCH)) {
                slave_conditions_ids_buffer.push_back(it_cond->Id());
            }
            if (it_cond->Is(MASTER) == !mOptions.Is(BaseContactSearchProcess::INVERTED_SEARCH)) {
                master_conditions_ids_buffer.push_back(it_cond->Id());
            }
        }

        // Combine buffers together
        #pragma omp critical
        {
            std::move(slave_nodes_ids_buffer.begin(),slave_nodes_ids_buffer.end(),back_inserter(slave_nodes_ids));
            std::move(master_nodes_ids_buffer.begin(),master_nodes_ids_buffer.end(),back_inserter(master_nodes_ids));
            std::move(slave_conditions_ids_buffer.begin(),slave_conditions_ids_buffer.end(),back_inserter(slave_conditions_ids));
            std::move(master_conditions_ids_buffer.begin(),master_conditions_ids_buffer.end(),back_inserter(master_conditions_ids));
        }
    }

    // Finally we add the nodes and conditions to the submodelparts
    r_slave_model_part.AddNodes(slave_nodes_ids);
    r_slave_model_part.AddConditions(slave_conditions_ids);
    r_master_model_part.AddNodes(master_nodes_ids);
    r_master_model_part.AddConditions(master_conditions_ids);

    KRATOS_ERROR_IF(r_master_model_part.Conditions().size() == 0) << "No origin conditions. Check your flags are properly set" << std::endl;
    KRATOS_ERROR_IF(r_slave_model_part.Conditions().size() == 0) << "No destination conditions. Check your flags are properly set" << std::endl;

    KRATOS_CATCH("")
}

/***********************************************************************************/
/***********************************************************************************/

template<SizeType TDim, SizeType TNumNodes, SizeType TNumNodesMaster>
void BaseContactSearchProcess<TDim, TNumNodes, TNumNodesMaster>::ClearMortarConditions()
{
    KRATOS_TRY

    ResetContactOperators();

    ModelPart& r_contact_model_part = mrMainModelPart.GetSubModelPart("Contact");
    ModelPart& r_sub_contact_model_part = mOptions.IsNot(BaseContactSearchProcess::MULTIPLE_SEARCHS) ? r_contact_model_part : r_contact_model_part.GetSubModelPart("ContactSub"+mThisParameters["id_name"].GetString());
    NodesArrayType& r_nodes_array = r_sub_contact_model_part.Nodes();

    switch(mTypeSolution) {
        case TypeSolution::VectorLagrangeMultiplier :
            ClearComponentsMortarConditions(r_nodes_array);
            break;
        case TypeSolution::ScalarLagrangeMultiplier :
            ClearScalarMortarConditions(r_nodes_array);
            break;
        case TypeSolution::NormalContactStress :
            ClearALMFrictionlessMortarConditions(r_nodes_array);
            break;
        case TypeSolution::FrictionlessPenaltyMethod :
            break;
        case TypeSolution::FrictionalPenaltyMethod :
            break;
        case TypeSolution::OtherFrictionless :
            break;
        case TypeSolution::OtherFrictional :
            break;
    }

    KRATOS_CATCH("")
}

/***********************************************************************************/
/***********************************************************************************/

template<SizeType TDim, SizeType TNumNodes, SizeType TNumNodesMaster>
void BaseContactSearchProcess<TDim, TNumNodes, TNumNodesMaster>::CheckContactModelParts()
{
    KRATOS_TRY

    // Iterate in the conditions
    ModelPart& r_contact_model_part = mrMainModelPart.GetSubModelPart("Contact");
    ModelPart& r_sub_contact_model_part = mOptions.IsNot(BaseContactSearchProcess::MULTIPLE_SEARCHS) ? r_contact_model_part : r_contact_model_part.GetSubModelPart("ContactSub"+mThisParameters["id_name"].GetString());
    ConditionsArrayType& r_conditions_array = r_sub_contact_model_part.Conditions();

    const SizeType total_number_conditions = mrMainModelPart.GetRootModelPart().NumberOfConditions();

    std::vector<Condition::Pointer> auxiliar_conditions_vector;

    for(Condition& r_cond : r_conditions_array) {
        if (r_cond.Is(MARKER)) {
            // Setting the flag to remove
            r_cond.Set(TO_ERASE, true);

            // Creating new condition
            Condition::Pointer p_new_cond = r_cond.Clone(total_number_conditions + r_cond.Id(), r_cond.GetGeometry());
            auxiliar_conditions_vector.push_back(p_new_cond);

            p_new_cond->SetData(r_cond.GetData()); // TODO: Remove when fixed on the core
            p_new_cond->SetValue(INDEX_MAP, Kratos::make_shared<IndexMap>());
//             p_new_cond->GetValue(INDEX_MAP)->clear();
//             p_new_cond->GetValue(INDEX_MAP)->reserve(mThisParameters["allocation_size"].GetInt());
            p_new_cond->Set(Flags(r_cond));
            p_new_cond->Set(MARKER, true);
        } else {
            // Setting the flag to mark
            r_cond.Set(MARKER, true);
        }
    }

    // Finally we add the new conditions to the model part
    r_sub_contact_model_part.RemoveConditions(TO_ERASE);
    // Reorder ids (in order to keep the ids consistent)
    for (int i = 0; i < static_cast<int>(auxiliar_conditions_vector.size()); ++i) {
        auxiliar_conditions_vector[i]->SetId(total_number_conditions + i + 1);
    }
    ConditionsArrayType aux_conds;
    aux_conds.GetContainer() = auxiliar_conditions_vector;
    r_sub_contact_model_part.AddConditions(aux_conds.begin(), aux_conds.end());

    // Unsetting TO_ERASE
    VariableUtils().SetFlag(TO_ERASE, false, r_contact_model_part.Conditions());

    KRATOS_CATCH("")
}

/***********************************************************************************/
/***********************************************************************************/

template<SizeType TDim, SizeType TNumNodes, SizeType TNumNodesMaster>
void BaseContactSearchProcess<TDim, TNumNodes, TNumNodesMaster>::CreatePointListMortar()
{
    KRATOS_TRY

    // The search tree considered
    const SearchTreeType type_search = ConvertSearchTree(mThisParameters["type_search"].GetString());

    // Using KDTree
    if (type_search != SearchTreeType::OctreeWithOBB) {
        // Clearing the vector
        mPointListDestination.clear();

        // Iterate in the conditions
        ModelPart& r_contact_model_part = mrMainModelPart.GetSubModelPart("Contact");
        ModelPart& r_sub_contact_model_part = mOptions.IsNot(BaseContactSearchProcess::MULTIPLE_SEARCHS) ? r_contact_model_part : r_contact_model_part.GetSubModelPart("ContactSub"+mThisParameters["id_name"].GetString());
        ConditionsArrayType& r_conditions_array = r_sub_contact_model_part.Conditions();
        const auto it_cond_begin = r_conditions_array.begin();

        for(IndexType i = 0; i < r_conditions_array.size(); ++i) {
            auto it_cond = it_cond_begin + i;
            if (it_cond->Is(MASTER) == !mOptions.Is(BaseContactSearchProcess::INVERTED_SEARCH) || mOptions.IsNot(BaseContactSearchProcess::PREDEFINE_MASTER_SLAVE)) {
                mPointListDestination.push_back(Kratos::make_shared<PointType>((*it_cond.base())));
            }
        }

    #ifdef KRATOS_DEBUG
        // NOTE: We check the list
        for (IndexType i_point = 0; i_point < mPointListDestination.size(); ++i_point )
            mPointListDestination[i_point]->Check();
    #endif
    }

    KRATOS_CATCH("")
}

/***********************************************************************************/
/***********************************************************************************/

template<SizeType TDim, SizeType TNumNodes, SizeType TNumNodesMaster>
void BaseContactSearchProcess<TDim, TNumNodes, TNumNodesMaster>::UpdatePointListMortar()
{
    KRATOS_TRY

    // The search tree considered
    const SearchTreeType type_search = ConvertSearchTree(mThisParameters["type_search"].GetString());

    // Using KDTree
    if (type_search != SearchTreeType::OctreeWithOBB) {
        // We check if we are in a dynamic or static case
        const bool dynamic = mThisParameters["dynamic_search"].GetBool() ? mrMainModelPart.HasNodalSolutionStepVariable(VELOCITY) : false;
        const double delta_time = (dynamic) ? mrMainModelPart.GetProcessInfo()[DELTA_TIME] : 0.0;

        // The contact model parts
        ModelPart& r_contact_model_part = mrMainModelPart.GetSubModelPart("Contact");
        ModelPart& r_sub_contact_model_part = mOptions.IsNot(BaseContactSearchProcess::MULTIPLE_SEARCHS) ? r_contact_model_part : r_contact_model_part.GetSubModelPart("ContactSub"+mThisParameters["id_name"].GetString());

        // We compute the delta displacement
        if (dynamic) {
            ContactUtilities::ComputeStepJump(r_sub_contact_model_part, delta_time);
        }

        if (mCheckGap == CheckGap::MappingCheck && dynamic) {
            NodesArrayType& r_update_r_nodes_array = r_sub_contact_model_part.Nodes();
            const auto it_node_begin = r_update_r_nodes_array.begin();

            #pragma omp parallel for
            for(int i = 0; i < static_cast<int>(r_update_r_nodes_array.size()); ++i) {
                auto it_node = it_node_begin + i;
                noalias(it_node->Coordinates()) += it_node->GetValue(DELTA_COORDINATES);
            }
        }

        #pragma omp parallel for
        for(int i = 0; i < static_cast<int>(mPointListDestination.size()); ++i)
            mPointListDestination[i]->UpdatePoint();
    }

    KRATOS_CATCH("")
}

/***********************************************************************************/
/***********************************************************************************/

template<SizeType TDim, SizeType TNumNodes, SizeType TNumNodesMaster>
void BaseContactSearchProcess<TDim, TNumNodes, TNumNodesMaster>::UpdateMortarConditions()
{
    KRATOS_TRY

    // We update the list of points
    UpdatePointListMortar();

    // The contact model parts
    ModelPart& r_contact_model_part = mrMainModelPart.GetSubModelPart("Contact");
    ModelPart& r_sub_contact_model_part = mOptions.IsNot(BaseContactSearchProcess::MULTIPLE_SEARCHS) ? r_contact_model_part : r_contact_model_part.GetSubModelPart("ContactSub"+mThisParameters["id_name"].GetString());

    // Calculate the mean of the normal in all the nodes
    MortarUtilities::ComputeNodesMeanNormalModelPart(r_sub_contact_model_part);

    // We get the computing model part
    IndexType condition_id = GetMaximumConditionsIds();
    const std::string sub_computing_model_part_name = "ComputingContactSub" + mThisParameters["id_name"].GetString();
    ModelPart& r_computing_contact_model_part = mrMainModelPart.GetSubModelPart("ComputingContact");
    ModelPart& r_sub_computing_contact_model_part = mOptions.IsNot(BaseContactSearchProcess::MULTIPLE_SEARCHS) ? r_computing_contact_model_part : r_computing_contact_model_part.GetSubModelPart(sub_computing_model_part_name);

    // We reset the computing contact model part in case of already initialized
    if (r_sub_computing_contact_model_part.Conditions().size() > 0)
        ClearMortarConditions();

    // In case of not predefined master/slave we reset the flags
    if (mOptions.IsNot(BaseContactSearchProcess::PREDEFINE_MASTER_SLAVE)) {
        NodesArrayType& r_nodes_array = r_sub_contact_model_part.Nodes();
        ConditionsArrayType& r_conditions_array = r_sub_contact_model_part.Conditions();
        VariableUtils().SetFlag(SLAVE, false, r_nodes_array);
        VariableUtils().SetFlag(MASTER, false, r_nodes_array);
        VariableUtils().SetFlag(SLAVE, false, r_conditions_array);
        VariableUtils().SetFlag(MASTER, false, r_conditions_array);
    }

    // The search tree considered
    const SearchTreeType type_search = ConvertSearchTree(mThisParameters["type_search"].GetString());

    // Using KDTree
    if (type_search != SearchTreeType::OctreeWithOBB) {
        SearchUsingKDTree(r_sub_contact_model_part, r_sub_computing_contact_model_part);
    } else { // Using octree
        // We create the submodelparts for master and slave
        SetOriginDestinationModelParts(r_sub_contact_model_part);

        // We actually compute the search
        SearchUsingOcTree(r_sub_contact_model_part, r_sub_computing_contact_model_part);
    }

    // In case of not predefined master/slave we assign the master/slave nodes and conditions
    if (mOptions.IsNot(BaseContactSearchProcess::PREDEFINE_MASTER_SLAVE))
        NotPredefinedMasterSlave(r_sub_contact_model_part);

    // We create the submodelparts for master and slave
    if (type_search != SearchTreeType::OctreeWithOBB) {
        SetOriginDestinationModelParts(r_sub_contact_model_part);
    }

    // We map the Coordinates to the slave side from the master
    if (mCheckGap == CheckGap::MappingCheck) {
        CheckPairing(r_sub_computing_contact_model_part, condition_id);
    } else {
        // We revert the nodes to the original position
        if (mThisParameters["dynamic_search"].GetBool()) {
            if (mrMainModelPart.HasNodalSolutionStepVariable(VELOCITY)) {
                NodesArrayType& r_nodes_array = r_sub_contact_model_part.Nodes();
                #pragma omp parallel for
                for(int i = 0; i < static_cast<int>(r_nodes_array.size()); ++i) {
                    auto it_node = r_nodes_array.begin() + i;
                    noalias(it_node->Coordinates()) -= it_node->GetValue(DELTA_COORDINATES);
                }
            }
        }
        // We compute the weighted reaction
        ComputeWeightedReaction();
    }

    KRATOS_CATCH("")
}

/***********************************************************************************/
/***********************************************************************************/

template<SizeType TDim, SizeType TNumNodes, SizeType TNumNodesMaster>
void BaseContactSearchProcess<TDim, TNumNodes, TNumNodesMaster>::SearchUsingKDTree(
    ModelPart& rSubContactModelPart,
    ModelPart& rSubComputingContactModelPart
    )
{
    KRATOS_TRY

    // Some auxiliar values
    const IndexType allocation_size = mThisParameters["allocation_size"].GetInt(); // Allocation size for the vectors and max number of potential results
    const double search_factor = mThisParameters["search_factor"].GetDouble();     // The search factor to be considered
    IndexType bucket_size = mThisParameters["bucket_size"].GetInt();               // Bucket size for kd-tree

    // We check if we are in a dynamic or static case
    const bool dynamic = mThisParameters["dynamic_search"].GetBool() ? mrMainModelPart.HasNodalSolutionStepVariable(VELOCITY) : false;

    // The search tree considered
    const SearchTreeType type_search = ConvertSearchTree(mThisParameters["type_search"].GetString());

    // Create a tree
    // It will use a copy of mNodeList (a std::vector which contains pointers)
    // Copying the list is required because the tree will reorder it for efficiency
    KDTree tree_points(mPointListDestination.begin(), mPointListDestination.end(), bucket_size);

    // Auxiliar model parts and components
    ConditionsArrayType& r_conditions_array = rSubContactModelPart.Conditions();
    const int num_conditions = static_cast<int>(r_conditions_array.size());
    const auto it_cond_begin = r_conditions_array.begin();
    IndexType condition_id = GetMaximumConditionsIds();

    // If considering OBB
    const bool with_obb = (type_search == SearchTreeType::KdtreeInRadiusWithOBB || type_search == SearchTreeType::KdtreeInBoxWithOBB) ? true : false;
    Parameters octree_parameters = mThisParameters["octree_search_parameters"];
    double h_mean = ContactUtilities::CalculateMaxNodalH(rSubContactModelPart);
    h_mean = h_mean < std::numeric_limits<double>::epsilon() ? 1.0 : h_mean;
    const double bounding_box_factor = octree_parameters["bounding_box_factor"].GetDouble() * h_mean;

    // Now we iterate over the conditions
//     #pragma omp parallel for firstprivate(tree_points) // TODO: Make me parallel!!!
    for(int i = 0; i < num_conditions; ++i) {
        auto it_cond = it_cond_begin + i;

        if (mOptions.IsNot(BaseContactSearchProcess::PREDEFINE_MASTER_SLAVE) || it_cond->Is(SLAVE) == !mOptions.Is(BaseContactSearchProcess::INVERTED_SEARCH)) {
            // Initialize values
            PointVector points_found(allocation_size);

            IndexType number_points_found = 0;

            // Getting geometry
            GeometryType& r_geometry = it_cond->GetGeometry();
            OrientedBoundingBox<TDim> slave_obb(r_geometry, bounding_box_factor);

            if (type_search == SearchTreeType::KdtreeInRadius || type_search == SearchTreeType::KdtreeInRadiusWithOBB) {
                const Point& r_center = dynamic ? Point(ContactUtilities::GetHalfJumpCenter(r_geometry)) : r_geometry.Center(); // NOTE: Center in half delta time or real center

                const double search_radius = search_factor * Radius(it_cond->GetGeometry());

                number_points_found = tree_points.SearchInRadius(r_center, search_radius, points_found.begin(), allocation_size);
            } else if (type_search == SearchTreeType::KdtreeInBox || type_search == SearchTreeType::KdtreeInBoxWithOBB) {
                // Auxiliar values
                const double length_search = search_factor * r_geometry.Length();

                // Compute max/min points
                NodeType min_point, max_point;
                r_geometry.BoundingBox(min_point, max_point);

                // Get the normal in the extrema points
                Vector N_min, N_max;
                GeometryType::CoordinatesArrayType local_point_min, local_point_max;
                r_geometry.PointLocalCoordinates( local_point_min, min_point.Coordinates( ) ) ;
                r_geometry.PointLocalCoordinates( local_point_max, max_point.Coordinates( ) ) ;
                r_geometry.ShapeFunctionsValues( N_min, local_point_min );
                r_geometry.ShapeFunctionsValues( N_max, local_point_max );

                const array_1d<double,3> normal_min = MortarUtilities::GaussPointUnitNormal(N_min, r_geometry);
                const array_1d<double,3> normal_max = MortarUtilities::GaussPointUnitNormal(N_max, r_geometry);

                ContactUtilities::ScaleNode<NodeType>(min_point, normal_min, length_search);
                ContactUtilities::ScaleNode<NodeType>(max_point, normal_max, length_search);

                number_points_found = tree_points.SearchInBox(min_point, max_point, points_found.begin(), allocation_size);
            } else {
                KRATOS_ERROR << " The type search is not implemented yet does not exist!!!!. SearchTreeType = " << mThisParameters["type_search"].GetString() << std::endl;
            }

            if (number_points_found > 0) {
                // We resize the vector to the actual size
//                 points_found.resize(number_points_found); // NOTE: May be ineficient

            #ifdef KRATOS_DEBUG
                // NOTE: We check the list
                for (IndexType i_point = 0; i_point < number_points_found; ++i_point )
                    points_found[i_point]->Check();
            #endif

                IndexMap::Pointer p_indexes_pairs = it_cond->GetValue(INDEX_MAP);

                // If not active we check if can be potentially in contact
                if (mCheckGap == CheckGap::MappingCheck) {
                    for (IndexType i_point = 0; i_point < number_points_found; ++i_point ) {
                        // Master condition
                        Condition::Pointer p_cond_master = points_found[i_point]->GetEntity();

                        // Checking with OBB
                        if (with_obb) {
                            OrientedBoundingBox<TDim> master_obb(p_cond_master->GetGeometry(), bounding_box_factor);
                            if (!slave_obb.HasIntersection(master_obb)) {
                                continue;
                            }
                        }

                        const CheckResult condition_checked_right = CheckCondition(p_indexes_pairs, (*it_cond.base()), p_cond_master, mOptions.Is(BaseContactSearchProcess::INVERTED_SEARCH));

                        if (condition_checked_right == CheckResult::OK)
                            p_indexes_pairs->AddId(p_cond_master->Id());
                    }
                } else {
                    // Some auxiliar values
                    const double active_check_factor = mrMainModelPart.GetProcessInfo()[ACTIVE_CHECK_FACTOR];
                    const bool frictional_problem = mrMainModelPart.Is(SLIP);

                    // Slave geometry and data
                    Properties::Pointer p_prop = it_cond->pGetProperties();
                    const array_1d<double, 3>& r_normal_slave = it_cond->GetValue(NORMAL);

                    for (IndexType i_point = 0; i_point < number_points_found; ++i_point ) {
                        // Master condition
                        Condition::Pointer p_cond_master = points_found[i_point]->GetEntity();

                        // Checking with OBB
                        if (with_obb) {
                            OrientedBoundingBox<TDim> master_obb(p_cond_master->GetGeometry(), bounding_box_factor);
                            if (!slave_obb.HasIntersection(master_obb)) {
                                continue;
                            }
                        }

                        AddPotentialPairing(rSubComputingContactModelPart, condition_id, (*it_cond.base()), r_normal_slave, p_cond_master, p_cond_master->GetValue(NORMAL), p_indexes_pairs, p_prop, active_check_factor, frictional_problem);
                    }
                }
            }
        }
    }

    KRATOS_CATCH("")
}

/***********************************************************************************/
/***********************************************************************************/

template<SizeType TDim, SizeType TNumNodes, SizeType TNumNodesMaster>
void BaseContactSearchProcess<TDim, TNumNodes, TNumNodesMaster>::SearchUsingOcTree(
    ModelPart& rSubContactModelPart,
    ModelPart& rSubComputingContactModelPart
    )
{
    KRATOS_TRY

    KRATOS_ERROR_IF(mOptions.Is(BaseContactSearchProcess::INVERTED_SEARCH)) << "Octree only works with not inverted master/slave model parts (for now)" << std::endl;
    KRATOS_ERROR_IF_NOT(mOptions.Is(BaseContactSearchProcess::PREDEFINE_MASTER_SLAVE)) << "Octree only works with predefined master/slave model part (for now)" << std::endl;

    // Getting model
    ModelPart& r_contact_model_part = mrMainModelPart.GetSubModelPart("Contact");
    ModelPart& r_sub_contact_model_part = mOptions.IsNot(BaseContactSearchProcess::MULTIPLE_SEARCHS) ? r_contact_model_part : r_contact_model_part.GetSubModelPart("ContactSub"+mThisParameters["id_name"].GetString());
    const std::string master_model_part_name = "MasterSubModelPart" + mThisParameters["id_name"].GetString();
    ModelPart& r_master_model_part = r_sub_contact_model_part.GetSubModelPart(master_model_part_name);
    const std::string slave_model_part_name = "SlaveSubModelPart" + mThisParameters["id_name"].GetString();
    ModelPart& r_slave_model_part = r_sub_contact_model_part.GetSubModelPart(slave_model_part_name);

    Parameters octree_parameters = mThisParameters["octree_search_parameters"];
    octree_parameters.AddEmptyValue("intersected_model_part_name");
    octree_parameters.AddEmptyValue("intersecting_model_part_name");
    octree_parameters["intersected_model_part_name"].SetString(slave_model_part_name);
    octree_parameters["intersecting_model_part_name"].SetString(master_model_part_name);

    double h_mean = std::max(ContactUtilities::CalculateMaxNodalH(r_slave_model_part), ContactUtilities::CalculateMaxNodalH(r_master_model_part));
    h_mean = h_mean < std::numeric_limits<double>::epsilon() ? 1.0 : h_mean;
    const double bounding_box_factor = octree_parameters["bounding_box_factor"].GetDouble();
    octree_parameters["bounding_box_factor"].SetDouble(bounding_box_factor * h_mean);

    // Creating the process
    FindIntersectedGeometricalObjectsWithOBBContactSearchProcess octree_search_process(mrMainModelPart.GetModel(), octree_parameters);
    octree_search_process.ExecuteInitialize();

    // Definition of the leaves of the tree
    FindIntersectedGeometricalObjectsWithOBBContactSearchProcess::OtreeCellVectorType leaves;

    // Auxiliar model parts and components
    const array_1d<double, 3> zero_array = ZeroVector(3);
    ConditionsArrayType& r_conditions_array = rSubContactModelPart.Conditions();
    const int num_conditions = static_cast<int>(r_conditions_array.size());
    const auto it_cond_begin = r_conditions_array.begin();
    IndexType condition_id = GetMaximumConditionsIds();

//     #pragma omp parallel for // TODO: Make me parallel!!!
    for(int i = 0; i < num_conditions; ++i) {
        auto it_cond = it_cond_begin + i;

        // We perform the search
        leaves.clear();
        octree_search_process.IdentifyNearEntitiesAndCheckEntityForIntersection(*(it_cond.base()), leaves);

        if (it_cond->Is(SELECTED)) {
            IndexMap::Pointer p_indexes_pairs = it_cond->GetValue(INDEX_MAP);

            // If not active we check if can be potentially in contact
            if (mCheckGap == CheckGap::MappingCheck) {
                for (auto p_leaf : leaves) {
                    for (auto p_cond_master : *(p_leaf->pGetObjects())) {
                        if (p_cond_master->Is(SELECTED)) {
                            const CheckResult condition_checked_right = CheckGeometricalObject(p_indexes_pairs, (*it_cond.base()), p_cond_master, mOptions.Is(BaseContactSearchProcess::INVERTED_SEARCH));

                            if (condition_checked_right == CheckResult::OK) {
                                p_indexes_pairs->AddId(p_cond_master->Id());
                            }
                        }
                    }
                }
            } else {
                // Some auxiliar values
                const double active_check_factor = mrMainModelPart.GetProcessInfo()[ACTIVE_CHECK_FACTOR];
                const bool frictional_problem = mrMainModelPart.Is(SLIP);

                // Slave geometry and data
                Properties::Pointer p_prop = it_cond->pGetProperties();
                const array_1d<double, 3>& r_normal_slave = it_cond->GetValue(NORMAL);

                for (auto p_leaf : leaves) {
                    for (auto p_cond_master : *(p_leaf->pGetObjects())) {
                        if (p_cond_master->Is(SELECTED)) {
                            const array_1d<double, 3>& r_normal_master = (p_cond_master->GetGeometry()).UnitNormal(zero_array);
                            AddPotentialPairing(rSubComputingContactModelPart, condition_id, (*it_cond.base()), r_normal_slave, p_cond_master, r_normal_master, p_indexes_pairs, p_prop, active_check_factor, frictional_problem);
                        }
                    }
                }
            }
        }
    }

    KRATOS_CATCH("")
}

/***********************************************************************************/
/***********************************************************************************/

template<SizeType TDim, SizeType TNumNodes, SizeType TNumNodesMaster>
Condition::Pointer BaseContactSearchProcess<TDim, TNumNodes, TNumNodesMaster>::AddPairing(
    ModelPart& rComputingModelPart,
    IndexType& rConditionId,
    GeometricalObject::Pointer pObjectSlave,
    const array_1d<double, 3>& rSlaveNormal,
    GeometricalObject::Pointer pObjectMaster,
    const array_1d<double, 3>& rMasterNormal,
    IndexMap::Pointer pIndexesPairs,
    Properties::Pointer pProperties
    )
{
    KRATOS_TRY

    pIndexesPairs->AddId(pObjectMaster->Id());

    // We add the ID and we create a new auxiliar condition
    if (mOptions.Is(BaseContactSearchProcess::CREATE_AUXILIAR_CONDITIONS)) { // TODO: Check this!!
        ++rConditionId;
        Condition::Pointer p_auxiliar_condition = rComputingModelPart.CreateNewCondition(mConditionName, rConditionId, pObjectSlave->GetGeometry(), pProperties);
        // We set the geometrical values
        pIndexesPairs->SetNewEntityId(pObjectMaster->Id(), rConditionId);
        p_auxiliar_condition->SetValue(PAIRED_GEOMETRY, pObjectMaster->pGetGeometry());
        p_auxiliar_condition->SetValue(NORMAL, rSlaveNormal);
        p_auxiliar_condition->SetValue(PAIRED_NORMAL, rMasterNormal);
        // We activate the condition and initialize it
        p_auxiliar_condition->Set(ACTIVE, true);
        p_auxiliar_condition->Initialize();
        return p_auxiliar_condition;
    }

    return nullptr;

    KRATOS_CATCH("")
}

/***********************************************************************************/
/***********************************************************************************/

template<SizeType TDim, SizeType TNumNodes, SizeType TNumNodesMaster>
void BaseContactSearchProcess<TDim, TNumNodes, TNumNodesMaster>::CheckMortarConditions()
{
    KRATOS_TRY

    // Iterate in the conditions
    ModelPart& r_contact_model_part = mrMainModelPart.GetSubModelPart("Contact");
    ModelPart& r_sub_contact_model_part = mOptions.IsNot(BaseContactSearchProcess::MULTIPLE_SEARCHS) ? r_contact_model_part : r_contact_model_part.GetSubModelPart("ContactSub"+mThisParameters["id_name"].GetString());
    ConditionsArrayType& r_conditions_array = r_sub_contact_model_part.Conditions();

    for(int i = 0; i < static_cast<int>(r_conditions_array.size()); ++i) {
        auto it_cond = r_conditions_array.begin() + i;

        if (it_cond->Has(INDEX_MAP)) {
            IndexMap::Pointer ids_destination = it_cond->GetValue(INDEX_MAP);
            if (ids_destination->size() > 0) {
                KRATOS_INFO("Check paired conditions (Origin)") << "Origin condition ID:" << it_cond->Id() << " Number of pairs: " << ids_destination->size() << std::endl;
                KRATOS_INFO("Check paired conditions (Destination)") << ids_destination->Info();
            }
        }
    }

    // Iterate over the nodes
    NodesArrayType& r_nodes_array = r_sub_contact_model_part.Nodes();
    const auto it_node_begin = r_nodes_array.begin();

    for(int i = 0; i < static_cast<int>(r_nodes_array.size()); ++i) {
        auto it_node = it_node_begin + i;
        KRATOS_INFO_IF("Check paired nodes", it_node->Is(ACTIVE)) << "Node: " << it_node->Id() << " is active" << std::endl;
    }

    KRATOS_CATCH("")
}

/***********************************************************************************/
/***********************************************************************************/

template<SizeType TDim, SizeType TNumNodes, SizeType TNumNodesMaster>
void BaseContactSearchProcess<TDim, TNumNodes, TNumNodesMaster>::InvertSearch()
{
    KRATOS_TRY

    mOptions.Flip(BaseContactSearchProcess::INVERTED_SEARCH);

    KRATOS_CATCH("")
}

/***********************************************************************************/
/***********************************************************************************/

template<SizeType TDim, SizeType TNumNodes, SizeType TNumNodesMaster>
void BaseContactSearchProcess<TDim, TNumNodes, TNumNodesMaster>::ClearScalarMortarConditions(NodesArrayType& rNodesArray)
{
    KRATOS_TRY

    VariableUtils().SetVariableForFlag(SCALAR_LAGRANGE_MULTIPLIER, 0.0, rNodesArray, ACTIVE, false);

    KRATOS_CATCH("")
}

/***********************************************************************************/
/***********************************************************************************/

template<SizeType TDim, SizeType TNumNodes, SizeType TNumNodesMaster>
void BaseContactSearchProcess<TDim, TNumNodes, TNumNodesMaster>::ClearComponentsMortarConditions(NodesArrayType& rNodesArray)
{
    KRATOS_TRY

    const array_1d<double, 3> zero_array = ZeroVector(3);
    VariableUtils().SetVariableForFlag(VECTOR_LAGRANGE_MULTIPLIER, zero_array, rNodesArray, ACTIVE, false);

    KRATOS_CATCH("")
}

/***********************************************************************************/
/***********************************************************************************/

template<SizeType TDim, SizeType TNumNodes, SizeType TNumNodesMaster>
void BaseContactSearchProcess<TDim, TNumNodes, TNumNodesMaster>::ClearALMFrictionlessMortarConditions(NodesArrayType& rNodesArray)
{
    KRATOS_TRY

    VariableUtils().SetVariableForFlag(LAGRANGE_MULTIPLIER_CONTACT_PRESSURE, 0.0, rNodesArray, ACTIVE, false);

    KRATOS_CATCH("")
}

/***********************************************************************************/
/***********************************************************************************/

template<SizeType TDim, SizeType TNumNodes, SizeType TNumNodesMaster>
inline typename BaseContactSearchProcess<TDim, TNumNodes, TNumNodesMaster>::CheckResult BaseContactSearchProcess<TDim, TNumNodes, TNumNodesMaster>::CheckGeometricalObject(
    IndexMap::Pointer pIndexesPairs,
    const GeometricalObject::Pointer pGeometricalObject1,
    const GeometricalObject::Pointer pGeometricalObject2,
    const bool InvertedSearch
    )
{
    KRATOS_TRY

    const IndexType index_1 = pGeometricalObject1->Id();
    const IndexType index_2 = pGeometricalObject2->Id();

    // Avoiding "auto self-contact"
    if (index_1 == index_2) {
        return CheckResult::Fail;
    }

    // To avoid to repeat twice the same condition
    if (pIndexesPairs->find(index_2) != pIndexesPairs->end()) {
        return CheckResult::AlreadyInTheMap;
    }

    return CheckResult::OK;

    KRATOS_CATCH("")
}

/***********************************************************************************/
/***********************************************************************************/

template<SizeType TDim, SizeType TNumNodes, SizeType TNumNodesMaster>
inline typename BaseContactSearchProcess<TDim, TNumNodes, TNumNodesMaster>::CheckResult BaseContactSearchProcess<TDim, TNumNodes, TNumNodesMaster>::CheckCondition(
    IndexMap::Pointer pIndexesPairs,
    const Condition::Pointer pCond1,
    const Condition::Pointer pCond2,
    const bool InvertedSearch
    )
{
    KRATOS_TRY

    if (CheckGeometricalObject(pIndexesPairs, pCond1, pCond2, InvertedSearch) == CheckResult::Fail) {
        return CheckResult::Fail;
    }

    // Otherwise will not be necessary to check
    if (mOptions.IsNot(BaseContactSearchProcess::PREDEFINE_MASTER_SLAVE) || pCond2->Is(SLAVE) == !InvertedSearch) {
        auto p_indexes_pairs_2 = pCond2->GetValue(INDEX_MAP);
        if (p_indexes_pairs_2->find(pCond1->Id()) != p_indexes_pairs_2->end())
            return CheckResult::Fail;
    }

    // Avoid conditions oriented in the same direction
    const double tolerance = 1.0e-16;
    if (norm_2(pCond1->GetValue(NORMAL) - pCond2->GetValue(NORMAL)) < tolerance) {
        return CheckResult::Fail;
    }

    return CheckResult::OK;

    KRATOS_CATCH("")
}

/***********************************************************************************/
/***********************************************************************************/

template<SizeType TDim, SizeType TNumNodes, SizeType TNumNodesMaster>
inline void BaseContactSearchProcess<TDim, TNumNodes, TNumNodesMaster>::NotPredefinedMasterSlave(ModelPart& rModelPart)
{
    KRATOS_TRY

    // We iterate over the conditions
    ConditionsArrayType& r_conditions_array = rModelPart.Conditions();
    const auto it_cond_begin = r_conditions_array.begin();
    const int num_conditions = static_cast<int>(r_conditions_array.size());

    std::vector<IndexType> master_conditions_ids;

    #pragma omp parallel
    {
        // Creating a buffer for parallel vector fill
        std::vector<IndexType> master_conditions_ids_buffer;

        #pragma omp for
        for(int i = 0; i < num_conditions; ++i) {
            auto it_cond = it_cond_begin + i;
            IndexMap::Pointer p_indexes_pairs = it_cond->GetValue(INDEX_MAP);
            if (p_indexes_pairs->size() > 0) {
                it_cond->Set(SLAVE, true);
                for (auto& i_pair : *p_indexes_pairs) {
                    master_conditions_ids_buffer.push_back(i_pair.first);
                }
            }
        }

        // Combine buffers together
        #pragma omp critical
        {
            std::move(master_conditions_ids_buffer.begin(),master_conditions_ids_buffer.end(),back_inserter(master_conditions_ids));
        }
    }

    // We create an auxiliar model part to add the MASTER flag
    rModelPart.CreateSubModelPart("AuxMasterModelPart");
    ModelPart& aux_model_part = rModelPart.GetSubModelPart("AuxMasterModelPart");

    // Remove duplicates
    std::sort( master_conditions_ids.begin(), master_conditions_ids.end() );
    master_conditions_ids.erase( std::unique( master_conditions_ids.begin(), master_conditions_ids.end() ), master_conditions_ids.end() );

    // Add to the auxiliar model part
    aux_model_part.AddConditions(master_conditions_ids);

    // Set the flag
    VariableUtils().SetFlag(MASTER, true, aux_model_part.Conditions());

    // Remove auxiliar model part
    rModelPart.RemoveSubModelPart("AuxMasterModelPart");

    // Now we iterate over the conditions to set the nodes indexes
    #pragma omp parallel for
    for(int i = 0; i < num_conditions; ++i) {
        auto it_cond = r_conditions_array.begin() + i;
        if (it_cond->Is(SLAVE)) {
            GeometryType& r_geometry = it_cond->GetGeometry();
            for (NodeType& r_node : r_geometry) {
                r_node.SetLock();
                r_node.Set(SLAVE, true);
                r_node.UnSetLock();
            }
        }
        if (it_cond->Is(MASTER)) {
            GeometryType& r_geometry = it_cond->GetGeometry();
            for (NodeType& r_node : r_geometry) {
                r_node.SetLock();
                r_node.Set(MASTER, true);
                r_node.UnSetLock();
            }
        }
    }

    KRATOS_CATCH("")
}

/***********************************************************************************/
/***********************************************************************************/

template<SizeType TDim, SizeType TNumNodes, SizeType TNumNodesMaster>
inline IndexType BaseContactSearchProcess<TDim, TNumNodes, TNumNodesMaster>::GetMaximumConditionsIds()
{
    KRATOS_TRY

    ConditionsArrayType& r_conditions_array = mrMainModelPart.Conditions();

    IndexType condition_id = 0;
    for(IndexType i = 0; i < r_conditions_array.size(); ++i)  {
        auto it_cond = r_conditions_array.begin() + i;
        const IndexType id = it_cond->GetId();
        if (id > condition_id)
            condition_id = id;
    }

    return condition_id;

    KRATOS_CATCH("")
}

/***********************************************************************************/
/***********************************************************************************/

template<SizeType TDim, SizeType TNumNodes, SizeType TNumNodesMaster>
inline void BaseContactSearchProcess<TDim, TNumNodes, TNumNodesMaster>::AddPotentialPairing(
    ModelPart& rComputingModelPart,
    IndexType& rConditionId,
    GeometricalObject::Pointer pObjectSlave,
    const array_1d<double, 3>& rSlaveNormal,
    GeometricalObject::Pointer pObjectMaster,
    const array_1d<double, 3>& rMasterNormal,
    IndexMap::Pointer pIndexesPairs,
    Properties::Pointer pProperties,
    const double ActiveCheckFactor,
    const bool FrictionalProblem
    )
{
    KRATOS_TRY

    // Slave geometry
    GeometryType& r_slave_geometry = pObjectSlave->GetGeometry();

    // Auxiliar bool
    bool at_least_one_node_potential_contact = false;

    Point projected_point;
    double aux_distance = 0.0;
    array_1d<double, 3> result;
    if (mCheckGap == CheckGap::DirectCheck) {
        // Master geometry
        GeometryType& r_geom_master = pObjectMaster->GetGeometry();

        for (IndexType i_node = 0; i_node < TNumNodes; ++i_node) {
            if (r_slave_geometry[i_node].IsNot(ACTIVE)) {
                const array_1d<double, 3>& r_normal = r_slave_geometry[i_node].GetValue(NORMAL);
                if (norm_2(r_normal) < ZeroTolerance)
                    aux_distance = GeometricalProjectionUtilities::FastProjectDirection(r_geom_master, r_slave_geometry[i_node], projected_point, rMasterNormal, rSlaveNormal);
                else
                    aux_distance = GeometricalProjectionUtilities::FastProjectDirection(r_geom_master, r_slave_geometry[i_node], projected_point, rMasterNormal, r_normal);

                if (aux_distance <= r_slave_geometry[i_node].FastGetSolutionStepValue(NODAL_H) * ActiveCheckFactor &&  r_geom_master.IsInside(projected_point, result, ZeroTolerance)) { // NOTE: This can be problematic (It depends the way IsInside() and the local_pointCoordinates() are implemented)
                    at_least_one_node_potential_contact = true;
                    r_slave_geometry[i_node].Set(ACTIVE, true);
                    if (mTypeSolution == TypeSolution::VectorLagrangeMultiplier && FrictionalProblem) {
                        NodeType& r_node = r_slave_geometry[i_node];
                        if (norm_2(r_node.FastGetSolutionStepValue(VECTOR_LAGRANGE_MULTIPLIER)) < ZeroTolerance) {
                            if (r_node.GetValue(FRICTION_COEFFICIENT) < ZeroTolerance || mOptions.Is(BaseContactSearchProcess::PURE_SLIP)) {
                                r_node.Set(SLIP, true);
                            } else if (!r_node.IsDefined(SLIP)) {
                                r_node.Set(SLIP, false);
                            }
                        }
                    }  else if (mTypeSolution == TypeSolution::FrictionalPenaltyMethod || mTypeSolution == TypeSolution::OtherFrictional) {
                        NodeType& r_node = r_slave_geometry[i_node];
                        if (r_node.GetValue(FRICTION_COEFFICIENT) < ZeroTolerance || mOptions.Is(BaseContactSearchProcess::PURE_SLIP)) {
                            r_node.Set(SLIP, true);
                        } else if (!r_node.IsDefined(SLIP)) {
                            r_node.Set(SLIP, false);
                        }
                    }
                }

                aux_distance = GeometricalProjectionUtilities::FastProjectDirection(r_geom_master, r_slave_geometry[i_node], projected_point, rMasterNormal, -rMasterNormal);
                if (aux_distance <= r_slave_geometry[i_node].FastGetSolutionStepValue(NODAL_H) * ActiveCheckFactor &&  r_geom_master.IsInside(projected_point, result, ZeroTolerance)) { // NOTE: This can be problematic (It depends the way IsInside() and the local_pointCoordinates() are implemented)
                    at_least_one_node_potential_contact = true;
                    r_slave_geometry[i_node].Set(ACTIVE, true);
                    if (mTypeSolution == TypeSolution::VectorLagrangeMultiplier && FrictionalProblem) {
                        NodeType& r_node = r_slave_geometry[i_node];
                        if (norm_2(r_node.FastGetSolutionStepValue(VECTOR_LAGRANGE_MULTIPLIER)) < ZeroTolerance) {
                            if (r_node.GetValue(FRICTION_COEFFICIENT) < ZeroTolerance || mOptions.Is(BaseContactSearchProcess::PURE_SLIP)) {
                                r_node.Set(SLIP, true);
                            } else if (!r_node.IsDefined(SLIP)) {
                                r_node.Set(SLIP, false);
                            }
                        }
                    } else if (mTypeSolution == TypeSolution::FrictionalPenaltyMethod || mTypeSolution == TypeSolution::OtherFrictional) {
                        NodeType& r_node = r_slave_geometry[i_node];
                        if (r_node.GetValue(FRICTION_COEFFICIENT) < ZeroTolerance || mOptions.Is(BaseContactSearchProcess::PURE_SLIP)) {
                            r_node.Set(SLIP, true);
                        } else if (!r_node.IsDefined(SLIP)) {
                            r_node.Set(SLIP, false);
                        }
                    }
                }
            } else {
                at_least_one_node_potential_contact = true;
            }
        }
    } else {
        at_least_one_node_potential_contact = true;
        for (IndexType i_node = 0; i_node < TNumNodes; ++i_node) {
            r_slave_geometry[i_node].Set(ACTIVE, true);
            if (mTypeSolution == TypeSolution::VectorLagrangeMultiplier && FrictionalProblem) {
                NodeType& r_node = r_slave_geometry[i_node];
                if (norm_2(r_node.FastGetSolutionStepValue(VECTOR_LAGRANGE_MULTIPLIER)) < ZeroTolerance) {
                    if (r_node.GetValue(FRICTION_COEFFICIENT) < ZeroTolerance || mOptions.Is(BaseContactSearchProcess::PURE_SLIP)) {
                        r_node.Set(SLIP, true);
                    } else if (!r_node.IsDefined(SLIP)) {
                        r_node.Set(SLIP, false);
                    }
                }
            } else if (mTypeSolution == TypeSolution::FrictionalPenaltyMethod || mTypeSolution == TypeSolution::OtherFrictional) {
                NodeType& r_node = r_slave_geometry[i_node];
                if (r_node.GetValue(FRICTION_COEFFICIENT) < ZeroTolerance || mOptions.Is(BaseContactSearchProcess::PURE_SLIP)) {
                    r_node.Set(SLIP, true);
                } else if (!r_node.IsDefined(SLIP)) {
                    r_node.Set(SLIP, false);
                }
            }
        }
    }

    if (at_least_one_node_potential_contact)
        AddPairing(rComputingModelPart, rConditionId, pObjectSlave, rSlaveNormal, pObjectMaster, rMasterNormal, pIndexesPairs, pProperties);

    KRATOS_CATCH("")
}

/***********************************************************************************/
/***********************************************************************************/

template<SizeType TDim, SizeType TNumNodes, SizeType TNumNodesMaster>
void BaseContactSearchProcess<TDim, TNumNodes, TNumNodesMaster>::CleanModelPart(ModelPart& rModelPart)
{
    KRATOS_TRY

    // We clean only the conditions
    ConditionsArrayType& r_conditions_array = rModelPart.Conditions();
    VariableUtils().SetFlag(TO_ERASE, true, r_conditions_array);
    mrMainModelPart.RemoveConditionsFromAllLevels(TO_ERASE);

    KRATOS_CATCH("")
}

/***********************************************************************************/
/***********************************************************************************/

template<SizeType TDim, SizeType TNumNodes, SizeType TNumNodesMaster>
void BaseContactSearchProcess<TDim, TNumNodes, TNumNodesMaster>::CheckPairing(
    ModelPart& rComputingModelPart,
    IndexType& rConditionId
    )
{
    KRATOS_TRY

    // Getting the corresponding submodelparts
    ModelPart& r_contact_model_part = mrMainModelPart.GetSubModelPart("Contact");
    ModelPart& r_sub_contact_model_part = mOptions.IsNot(BaseContactSearchProcess::MULTIPLE_SEARCHS) ? r_contact_model_part : r_contact_model_part.GetSubModelPart("ContactSub" + mThisParameters["id_name"].GetString());

    // We set the gap to an enormous value in order to initialize it
    VariableUtils().SetNonHistoricalVariable(NORMAL_GAP, 1.0e12, r_sub_contact_model_part.Nodes());

    // We compute the gap in the slave
    ComputeMappedGap(!mOptions.Is(BaseContactSearchProcess::INVERTED_SEARCH));

    // We revert the nodes to the original position
    NodesArrayType& r_nodes_array = r_sub_contact_model_part.Nodes();
    const auto it_node_begin = r_nodes_array.begin();
    if (mThisParameters["dynamic_search"].GetBool()) {
        if (mrMainModelPart.HasNodalSolutionStepVariable(VELOCITY)) {
            #pragma omp parallel for
            for(int i = 0; i < static_cast<int>(r_nodes_array.size()); ++i) {
                auto it_node = it_node_begin + i;
                noalias(it_node->Coordinates()) -= it_node->GetValue(DELTA_COORDINATES);
            }
        }
    }

    // Calculate the mean of the normal in all the nodes
    MortarUtilities::ComputeNodesMeanNormalModelPart(r_sub_contact_model_part);

    // Iterate in the conditions and create the new ones
    CreateAuxiliarConditions(r_sub_contact_model_part, rComputingModelPart, rConditionId);

    // We compute the weighted reaction
    ComputeWeightedReaction();

    // Finally we compute the active/inactive nodes
    ComputeActiveInactiveNodes();

    KRATOS_CATCH("")
}

/***********************************************************************************/
/***********************************************************************************/

template<SizeType TDim, SizeType TNumNodes, SizeType TNumNodesMaster>
inline void BaseContactSearchProcess<TDim, TNumNodes, TNumNodesMaster>::ComputeMappedGap(const bool SearchOrientation)
{
    KRATOS_TRY

    // We get the process info
    const ProcessInfo& r_process_info = mrMainModelPart.GetProcessInfo();

    // Iterate over the nodes
    ModelPart& r_contact_model_part = mrMainModelPart.GetSubModelPart("Contact");
    ModelPart& r_sub_contact_model_part = mOptions.IsNot(BaseContactSearchProcess::MULTIPLE_SEARCHS) ? r_contact_model_part : r_contact_model_part.GetSubModelPart("ContactSub"+mThisParameters["id_name"].GetString());
    ModelPart& r_master_model_part = r_sub_contact_model_part.GetSubModelPart("MasterSubModelPart"+mThisParameters["id_name"].GetString());
    NodesArrayType& r_nodes_array_master = r_master_model_part.Nodes();
    const auto it_node_begin_master = r_nodes_array_master.begin();
    ModelPart& r_slave_model_part = r_sub_contact_model_part.GetSubModelPart("SlaveSubModelPart"+mThisParameters["id_name"].GetString());
    NodesArrayType& r_nodes_array_slave = r_slave_model_part.Nodes();
    const auto it_node_begin_slave = r_nodes_array_slave.begin();

    // We set the auxiliar Coordinates
    const array_1d<double, 3> zero_array = ZeroVector(3);
    #pragma omp parallel for
    for(int i = 0; i < static_cast<int>(r_nodes_array_master.size()); ++i) {
        auto it_node = it_node_begin_master + i;

        if (SearchOrientation) {
            it_node->SetValue(AUXILIAR_COORDINATES, it_node->Coordinates());
        } else {
            it_node->SetValue(AUXILIAR_COORDINATES, zero_array);
        }
    }
    #pragma omp parallel for
    for(int i = 0; i < static_cast<int>(r_nodes_array_slave.size()); ++i) {
        auto it_node = it_node_begin_slave + i;

        if (!SearchOrientation) {
            it_node->SetValue(AUXILIAR_COORDINATES, it_node->Coordinates());
        } else {
            it_node->SetValue(AUXILIAR_COORDINATES, zero_array);
        }
    }

    // Switch MASTER/SLAVE
    NodesArrayType& r_nodes_array = r_sub_contact_model_part.Nodes();
    if (!SearchOrientation)
        SwitchFlagNodes(r_nodes_array);

    // We set the mapper parameters
    Parameters mapping_parameters = Parameters(R"({"distance_threshold" : 1.0e24,"update_interface" : false, "remove_isolated_conditions" : true, "origin_variable_historical" : false, "destination_variable_historical" : false})" );
    if (r_process_info.Has(DISTANCE_THRESHOLD)) {
        mapping_parameters["distance_threshold"].SetDouble(r_process_info[DISTANCE_THRESHOLD]);
    }
    MapperType mapper(r_master_model_part, r_slave_model_part, AUXILIAR_COORDINATES, mapping_parameters);
    mapper.Execute();

    // Switch again MASTER/SLAVE
    if (!SearchOrientation)
        SwitchFlagNodes(r_nodes_array);

    // We compute now the normal gap and set the nodes under certain threshold as active
    array_1d<double, 3> normal, auxiliar_coordinates, components_gap;
    double gap = 0.0;
    const auto it_node_begin = r_nodes_array.begin();
    #pragma omp parallel for firstprivate(gap, normal, auxiliar_coordinates, components_gap)
    for(int i = 0; i < static_cast<int>(r_nodes_array.size()); ++i) {
        auto it_node = it_node_begin + i;

        if (it_node->Is(SLAVE) == SearchOrientation) {
            // We compute the gap
            noalias(normal) = it_node->FastGetSolutionStepValue(NORMAL);
            noalias(auxiliar_coordinates) = it_node->GetValue(AUXILIAR_COORDINATES);
            noalias(components_gap) = ( it_node->Coordinates() - auxiliar_coordinates);
            gap = inner_prod(components_gap, - normal);

            // We activate if the node is close enough
            if (norm_2(auxiliar_coordinates) > ZeroTolerance)
                it_node->SetValue(NORMAL_GAP, gap);
        } else {
            it_node->SetValue(NORMAL_GAP, 0.0);
        }
    }

    KRATOS_CATCH("")
}

/***********************************************************************************/
/***********************************************************************************/

template<SizeType TDim, SizeType TNumNodes, SizeType TNumNodesMaster>
void BaseContactSearchProcess<TDim, TNumNodes, TNumNodesMaster>::ComputeActiveInactiveNodes()
{
    KRATOS_TRY

    // We get the process info
    const ProcessInfo& r_process_info = mrMainModelPart.GetProcessInfo();

    // The penalty value and scale factor
    const double common_epsilon = r_process_info[INITIAL_PENALTY];
    const double scale_factor = r_process_info[SCALE_FACTOR];

    // Iterate over the nodes
    ModelPart& r_contact_model_part = mrMainModelPart.GetSubModelPart("Contact");
    ModelPart& r_sub_contact_model_part = mOptions.IsNot(BaseContactSearchProcess::MULTIPLE_SEARCHS) ? r_contact_model_part : r_contact_model_part.GetSubModelPart("ContactSub"+mThisParameters["id_name"].GetString());
    NodesArrayType& r_nodes_array = r_sub_contact_model_part.Nodes();

    // We compute now the normal gap and set the nodes under certain threshold as active
    #pragma omp parallel for
    for(int i = 0; i < static_cast<int>(r_nodes_array.size()); ++i) {
        auto it_node = r_nodes_array.begin() + i;
        if (it_node->Is(SLAVE) == !mOptions.Is(BaseContactSearchProcess::INVERTED_SEARCH)) {
//             if (it_node->GetValue(NORMAL_GAP) < ZeroTolerance) {
            if (it_node->GetValue(NORMAL_GAP) < GapThreshold * it_node->FastGetSolutionStepValue(NODAL_H)) {
                SetActiveNode(it_node, common_epsilon, scale_factor);
            } else {
            #ifdef KRATOS_DEBUG
                KRATOS_WARNING_IF("BaseContactSearchProcess", it_node->Is(ACTIVE)) << "WARNING: A node that used to be active is not active anymore. Check that. Node ID: " << it_node->Id() << std::endl;
            #endif
                SetInactiveNode(it_node);
            }
        }
    }

    KRATOS_CATCH("")
}

/***********************************************************************************/
/***********************************************************************************/

template<SizeType TDim, SizeType TNumNodes, SizeType TNumNodesMaster>
void BaseContactSearchProcess<TDim, TNumNodes, TNumNodesMaster>::SetActiveNode(
    NodesArrayType::iterator ItNode,
    const double CommonEpsilon,
    const double ScaleFactor
    )
{
    KRATOS_TRY

    // We activate
    ItNode->Set(ACTIVE, true);
    ItNode->Set(MARKER, true);

    // Set SLIP flag
    if (mrMainModelPart.Is(SLIP)) {
        if (ItNode->GetValue(FRICTION_COEFFICIENT) < ZeroTolerance || mOptions.Is(BaseContactSearchProcess::PURE_SLIP)) {
            ItNode->Set(SLIP, true);
        } else if (!ItNode->IsDefined(SLIP)) {
            ItNode->Set(SLIP, false);
        }
    }

    KRATOS_CATCH("")
}

/***********************************************************************************/
/***********************************************************************************/

template<SizeType TDim, SizeType TNumNodes, SizeType TNumNodesMaster>
void BaseContactSearchProcess<TDim, TNumNodes, TNumNodesMaster>::SetInactiveNode(NodesArrayType::iterator ItNode)
{
    KRATOS_TRY

    // If the node has been already actived we do not inactivate
    if (ItNode->IsNot(MARKER)) {
        // Auxiliar zero array
        const array_1d<double, 3> zero_array = ZeroVector(3);

        if (ItNode->Is(ACTIVE) ) {
            ItNode->Set(ACTIVE, false);
            switch(mTypeSolution) {
                case TypeSolution::VectorLagrangeMultiplier :
                    noalias(ItNode->FastGetSolutionStepValue(VECTOR_LAGRANGE_MULTIPLIER)) = zero_array;
                    break;
                case TypeSolution::ScalarLagrangeMultiplier :
                    ItNode->FastGetSolutionStepValue(SCALAR_LAGRANGE_MULTIPLIER) = 0.0;
                    break;
                case TypeSolution::NormalContactStress :
                    ItNode->FastGetSolutionStepValue(LAGRANGE_MULTIPLIER_CONTACT_PRESSURE) = 0.0;
                    break;
                case TypeSolution::FrictionlessPenaltyMethod :
                    break;
                case TypeSolution::FrictionalPenaltyMethod :
                    break;
                case TypeSolution::OtherFrictionless :
                    break;
                case TypeSolution::OtherFrictional :
                    break;
            }
        }

        // We set the gap to zero (in order to have something "visible" to post process)
        ItNode->SetValue(NORMAL_GAP, 0.0);
    }

    KRATOS_CATCH("")
}

/***********************************************************************************/
/***********************************************************************************/

template<SizeType TDim, SizeType TNumNodes, SizeType TNumNodesMaster>
inline void BaseContactSearchProcess<TDim, TNumNodes, TNumNodesMaster>::ComputeWeightedReaction()
{
    KRATOS_TRY

    // Auxiliar zero array
    const array_1d<double, 3> zero_array = ZeroVector(3);

    // Auxiliar gap
    ModelPart& r_contact_model_part = mrMainModelPart.GetSubModelPart("Contact");
    ModelPart& r_sub_contact_model_part = mOptions.IsNot(BaseContactSearchProcess::MULTIPLE_SEARCHS) ? r_contact_model_part : r_contact_model_part.GetSubModelPart("ContactSub"+mThisParameters["id_name"].GetString());
    NodesArrayType& r_nodes_array = r_sub_contact_model_part.Nodes();
    switch(mTypeSolution) {
        case TypeSolution::VectorLagrangeMultiplier :
            if (mrMainModelPart.Is(SLIP)) {
                VariableUtils().SetScalarVar<Variable<double>>(WEIGHTED_GAP, 0.0, r_nodes_array);
                VariableUtils().SetVectorVar(WEIGHTED_SLIP, zero_array, r_nodes_array);
            } else if (mrMainModelPart.Is(CONTACT)) {
                VariableUtils().SetScalarVar<Variable<double>>(WEIGHTED_GAP, 0.0, r_nodes_array);
            } else
                VariableUtils().SetVectorVar(WEIGHTED_VECTOR_RESIDUAL, zero_array, r_nodes_array);
            break;
        case TypeSolution::ScalarLagrangeMultiplier :
            VariableUtils().SetScalarVar<Variable<double>>(WEIGHTED_SCALAR_RESIDUAL, 0.0, r_nodes_array);
            break;
        case TypeSolution::NormalContactStress :
            VariableUtils().SetScalarVar<Variable<double>>(WEIGHTED_GAP, 0.0, r_nodes_array);
            break;
        case TypeSolution::FrictionlessPenaltyMethod :
            VariableUtils().SetScalarVar<Variable<double>>(WEIGHTED_GAP, 0.0, r_nodes_array);
            break;
        case TypeSolution::FrictionalPenaltyMethod :
            VariableUtils().SetScalarVar<Variable<double>>(WEIGHTED_GAP, 0.0, r_nodes_array);
            VariableUtils().SetVectorVar(WEIGHTED_SLIP, zero_array, r_nodes_array);
            break;
        case TypeSolution::OtherFrictionless :
            VariableUtils().SetScalarVar<Variable<double>>(WEIGHTED_GAP, 0.0, r_nodes_array);
            break;
        case TypeSolution::OtherFrictional :
            VariableUtils().SetScalarVar<Variable<double>>(WEIGHTED_GAP, 0.0, r_nodes_array);
            VariableUtils().SetVectorVar(WEIGHTED_SLIP, zero_array, r_nodes_array);
            break;
    }

    // Compute explicit contibution of the conditions
    const std::string sub_computing_model_part_name = "ComputingContactSub" + mThisParameters["id_name"].GetString();
    ModelPart& r_computing_contact_model_part = mrMainModelPart.GetSubModelPart("ComputingContact");
    ModelPart& r_sub_computing_contact_model_part = mOptions.IsNot(BaseContactSearchProcess::MULTIPLE_SEARCHS) ? r_computing_contact_model_part : r_computing_contact_model_part.GetSubModelPart(sub_computing_model_part_name);
    ContactUtilities::ComputeExplicitContributionConditions(r_sub_computing_contact_model_part);

    KRATOS_CATCH("")
}

/***********************************************************************************/
/***********************************************************************************/

template<SizeType TDim, SizeType TNumNodes, SizeType TNumNodesMaster>
inline void BaseContactSearchProcess<TDim, TNumNodes, TNumNodesMaster>::CreateAuxiliarConditions(
    ModelPart& rContactModelPart,
    ModelPart& rComputingModelPart,
    IndexType& rConditionId
    )
{
    KRATOS_TRY

    // In case of debug mode
    CreateDebugFile(rContactModelPart, "original_conditions_normal_debug_");

    // Iterate in the conditions and create the new ones
    auto& r_conditions_array = rContactModelPart.Conditions();
    const auto it_cond_begin = r_conditions_array.begin();
    for(IndexType i = 0; i < r_conditions_array.size(); ++i) {
        auto it_cond = it_cond_begin + i;
        if (it_cond->Is(SLAVE) == !mOptions.Is(BaseContactSearchProcess::INVERTED_SEARCH)) {
            IndexMap::Pointer p_indexes_pairs = it_cond->GetValue(INDEX_MAP);
            for (auto it_pair = p_indexes_pairs->begin(); it_pair != p_indexes_pairs->end(); ++it_pair ) {
                if (it_pair->second == 0) { // If different than 0 it is an existing condition
                    Condition::Pointer p_cond_master = mrMainModelPart.pGetCondition(it_pair->first); // MASTER
                    AddPairing(rComputingModelPart, rConditionId, (*it_cond.base()), it_cond->GetValue(NORMAL), p_cond_master, p_cond_master->GetValue(NORMAL), p_indexes_pairs, it_cond->pGetProperties());
                }
            }
        }
    }

    // In case of debug mode
    CreateDebugFile(rContactModelPart, "created_conditions_normal_debug_");

    KRATOS_CATCH("")
}

/***********************************************************************************/
/***********************************************************************************/

template<SizeType TDim, SizeType TNumNodes, SizeType TNumNodesMaster>
inline double BaseContactSearchProcess<TDim, TNumNodes, TNumNodesMaster>::Radius(GeometryType& ThisGeometry)
{
    KRATOS_TRY

    double radius = 0.0;
    const Point& r_center = ThisGeometry.Center();

    for(IndexType i_node = 0; i_node < ThisGeometry.PointsNumber(); ++i_node)  {
        const array_1d<double, 3>& aux_vector = r_center.Coordinates() - ThisGeometry[i_node].Coordinates();
        const double aux_value = norm_2(aux_vector);
        if(aux_value > radius)
            radius = aux_value;
    }

    return radius;

    KRATOS_CATCH("")
}

/***********************************************************************************/
/***********************************************************************************/

template<SizeType TDim, SizeType TNumNodes, SizeType TNumNodesMaster>
void BaseContactSearchProcess<TDim, TNumNodes, TNumNodesMaster>::ResetContactOperators()
{
    KRATOS_TRY

    // We iterate over the conditions
    ModelPart& r_contact_model_part = mrMainModelPart.GetSubModelPart("Contact");
    ModelPart& r_sub_contact_model_part = mOptions.IsNot(BaseContactSearchProcess::MULTIPLE_SEARCHS) ? r_contact_model_part : r_contact_model_part.GetSubModelPart("ContactSub"+mThisParameters["id_name"].GetString());
    ConditionsArrayType& r_conditions_array = r_sub_contact_model_part.Conditions();
    const auto it_cond_begin = r_conditions_array.begin();

    if (mrMainModelPart.Is(MODIFIED)) { // It has been remeshed. We remove everything

        #pragma omp parallel for
        for(int i = 0; i < static_cast<int>(r_conditions_array.size()); ++i) {
            auto it_cond = it_cond_begin + i;
            if (it_cond->Is(SLAVE) == !mOptions.Is(BaseContactSearchProcess::INVERTED_SEARCH)) {
                IndexMap::Pointer p_indexes_pairs = it_cond->GetValue(INDEX_MAP);

                if (p_indexes_pairs != nullptr) {
                    p_indexes_pairs->clear();
//                     p_indexes_pairs->reserve(mAllocationSize);
                }
            }
        }

        // We remove all the computing conditions conditions
        const std::string sub_computing_model_part_name = "ComputingContactSub" + mThisParameters["id_name"].GetString();
        ModelPart& r_computing_contact_model_part = mrMainModelPart.GetSubModelPart("ComputingContact");
        ModelPart& r_sub_computing_contact_model_part = mOptions.IsNot(BaseContactSearchProcess::MULTIPLE_SEARCHS) ? r_computing_contact_model_part : r_computing_contact_model_part.GetSubModelPart(sub_computing_model_part_name);
        ConditionsArrayType& r_computing_conditions_array = r_sub_computing_contact_model_part.Conditions();
        VariableUtils().SetFlag(TO_ERASE, true, r_computing_conditions_array);
    } else {
        // We iterate, but not in OMP
        for(IndexType i = 0; i < r_conditions_array.size(); ++i) {
            auto it_cond = r_conditions_array.begin() + i;
            if (it_cond->Is(SLAVE) == !mOptions.Is(BaseContactSearchProcess::INVERTED_SEARCH)) {
                IndexMap::Pointer p_indexes_pairs = it_cond->GetValue(INDEX_MAP);
                if (p_indexes_pairs != nullptr) {
                    // The vector with the ids to remove
                    std::vector<IndexType> inactive_conditions_ids;
                    for (auto it_pair = p_indexes_pairs->begin(); it_pair != p_indexes_pairs->end(); ++it_pair ) {
                        Condition::Pointer p_cond = mrMainModelPart.pGetCondition(it_pair->second);
                        if (p_cond->IsNot(ACTIVE)) {
                            p_cond->Set(TO_ERASE, true);
                            inactive_conditions_ids.push_back(it_pair->first);
                        }
                    }
                    for (auto& i_to_remove : inactive_conditions_ids) {
                        p_indexes_pairs->RemoveId(i_to_remove);
                    }
                }
            }
        }
    }

    mrMainModelPart.RemoveConditionsFromAllLevels(TO_ERASE);

    KRATOS_CATCH("")
}

/***********************************************************************************/
/***********************************************************************************/

template<SizeType TDim, SizeType TNumNodes, SizeType TNumNodesMaster>
void BaseContactSearchProcess<TDim, TNumNodes, TNumNodesMaster>::CreateDebugFile(
    ModelPart& rModelPart,
    const std::string& rName
    )
{
    KRATOS_TRY

    if (mThisParameters["debug_mode"].GetBool()) {
        ConditionsArrayType& r_conditions_array = rModelPart.Conditions();
        std::filebuf debug_buffer;
        debug_buffer.open(rName + rModelPart.Name() + "_step=" + std::to_string( rModelPart.GetProcessInfo()[STEP]) + ".out",std::ios::out);
        std::ostream os(&debug_buffer);
        for (const auto& r_cond : r_conditions_array) {
            const array_1d<double, 3>& r_normal = r_cond.GetValue(NORMAL);
            os << "Condition " << r_cond.Id() << "\tNodes ID:";
            for (auto& r_node : r_cond.GetGeometry()) {
                os << "\t" << r_node.Id();
            }
            os << "\tNORMAL: " << r_normal[0] << "\t" << r_normal[1] << "\t" << r_normal[2] <<"\n";
        }
        debug_buffer.close();
    }

    KRATOS_CATCH("")
}

/***********************************************************************************/
/***********************************************************************************/

template<SizeType TDim, SizeType TNumNodes, SizeType TNumNodesMaster>
typename BaseContactSearchProcess<TDim, TNumNodes, TNumNodesMaster>::SearchTreeType BaseContactSearchProcess<TDim, TNumNodes, TNumNodesMaster>::ConvertSearchTree(const std::string& str)
{
    KRATOS_TRY

    KRATOS_ERROR_IF(str == "KDOP") << "KDOP contact search: Not yet implemented" << std::endl;

    if (str == "InRadius" || str == "in_radius") {
        return SearchTreeType::KdtreeInRadius;
    } else if(str == "InBox" || str == "in_box") {
        return SearchTreeType::KdtreeInBox;
    } else if(str == "InRadiusWithOBB" || str == "in_radius_with_obb") {
        return SearchTreeType::KdtreeInRadiusWithOBB;
    } else if(str == "InBoxWithOBB" || str == "in_box_with_obb") {
        return SearchTreeType::KdtreeInBoxWithOBB;
    } else if (str == "OctreeWithOBB" || str == "octree_with_obb") {
        return SearchTreeType::OctreeWithOBB;
    } else if (str == "KDOP" || str == "kdop") {
        return SearchTreeType::Kdop;
    } else {
        return SearchTreeType::KdtreeInRadius;
    }

    KRATOS_CATCH("")
}

/***********************************************************************************/
/***********************************************************************************/

template<SizeType TDim, SizeType TNumNodes, SizeType TNumNodesMaster>
typename BaseContactSearchProcess<TDim, TNumNodes, TNumNodesMaster>::CheckGap BaseContactSearchProcess<TDim, TNumNodes, TNumNodesMaster>::ConvertCheckGap(const std::string& str)
{
    KRATOS_TRY

    if(str == "NoCheck" || str == "no_check")
        return CheckGap::NoCheck;
    else if(str == "DirectCheck" || str == "direct_check")
        return CheckGap::DirectCheck;
    else if (str == "MappingCheck" || str == "mapping_check")
        return CheckGap::MappingCheck;
    else
        return CheckGap::MappingCheck;

    KRATOS_CATCH("")
}

/***********************************************************************************/
/***********************************************************************************/

template<SizeType TDim, SizeType TNumNodes, SizeType TNumNodesMaster>
Parameters BaseContactSearchProcess<TDim, TNumNodes, TNumNodesMaster>::GetDefaultParameters()
{
    KRATOS_TRY

    Parameters default_parameters = Parameters(R"(
    {
        "allocation_size"                      : 1000,
        "bucket_size"                          : 4,
        "search_factor"                        : 3.5,
        "type_search"                          : "InRadius",
        "check_gap"                            : "MappingCheck",
        "condition_name"                       : "",
        "final_string"                         : "",
        "inverted_search"                      : false,
        "dynamic_search"                       : false,
        "static_check_movement"                : false,
        "predefined_master_slave"              : true,
        "id_name"                              : "",
        "consider_gap_threshold"               : false,
        "predict_correct_lagrange_multiplier"  : false,
        "pure_slip"                            : false,
        "debug_mode"                           : false,
        "octree_search_parameters" : {
            "bounding_box_factor"    : 0.1,
            "debug_obb"              : false,
            "OBB_intersection_type"  : "SeparatingAxisTheorem"
        }
    })" );

    return default_parameters;

    KRATOS_CATCH("")
}

/***********************************************************************************/
/***********************************************************************************/

template class BaseContactSearchProcess<2, 2>;
template class BaseContactSearchProcess<3, 3>;
template class BaseContactSearchProcess<3, 4>;
template class BaseContactSearchProcess<3, 3, 4>;
template class BaseContactSearchProcess<3, 4, 3>;

}  // namespace Kratos.
