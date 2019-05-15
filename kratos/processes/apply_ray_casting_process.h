//    |  /           |
//    ' /   __| _` | __|  _ \   __|
//    . \  |   (   | |   (   |\__ `
//   _|\_\_|  \__,_|\__|\___/ ____/
//                   Multi-Physics
//
//  License:		 BSD License
//					 Kratos default license: kratos/license.txt
//
//  Main authors:    Pooyan Dadvand, Ruben Zorrilla
//

#if !defined(KRATOS_APPLY_RAY_CASTING_PROCESS_H_INCLUDED )
#define  KRATOS_APPLY_RAY_CASTING_PROCESS_H_INCLUDED

// System includes
#include <string>
#include <iostream>

// External includes

// Project includes
#include "processes/find_intersected_geometrical_objects_process.h"

namespace Kratos
{
///@addtogroup Kratos Core
///@{

///@name Kratos Classes
///@{

/// Applies ray casting to distinguish the color (like in/out) of each node in modelpart
/** This class is used to define the which nodes are inside or outside of certain volume described by its contour
 */
template<std::size_t TDim = 3>
class KRATOS_API(KRATOS_CORE) ApplyRayCastingProcess : public Process
{

public:
    ///@name Type Definitions
    ///@{

    /// Pointer definition of ApplyRayCastingProcess
    KRATOS_CLASS_POINTER_DEFINITION(ApplyRayCastingProcess);

    //TODO: These using statements have been included to make the old functions able to compile. It is still pending to update them.
    using ConfigurationType = Internals::DistanceSpatialContainersConfigure;
    using CellType = OctreeBinaryCell<ConfigurationType>;
    using OctreeType = OctreeBinary<CellType>;
    using CellNodeDataType = ConfigurationType::cell_node_data_type;

    typedef Element::GeometryType IntersectionGeometryType;
    typedef std::vector<std::pair<double, IntersectionGeometryType*> > IntersectionsContainerType;

    ///@}
    ///@name Life Cycle
    ///@{

    /**
     * @brief Construct a new ApplyRayCastingProcess object using volume and skin model parts
     * Constructor without user defined extra rays epsilon, used to 
     * generate the extra rays when voting is required for coloring
     * @param rVolumePart model part containing the volume elements
     * @param rSkinPart model part containing the skin to compute 
     * the distance to as conditions
     */
    ApplyRayCastingProcess(
        ModelPart& rVolumePart,
        ModelPart& rSkinPart);

    /**
     * @brief Construct a new ApplyRayCastingProcess object using volume and skin model parts
     * Constructor with user defined extra rays epsilon, used to 
     * generate the extra rays when voting is required for coloring
     * @param rVolumePart model part containing the volume elements
     * @param rSkinPart model part containing the skin to compute 
     * the distance to as conditions
     * @param ExtraRaysEpsilon user-defined extra rays epsilon
     */
    ApplyRayCastingProcess(
        ModelPart& rVolumePart,
        ModelPart& rSkinPart,
        const double ExtraRaysEpsilon);

    /**
     * @brief Construct a new ApplyRayCastingProcess object using an already created search structure
     * This constructor is used by 
     */
    ApplyRayCastingProcess(
        FindIntersectedGeometricalObjectsProcess& TheFindIntersectedObjectsProcess,
        const double ExtraRaysEpsilon);

    /// Destructor.
    ~ApplyRayCastingProcess() override;

    ///@}
    ///@name Deleted
    ///@{

    /// Default constructor.
    ApplyRayCastingProcess() = delete;;

    /// Copy constructor.
    ApplyRayCastingProcess(ApplyRayCastingProcess const& rOther) = delete;

    /// Assignment operator.
    ApplyRayCastingProcess& operator=(ApplyRayCastingProcess const& rOther) = delete;

    ///@}
    ///@name Operations
    ///@{

    /**
     * @brief Computes the raycasting distance for a node
     * This method computes the raycasting distance for a given node. It casts a ray
     * in the x and y (as well as z in 3D) directions and computes the distance from 
     * the ray origin point (the node of interest) to each one of the intersecting objects.
     * @param rNode reference to the node of interest
     * @return double raycasting distance value computed
     */
    virtual double DistancePositionInSpace(const Node<3> &rNode);

