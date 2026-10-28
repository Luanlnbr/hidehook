cmake_minimum_required(VERSION 3.10)
project(hidehook CXX)

include(FetchContent)

# Baixa o Zygisk NDK (API do Zygisk)
FetchContent_Declare(
  zygisk_ndk
  GIT_REPOSITORY https://github.com/Dr-TSNG/Zygisk-NDK.git
  GIT_TAG master
)
FetchContent_MakeAvailable(zygisk_ndk)

# Baixa o xHook (Biblioteca de Hook em C++)
FetchContent_Declare(
  xhook
  GIT_REPOSITORY https://github.com/iqiyi/xHook.git
  GIT_TAG v1.3.2
)
FetchContent_MakeAvailable(xhook)

add_library(hidehook SHARED zygisk/module.cpp)
target_compile_features(hidehook PRIVATE cxx_std_17)

target_include_directories(hidehook PRIVATE 
    ${zygisk_ndk_SOURCE_DIR}/include
    ${xhook_SOURCE_DIR}/libxhook/jni
)

target_link_libraries(hidehook PRIVATE xhook log dl)
