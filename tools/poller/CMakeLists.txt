set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99 -D_GNU_SOURCE")
set(SRC poller.c)
add_executable(poller ${SRC})
