cmake_minimum_required(VERSION 3.10)
project(hidehook CXX)

# Baixa o Zygisk API e o xHook automaticamente
include(FetchContent)
FetchContent_Declare(
  zygisk_ndk
  GIT_REPOSITORY https://github.com/Dr-TSNG/Zygisk-NDK.git
  GIT_TAG master
)
FetchContent_Declare(
  xhook
  GIT_REPOSITORY https://github.com/iqiyi/xHook.git
  GIT_TAG master
)

FetchContent_MakeAvailable(zygisk_ndk xhook)

add_library(hidehook SHARED zygisk/module.cpp)
target_compile_features(hidehook PRIVATE cxx_std_17)

# Inclui os headers corretamente
target_include_directories(hidehook PRIVATE 
    ${zygisk_ndk_SOURCE_DIR}/include
    ${xhook_SOURCE_DIR}/libxhook/jni
)

target_link_libraries(hidehook PRIVATE xhook log dl)
