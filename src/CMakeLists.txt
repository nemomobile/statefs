add_executable(statefs server.cpp config.cpp util.cpp)

target_link_libraries(statefs
  ${COR_LIBRARIES}
  ${Boost_FILESYSTEM_LIBRARY}
  ${Boost_SYSTEM_LIBRARY}
  ${FUSE_LIBRARIES}
  -ldl
)

install(TARGETS statefs DESTINATION bin)

add_library(statefspp
  SHARED provider.cpp
)

target_link_libraries(statefspp
  ${COR_LIBRARIES}
)

install(TARGETS statefspp DESTINATION ${DST_LIB})

configure_file(statefs.pc.in statefs.pc @ONLY)
configure_file(statefs-cpp.pc.in statefs-cpp.pc @ONLY)
install(FILES statefs.pc DESTINATION ${DST_LIB}/pkgconfig)
install(FILES statefs-cpp.pc DESTINATION ${DST_LIB}/pkgconfig)
