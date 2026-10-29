#include "net/winsock_runtime.hpp"

#include <winsock2.h>
#include <stdexcept>
#include <string>

// Constructor implementation
WinsockRuntime::WinsockRuntime() {
    WSADATA data{};
    int result{WSAStartup(MAKEWORD(2, 2), &data)};

    if (result != 0) {
        throw std::runtime_error("WSAStartup returned error code: " + std::to_string(result));
    }
}

// Destructor implementation
WinsockRuntime::~WinsockRuntime() {
    WSACleanup();
}
 

