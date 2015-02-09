testrunner_project(statefs)
add_library(test MODULE test-provider.c)
testrunner_install(
  TARGETS test
  PROGRAMS test-statefs.py
  FILES UT.py
)

add_executable(test-link-statefs-util
  link-statefs-util.cpp link-statefs-util2.cpp)
target_link_libraries(test-link-statefs-util statefs-util)

add_executable(test-link-statefs-config
  link-statefs-config.cpp link-statefs-config2.cpp)
target_link_libraries(test-link-statefs-config statefs-config)

add_executable(test-link-statefspp link-statefspp.cpp link-statefspp2.cpp)
target_link_libraries(test-link-statefspp statefs-pp)
