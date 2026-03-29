cmake_minimum_required(VERSION 3.16)
project(bsphp_vmp_cpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT WIN32)
  message(FATAL_ERROR "Windows only (Win32 GUI + CNG).")
endif()

add_library(bl_vmp_core STATIC
  core/base64.cpp
  core/hwid.cpp
  core/rsa_cng.cpp
  core/license.cpp
)
target_include_directories(bl_vmp_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/core)
target_compile_definitions(bl_vmp_core PRIVATE UNICODE _UNICODE)
if(MSVC)
  target_compile_options(bl_vmp_core PRIVATE /utf-8)
endif()
target_link_libraries(bl_vmp_core PUBLIC bcrypt crypt32)

add_executable(bl_launcher WIN32 launcher/main.cpp launcher/launcher.rc)
target_include_directories(bl_launcher PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/launcher)
target_link_libraries(bl_launcher PRIVATE bl_vmp_core comctl32 user32 gdi32 shell32)
if(MSVC)
  target_compile_options(bl_launcher PRIVATE /utf-8)
endif()

add_executable(bl_keygen WIN32 keygen/main.cpp)
target_link_libraries(bl_keygen PRIVATE bl_vmp_core comctl32 user32 gdi32 shell32)
if(MSVC)
  target_compile_options(bl_keygen PRIVATE /utf-8)
endif()

add_executable(bl_packer WIN32 packer/main.cpp)
target_link_libraries(bl_packer PRIVATE comctl32 user32 gdi32 comdlg32)
if(MSVC)
  target_compile_options(bl_packer PRIVATE /utf-8)
endif()
