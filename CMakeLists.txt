PROJECT(statefs)
cmake_minimum_required(VERSION 2.8)

option(ENABLE_USER_SESSION "Enable user session support" OFF)
option(ENABLE_ONESHOT "If oneshot can be used" OFF)

find_package(PkgConfig)
set(prefix ${CMAKE_INSTALL_PREFIX})
find_package(Cor)

set(STATEFS_LIB_DIR ${prefix}/${DST_LIB}/statefs)

if(NOT DEFINED SYS_CONFIG_DIR)
  set(SYS_CONFIG_DIR "/etc/sysconfig/statefs")
endif()
if (NOT DEFINED SYSTEMD_UNIT_DIR)
  set(SYSTEMD_UNIT_DIR "${prefix}/lib/systemd/system")
endif()
if(NOT DEFINED STATEFS_UMASK)
  set(STATEFS_UMASK "0002")
endif()
if(NOT DEFINED STATEFS_GROUP)
  set(STATEFS_UMASK "wheel")
  message(WARNING "Setting wheel as default statefs group")
endif()
if(ENABLE_ONESHOT)
  set(ONESHOT_CMD "add-oneshot --now ")
  set(ONESHOT_DIR ${prefix}/${DST_LIB}/oneshot.d)
else()
  set(ONESHOT_CMD ${STATEFS_LIB_DIR}/)
  set(ONESHOT_DIR ${STATEFS_LIB_DIR})
endif()

set(Boost_USE_MULTITHREADED ON)
set(Boost_FILESYSTEM_LIBRARY ADVANCED)
find_package(Boost COMPONENTS filesystem system REQUIRED)

pkg_check_modules(FUSE fuse REQUIRED)
pkg_check_modules(COR cor REQUIRED)

#fuse version
set(CMAKE_CXX_FLAGS
  "${CMAKE_CXX_FLAGS} -DFUSE_USE_VERSION=28")

set(CMAKE_CXX_FLAGS
  "${CMAKE_CXX_FLAGS} -DSTATEFS_VERSION=${VERSION} -DSTATEFS_LIB_DIR=${STATEFS_LIB_DIR} -DUSE_XATTR -D_GNU_SOURCE")

add_definitions(${FUSE_CFLAGS})

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
install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/doc/html DESTINATION share/doc/statefs)

add_subdirectory(src)
add_subdirectory(examples/src)
add_subdirectory(packaging)
add_subdirectory(tests)
add_subdirectory(include/statefs)
add_subdirectory(tools/poller)