    /**
     * @brief Get the ray intersecting objects and its distance
     * For a given ray and direction, this method search for all the intersecting entities 
     * to this ray. This operation is performed using the binary octree in the discontinuous 
     * distance base class to check each one of the cells crossed by the ray.
     * @param ray casted ray coordinates
     * @param direction direction of the casted ray (0 for x, 1 for y and 2 for z)
     * @param rIntersections array containing a pair for each intersection found. The 
     * first value of the pair contains the ray distance to the intersection entity
     * while the second one contains a pointer to the intersection entity geometry
     */
    virtual void GetRayIntersections(
        const double* ray,
        const unsigned int direction,
        std::vector<std::pair<double,Element::GeometryType*> > &rIntersections); 

    /**
     * @brief Get the intersecting objects contained in the current cell
     * 
     * @param cell current cell
     * @param ray casted ray coordinates
     * @param ray_key binary octree ray key
     * @param direction direction of the casted ray (0 for x, 1 for y and 2 for z)
     * @param rIntersections array containing a pair for each intersection found. The 
     * first value of the pair contains the ray distance to the intersection entity
     * while the second one contains a pointer to the intersection entity geometry
     * @return int 0 if the cell intersection search has succeeded
     */
    virtual int GetCellIntersections(
        OctreeType::cell_type* cell, 
        const double* ray,
        OctreeType::key_type* ray_key, 
        const unsigned int direction,
        std::vector<std::pair<double, Element::GeometryType*> > &rIntersections); 

    /**
     * @brief Executes the ApplyRayCastingProcess
     * This method automatically does all the calls required to compute the signed distance function.
     */
    void Execute() override;

    ///@}
    ///@name Input and output
    ///@{

    /// Turn back information as a string.
    std::string Info() const override;

    /// Print information about this object.
    void PrintInfo(std::ostream& rOStream) const override;

    /// Print object's data.
    void PrintData(std::ostream& rOStream) const override;

    ///@}
private:
    ///@name Static Member Variables
    ///@{


    ///@}
    ///@name Member Variables
    ///@{

    const double mExtraRaysEpsilon = 1.0e-8;
    FindIntersectedGeometricalObjectsProcess* mpFindIntersectedObjectsProcess;
    bool mIsSearchStructureAllocated;
    double mCharactresticLength;

    ///@}
    ///@name Private Operators
    ///@{


    ///@}
    ///@name Private Operations
    ///@{

    /**
     * @brief Checks if a ray intersects an intersecting geometry candidate
     * For a given intersecting geometry, it checks if the ray intersects it
     * @param rGeometry reference to the candidate intersecting object
     * @param pRayPoint1 ray origin point
     * @param pRayPoint2 ray end point
     * @param pIntersectionPoint obtained intersecting point
     * @return int integer containing the result of the intersection (1 if intersects)
     */
    int ComputeRayIntersection(
        Element::GeometryType& rGeometry,
        const double* pRayPoint1,
        const double* pRayPoint2,
        double* pIntersectionPoint);

    void GetExtraRayOrigins(
        const double RayEpsilon,
        const array_1d<double,3> &rCoords,
        std::vector<array_1d<double,3>> &rExtraRayOrigs);


    void CorrectExtraRayOrigin(double* ExtraRayCoords);

    void ComputeExtraRayColors(
        const double Epsilon,
        const double RayPerturbation,
        const array_1d<double,3> &rCoords,
        array_1d<double,TDim> &rDistances);

    void CalculateCharacteristicLength();

    ///@}
    ///@name Private  Access
    ///@{


    ///@}
    ///@name Private Inquiry
    ///@{


    ///@}
    ///@name Un accessible methods
    ///@{


    ///@}
}; // Class ApplyRayCastingProcess

///@}
///@name Type Definitions
///@{


///@}
///@name Input and output
///@{

/// input stream function
inline std::istream& operator >> (
    std::istream& rIStream,
    ApplyRayCastingProcess<>& rThis);

/// output stream function
inline std::ostream& operator << (
    std::ostream& rOStream,
    const ApplyRayCastingProcess<>& rThis)
{
    rThis.PrintInfo(rOStream);
    rOStream << std::endl;
    rThis.PrintData(rOStream);

    return rOStream;
}
///@}
///@} addtogroup block

} // namespace Kratos.

#endif // KRATOS_APPLY_RAY_CASTING_PROCESS_H_INCLUDED  defined