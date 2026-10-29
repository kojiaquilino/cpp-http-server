#include "net/socket.hpp"
#include "net/winsock_runtime.hpp"

#include <iostream>
#include <stdexcept>    
#include <string>
#include <winsock2.h>
#include <ws2tcpip.h> // Address conversion functions

int main() {
    try {
        WinsockRuntime runtime; // Start windows socket library
        std::cout << "Winsock initialized.\n";

        SOCKET raw_listener_handle{::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)};

        if(raw_listener_handle == INVALID_SOCKET) {
            const int error{WSAGetLastError()};
            throw std::runtime_error("socket() failed with error code: " + std::to_string(error));
        }

        // Create listener socket wrapper
        Socket listener{raw_listener_handle};
        sockaddr_in address{};          // IPv4 socket address structure zero-initialized
        address.sin_family = AF_INET;   // Specify IPv4 address
        address.sin_port = htons(8080); // Set port to 8080, and converts host integer byte order to network byte order (match endianness)

        int converted_result{
            InetPtonA(AF_INET, "127.0.0.1", &address.sin_addr) // represents 127.0.0.1:8080 in "address"
        };

        // Check for address conversion success
        if(converted_result != 1) {
            throw std::runtime_error("InetPtonA() failed to convert to binary IPv4 format, returning code: " + converted_result);
        }

        // Bind socket to local ip address and port
        int bind_result{
            ::bind(
                listener.native_handle(),
                reinterpret_cast<const sockaddr*>(&address),
                static_cast<int>(sizeof(address))
            )
        };

        if(bind_result == SOCKET_ERROR) {
            const int error{WSAGetLastError()};
            throw std::runtime_error("bind() failed with error code: " + std::to_string(error));
        }

        // Start listening on address and port via listener socket
        int listen_result{
            ::listen(listener.native_handle(), SOMAXCONN) // mark bound socket as a listening socket with max-length pending-connection queue
        };

        if(listen_result == SOCKET_ERROR) {
            const int error{WSAGetLastError()};
            throw std::runtime_error("listen() failed with error code: " + std::to_string(error));
        }

        std::cout << "Listening on 127.0.0.1:8080. Press Enter to stop.\n";
        std::cin.get(); // suspend program until further input


        std::cout << "Cleaning up...\n";


      

    } catch (const std::exception& error) {
        std::cerr << "Encountered an error: " << error.what() << '\n';
        return 1;
    }

    return 0;
}