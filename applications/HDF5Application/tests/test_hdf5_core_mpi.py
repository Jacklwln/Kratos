import KratosMultiphysics
import KratosMultiphysics.HDF5Application as KratosHDF5
from KratosMultiphysics.HDF5Application import core
from KratosMultiphysics.HDF5Application.core import operations, file_io
import KratosMultiphysics.KratosUnittest as KratosUnittest
from unittest.mock import patch, MagicMock
import test_hdf5_core

class TestFileIO(KratosUnittest.TestCase):

    def test_HDF5ParallelFileIO_Creation(self):
        io = file_io._HDF5ParallelFileIO()
        test_hdf5_core.TestFileIO._BuildTestFileIOObject(io)
        obj = io.Get('kratos.h5')
        self.assertIsInstance(obj, KratosHDF5.HDF5FileParallel)


class TestOperations(KratosUnittest.TestCase):

    def test_PartitionedModelPartOutput(self):
        settings = KratosMultiphysics.Parameters()
        settings.AddEmptyValue('operation_type').SetString(
            'partitioned_model_part_output')
        partitioned_model_part_output = operations.Create(settings)
        self.assertTrue(settings.Has('operation_type'))
        self.assertTrue(settings.Has('prefix'))
        with patch('KratosMultiphysics.HDF5Application.core.operations.KratosHDF5.HDF5PartitionedModelPartIO') as p:
            partitioned_model_part_io = p.return_value
            model_part = test_hdf5_core._SurrogateModelPart()
            hdf5_file = MagicMock(spec=KratosHDF5.HDF5FileParallel)
            partitioned_model_part_output(model_part, hdf5_file)
            p.assert_called_once_with(hdf5_file, '/ModelData')
            partitioned_model_part_io.WriteModelPart.assert_called_once_with(
                model_part)

    def test_PartitionedModelPartOutput_NonTerminalPrefix(self):
        settings = KratosMultiphysics.Parameters('''
            {
                "operation_type": "partitioned_model_part_output",
                "prefix": "/ModelData/<identifier>/<time>",
                "time_format": "0.2f"
            }
            ''')
        partitioned_model_part_output = operations.Create(settings)
        with patch('KratosMultiphysics.HDF5Application.core.operations.KratosHDF5.HDF5PartitionedModelPartIO', autospec=True) as p:
            model_part = test_hdf5_core._SurrogateModelPart()
            hdf5_file = MagicMock(spec=KratosHDF5.HDF5FileParallel)
            partitioned_model_part_output(model_part, hdf5_file)
            args, _ = p.call_args
            self.assertEqual(args[1], '/ModelData/model_part/1.23')

if __name__ == "__main__":
    KratosUnittest.main()
