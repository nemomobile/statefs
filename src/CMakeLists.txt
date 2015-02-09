configure_file(misc.hpp.in misc.hpp @ONLY)

add_library(statefs-config SHARED
  config.cpp
  util.cpp
)

SET_TARGET_PROPERTIES(
  statefs-config
  PROPERTIES
  SOVERSION 0
  VERSION ${VERSION}
  )

target_link_libraries(statefs-config
  ${COR_LIBRARIES}
  ${CMAKE_DL_LIBS}
  ${Boost_FILESYSTEM_LIBRARY}
  ${Boost_SYSTEM_LIBRARY}
  )

add_executable(statefs server.cpp)

target_link_libraries(statefs
  statefs-config
  ${COR_LIBRARIES}
  ${Boost_FILESYSTEM_LIBRARY}
  ${Boost_SYSTEM_LIBRARY}
  ${FUSE_LIBRARIES}
)

install(TARGETS statefs DESTINATION bin)
install(TARGETS statefs-config DESTINATION ${DST_LIB})

add_library(statefs-pp
  SHARED provider.cpp property.cpp
)

SET_TARGET_PROPERTIES(
  statefs-pp
  PROPERTIES
  SOVERSION 0
  VERSION ${VERSION}
  )

target_link_libraries(statefs-pp
  ${COR_LIBRARIES}
)

add_library(statefs-util
  SHARED common_util.cpp consumer.cpp
)

target_link_libraries(statefs-util
  ${COR_LIBRARIES}
)

SET_TARGET_PROPERTIES(
  statefs-util
  PROPERTIES
  SOVERSION 0
  VERSION ${VERSION}
  )

add_library(loader-default
  SHARED loader.cpp
)

target_link_libraries(loader-default
  ${COR_LIBRARIES}
  ${CMAKE_DL_LIBS}
)

add_library(loader-inout
  SHARED inout.cpp
)

target_link_libraries(loader-inout
  statefs-config
  statefs-pp
  ${COR_LIBRARIES}
)

install(TARGETS statefs-pp DESTINATION ${DST_LIB})
install(TARGETS statefs-util DESTINATION ${DST_LIB})
install(TARGETS loader-default DESTINATION ${DST_LIB}/statefs)
install(TARGETS loader-inout DESTINATION ${DST_LIB}/statefs)

configure_install_pkgconfig(statefs)
configure_install_pkgconfig(statefs-cpp)
configure_install_pkgconfig(statefs-util)
