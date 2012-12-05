set(POWER_SRC power.c)
add_library(power MODULE ${POWER_SRC})

find_package(Threads)

target_link_libraries(power
  ${CMAKE_THREAD_LIBS_INIT}
)
