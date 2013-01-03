PROJECT(statefs)
cmake_minimum_required(VERSION 2.6)

find_package(PkgConfig)

set(Boost_USE_MULTITHREADED ON)
set(Boost_FILESYSTEM_LIBRARY ADVANCED)
find_package(Boost COMPONENTS filesystem system REQUIRED)

pkg_check_modules(FUSE fuse REQUIRED)

set(CMAKE_CXX_FLAGS
  "${CMAKE_CXX_FLAGS} -Wall -O2 -feliminate-unused-debug-types -std=c++0x -DFUSE_USE_VERSION=28 -DDEBUG"
  )
add_definitions(${FUSE_CFLAGS})

set(CMAKE_C_FLAGS
  "${CMAKE_C_FLAGS} -Wall -O2 -feliminate-unused-debug-types -DDEBUG"
  )

# fix for gcc 4.6 specific compiling/linking issues, no dwarf-4
set(CMAKE_CXX_FLAGS
  "${CMAKE_CXX_FLAGS} -gdwarf-3"
  )

add_subdirectory(cor)

INCLUDE_DIRECTORIES(
  ${BOOST_INCLUDE_DIR}
  ${FUSE_INCLUDE_DIRS}
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/cor/include
  ${CMAKE_CURRENT_SOURCE_DIR}/metafuse/include
)

link_directories(
  ${BOOST_LIBRARY_DIRS}
  ${FUSE_LIBRARY_DIRS}
)

#add_executable(test test.cpp)

# target_link_libraries(test
#   ${FUSE_LIBRARIES}
# )

add_executable(statefs server.cpp config.cpp introspect.cpp)

target_link_libraries(statefs
  cor
  ${Boost_FILESYSTEM_LIBRARY}
  ${Boost_SYSTEM_LIBRARY}
  ${FUSE_LIBRARIES}
  -ldl
)

add_subdirectory(examples/src)
add_subdirectory(tools/introspect)
add_subdirectory(tools/poller)
