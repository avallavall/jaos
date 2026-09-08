cmake_minimum_required(VERSION 3.21)

file(STRINGS include/jaos.h JAOS_VERSION_LINE
     REGEX "^#define JAOS_VERSION_STRING \"[0-9.]+\"$")
string(REGEX REPLACE ".*\"([0-9.]+)\".*" "\\1" JAOS_VERSION
       "${JAOS_VERSION_LINE}")

project(jaos
        VERSION ${JAOS_VERSION}
        DESCRIPTION "Just Another Optimization Solver"
        HOMEPAGE_URL "https://github.com/avallavall/jaos"
        LANGUAGES C)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(CheckIPOSupported)

option(JAOS_BUILD_SHARED "build the shared library beside the archive" ON)
option(JAOS_BUILD_CLI "build the jaos tool" ON)
option(JAOS_BUILD_TESTS "build the unit suite for ctest"
       ${PROJECT_IS_TOP_LEVEL})
option(JAOS_LTO "link-time optimisation in Release" ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "build type" FORCE)
endif()

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(JAOS_SOURCES
    src/alloc.c
    src/barrier.c
    src/check.c
    src/chol.c
    src/deflate.c
    src/exact.c
    src/iis.c
    src/inflate.c
    src/lpfmt.c
    src/lu.c
    src/mip.c
    src/model.c
    src/mps.c
    src/nl.c
    src/symmetry.c
    src/options.c
    src/presolve.c
    src/proof.c
    src/ranging.c
    src/relax.c
    src/scale.c
    src/simplex.c
    src/status.c
    src/sys.c
    src/util.c
    src/verify.c
    src/version.c
    src/write.c)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  set(JAOS_C_FLAGS -Wall -Wextra -Wpedantic -Werror -ffp-contract=off -g)
  set(JAOS_DEV_FLAGS -Og -UNDEBUG)
elseif(MSVC)
  set(JAOS_C_FLAGS /W4 /fp:precise)
  set(JAOS_DEV_FLAGS /Od /UNDEBUG)
else()
  set(JAOS_C_FLAGS)
  set(JAOS_DEV_FLAGS)
endif()

set(JAOS_LIBM $<$<NOT:$<PLATFORM_ID:Windows>>:m>)

set(JAOS_IPO OFF)
if(JAOS_LTO)
  check_ipo_supported(RESULT JAOS_IPO LANGUAGES C)
endif()

function(jaos_shipping_target target)
  target_compile_options(${target} PRIVATE ${JAOS_C_FLAGS})
  if(JAOS_IPO)
    set_property(TARGET ${target}
                 PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
  endif()
endfunction()

add_library(jaos STATIC ${JAOS_SOURCES})
add_library(jaos::jaos ALIAS jaos)
target_include_directories(jaos PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(jaos PUBLIC ${JAOS_LIBM})
set_target_properties(jaos PROPERTIES OUTPUT_NAME jaos)
if(MSVC)
  set_target_properties(jaos PROPERTIES OUTPUT_NAME jaos_static)
endif()
jaos_shipping_target(jaos)

set(JAOS_INSTALL_TARGETS jaos)

if(JAOS_BUILD_SHARED)
  add_library(jaos_shared SHARED ${JAOS_SOURCES})
  add_library(jaos::shared ALIAS jaos_shared)
  target_include_directories(jaos_shared PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
      $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
  target_link_libraries(jaos_shared PUBLIC ${JAOS_LIBM})
  set_target_properties(jaos_shared PROPERTIES
      OUTPUT_NAME jaos
      EXPORT_NAME shared
      VERSION ${PROJECT_VERSION}
      SOVERSION ${PROJECT_VERSION_MAJOR}
      WINDOWS_EXPORT_ALL_SYMBOLS ON)
  jaos_shipping_target(jaos_shared)
  list(APPEND JAOS_INSTALL_TARGETS jaos_shared)
endif()

if(JAOS_BUILD_CLI)
  add_executable(jaos_cli cli/jaos.c)
  add_executable(jaos::cli ALIAS jaos_cli)
  set_target_properties(jaos_cli PROPERTIES OUTPUT_NAME jaos EXPORT_NAME cli)
  target_link_libraries(jaos_cli PRIVATE jaos)
  jaos_shipping_target(jaos_cli)
  list(APPEND JAOS_INSTALL_TARGETS jaos_cli)
endif()

if(JAOS_BUILD_TESTS)
  enable_testing()

  add_library(jaos_dev OBJECT ${JAOS_SOURCES})
  target_include_directories(jaos_dev PUBLIC include)
  target_compile_options(jaos_dev PRIVATE ${JAOS_C_FLAGS} ${JAOS_DEV_FLAGS})

  add_library(jaos_unity STATIC tests/vendor/unity/unity.c)
  target_include_directories(jaos_unity PUBLIC tests/vendor/unity)
  target_compile_definitions(jaos_unity PUBLIC UNITY_INCLUDE_DOUBLE)
  target_compile_options(jaos_unity PRIVATE ${JAOS_DEV_FLAGS})

  file(GLOB JAOS_TEST_SOURCES CONFIGURE_DEPENDS tests/test_*.c)
  foreach(test_source IN LISTS JAOS_TEST_SOURCES)
    get_filename_component(test_name ${test_source} NAME_WE)
    add_executable(${test_name} ${test_source} $<TARGET_OBJECTS:jaos_dev>)
    target_include_directories(${test_name} PRIVATE include src)
    target_link_libraries(${test_name} PRIVATE jaos_unity ${JAOS_LIBM})
    target_compile_options(${test_name} PRIVATE
        ${JAOS_C_FLAGS} ${JAOS_DEV_FLAGS})
    add_test(NAME ${test_name} COMMAND ${test_name}
             WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
  endforeach()

  if(JAOS_BUILD_CLI AND NOT WIN32)
    add_test(NAME cli
             COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli.sh
                     $<TARGET_FILE:jaos_cli>
             WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
  endif()
endif()

install(TARGETS ${JAOS_INSTALL_TARGETS}
        EXPORT jaosTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES include/jaos.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

set(JAOS_CMAKE_DIR ${CMAKE_INSTALL_LIBDIR}/cmake/jaos)
install(EXPORT jaosTargets
        NAMESPACE jaos::
        FILE jaosTargets.cmake
        DESTINATION ${JAOS_CMAKE_DIR})
configure_package_config_file(cmake/jaosConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/jaosConfig.cmake
    INSTALL_DESTINATION ${JAOS_CMAKE_DIR})
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/jaosConfigVersion.cmake
    COMPATIBILITY SameMajorVersion)
install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/jaosConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/jaosConfigVersion.cmake
        DESTINATION ${JAOS_CMAKE_DIR})

configure_file(cmake/jaos.pc.in ${CMAKE_CURRENT_BINARY_DIR}/jaos.pc @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/jaos.pc
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
