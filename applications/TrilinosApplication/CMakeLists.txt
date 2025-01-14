set(CMAKE_INCLUDE_CURRENT_DIR ON)

message("**** configuring KratosTrilinosApplication ****")

include(pybind11Tools)

include_directories( ${CMAKE_SOURCE_DIR}/kratos )
include_directories( ${CMAKE_SOURCE_DIR}/applications/ )

# Dependencies on other application -- TRILINOS specific
include_directories( ${CMAKE_SOURCE_DIR}/applications/FluidDynamicsApplication )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/custom_external_libraries )

# Find trilinos solver package -- this is our own find package!!
find_package(TRILINOS REQUIRED)
include_directories(SYSTEM ${TRILINOS_INCLUDE_DIR})
link_directories(${TRILINOS_LIBRARIES})

if(TRILINOS_USE_AMGCL MATCHES ON)
    add_definitions(-DAMGCL_INCLUDED)

    find_package(Pastix QUIET)
    if (PASTIX_INCLUDES)
        set(HAVE_PASTIX ON)
        find_package(Scotch REQUIRED)
        #include_directories(${PASTIX_INCLUDES} ${SCOTCH_INCLUDES})
        include_directories(${PASTIX_INCLUDES})
        add_definitions(-DAMGCL_HAVE_PASTIX)
    endif()

    # Including the amgcl, which is header only
    set(AMGCL_ROOT "../../external_libraries" )
    message(STATUS "Found AMGCL at ${AMGCL_ROOT}")
    include_directories(${AMGCL_ROOT})
endif(TRILINOS_USE_AMGCL MATCHES ON)

# Sources for the Trilinos application core
set( KRATOS_TRILINOS_APPLICATION_CORE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/trilinos_application.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/amgcl_mpi_solver_impl.cpp;
	${CMAKE_CURRENT_SOURCE_DIR}/custom_factories/trilinos_linear_solver_factory.cpp
    ${CMAKE_SOURCE_DIR}/applications/FluidDynamicsApplication/fluid_dynamics_application_variables.cpp  #TODO: this should REALLY NOT BE HERE
    ${CMAKE_CURRENT_SOURCE_DIR}/custom_utilities/mpi_normal_calculation_utilities.cpp;
)

# Sources for the Python module
file(
  GLOB_RECURSE
  KRATOS_TRILINOS_PYTHON_INTERFACE_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/custom_python/*.cpp
)

## Kratos tests sources. Enabled by default
if(${KRATOS_BUILD_TESTING} MATCHES ON)
    file(GLOB_RECURSE KRATOS_TRILINOS_TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp)
endif(${KRATOS_BUILD_TESTING} MATCHES ON)

# ###############################################################
## TrilinosApplication core library (C++ parts)
add_library( KratosTrilinosCore SHARED ${KRATOS_TRILINOS_APPLICATION_CORE_SOURCES} ${KRATOS_TRILINOS_TEST_SOURCES} )
if (HAVE_PASTIX)
    target_link_libraries(KratosTrilinosCore PRIVATE KratosCore KratosMPICore ${TRILINOS_LIBRARIES} ${MPI_LIBRARIES} ${PASTIX_LIBRARIES} ${SCOTCH_LIBRARIES} ${BLAS_LIBRARIES} )
    install(FILES ${PASTIX_LIBRARIES} DESTINATION libs )

    message("linking PASTIX in the AMGCL lib")
else(HAVE_PASTIX)
    target_link_libraries(KratosTrilinosCore PRIVATE KratosCore KratosMPICore ${TRILINOS_LIBRARIES} ${MPI_LIBRARIES} )
endif(HAVE_PASTIX)
set_target_properties( KratosTrilinosCore PROPERTIES COMPILE_DEFINITIONS "TRILINOS_APPLICATION=EXPORT,API")

## TrilinosApplication python module
pybind11_add_module( KratosTrilinosApplication MODULE THIN_LTO ${KRATOS_TRILINOS_PYTHON_INTERFACE_SOURCES} )
target_link_libraries( KratosTrilinosApplication PRIVATE KratosTrilinosCore)
set_target_properties( KratosTrilinosApplication PROPERTIES PREFIX "")

# Changing the .dll suffix to .pyd
if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
	set_target_properties(KratosTrilinosApplication PROPERTIES SUFFIX .pyd)
endif(${CMAKE_SYSTEM_NAME} MATCHES "Windows")

# Changing the .dylib suffix to .so
if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
	set_target_properties(KratosTrilinosApplication PROPERTIES SUFFIX .so)
endif(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")

if(USE_COTIRE MATCHES ON)
    cotire(KratosTrilinosCore)
    cotire(KratosTrilinosApplication)
endif(USE_COTIRE MATCHES ON)

install(TARGETS KratosTrilinosCore DESTINATION libs )
install(TARGETS KratosTrilinosApplication DESTINATION libs )

# Kratos Testing. Install everything except sources
if(${INSTALL_TESTING_FILES} MATCHES ON )
    get_filename_component (CURRENT_DIR_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests DESTINATION applications/${CURRENT_DIR_NAME}
    PATTERN "*.git" EXCLUDE
    PATTERN "*.c" EXCLUDE
    PATTERN "*.h" EXCLUDE
    PATTERN "*.cpp" EXCLUDE
    PATTERN "*.hpp" EXCLUDE
    )
endif(${INSTALL_TESTING_FILES} MATCHES ON)

# Install python scripts
if(${INSTALL_PYTHON_FILES} MATCHES ON)
  get_filename_component (CURRENT_DIR_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
  install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/python_scripts DESTINATION applications/${CURRENT_DIR_NAME}  FILES_MATCHING PATTERN "*.py"  PATTERN ".svn" EXCLUDE)

  message("trilinos libs to be installed" ${TRILINOS_LIBRARIES})
  install(FILES ${TRILINOS_LIBRARIES} DESTINATION libs )
endif(${INSTALL_PYTHON_FILES} MATCHES ON)

# Add to the KratosMultiphisics Python module
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/TrilinosApplication.py" DESTINATION "KratosMultiphysics/TrilinosApplication" RENAME "__init__.py")
