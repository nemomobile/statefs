PROJECT(statefs)
cmake_minimum_required(VERSION 2.6)

option(ENABLE_MULTIARCH "Enable support for multi-arch distros (lib64)" OFF)
IF(ENABLE_MULTIARCH)
  IF(CMAKE_SIZEOF_VOID_P EQUAL 4)
    SET(LIB_SUFFIX "")
  ELSE(CMAKE_SIZEOF_VOID_P EQUAL 4)
    SET(LIB_SUFFIX 64)
  ENDIF(CMAKE_SIZEOF_VOID_P EQUAL 4)
ELSE()
  SET(LIB_SUFFIX "")
ENDIF()

set(DST_LIB lib${LIB_SUFFIX})
set(prefix ${CMAKE_INSTALL_PREFIX})

find_package(PkgConfig)

set(Boost_USE_MULTITHREADED ON)
set(Boost_FILESYSTEM_LIBRARY ADVANCED)
find_package(Boost COMPONENTS filesystem system REQUIRED)

pkg_check_modules(FUSE fuse REQUIRED)
pkg_check_modules(COR cor REQUIRED)

set(CMAKE_CXX_FLAGS
  "${CMAKE_CXX_FLAGS} -Wall -O2 -feliminate-unused-debug-types -std=c++0x")

#fuse version
set(CMAKE_CXX_FLAGS
  "${CMAKE_CXX_FLAGS} -DFUSE_USE_VERSION=28")

set(STATEFS_LIB_DIR ${prefix}/${DST_LIB}/statefs)

set(CMAKE_CXX_FLAGS
  "${CMAKE_CXX_FLAGS} -DSTATEFS_VERSION=${STATEFS_VERSION} -DSTATEFS_LIB_DIR=${STATEFS_LIB_DIR}")

add_definitions(${FUSE_CFLAGS})

set(CMAKE_C_FLAGS
  "${CMAKE_C_FLAGS} -Wall -O2 -feliminate-unused-debug-types"
  )

# fix for gcc 4.6 specific compiling/linking issues, no dwarf-4
set(CMAKE_CXX_FLAGS
  "${CMAKE_CXX_FLAGS} -gdwarf-3"
  )

INCLUDE_DIRECTORIES(
  ${BOOST_INCLUDE_DIR}
  ${FUSE_INCLUDE_DIRS}
  ${COR_INCLUDE_DIRS}
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/metafuse/include
)

link_directories(
  ${BOOST_LIBRARY_DIRS}
  ${FUSE_LIBRARY_DIRS}
  ${COR_LIBRARY_DIRS}
)

add_custom_target(statefs-doc
  COMMAND doxygen ${CMAKE_CURRENT_SOURCE_DIR}/doc/statefs.Doxyfile)

add_subdirectory(src)
add_subdirectory(examples/src)
add_subdirectory(packaging)
add_subdirectory(tests)
add_subdirectory(include/statefs)
add_subdirectory(tools/poller)