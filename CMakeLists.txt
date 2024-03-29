cmake_minimum_required(VERSION 3.12.1)

project(3DApps)
SET(CMAKE_CXX_STANDARD 20)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_INCLUDE_CURRENT_DIR_IN_INTERFACE ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS 1 ) 

set(MAJOR_VERSION 1)
set(MINOR_VERSION 0)
set(REVISION_VERSION 0)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/CMakeModules")

find_package(ArgumentViewer 1.0 CONFIG QUIET)
find_package(BasicCamera 1.0    CONFIG QUIET)
find_package(Simple3DApp 1.0.0  CONFIG QUIET)
find_package(Vars               CONFIG QUIET)
find_package(assimp             CONFIG QUIET)
find_package(StringAligner      CONFIG QUIET)
find_package(glm                CONFIG QUIET)
find_package(VarsGLMDecorator   CONFIG QUIET)
find_package(imguiVars          CONFIG QUIET)

find_package(FFMPEG        QUIET)
find_package(FreeImage     QUIET)
find_package(FreeImagePlus QUIET)
find_package(k4a           QUIET)
find_package(OpenCV        QUIET)
find_package(PCL           QUIET)
find_package(libusb-1.0    QUIET)
find_package(Threads       QUIET)
find_package(OpenCL        QUIET)

set(SOURCES
  )

set(INCLUDES
  src/Timer.h
  )

include(CMakeUtils.cmake)

createTargetForBadLibrary(cv      "${OpenCV_LIBS}"            "${OpenCV_INCLUDE_DIRS}"  )
createTargetForBadLibrary(pcl     "${PCL_LIBRARIES}"          "${PCL_INCLUDE_DIRS}"     )
createTargetForBadLibrary(libusb  "${LIBUSB_1_LIBRARY}"       "${LIBUSB_1_INCLUDE_DIRS}")
createTargetForBadLibrary(ffmpeg  "${FFMPEG_LIBRARIES}"       "${FFMPEG_INCLUDE_DIRS}"  )
createTargetForBadLibrary(Threads "${CMAKE_THREAD_LIBS_INIT}" ""                        )

addHeaderLibrary(timer            "")
addObjectLibrary(barrier          "src/Barrier.cpp;src/Barrier.h"                                               "Vars::Vars"                                                                            )
addObjectLibrary(makeShader       "src/makeShader.h;src/makeShader.cpp"                                         "geGL::geGL"                                             
                  )
