cmake_minimum_required(VERSION 3.12.1)

project(3DApps)

SET(CMAKE_CXX_STANDARD 14)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_INCLUDE_CURRENT_DIR_IN_INTERFACE ON)

set(MAJOR_VERSION 1)
set(MINOR_VERSION 0)
set(REVISION_VERSION 0)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/CMakeModules")

find_package(ArgumentViewer 1.0 CONFIG REQUIRED)
find_package(BasicCamera 1.0 CONFIG REQUIRED)
find_package(Simple3DApp 1.0.0 CONFIG REQUIRED)
find_package(Vars CONFIG REQUIRED)
find_package(assimp CONFIG REQUIRED)

find_package(FreeImage REQUIRED)
find_package(FreeImagePlus REQUIRED)

set(SOURCES
  src/Barrier.cpp
  src/imguiVars.cpp
  src/DrawGrid.cpp
  )

set(INCLUDES
  src/Barrier.h
  src/imguiVars.h
  src/DrawGrid.h
  )

add_library(commonSources OBJECT ${SOURCES} ${INCLUDES})
target_include_directories(commonSources PUBLIC $<INSTALL_INTERFACE:include>)
target_include_directories(commonSources PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>)
target_link_libraries(commonSources PUBLIC Simple3DApp::Simple3DApp BasicCamera::BasicCamera ArgumentViewer::ArgumentViewer assimp::assimp FreeImage::FreeImage FreeImagePlus::FreeImagePlus Vars::Vars -lstdc++fs)

function(addApp name srcs)
  add_executable(${name} ${srcs})
  target_include_directories(${name} PUBLIC $<INSTALL_INTERFACE:include>)
  target_include_directories(${name} PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>)
  target_link_libraries(${name} PUBLIC commonSources)
endfunction()

addApp(emptyProject src/emptyProject.cpp)
addApp(rasterizationOrder src/rasterizationOrder.cpp)
addApp(gameOfLife src/gameOfLife.cpp)
addApp(noise src/noise.cpp)
addApp(spirv src/spirv.cpp)
addApp(sort src/Sort.cpp)

#function(getSharedLibraries out lib cfg)
#  #message("getSharedLibraries(${lib} ${cfg})")
#
#  if(TARGET ${lib})
#    get_target_property(target_type ${lib} TYPE)
#    if(${target_type} STREQUAL "INTERFACE_LIBRARY")
#      #message("${lib} JE ${target_type}")
#      return()
#    endif()
#  else()
#    #message("${lib} NENI TARGET")
#    return()
#  endif()
#
#
#  get_target_property(dll ${lib} IMPORTED_LOCATION_${cfg})
#  list(APPEND dlls ${dll})
#
#  get_target_property(interfaceLibs ${lib} INTERFACE_LINK_LIBRARIES)
#  if(NOT "${interfaceLibs}" STREQUAL "interfaceLibs-NOTFOUND")
#    foreach(interfaceLib ${interfaceLibs})
#      getSharedLibraries(ilibs ${interfaceLib} ${cfg})
#      list(APPEND dlls ${ilibs})
#    endforeach()
#  endif()
#  list(REMOVE_DUPLICATES dlls)
#  set(${out} ${dlls} PARENT_SCOPE)
#endfunction()
#
#
#function(getAllSharedLibraries allLibraries app cfg)
#  get_target_property(libs ${app} LINK_LIBRARIES)
#  foreach(lib ${libs})
#    getSharedLibraries(libList ${lib} ${cfg})
#    #message("${lib} ###### ${libList}")
#    list(APPEND allLibs ${libList})
#  endforeach()
#  list(REMOVE_DUPLICATES allLibs)
#  set(${allLibraries} ${allLibs} PARENT_SCOPE)
#endfunction()
#
#getAllSharedLibraries(allDebugSharedLibraries ${PROJECT_NAME} DEBUG)
#getAllSharedLibraries(allReleaseSharedLibraries ${PROJECT_NAME} RELEASE)
#
##message("SharedDebug  : ${allDebugSharedLibraries}")
##message("SharedRelease: ${allReleaseSharedLibraries}")
#
#if(MSVC)
#  foreach(lib ${allDebugSharedLibraries})
#    file(COPY ${lib} DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/Debug)
#  endforeach()
#  foreach(lib ${allReleaseSharedLibraries})
#    file(COPY ${lib} DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/Release)
#  endforeach()
#endif() 
#
#install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION .)
#install(FILES ${allDebugSharedLibraries} DESTINATION .)
