set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99 -Werror -Wall")
set(SRC main.c filewatcher.c filecontents.c)
add_executable(change-notifier ${SRC})