addObjectLibrary(drawGrid         "src/drawGrid.cpp;src/drawGrid.h"                                             "Vars::Vars;BasicCamera::BasicCamera;barrier;makeShader"                                           )
addObjectLibrary(drawBunny        "src/drawBunny.cpp;src/drawBunny.h;src/bunny.h;src/bunny.c"                   "Vars::Vars;BasicCamera::BasicCamera;barrier"                                           )
addObjectLibrary(loadTxtFile      "src/utils/loadTxtFile.cpp;src/utils/loadTxtFile.hpp" "")
addObjectLibrary(ls               "src/utils/ls.cpp;src/utils/ls.hpp" "")
addObjectLibrary(azureKinectUtils "src/azureKinectUtils.cpp;src/azureKinectUtils.h"                             "glm;k4a::k4a"                                                                          )
addObjectLibrary(kinectPointCloud "src/kinectPointCloud.cpp;src/kinectPointCloud.h"                             "Vars::Vars;BasicCamera::BasicCamera;barrier;k4a::k4a;azureKinectUtils;imguiVars::imguiVars;pcl;cv")
addObjectLibrary(faceDetect       "src/faceDetect.cpp;src/faceDetect.h"                                         "cv;glm"                                                                                )
addObjectLibrary(drawFace         "src/drawFace.cpp;src/bunny.h;src/bunny.c"                                    "Vars::Vars;BasicCamera::BasicCamera;barrier;faceDetect;k4a::k4a"                       )
addObjectLibrary(gpuDecoder       "src/gpuDecoder.h;src/gpuDecoder.cpp"                                         "ffmpeg;Simple3DApp::Simple3DApp"                                                       )
#addApp(gameOfLife src/gameOfLife.cpp)
#addApp2(spirv "src/spirv.cpp" "")
addApp2(emptyProject                          "src/emptyProject.cpp"                          "Simple3DApp::Simple3DApp;drawGrid;imguiVars::imguiVars;VarsGLMDecorator::VarsGLMDecorator")
addApp2(splines                               "src/splines.cpp"                               "Simple3DApp::Simple3DApp;drawGrid;imguiVars::imguiVars;VarsGLMDecorator::VarsGLMDecorator")
addApp2(skybox                                "src/skybox.cpp"                                "Simple3DApp::Simple3DApp;drawGrid;imguiVars::imguiVars;VarsGLMDecorator::VarsGLMDecorator")
addApp2(svSide                                "src/svSide.cpp"                                "Simple3DApp::Simple3DApp;drawGrid;imguiVars::imguiVars;VarsGLMDecorator::VarsGLMDecorator")
addApp2(shadowVolumes                         "src/shadowVolumes.cpp"                         "Simple3DApp::Simple3DApp;drawGrid;imguiVars::imguiVars;VarsGLMDecorator::VarsGLMDecorator")
addApp2(openglTest                            "src/openglTest.cpp"                            "SDL2::SDL2;SDL2::SDL2main;geGL::geGL")
addApp2(bufferBindingTest                     "src/bufferBindingTest.cpp"                     "SDL2::SDL2;SDL2::SDL2main;geGL::geGL")
addApp2(workgroupCoop                         "src/workgroupCoop.cpp"                         "SDL2::SDL2;SDL2::SDL2main;geGL::geGL")
addApp2(heatUp                                "src/heatUp.cpp"                                "Threads")
addApp2(pbr                                   "src/pbr.cpp"                                   "Simple3DApp::Simple3DApp;drawGrid;imguiVars::imguiVars")
addApp2(drawVars                              "src/drawVars.cpp"                              "Simple3DApp::Simple3DApp;imguiVars::imguiVars")
addApp2(pathTracing                           "src/pathTracing.cpp"                           "Simple3DApp::Simple3DApp;drawGrid;imguiVars::imguiVars")
addApp2(rasterizationOrder                    "src/rasterizationOrder.cpp"                    "Simple3DApp::Simple3DApp;barrier;imguiVars::imguiVars")
addApp2(fragCoords                            "src/fragCoords.cpp"                            "Simple3DApp::Simple3DApp;barrier;imguiVars::imguiVars")
addApp2(relaxedOrder                          "src/relaxedOrder.cpp"                          "Simple3DApp::Simple3DApp;drawGrid;imguiVars::imguiVars;VarsGLMDecorator::VarsGLMDecorator")
addApp2(noise                                 "src/noise.cpp"                                 "Simple3DApp::Simple3DApp;barrier;imguiVars::imguiVars")
addApp2(uintGeometryShader                    "src/uintGeometryShader.cpp"                    "Simple3DApp::Simple3DApp;barrier;imguiVars::imguiVars")
addApp2(sort                                  "src/Sort.cpp"                                  "Simple3DApp::Simple3DApp;ArgumentViewer::ArgumentViewer;StringAligner::StringAligner;imguiVars::imguiVars")
addApp2(linearInterpolationOfHomogenousCoords "src/linearInterpolationOfHomogenousCoords.cpp" "Simple3DApp::Simple3DApp;drawGrid;imguiVars::imguiVars")
addApp2(conservativeRasterization             "src/conservativeRasterization.cpp"             "Simple3DApp::Simple3DApp;drawGrid;imguiVars::imguiVars")
addApp2(infiniteInterpolation                 "src/infiniteInterpolation.cpp"                 "Simple3DApp::Simple3DApp;drawGrid;imguiVars::imguiVars")
addApp2(csCompiler                            "src/csCompiler.cpp"                            "Simple3DApp::Simple3DApp;ArgumentViewer::ArgumentViewer")
addApp2(imagePointCloud                       "src/imagePointCloud.cpp"                       "Simple3DApp::Simple3DApp;ArgumentViewer::ArgumentViewer;drawGrid;imguiVars::imguiVars;FreeImagePlus;FreeImage")
addApp2(fragmentPlay                          "src/fragmentPlay.cpp"                          "Simple3DApp::Simple3DApp;ArgumentViewer::ArgumentViewer;barrier;imguiVars::imguiVars;FreeImagePlus;FreeImage")
addApp2(frameInterpolation                    "src/frameInterpolation.cpp"                    "Simple3DApp::Simple3DApp;ArgumentViewer::ArgumentViewer;barrier;imguiVars::imguiVars;FreeImagePlus;FreeImage")
addApp2(renderHolo                            "src/renderHolo.cpp"                            "Simple3DApp::Simple3DApp;ArgumentViewer::ArgumentViewer;drawGrid;barrier;imguiVars::imguiVars;FreeImagePlus;FreeImage;drawBunny")
addApp2(renderHoloFocus                       "src/renderHoloFocus.cpp"                       "Simple3DApp::Simple3DApp;ArgumentViewer::ArgumentViewer;drawGrid;barrier;imguiVars::imguiVars;FreeImagePlus;FreeImage;drawBunny")
addApp2(renderHoloFocusStudy                       "src/renderHoloFocusStudy.cpp;src/holoCalibration.h;src/holoCalibration.cpp"                       "Simple3DApp::Simple3DApp;ArgumentViewer::ArgumentViewer;drawGrid;barrier;imguiVars::imguiVars;FreeImagePlus;FreeImage;drawBunny;libusb")
addApp2(lf_prob_core                          "src/lf_prob_core.cpp"                          "Simple3DApp::Simple3DApp;ArgumentViewer::ArgumentViewer;barrier;imguiVars::imguiVars;FreeImagePlus;FreeImage;loadTxtFile;ls")
addApp2(renderHoloModel                       "src/renderHoloModel.cpp"                       "Simple3DApp::Simple3DApp;ArgumentViewer::ArgumentViewer;drawGrid;barrier;imguiVars::imguiVars;FreeImagePlus;FreeImage;drawBunny;assimp::assimp")
addApp2(renderHoloUserStudy                       "src/renderHoloUserStudy.cpp;src/holoCalibration.h;src/holoCalibration.cpp"                       "Simple3DApp::Simple3DApp;ArgumentViewer::ArgumentViewer;drawGrid;barrier;imguiVars::imguiVars;FreeImagePlus;FreeImage;drawBunny;assimp::assimp;libusb")
addApp2(renderHoloApps                        "src/renderHoloApps.cpp;src/holoCalibration.h;src/holoCalibration.cpp;src/json.hpp"                        "Simple3DApp::Simple3DApp;ArgumentViewer::ArgumentViewer;drawGrid;barrier;FreeImagePlus;FreeImage;drawBunny;kinectPointCloud;drawFace;libusb")
addApp2(memoryBandwidth                       "src/memoryBandwidth.cpp"                       "Simple3DApp::Simple3DApp;barrier;imguiVars::imguiVars")
addApp2(draw2d                                "src/draw2d.cpp"                                "Simple3DApp::Simple3DApp;barrier;imguiVars::imguiVars")
addApp2(morton2d                              "src/morton2d.cpp"                              "Simple3DApp::Simple3DApp;barrier;imguiVars::imguiVars;assimp::assimp")
addApp2(triangle                              "src/triangle.cpp"                              "Simple3DApp::Simple3DApp")
addApp2(killGPU                               "src/killGPU.cpp"                               "Simple3DApp::Simple3DApp;ArgumentViewer::ArgumentViewer;Vars::Vars")
addApp2(nofWG                                 "src/nofWG.cpp"                               "Simple3DApp::Simple3DApp;ArgumentViewer::ArgumentViewer;Vars::Vars")
addApp2(quiltToNative                         "src/quiltToNative.cpp"                         "Simple3DApp::Simple3DApp;ArgumentViewer::ArgumentViewer;FreeImagePlus;FreeImage;glm")
addApp2(stupidCompiler                        "src/stupidCompiler.cpp"                        "Simple3DApp::Simple3DApp;barrier;imguiVars::imguiVars;ArgumentViewer::ArgumentViewer")
addApp2(stupidScheduler                       "src/stupidScheduler.cpp"                       "SDL2::SDL2;SDL2::SDL2main;geGL::geGL;ArgumentViewer::ArgumentViewer;OpenCL::OpenCL")
addApp2(stupidSchedulerCL                     "src/stupidSchedulerCL.cpp"                     "OpenCL::OpenCL;ArgumentViewer::ArgumentViewer")
addApp2(mortonTest                            "src/mortonTest.cpp"                            "Simple3DApp::Simple3DApp;barrier;imguiVars::imguiVars;ArgumentViewer::ArgumentViewer")
addApp2(mortonVeri                            "src/mortonVeri.cpp"                            "Simple3DApp::Simple3DApp;barrier;imguiVars::imguiVars;ArgumentViewer::ArgumentViewer")
addApp2(ballotARBTest                         "src/ballotARBTest.cpp"                         "Simple3DApp::Simple3DApp;barrier;imguiVars::imguiVars;ArgumentViewer::ArgumentViewer")
addApp2(lightfieldPlayer                      "src/lightfieldPlayer.cpp"                      "Simple3DApp::Simple3DApp;barrier;imguiVars::imguiVars;ArgumentViewer::ArgumentViewer;gpuDecoder;Vars::Vars;drawGrid;BasicCamera::BasicCamera")
addApp2(tessellationLevels                    "src/tessellationLevels.cpp"                    "Simple3DApp::Simple3DApp;barrier;imguiVars::imguiVars;Vars::Vars")
addApp2(perPixelLf                               "src/perPixelLf.cpp"                         "ArgumentViewer::ArgumentViewer")
addApp2(quiltDetector                        "src/quiltDetector.cpp"                          "ArgumentViewer::ArgumentViewer;cv")
addApp2(lkgGetCalibration                    "src/lkgGetCalibration.cpp;src/holoCalibration.h;src/holoCalibration.cpp" "libusb-1.0")

