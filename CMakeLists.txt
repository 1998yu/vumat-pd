cmake_minimum_required(VERSION 3.20)

project(VumatPd LANGUAGES CXX Fortran)

include(CTest)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

add_library(elastic_vumat_plugin SHARED
  materials/examples/Elastic.f
  materials/bridge/vumat_c_wrapper_jinfo.f90
  materials/bridge/abaqus_stubs.f90
)
target_include_directories(elastic_vumat_plugin PRIVATE materials/bridge)
target_compile_options(elastic_vumat_plugin PRIVATE
  $<$<COMPILE_LANGUAGE:Fortran>:-std=legacy>
  $<$<COMPILE_LANGUAGE:Fortran>:-ffixed-line-length-none>
  $<$<COMPILE_LANGUAGE:Fortran>:-fallow-argument-mismatch>
  $<$<COMPILE_LANGUAGE:Fortran>:-fcray-pointer>
)
set_target_properties(elastic_vumat_plugin PROPERTIES
  PREFIX ""
  OUTPUT_NAME "elastic_vumat_plugin"
)

add_library(jc_vumat_plugin SHARED
  materials/examples/JohnsonCook.f90
  materials/bridge/vumat_c_wrapper_jinfo.f90
  materials/bridge/abaqus_stubs.f90
)
target_include_directories(jc_vumat_plugin PRIVATE materials/bridge)
target_compile_options(jc_vumat_plugin PRIVATE
  $<$<COMPILE_LANGUAGE:Fortran>:-std=legacy>
  $<$<COMPILE_LANGUAGE:Fortran>:-ffixed-line-length-none>
  $<$<COMPILE_LANGUAGE:Fortran>:-fallow-argument-mismatch>
  $<$<COMPILE_LANGUAGE:Fortran>:-fcray-pointer>
)
set_target_properties(jc_vumat_plugin PROPERTIES
  PREFIX ""
  OUTPUT_NAME "jc_vumat_plugin"
)

add_library(vumat_pd STATIC
  src/MaterialConfigJson.cpp
  src/SimpleJson.cpp
  src/SimpleDamageModel.cpp
  src/TensorVoigtMap.cpp
  src/VumatMaterialAdapter.cpp
  src/DemoConfig.cpp
  src/BoundaryConditions.cpp
  src/ParticleSets.cpp
  src/Geometry.cpp
  src/DemoCaseRunner.cpp
  src/MaterialPointCaseRunner.cpp
  src/DynamicCaseRunner.cpp
  src/ImpactCaseRunner.cpp
  src/BoundaryValueCaseRunner.cpp
  src/ExplicitTimeIntegrator.cpp
  src/CorotationalKinematics.cpp
  src/RigidWallContact.cpp
  src/PdCorrespondenceSkeleton.cpp
  src/PdSimpleDamage.cpp
  src/VumatPdCorrespondenceMaterial.cpp
)
target_include_directories(vumat_pd PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
if(NOT WIN32)
  target_link_libraries(vumat_pd PUBLIC dl)
endif()

add_executable(vumat_pd_run src/main_pd_case_runner.cpp)
target_link_libraries(vumat_pd_run PRIVATE vumat_pd)

add_executable(jc_2d_fem reproducibility/src/main_jc_2d_fem.cpp)
target_link_libraries(jc_2d_fem PRIVATE vumat_pd)

add_executable(corotational_kinematics_test
  tests/corotational_kinematics_test.cpp)
target_link_libraries(corotational_kinematics_test PRIVATE vumat_pd)

add_executable(neighbor_search_test tests/neighbor_search_test.cpp)
target_link_libraries(neighbor_search_test PRIVATE vumat_pd)

add_executable(rigid_wall_contact_test tests/rigid_wall_contact_test.cpp)
target_link_libraries(rigid_wall_contact_test PRIVATE vumat_pd)

if(BUILD_TESTING)
  get_filename_component(VUMAT_FORTRAN_RUNTIME_DIR
    "${CMAKE_Fortran_COMPILER}" DIRECTORY)
  function(configure_vumat_test test_name)
    set_tests_properties(${test_name} PROPERTIES
      WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
      ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:${VUMAT_FORTRAN_RUNTIME_DIR}"
    )
  endfunction()

  add_test(NAME corotational_kinematics
    COMMAND corotational_kinematics_test)
  add_test(NAME neighbor_search
    COMMAND neighbor_search_test)
  add_test(NAME rigid_wall_contact
    COMMAND rigid_wall_contact_test
            examples/rigid_wall_contact.json)
  add_test(NAME affine_bar_elastic
    COMMAND vumat_pd_run examples/affine_bar_elastic.json)
  add_test(NAME affine_shear_elastic
    COMMAND vumat_pd_run examples/affine_shear_elastic.json)
  add_test(NAME jc_vumat_material_point
    COMMAND vumat_pd_run reproducibility/cases/jc_material_point.json)
  foreach(test_name IN ITEMS corotational_kinematics neighbor_search
      rigid_wall_contact affine_bar_elastic affine_shear_elastic
      jc_vumat_material_point)
    configure_vumat_test(${test_name})
  endforeach()
endif()

# Parallel material blocks require reentrant user material routines.
find_package(OpenMP REQUIRED COMPONENTS CXX)
target_link_libraries(vumat_pd PUBLIC OpenMP::OpenMP_CXX)
target_compile_options(jc_vumat_plugin PRIVATE
  $<$<COMPILE_LANGUAGE:Fortran>:-frecursive>)

add_executable(jc_3d_fem reproducibility/src/main_jc_3d_fem.cpp)
target_link_libraries(jc_3d_fem PRIVATE vumat_pd)
add_executable(neighbor_lattice_regression tests/neighbor_lattice_regression.cpp)
target_link_libraries(neighbor_lattice_regression PRIVATE vumat_pd)
if(BUILD_TESTING)
  add_test(NAME fem_h8_patch COMMAND jc_3d_fem --selftest)
  add_test(NAME neighbor_lattice_regression COMMAND neighbor_lattice_regression)
  configure_vumat_test(fem_h8_patch)
  configure_vumat_test(neighbor_lattice_regression)
  set_tests_properties(corotational_kinematics neighbor_search rigid_wall_contact
    affine_bar_elastic affine_shear_elastic jc_vumat_material_point
    fem_h8_patch neighbor_lattice_regression
    PROPERTIES ENVIRONMENT "OMP_NUM_THREADS=2;OMP_DYNAMIC=FALSE")
endif()

install(TARGETS vumat_pd vumat_pd_run elastic_vumat_plugin jc_vumat_plugin jc_2d_fem jc_3d_fem)
install(DIRECTORY include/ DESTINATION include)
