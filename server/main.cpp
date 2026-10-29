#include "net/socket.hpp"
#include "net/winsock_runtime.hpp"

#include <winsock2.h>
#include <iostream>
#include <string>

int main() {
    try {
        WinsockRuntime runtime;
        std::cout << "Winsock initialized.\n";

        SOCKET raw_handle{::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)};

        if(raw_handle == INVALID_SOCKET) {
            const int error{WSAGetLastError()};

            throw std::runtime_error("socket() failed with error code: " + std::to_string(error));
        }

        Socket connection{raw_handle};
        std::cout << connection.valid() << '\n'; // should be true
        
        Socket second{std::move(connection)};

        std::cout << connection.valid() << '\n'; // should be false
        std::cout << second.valid() << '\n';     // should be true

    } catch (const std::exception& error) {
        std::cerr << "Encountered an error: " << error.what() << '\n';
        return 1;
    }

    return 0;
}