'''HDF5 file IO.

license: HDF5Application/license.txt
'''
import KratosMultiphysics
import KratosMultiphysics.HDF5Application as KratosHDF5
from . import utils
import os


class _FileIO(object):

    def _FileSettings(self, identifier):
        settings = KratosMultiphysics.Parameters()
        settings.AddEmptyValue('file_name').SetString(
            self.filename_getter.Get(identifier))
        settings.AddEmptyValue('file_access_mode').SetString(
            self.file_access_mode)
        settings.AddEmptyValue('file_driver').SetString(self.file_driver)
        settings.AddEmptyValue('echo_level').SetInt(self.echo_level)
        return settings


class _HDF5SerialFileIO(_FileIO):

    def Get(self, identifier=None):
        return KratosHDF5.HDF5FileSerial(self._FileSettings(identifier))


class _HDF5ParallelFileIO(_FileIO):

    def Get(self, identifier=None):
        return KratosHDF5.HDF5FileParallel(self._FileSettings(identifier))


class _HDF5MockFileIO(_FileIO):

    class MockFile(object):

        def __init__(self, file_name):
            self.file_name = file_name

        def GetFileName(self): return self.file_name

    def Get(self, identifier=None):
        settings = self._FileSettings(identifier)
        file_name = settings['file_name'].GetString()
        return self.MockFile(file_name)


def _SetDefaults(settings):
    default_setter = utils.DefaultSetter(settings)
    default_setter.AddString('io_type', 'serial_hdf5_file_io')
    default_setter.AddString('file_name', 'kratos')
    if '<time>' in settings['file_name'].GetString():
        default_setter.AddString('time_format', '0.4f')
    default_setter.AddString('file_access_mode', 'exclusive')
    if 'parallel' in settings['io_type'].GetString():
        default_setter.AddString('file_driver', 'mpio')
    elif os.name == 'nt':
        default_setter.AddString('file_driver', 'windows')
    else:
        default_setter.AddString('file_driver', 'sec2')
    default_setter.AddInt('echo_level', 0)


def _GetIO(io_type):
    if io_type == 'serial_hdf5_file_io':
        io = _HDF5SerialFileIO()
    elif io_type == 'parallel_hdf5_file_io':
        io = _HDF5ParallelFileIO()
    elif io_type == 'mock_hdf5_file_io':
        io = _HDF5MockFileIO()
    else:
        raise ValueError('"io_type" has invalid value "' + io_type + '"')
    return io


class _FilenameGetter(object):

    def __init__(self, settings):
        filename = settings['file_name'].GetString()
        self.filename_parts = filename.split('<time>')
        if settings.Has('time_format'):
            self.time_format = settings['time_format'].GetString()
        else:
            self.time_format = ''

    def Get(self, identifier=None):
        if hasattr(identifier, 'ProcessInfo'):
            time = identifier.ProcessInfo[KratosMultiphysics.TIME]
            filename = format(time, self.time_format).join(self.filename_parts)
        else:
            filename = ''.join(self.filename_parts)
        if hasattr(identifier, 'Name'):
            filename = filename.replace('<identifier>', identifier.Name)
        if not filename.endswith('.h5'):
            filename += '.h5'
        return filename


class _FilenameGetterWithDirectoryInitialization(object):

    def __init__(self, settings):
        self.filename_getter = _FilenameGetter(settings)

    def Get(self, identifier=None):
        file_name = self.filename_getter.Get(identifier)
        self._InitializeDirectory(file_name)
        return file_name

    @staticmethod
    def _InitializeDirectory(file_name):
        dirname = os.path.dirname(file_name)
        if dirname != '' and not os.path.exists(dirname):
            if KratosMultiphysics.DataCommunicator.GetDefault().Rank() == 0:
                os.makedirs(dirname)
            KratosMultiphysics.DataCommunicator.GetDefault().Barrier()


def Create(settings):
    '''Return the IO object specified by the setting 'io_type'.

    Empty settings will contain default values after returning from the
    function call.
    '''
    _SetDefaults(settings)
    io = _GetIO(settings['io_type'].GetString())
    io.filename_getter = _FilenameGetterWithDirectoryInitialization(settings)
    io.file_access_mode = settings['file_access_mode'].GetString()
    io.file_driver = settings['file_driver'].GetString()
    io.echo_level = settings['echo_level'].GetInt()
    return io
