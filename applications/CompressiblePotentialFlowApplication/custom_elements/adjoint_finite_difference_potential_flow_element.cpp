//    |  /           |
//    ' /   __| _` | __|  _ \   __|
//    . \  |   (   | |   (   |\__ `
//   _|\_\_|  \__,_|\__|\___/ ____/
//                   Multi-Physics
//
//  License:        BSD License
//                  Kratos default license: kratos/license.txt
//
//
//  Main authors:    Marc Nunez, based on A. Geiser, M. Fusseder, I. Lopez and R. Rossi work
//
#include "compressible_potential_flow_application_variables.h"
#include "incompressible_potential_flow_element.h"
#include "compressible_potential_flow_element.h"
#include "adjoint_finite_difference_potential_flow_element.h"

namespace Kratos
{
    template <class TPrimalElement>
    Element::Pointer AdjointFiniteDifferencePotentialFlowElement<TPrimalElement>::Create(IndexType NewId, NodesArrayType const& ThisNodes, typename PropertiesType::Pointer pProperties) const
    {
        KRATOS_TRY
          return Kratos::make_intrusive<AdjointFiniteDifferencePotentialFlowElement<TPrimalElement>>(NewId, this->GetGeometry().Create(ThisNodes), pProperties);
        KRATOS_CATCH("");
    }

    template <class TPrimalElement>
    Element::Pointer AdjointFiniteDifferencePotentialFlowElement<TPrimalElement>::Create(IndexType NewId, typename GeometryType::Pointer pGeom, typename PropertiesType::Pointer pProperties) const
    {
        KRATOS_TRY
            return Kratos::make_intrusive<AdjointFiniteDifferencePotentialFlowElement<TPrimalElement>>(NewId, pGeom, pProperties);
        KRATOS_CATCH("");
    }

    template <class TPrimalElement>
    Element::Pointer AdjointFiniteDifferencePotentialFlowElement<TPrimalElement>::Clone(IndexType NewId, NodesArrayType const& ThisNodes) const
    {
        KRATOS_TRY
        return Element::Pointer(new AdjointFiniteDifferencePotentialFlowElement(NewId, this->GetGeometry().Create(ThisNodes), this->pGetProperties()));
        KRATOS_CATCH("");
    }


    template <class TPrimalElement>
    void AdjointFiniteDifferencePotentialFlowElement<TPrimalElement>::CalculateSensitivityMatrix(const Variable<array_1d<double,3> >& rDesignVariable,
                                            Matrix& rOutput,
                                            const ProcessInfo& rCurrentProcessInfo)
    {
        KRATOS_TRY;
        const double delta = this->GetPerturbationSize();
        ProcessInfo process_info = rCurrentProcessInfo;

        Vector RHS;
        Vector RHS_perturbed;

        auto pPrimalElement = this->pGetPrimalElement();
        const auto& r_geometry = this->GetGeometry();

        pPrimalElement->CalculateRightHandSide(RHS, process_info);

        if (rOutput.size1() != NumNodes)
            rOutput.resize(Dim*NumNodes, RHS.size(), false);

        for(unsigned int i_node = 0; i_node<NumNodes; i_node++){
            for(unsigned int i_dim = 0; i_dim<Dim; i_dim++){
                if ((r_geometry[i_node].Is(SOLID)) && (!r_geometry[i_node].GetValue(TRAILING_EDGE))){
                    pPrimalElement->GetGeometry()[i_node].GetInitialPosition()[i_dim] += delta;
                    pPrimalElement->GetGeometry()[i_node].Coordinates()[i_dim] += delta;

                    // compute LHS after perturbation
                    pPrimalElement->CalculateRightHandSide(RHS_perturbed, process_info);

                    //compute derivative of RHS w.r.t. design variable with finite differences
                    for(unsigned int i = 0; i < RHS.size(); ++i)
                        rOutput( (i_dim + i_node*Dim), i) = (RHS_perturbed[i] - RHS[i]) / delta;

                    // unperturb the design variable
                    pPrimalElement->GetGeometry()[i_node].GetInitialPosition()[i_dim] -= delta;
                    pPrimalElement->GetGeometry()[i_node].Coordinates()[i_dim] -= delta;
                }else{
                    for(unsigned int i = 0; i < RHS.size(); ++i)
                        rOutput((i_dim + i_node*Dim), i) = 0.0;
                }
            }
        }

        KRATOS_CATCH("")
    }

    template <class TPrimalElement>
    double AdjointFiniteDifferencePotentialFlowElement<TPrimalElement>::GetPerturbationSize()
    {
        const double delta = this->GetValue(SCALE_FACTOR);
        KRATOS_DEBUG_ERROR_IF_NOT(delta > 0) << "The perturbation size is not > 0!";
        return delta;
    }

    /// Turn back information as a string.
    template <class TPrimalElement>
    std::string AdjointFiniteDifferencePotentialFlowElement<TPrimalElement>::Info() const
    {
        std::stringstream buffer;
        buffer << "AdjointFiniteDifferencePotentialFlowElement #" << this->Id();
        return buffer.str();
    }

    template <class TPrimalElement>
    void AdjointFiniteDifferencePotentialFlowElement<TPrimalElement>::PrintInfo(std::ostream& rOStream) const
    {
        rOStream << "AdjointFiniteDifferencePotentialFlowElement #" << this->Id();
    }


    template <class TPrimalElement>
    void AdjointFiniteDifferencePotentialFlowElement<TPrimalElement>::save(Serializer& rSerializer) const
    {
        KRATOS_SERIALIZE_SAVE_BASE_CLASS(rSerializer, AdjointBasePotentialFlowElement<TPrimalElement> );
        rSerializer.save("mpPrimalElement", this->mpPrimalElement);
    }

    template <class TPrimalElement>
    void AdjointFiniteDifferencePotentialFlowElement<TPrimalElement>::load(Serializer& rSerializer)
    {
        KRATOS_SERIALIZE_LOAD_BASE_CLASS(rSerializer, AdjointBasePotentialFlowElement<TPrimalElement> );
        rSerializer.load("mpPrimalElement", this->mpPrimalElement);
    }
    ///////////////////////////////////////////////////////////////////////////////////////////////////
    // Template class instantiation

    template class AdjointFiniteDifferencePotentialFlowElement<IncompressiblePotentialFlowElement<2,3>>;
    template class AdjointFiniteDifferencePotentialFlowElement<CompressiblePotentialFlowElement<2,3>>;
} // namespace Kratos.

