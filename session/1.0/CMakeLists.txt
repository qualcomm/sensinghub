#  @file    CMakeLists.txt
#  @brief   This file is being used for compiling sensing hub modules
#           with the help of cmake
#

add_library(sensinghubsession SHARED  ${CMAKE_CURRENT_SOURCE_DIR}/src/SessionFactory.cpp)

target_include_directories(sensinghubsession PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/inc>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../common/inc/>
  $<INSTALL_INTERFACE:include>
)

target_add_all_options_modes(sensinghubsession)

install(TARGETS sensinghubsession)
