cmake_minimum_required(VERSION 3.10)
project(hidehook CXX)

add_library(hidehook SHARED zygisk/module.cpp)
target_compile_features(hidehook PRIVATE cxx_std_17)

# Inclui os headers do Zygisk e do xHook
target_include_directories(hidehook PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/zygisk-ndk/include
    ${CMAKE_CURRENT_SOURCE_DIR}/xHook/libxhook/jni
)

# Adiciona a pasta do xHook para ser compilada junto
add_subdirectory(xHook)

# Linka tudo
target_link_libraries(hidehook PRIVATE xhook log dl)
