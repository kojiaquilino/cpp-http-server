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

        /// 1. Create listener socket

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


        /// 2. Bind socket to address

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


        /// 3. Start listener socket on specified address and port number


        // Start listening on address and port via listener socket
        check_socket_error(
            ::listen(listener.native_handle(), SOMAXCONN), // mark bound socket as a listening socket with max-length pending-connection queue
            "listen"
        );

        std::cout << "Listening on 127.0.0.1:8080.\n";
        

        /// 4. Receive and accept incoming connections
        // Currently closes accepted socket after receiving one request

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
                    ::recv(
                        connection.native_handle(), // Socket to receive data from
                        buffer,                     // Destination buffer
                        sizeof(buffer),             // Maximum number of bytes to receive
                        0                           // Flags (0=default)
                    )
                };

                // Connection received "received" bytes via socket
                if(received > 0) {

                    // Construct HTTP request and look for header terminator "\r\n\r\n"
                    incoming.append(buffer, static_cast<std::size_t>(received));

                    /// 5. Begin parsing HTTP request header
                    // Header terminator
                    const auto header_end = incoming.find("\r\n\r\n");

                    // Begin parsing headers once located CRLF sequence in received bytes
                    if(header_end != std::string::npos) {

                        // Compare up to header terminator start + 4 characters
                        if(header_end + 4 > max_header_bytes) {
                            std::cerr << "Rejected connection: HTTP headers exceed limit.\n";
                            break; // Close connection when exceeding header size
                        }

                        // Start parsing header fields once received sufficient bytes
                        RequestLine r;
                        std::vector<Header> headers;
                        
                        /// 6. Parse request line of HTTP request

                        const auto request_line_end = incoming.find("\r\n"); // Request line terminator

                        try {
                            r = parse_request_line(incoming.substr(0, request_line_end));
                            std::cout << ("Output: " + r.method + ' ' + r.target + ' ' + r.version) << '\n';
                        } catch(const std::exception& error) {
                            std::cerr << "Encountered an error when parsing request line: " << error.what() << '\n';
                            break; // Reject connection and move onto receiving next incoming request
                        }

                        /// 7. Parse remaining HTTP headers after the request line
                        
                        try {
                            if(header_end > request_line_end) {
                                const auto headers_start = request_line_end + 2; // +2 to pass carriage return + line feed  
                                std::string headers_data{incoming.substr(headers_start, header_end - headers_start)};
                                auto header_lines = std::views::split(headers_data, std::string_view{"\r\n"});

                                for(const auto& h : header_lines) {
                                    const std::string_view header{std::string_view(h.begin(), h.end())};

                                    // collect header lines into a vector constructed
                                    // as a Header object
                                    headers.push_back(parse_header_line(header));
                                }
                            } else {
                                throw std::invalid_argument("Invalid HTTP header: header cannot be empty");
                            }
                        } catch(const std::exception& error) {
                            std::cerr << "Encountered an error when parsing headers : " << error.what() << '\n';
                            break;
                        }

                        /// 8. Validate collected headers and extract header values
                        // Extracts Content-Length to get number of body bytes to receive from HTTP request
                        try {
                            const auto request_framing = validate_request_headers(headers);
                            const auto expected_body_bytes = request_framing.content_length.value_or(0); // content_bytes is optional
                            const auto body_start = header_end + 4; // +4 to account for CRLF char sequence
                            
                            // No body attached: complete immediately
                            if(expected_body_bytes == 0) {
                                std::cout << "No body attached to current HTTP request.\n";
                                break;
                            }

                            // Append current body bytes previously received with header
                            std::string request_body{incoming.substr(body_start, expected_body_bytes)};

                            while(request_body.size() < expected_body_bytes) {
                                const auto remaining = expected_body_bytes - request_body.size();
                                const auto bytes_to_read = min(remaining, static_cast<std::size_t>(receive_buffer_size));

                                int received {
                                    ::recv(
                                        connection.native_handle(),
                                        buffer,
                                        bytes_to_read,
                                        0
                                    )
                                };

                                if(received > 0) {
                                    request_body.append(buffer, static_cast<std::size_t>(received));
                                } 
                                
                                // Request did not send enough bytes to complete body:
                                // Reject request with incomplete body
                                else if(received == 0) {
                                    std::cerr << "Rejected request: client disconnected before the body was complete.\n";
                                    break;
                                }

                                else if (received == SOCKET_ERROR) {
                                    const int error = ::WSAGetLastError();
                                    std::cerr << "Rejected request: body receive failed "
                                            << "(Winsock error " << error << ").\n";
                                    break;
                                }

                                // Expected length == request_body's size:
                                // Received enough bytes: complete without another recv()
                                else if(request_body.size() == expected_body_bytes) {
                                    std::cout << "Received sufficient body bytes. Total: " <<  request_body.size() << " bytes\n";
                                    break;
                                }

                                else {
                                    std::cerr << "Rejected request: an unknown error has occurred.\n";
                                    break; 
                                }
                            }
                        } catch (const std::exception& error) {
                            std::cerr << "Encountered an error: " << error.what() << '\n';
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