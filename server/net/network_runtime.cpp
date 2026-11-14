#include "net/network_runtime.hpp"

#include <stdexcept>
#include <string>

#ifdef _WIN32
#include <winsock2.h>
#endif

// Constructor implementation (macOS does not need a runtime initialized)
NetworkRuntime::NetworkRuntime() {
#ifdef _WIN32
    WSADATA data{};
    const int result{WSAStartup(MAKEWORD(2, 2), &data)};

    if(result != 0) {
        throw std::runtime_error{
            "WSAStartup returned error code: " + std::to_string(result)
        };
    }
#endif
}

// Destructor implementation
NetworkRuntime::~NetworkRuntime() noexcept {
#ifdef _WIN32
    WSACleanup();
#endif
}
 

