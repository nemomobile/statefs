find_package(Qt4 4.7.0 REQUIRED)
find_package(PkgConfig)

pkg_check_modules(CONTEXTSUBSCRIBER contextsubscriber-1.0)
pkg_check_modules(CONTEXTPROVIDER contextprovider-1.0)

include(${QT_USE_FILE})

#-Wno-psabi is to remove next g++ warning/note:
#the mangling of 'va_list' has changed in GCC 4.4
set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -Wno-psabi -std=c++0x")

# using offsetof to "cast" members to wrapper
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-invalid-offsetof")

include_directories(
  ${QT_INCLUDE_DIR}
  ${CONTEXTSUBSCRIBER_INCLUDE_DIRS}
  ${CONTEXTPROVIDER_INCLUDE_DIRS}
)

link_directories(
  ${CONTEXTSUBSCRIBER_LIBRARY_DIRS}
  ${CONTEXTPROVIDER_LIBRARY_DIRS}
)
#set(QT4_AUTOMOC ON)

add_definitions(-DQT_SHARED)

set(SRC bridge.cpp util.cpp)
set(HDRS bridge.hpp)
qt4_wrap_cpp(MOC_SRC ${HDRS})

#add_executable(bridge ${SRC} ${MOC_SRC})
add_library(ckit_bridge SHARED ${SRC} ${MOC_SRC})

set(LIB_HDRS property.hpp)
qt4_wrap_cpp(LIB_MOC_SRC ${LIB_HDRS})
add_library(ckit_statefs SHARED property.cpp ${LIB_MOC_SRC})

target_link_libraries(ckit_bridge
  ${QT_QTCORE_LIBRARY}
  ${QT_QTXML_LIBRARY}
  ${CONTEXTSUBSCRIBER_LIBRARIES}
  ${CONTEXTPROVIDER_LIBRARIES}
)

target_link_libraries(ckit_statefs
  ${QT_QTCORE_LIBRARY}
  ${QT_QTXML_LIBRARY}
)

install(TARGETS ckit_statefs DESTINATION lib${LIB_SUFFIX})