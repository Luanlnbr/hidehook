cmake_minimum_required(VERSION 3.10)
project(hidehook CXX)

add_library(hidehook SHARED zygisk/module.cpp)
target_compile_features(hidehook PRIVATE cxx_std_17)

# Linka apenas as libs nativas do Android
target_link_libraries(hidehook PRIVATE log dl)
