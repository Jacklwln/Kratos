//    |  /           |
//    ' /   __| _` | __|  _ \   __|
//    . \  |   (   | |   (   |\__ `
//   _|\_\_|  \__,_|\__|\___/ ____/
//                   Multi-Physics
//
//  License:        BSD License
//                  Kratos default license: kratos/license.txt
//
//  Main authors:    Marc Núñez,
//

#ifndef KRATOS_PROJECT_SENSITIVITY_TO_SKIN_PROCESS_H
#define KRATOS_PROJECT_SENSITIVITY_TO_SKIN_PROCESS_H

#include "includes/define.h"

#include "includes/kratos_parameters.h"
#include "includes/model_part.h"
#include "processes/process.h"

namespace Kratos
{

	class KRATOS_API(COMPRESSIBLE_POTENTIAL_FLOW_APPLICATION) ProjectSensitivityToSkinProcess : public Process
{
public:
    ///@name Type Definitions
    ///@{

    /// Pointer definition of Process
    KRATOS_CLASS_POINTER_DEFINITION(ProjectSensitivityToSkinProcess);

    // Constructor for ProjectSensitivityToSkinProcess Process
    ProjectSensitivityToSkinProcess(ModelPart& rModelPart, ModelPart& rSkinModelPart);

    /// Destructor.
    ~ProjectSensitivityToSkinProcess() = default;

    /// Assignment operator.
    ProjectSensitivityToSkinProcess& operator=(ProjectSensitivityToSkinProcess const& rOther) = delete;

    /// Copy constructor.
    ProjectSensitivityToSkinProcess(ProjectSensitivityToSkinProcess const& rOther) = delete;

    /// This operator is provided to call the process as a function and simply calls the Execute method.
    void operator()()
    {
        Execute();
    }

    void Execute() override;

    /// Turn back information as a string.
    std::string Info() const override
    {
        return "ProjectSensitivityToSkinProcess";
    }

    /// Print information about this object.
    void PrintInfo(std::ostream& rOStream) const override
    {
        rOStream << "ProjectSensitivityToSkinProcess";
    }

    /// Print object's data.
    void PrintData(std::ostream& rOStream) const override
    {
        this->PrintInfo(rOStream);
    }

private:
    ///@name Static Member Variables
    ///@{


    ///@}
    ///@name Member Variables
    ///@{

    ModelPart& mrModelPart;
    ModelPart& mrSkinModelPart;

}; // Class Process
} // namespace Kratos


#endif // KRATOS_MOVE_MODEL_PART_PROCESS_H