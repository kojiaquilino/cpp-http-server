cmake_minimum_required(VERSION 3.23)
project(cpp_media_library CXX)
add_executable(media_demo src/main.cpp)
target_compile_features(media_demo PRIVATE cxx_std_20)
