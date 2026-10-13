cmake_minimum_required(VERSION 3.25)

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
  $<$<COMPILE_LANGUAGE:Fortran>:-frecursive>
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

add_library(neohookean_kv_vumat_plugin SHARED
  materials/examples/NeoHookeanKV.f90
  materials/bridge/vumat_c_wrapper_jinfo.f90
  materials/bridge/abaqus_stubs.f90
)
target_include_directories(neohookean_kv_vumat_plugin PRIVATE materials/bridge)
target_compile_options(neohookean_kv_vumat_plugin PRIVATE
  $<$<COMPILE_LANGUAGE:Fortran>:-std=legacy>
  $<$<COMPILE_LANGUAGE:Fortran>:-frecursive>
)
set_target_properties(neohookean_kv_vumat_plugin PROPERTIES PREFIX "")

add_library(vumat_pd STATIC
  src/MaterialConfigJson.cpp
  src/SimpleJson.cpp
  src/SimpleDamageModel.cpp
  src/PdSimpleDamage.cpp
  src/TensorVoigtMap.cpp
  src/VumatMaterialAdapter.cpp
  src/DemoConfig.cpp
  src/BoundaryConditions.cpp
  src/ParticleSets.cpp
  src/Geometry.cpp
  src/DemoCaseRunner.cpp
  src/MaterialPointCaseRunner.cpp
  src/BoundaryValueCaseRunner.cpp
  src/ExplicitTimeIntegrator.cpp
  src/CorotationalKinematics.cpp
  src/PdCorrespondenceSkeleton.cpp
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

# Parallel material blocks require reentrant user material routines.
find_package(OpenMP REQUIRED COMPONENTS CXX)
target_link_libraries(vumat_pd PUBLIC OpenMP::OpenMP_CXX)
target_compile_options(jc_vumat_plugin PRIVATE
  $<$<COMPILE_LANGUAGE:Fortran>:-frecursive>)

add_executable(vumat_pd_large_motion_3d src/main_large_motion_3d.cpp)
target_link_libraries(vumat_pd_large_motion_3d PRIVATE vumat_pd)

if(BUILD_TESTING)
  # Verification executables do not belong to a PD-only runtime build.
  add_executable(corotational_kinematics_test tests/corotational_kinematics_test.cpp)
  target_link_libraries(corotational_kinematics_test PRIVATE vumat_pd)
  add_executable(neighbor_search_test tests/neighbor_search_test.cpp)
  target_link_libraries(neighbor_search_test PRIVATE vumat_pd)
  add_executable(neohookean_material_test tests/neohookean_material_test.cpp)
  target_link_libraries(neohookean_material_test PRIVATE vumat_pd)
  add_executable(neighbor_lattice_regression tests/neighbor_lattice_regression.cpp)
  target_link_libraries(neighbor_lattice_regression PRIVATE vumat_pd)
  add_executable(outer_damage_test tests/outer_damage_test.cpp)
  target_link_libraries(outer_damage_test PRIVATE vumat_pd)
  add_executable(jc_3d_fem benchmarks/tensile3d/src/main_jc_3d_fem.cpp)
  target_link_libraries(jc_3d_fem PRIVATE vumat_pd)

  add_test(NAME corotational_kinematics COMMAND corotational_kinematics_test)
  add_test(NAME neighbor_search COMMAND neighbor_search_test)
  add_test(NAME affine_bar_elastic COMMAND vumat_pd_run examples/affine_bar_elastic.json)
  add_test(NAME affine_shear_elastic COMMAND vumat_pd_run examples/affine_shear_elastic.json)
  add_test(NAME jc_vumat_material_point COMMAND vumat_pd_run examples/jc_material_point.json)
  add_test(NAME fem_h8_patch COMMAND jc_3d_fem --selftest)
  add_test(NAME neighbor_lattice_regression COMMAND neighbor_lattice_regression)
  add_test(NAME outer_damage COMMAND outer_damage_test)
  add_test(NAME neohookean_material COMMAND neohookean_material_test
    configs/neohookean.json demos/output/hyperelastic/material-point-check)
  add_test(NAME hyperelastic_bending COMMAND vumat_pd_large_motion_3d
    bending demos/output/hyperelastic/bending 2400 24 150)
  add_test(NAME hyperelastic_torsion COMMAND vumat_pd_large_motion_3d
    torsion demos/output/hyperelastic/torsion 2400 24 540)
  add_test(NAME elastic_pd_tension COMMAND vumat_pd_run examples/elastic_tension_pd.json)
  add_test(NAME jc_pd_tension COMMAND vumat_pd_run examples/jc_tension_pd.json)

  get_filename_component(VUMAT_FORTRAN_RUNTIME_DIR "${CMAKE_Fortran_COMPILER}" DIRECTORY)
  set_tests_properties(corotational_kinematics neighbor_search affine_bar_elastic
    affine_shear_elastic jc_vumat_material_point fem_h8_patch neighbor_lattice_regression outer_damage
    neohookean_material hyperelastic_bending hyperelastic_torsion elastic_pd_tension jc_pd_tension
    PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
      ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${VUMAT_FORTRAN_RUNTIME_DIR}"
      ENVIRONMENT "OMP_NUM_THREADS=2;OMP_DYNAMIC=FALSE")
  set_tests_properties(hyperelastic_bending hyperelastic_torsion PROPERTIES TIMEOUT 120)
  set_tests_properties(elastic_pd_tension jc_pd_tension PROPERTIES TIMEOUT 180)
endif()

install(TARGETS vumat_pd vumat_pd_run elastic_vumat_plugin jc_vumat_plugin
  neohookean_kv_vumat_plugin vumat_pd_large_motion_3d)
install(DIRECTORY include/ DESTINATION include)
