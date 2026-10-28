cmake_minimum_required(VERSION 3.10)
project(hidehook CXX)

# Faz o GitHub baixar o xHook certinho, sem dar erro
include(FetchContent)
FetchContent_Declare(
  xhook
  GIT_REPOSITORY https://github.com/iqiyi/xHook.git
  GIT_TAG v1.3.2
)
FetchContent_MakeAvailable(xhook)

add_library(hidehook SHARED zygisk/module.cpp)
target_compile_features(hidehook PRIVATE cxx_std_17)

target_include_directories(hidehook PRIVATE 
    ${xhook_SOURCE_DIR}/libxhook/jni
)

target_link_libraries(hidehook PRIVATE xhook log dl)
