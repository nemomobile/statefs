set(TESTS_DIR /opt/tests/statefs)
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

