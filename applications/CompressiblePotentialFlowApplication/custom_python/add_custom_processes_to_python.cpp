//    |  /           |
//    ' /   __| _` | __|  _ \   __|
//    . \  |   (   | |   (   |\__ `
//   _|\_\_|  \__,_|\__|\___/ ____/
//                   Multi-Physics
//
//  License:		 BSD License
//					 Kratos default license: kratos/license.txt
//
//  Main authors:    Riccardo Rossi
//

// System includes

// External includes


// Project includes
#include "includes/define.h"
#include "custom_python/add_custom_processes_to_python.h"
#include "custom_processes/kutta_condition_process.h"
#include "custom_processes/move_model_part_process.h"
#include "custom_processes/define_2d_wake_process.h"
#include "custom_processes/apply_far_field_process.h"

namespace Kratos {
namespace Python {

void  AddCustomProcessesToPython(pybind11::module& m)
{
	namespace py = pybind11;

    py::class_<KuttaConditionProcess, KuttaConditionProcess::Pointer, Process >
        (m, "KuttaConditionProcess")
        .def(py::init<ModelPart&>())
        ;

    py::class_<MoveModelPartProcess, MoveModelPartProcess::Pointer, Process >
        (m, "MoveModelPartProcess")
        .def(py::init<ModelPart&, Parameters>())
        ;

    py::class_<Define2DWakeProcess, Define2DWakeProcess::Pointer, Process >
        (m, "Define2DWakeProcess")
        .def(py::init<ModelPart&, const double>())
        ;

    py::class_<ApplyFarFieldProcess, ApplyFarFieldProcess::Pointer, Process >
        (m, "ApplyFarFieldProcess")
        .def(py::init<ModelPart&, const double, const bool>())
        ;
}

}  // namespace Python.

} // Namespace Kratos
