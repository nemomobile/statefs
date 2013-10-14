set(TESTS_DIR /opt/tests/statefs)
configure_file(tests.xml.in tests.xml @ONLY)
add_library(test MODULE test-provider.c)
install(TARGETS test DESTINATION ${TESTS_DIR})
install(
  PROGRAMS test-statefs.py
  DESTINATION ${TESTS_DIR}
)
install(
  FILES UT.py tests.xml
  DESTINATION ${TESTS_DIR}
)
#find_package(Threads)

# target_link_libraries(power
#   ${CMAKE_THREAD_LIBS_INIT}
# )

add_executable(test-link-statefs-util link-statefs-util.cpp)
target_link_libraries(test-link-statefs-util statefs-util)

add_executable(test-link-statefs-config link-statefs-config.cpp)
target_link_libraries(test-link-statefs-config statefs-config)

add_executable(test-link-statefspp link-statefspp.cpp)
target_link_libraries(test-link-statefspp statefs-pp)
