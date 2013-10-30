find_package(Threads)

set(POWER_SRC example_power.c)
add_library(example_power MODULE ${POWER_SRC})
target_link_libraries(example_power
  ${CMAKE_THREAD_LIBS_INIT}
)
install(TARGETS example_power DESTINATION ${DST_LIB}/statefs)

add_library(example_statefspp SHARED example_statefspp.cpp)
target_link_libraries(example_statefspp statefs-pp)
install(TARGETS example_statefspp DESTINATION ${DST_LIB}/statefs)

set(POWER_SRC basic.c)
add_library(provider_basic_example MODULE basic.c)
target_link_libraries(provider_basic_example
  ${CMAKE_THREAD_LIBS_INIT}
)
install(TARGETS provider_basic_example DESTINATION ${DST_LIB}/statefs)
