add_library(test MODULE test-provider.c)

#find_package(Threads)

# target_link_libraries(power
#   ${CMAKE_THREAD_LIBS_INIT}
# )

if(CONTEXTKIT)
if(${USEQT} STREQUAL "4")

find_package(Qt4 4.7.0 REQUIRED)
include(${QT_USE_FILE})
add_definitions(-DQT_SHARED)
include_directories(${QT_INCLUDE_DIR})
set(SUBSCRIBER_LIB contextkit-statefs-qt4)

elseif(${USEQT} STREQUAL "5")

find_package(Qt5Core REQUIRED)
include_directories(${Qt5Core_INCLUDE_DIRS})
add_definitions(${Qt5Core_DEFINITIONS})
set(SUBSCRIBER_LIB contextkit-statefs)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${Qt5Core_EXECUTABLE_COMPILE_FLAGS}")

endif(${USEQT} STREQUAL "4")

add_executable(atest-statefs_contextkit_subscriber_linking
  statefs_contextkit_subscriber_linking.cpp)
target_link_libraries(atest-statefs_contextkit_subscriber_linking
  ${QT_QTCORE_LIBRARY}
  ${SUBSCRIBER_LIB}
)

endif(CONTEXTKIT)
