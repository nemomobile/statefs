add_executable(statefs server.cpp config.cpp util.cpp)

target_link_libraries(statefs
  cor
  ${Boost_FILESYSTEM_LIBRARY}
  ${Boost_SYSTEM_LIBRARY}
  ${FUSE_LIBRARIES}
  -ldl
)
