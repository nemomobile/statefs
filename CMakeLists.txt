PROJECT(statefs)
cmake_minimum_required(VERSION 2.6)

IF(CMAKE_SIZEOF_VOID_P EQUAL 4)
  SET(LIB_SUFFIX "")
ELSE(CMAKE_SIZEOF_VOID_P EQUAL 4)
  SET(LIB_SUFFIX 64)
ENDIF(CMAKE_SIZEOF_VOID_P EQUAL 4)

set(DST_LIB lib${LIB_SUFFIX})

find_package(PkgConfig)

set(Boost_USE_MULTITHREADED ON)
set(Boost_FILESYSTEM_LIBRARY ADVANCED)
find_package(Boost COMPONENTS filesystem system REQUIRED)

pkg_check_modules(FUSE fuse REQUIRED)
pkg_check_modules(COR cor REQUIRED)

set(CMAKE_CXX_FLAGS
  "${CMAKE_CXX_FLAGS} -Wall -O2 -feliminate-unused-debug-types -std=c++0x -DFUSE_USE_VERSION=28"
  )
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

add_custom_target(provider-doc
  COMMAND doxygen ${CMAKE_CURRENT_SOURCE_DIR}/doc/provider.Doxyfile)

add_subdirectory(src)
add_subdirectory(examples/src)
add_subdirectory(packaging)
add_subdirectory(tests)

install(DIRECTORY include/statefs DESTINATION include FILES_MATCHING REGEX "\\.hp?p?$")