add_subdirectory(src/GMU)

function(printParam target prop)
  get_target_property(lala ${target} ${prop})
  message("  ${prop} ${lala}")
endfunction()

function(getSharedLibraries out lib cfg)
  #message("getSharedLibraries(${lib} ${cfg})")

  if(TARGET ${lib})
    get_target_property(target_type ${lib} TYPE)
    if(${target_type} STREQUAL "INTERFACE_LIBRARY")
      #message("  ${lib} JE ${target_type}")
      return()
    endif()
    #message("  ${lib} ${target_type}")
  else()
    #message("  ${lib} NENI TARGET")
    return()
  endif()


  get_target_property(fullLibraryName ${lib} IMPORTED_LOCATION_${cfg})
  get_target_property(libraryName ${lib} IMPORTED_SONAME_${cfg})
  get_target_property(linkLibraries ${lib} LINK_LIBRARIES)

  #printParam(${lib} LINK_LIBRARIES)

  if(NOT "${linkLibraries}" STREQUAL "linkLibraries-NOTFOUND")
    getAllSharedLibraries(lilib ${lib} ${cfg})
    set(${out} "${lilib}" PARENT_SCOPE)
    return()
  endif()

  #message("  ${lib} ---- ${fullLibraryName} ${libraryName}")

  string(REGEX MATCH "\\.dll$|\\.so" isDll ${fullLibraryName})
  if("${isDll}" STREQUAL "")
    return()
  endif()

  set(dlls)
  list(APPEND dlls ${fullLibraryName}\\\\ ${libraryName})

  get_target_property(interfaceLibs ${lib} INTERFACE_LINK_LIBRARIES)
  if(NOT "${interfaceLibs}" STREQUAL "interfaceLibs-NOTFOUND")
    foreach(interfaceLib ${interfaceLibs})
      getSharedLibraries(ilibs ${interfaceLib} ${cfg})
      list(APPEND dlls "${ilibs}")
    endforeach()
  endif()

  #list(REMOVE_DUPLICATES dlls)
  set(${out} "${dlls}" PARENT_SCOPE)
