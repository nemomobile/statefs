set(POWER_SRC example_power.c)
add_library(example_power MODULE ${POWER_SRC})

find_package(Threads)

target_link_libraries(example_power
  ${CMAKE_THREAD_LIBS_INIT}
)
