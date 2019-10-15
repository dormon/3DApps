cmake_minimum_required(VERSION 3.12.1)

addApp3(000_AddVector "000_AddVector.cpp" "timer;Simple3DApp::Simple3DApp" "${CMAKE_BINARY_DIR}/GMU")
addApp3(001_Uniforms  "001_Uniforms.cpp"  "timer;Simple3DApp::Simple3DApp" "${CMAKE_BINARY_DIR}/GMU")
addApp3(002_Atomic    "002_Atomic.cpp"    "timer;Simple3DApp::Simple3DApp" "${CMAKE_BINARY_DIR}/GMU")

