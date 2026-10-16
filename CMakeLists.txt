cmake_minimum_required(VERSION 3.23)
project(cpp_media_library CXX)

add_library(media_core src/image.cpp)
target_include_directories(media_core PUBLIC include)
target_compile_features(media_core PUBLIC cxx_std_20)

add_executable(media_demo src/main.cpp)
target_link_libraries(media_demo PRIVATE media_core)