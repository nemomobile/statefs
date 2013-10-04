set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99")
set(SRC main.c filewatcher.c)
add_executable(change-notifier ${SRC})
