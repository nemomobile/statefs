set(SRC introspect.cpp)
add_executable(introspect ${SRC})

find_package(Threads)

target_link_libraries(introspect
  ${CMAKE_THREAD_LIBS_INIT}
  -ldl
)
