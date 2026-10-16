cmake_minimum_required(VERSION 3.23)
project(cpp_media_library CXX)

# Add project library media_core
add_library(media_core src/image.cpp)
target_include_directories(media_core PUBLIC include)
target_compile_features(media_core PUBLIC cxx_std_20)

# third party media I/O library
add_library(media_io src/image_io.cpp)
target_include_directories(media_io PRIVATE third_party/stb)

# Declare executable media_demo
add_executable(media_demo src/main.cpp)
target_link_libraries(media_demo PRIVATE media_core) # Add media_demo library to linker
target_link_libraries(media_io PRIVATE media_core)   # Add media_io library to linker