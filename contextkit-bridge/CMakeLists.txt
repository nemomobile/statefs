#set(QT_QMAKE_EXECUTABLE /usr/bin/qmake)
if(${USEQT} STREQUAL "4")

find_package(Qt4 4.7.0 REQUIRED)
include(${QT_USE_FILE})
add_definitions(-DQT_SHARED)
include_directories(${QT_INCLUDE_DIR})

elseif(${USEQT} STREQUAL "5")

find_package(Qt5Core REQUIRED)
include_directories(${Qt5Core_INCLUDE_DIRS})
add_definitions(${Qt5Core_DEFINITIONS})

endif(${USEQT} STREQUAL "4")

#-Wno-psabi is to remove next g++ warning/note:
#the mangling of 'va_list' has changed in GCC 4.4
set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -Wno-psabi -std=c++0x")

# using offsetof to "cast" members to wrapper
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-invalid-offsetof")

if(${USEQT} STREQUAL "4")

find_package(PkgConfig REQUIRED)
pkg_check_modules(CONTEXTSUBSCRIBER contextsubscriber-1.0)
pkg_check_modules(CONTEXTPROVIDER contextprovider-1.0)

include_directories(
  ${CONTEXTSUBSCRIBER_INCLUDE_DIRS}
  ${CONTEXTPROVIDER_INCLUDE_DIRS}
)

link_directories(
  ${CONTEXTSUBSCRIBER_LIBRARY_DIRS}
  ${CONTEXTPROVIDER_LIBRARY_DIRS}
)

set(SRC bridge.cpp util.cpp QCoreAppWrapper.cpp provider.cpp)
set(HDRS bridge.hpp QCoreAppWrapper.hpp)
qt4_wrap_cpp(BRIDGE_MOC_SRC ${HDRS})

add_library(statefs-provider-contextkit
  SHARED
  ${SRC} ${BRIDGE_MOC_SRC}
)

# dirty hack: absolute path, moc is $$$
qt4_wrap_cpp(LIB_MOC_SRC /usr/include/contextsubscriber/contextproperty.h property.hpp)

target_link_libraries(statefs-provider-contextkit
  ${QT_QTCORE_LIBRARY}
  ${QT_QTXML_LIBRARY}
  ${CONTEXTSUBSCRIBER_LIBRARIES}
  ${CONTEXTPROVIDER_LIBRARIES}
)

install(TARGETS statefs-provider-contextkit DESTINATION ${DST_LIB})
install(FILES statefs.pc DESTINATION ${DST_LIB}/pkgconfig)
install(FILES statefs-cpp.pc DESTINATION ${DST_LIB}/pkgconfig)

add_library(contextkit-statefs-qt4
  SHARED property.cpp util.cpp
  ${LIB_MOC_SRC}
)

target_link_libraries(contextkit-statefs-qt4
  ${QT_QTCORE_LIBRARY}
)

install(TARGETS contextkit-statefs-qt4 DESTINATION ${DST_LIB})
install(FILES contextkit-statefs-qt4.pc DESTINATION ${DST_LIB}/pkgconfig)

elseif(${USEQT} STREQUAL "5")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${Qt5Core_EXECUTABLE_COMPILE_FLAGS}")

qt5_wrap_cpp(LIB_MOC_SRC ${CMAKE_SOURCE_DIR}/include/contextproperty.h)
# to avoid clashing with qt4 version when building in the same dir
qt5_generate_moc(property.hpp moc_property_qt5.cpp)

add_library(contextkit-statefs
  SHARED property.cpp util.cpp moc_property_qt5.cpp
  ${LIB_MOC_SRC}
)

target_link_libraries(contextkit-statefs
  ${Qt5Core_LIBRARIES}
)

install(TARGETS contextkit-statefs DESTINATION ${DST_LIB})
install(FILES contextkit-statefs.pc DESTINATION ${DST_LIB}/pkgconfig)

endif(${USEQT} STREQUAL "4")

#add_executable(testprop testprop.cpp prov.cpp)

#add_executable(test2 testimpl.cpp provider_impl.cpp)
