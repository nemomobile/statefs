PROJECT(statefs)
cmake_minimum_required(VERSION 2.6)

find_package(PkgConfig)

set(Boost_USE_MULTITHREADED ON)
set(Boost_FILESYSTEM_LIBRARY ADVANCED)
find_package(Boost COMPONENTS filesystem system REQUIRED)

pkg_check_modules(FUSE fuse REQUIRED)

set(CMAKE_CXX_FLAGS 
  "${CMAKE_CXX_FLAGS} -Wall -g -O0 -std=c++0x -DFUSE_USE_VERSION=28 -DDEBUG"
  )
add_definitions(${FUSE_CFLAGS})

INCLUDE_DIRECTORIES(
  ${BOOST_INCLUDE_DIR}
  ${FUSE_INCLUDE_DIRS}
  ${CMAKE_CURRENT_SOURCE_DIR}/cor/include
)

link_directories(
  ${BOOST_LIBRARY_DIRS}
  ${FUSE_LIBRARY_DIRS}
)

#add_executable(test test.cpp)

# target_link_libraries(test
#   ${FUSE_LIBRARIES}
# )

add_executable(statefs server.cpp)

target_link_libraries(statefs
  ${Boost_FILESYSTEM_LIBRARY}
  ${Boost_SYSTEM_LIBRARY}
  ${FUSE_LIBRARIES}
  -ldl
)
