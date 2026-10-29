cmake_minimum_required(VERSION 3.23)
project(cpp_media_library CXX)

# Add project library media_core
add_library(media_core src/image.cpp src/operations.cpp
    src/pipeline.cpp)
target_include_directories(media_core PUBLIC include)
target_compile_features(media_core PUBLIC cxx_std_20)

# third party media I/O library
add_library(media_io src/image_io.cpp)
target_include_directories(media_io PUBLIC include)
target_include_directories(media_io PRIVATE third_party/stb)

# Declare executable media_demo
add_executable(media_demo src/main.cpp)
target_link_libraries(media_io PUBLIC media_core) # Add libraries to linker
target_link_libraries(media_demo PRIVATE media_io)

# Add project library server_net
add_library(server_net
    server/net/winsock_runtime.cpp
    server/net/socket.cpp
)

target_include_directories(server_net PUBLIC server)
target_compile_features(server_net PUBLIC cxx_std_20)

if(WIN32)
    target_link_libraries(server_net PRIVATE Ws2_32)
endif()

# Declare executable media_server
add_executable(media_server server/main.cpp)
target_link_libraries(media_server PRIVATE server_net)