endfunction()

function(getAllSharedLibraries allLibraries app cfg)
  get_target_property(libs ${app} LINK_LIBRARIES)
  foreach(lib ${libs})
    getSharedLibraries(libList ${lib} ${cfg})
    #message("${lib} ###### ${libList}")
    list(APPEND allLibs "${libList}")
  endforeach()
  #list(REMOVE_DUPLICATES allLibs)
  set(${allLibraries} "${allLibs}" PARENT_SCOPE)
endfunction()


function(getFinalAllSharedLibraries all app cfg)
  getAllSharedLibraries(notFiltered ${app} ${cfg})
  list(REMOVE_DUPLICATES notFiltered)
  set(${all} "${notFiltered}" PARENT_SCOPE)
endfunction()

#getFinalAllSharedLibraries(allDebugSharedLibraries noise DEBUG)
#getFinalAllSharedLibraries(allReleaseSharedLibraries noise RELEASE)

#message("SharedDebug  : ${allDebugSharedLibraries}")
#message("SharedRelease: ${allReleaseSharedLibraries}")

if(MSVC)
  foreach(lib ${allDebugSharedLibraries})
    list(GET lib 0 fullNamePath)
    file(COPY ${fullNamePath} DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/Debug)
  endforeach()
  foreach(lib ${allReleaseSharedLibraries})
    list(GET lib 0 fullNamePath)
    file(COPY ${fullNamePath} DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/Release)
  endforeach()
endif() 

if(UNIX)
  foreach(lib ${allDebugSharedLibraries})
    list(GET lib 0 fullNamePath)
    list(GET lib 1 name)
    get_filename_component(fullName ${fullNamePath} NAME)
    file(COPY ${fullNamePath} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
    execute_process(COMMAND cmake -E create_symlink
        ${fullName}
        ${name}
    )
  endforeach()
  #foreach(lib ${allReleaseSharedLibraries})
  #  list(GET lib 0 libName)
  #  message(${libName})
  #  #file(COPY ${libName} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
  #endforeach()
endif()

#install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION .)
#install(FILES ${allDebugSharedLibraries} DESTINATION .)
