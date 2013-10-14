add_library(statefs-config SHARED
  config.cpp
  util.cpp
)

SET_TARGET_PROPERTIES(
  statefs-config
  PROPERTIES
  SOVERSION 0
  VERSION ${STATEFS_VERSION}
  )

target_link_libraries(statefs-config
  ${COR_LIBRARIES}
  ${Boost_FILESYSTEM_LIBRARY}
  )

add_executable(statefs server.cpp)

target_link_libraries(statefs
  statefs-config
  ${COR_LIBRARIES}
  ${Boost_FILESYSTEM_LIBRARY}
  ${Boost_SYSTEM_LIBRARY}
  ${FUSE_LIBRARIES}
  -ldl
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
  VERSION ${STATEFS_VERSION}
  )

target_link_libraries(statefs-pp
  ${COR_LIBRARIES}
)

add_library(statefs-util
  SHARED common_util.cpp consumer.cpp
)

SET_TARGET_PROPERTIES(
  statefs-util
  PROPERTIES
  SOVERSION 0
  VERSION ${STATEFS_VERSION}
  )

add_library(loader-default
  SHARED loader.cpp
)

target_link_libraries(loader-default
  ${COR_LIBRARIES}
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

configure_file(statefs.pc.in statefs.pc @ONLY)
configure_file(statefs-cpp.pc.in statefs-cpp.pc @ONLY)
configure_file(statefs-util.pc.in statefs-util.pc @ONLY)
install(FILES statefs.pc DESTINATION ${DST_LIB}/pkgconfig)
install(FILES statefs-cpp.pc DESTINATION ${DST_LIB}/pkgconfig)
install(FILES statefs-util.pc DESTINATION ${DST_LIB}/pkgconfig)
