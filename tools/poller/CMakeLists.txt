set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99")
set(SRC poller.c)
add_executable(poller ${SRC})
