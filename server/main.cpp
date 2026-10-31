#include "http/header.hpp"
#include "http/request_line.hpp"
#include "net/socket.hpp"
#include "net/winsock_runtime.hpp"

#include <iostream>
#include <ranges>
#include <stdexcept>    
#include <string>
#include <vector>
#include <winsock2.h>
#include <ws2tcpip.h> // Address conversion functions

namespace{
    constexpr DWORD receive_timeout_ms{10000};
    constexpr int receive_buffer_size{4096};
    constexpr int max_header_bytes{20 * 1024};

    // Checks for errors involving SOCKET_ERROR
    void check_socket_error(int result, std::string method) {
        if(result == SOCKET_ERROR) {
            const int error{WSAGetLastError()};
            throw std::runtime_error(method + "() failed with error code: " + std::to_string(error));
        }
    }
}

int main() {
    try {
        WinsockRuntime runtime; // Start windows socket library
        std::cout << "Winsock initialized.\n";

        //// CREATE LISTENER SOCKET ////

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
            throw std::runtime_error("InetPtonA() failed to convert to binary IPv4 format, returning code: " + std::to_string(converted_result));
        }


        //// BIND SOCKET TO ADDRESS ////

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


        //// START LISTENER SOCKET ON SPECIFIED ADDRESS AND PORT ////


        // Start listening on address and port via listener socket
        check_socket_error(
            ::listen(listener.native_handle(), SOMAXCONN), // mark bound socket as a listening socket with max-length pending-connection queue
            "listen"
        );

        std::cout << "Listening on 127.0.0.1:8080.\n";
        

        //// RECEIVE AND ACCEPT INCOMING CONNECTIONS ////

        while(true) {
            
            // Blocks while awaiting incoming connections
            SOCKET accepted_handle {
                ::accept(listener.native_handle(), nullptr, nullptr)
            };

            if(accepted_handle == INVALID_SOCKET) {
                const int error{WSAGetLastError()};
                throw std::runtime_error(
                    "accept() failed with error code: " + std::to_string(error)
                );
            }

            // Wrap accepted handle
            Socket connection{accepted_handle};
            check_socket_error(
                ::setsockopt(
                    accepted_handle,                                    // Socket handle to modify
                    SOL_SOCKET,                                         // General socket options
                    SO_RCVTIMEO,                                        // Receive timeout option
                    reinterpret_cast<const char*>(&receive_timeout_ms), // Address of the value
                    static_cast<int>(sizeof(receive_timeout_ms))        // Value's size in bytes
                ),
                "setsockopt"
            );

            char buffer[receive_buffer_size]{}; // local buffer to store received raw character data
            std::string incoming;               // retain received bytes across multiple reads --> construct http headers

            while(true) {
                int received {
                    ::recv(connection.native_handle(), buffer, sizeof(buffer), 0)
                };

                // Connection received "received" bytes via socket
                if(received > 0) {

                    // Construct HTTP request and look for header terminator "\r\n\r\n"
                    incoming.append(buffer, static_cast<std::size_t>(received));

                    //// PARSE HTTP REQUEST HEADER ////

                    const auto header_end = incoming.find("\r\n\r\n"); // Header terminator

                    if(header_end != std::string::npos) {

                        // Compare up to header terminator start + 4 characters
                        if(header_end + 4 > max_header_bytes) {
                            std::cerr << "Rejected connection: HTTP headers exceed limit.\n";
                            break; // Close connection when exceeding header size
                        }

                        // Start parsing header fields once received sufficient bytes
                        RequestLine r;
                        std::vector<Header> headers;
                        
                        //// PARSE REQUEST LINE OF HTTP REQUEST ////

                        const auto request_line_end = incoming.find("\r\n"); // Request line terminator

                        try {
                            r = parse_request_line(incoming.substr(0, request_line_end));
                            std::cout << ("Output: " + r.method + ' ' + r.target + ' ' + r.version) << '\n'; 
                        } catch(const std::exception& error) {
                            std::cerr << "Encountered an error when parsing request line: " << error.what() << '\n';
                        }

                        //// PARSE REMAINING HTTP HEADERS ////
                        
                        try {                            
                            const auto headers_start = request_line_end + 2;
                            std::string headers_data{incoming.substr(headers_start, header_end - headers_start)};
                            auto header_lines = std::views::split(headers_data, std::string_view{"\r\n"});
                            auto num_headers = std::ranges::distance(header_lines);

                            for(const auto& h : header_lines) {
                                const std::string_view header{std::string_view(h.begin(), h.end())};
                                headers.push_back(parse_header_line(header));
                            } 

                        } catch(const std::exception& error) {
                            std::cerr << "Encountered an error when parsing headers : " << error.what() << '\n';
                            break;
                        }

                        break;
                    }

                    // Enforce header max size constraint when no escape sequence found
                    if(incoming.size() >= max_header_bytes)
                    {
                        std::cerr << "Rejected connection: HTTP headers exceed limit.\n";
                        break; // Close connection when exceeding header size
                    }
                }

                // Connection closed by client
                else if(received == 0) {
                    break;
                }

                // SOCKET_ERROR: report error 
                else {
                    const int error{WSAGetLastError()};
                    std::cerr << "recv() failed with error code: " + std::to_string(error) << '\n';
                    break;
                }
            }
        }

        std::cout << "Cleaning up...\n";

    } catch (const std::exception& error) {
        std::cerr << "Encountered an error: " << error.what() << '\n';
        return 1;
    }

    return 0;
}