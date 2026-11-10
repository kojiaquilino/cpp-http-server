#define NOMINMAX

#include "http/header.hpp"
#include "http/request_line.hpp"
#include "net/socket.hpp"
#include "net/winsock_runtime.hpp"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <optional>
#include <ranges>
#include <stdexcept>    
#include <string>
#include <vector>
#include <winsock2.h>
#include <ws2tcpip.h> // Address conversion functions

using RequestClock = std::chrono::steady_clock;

namespace{
    constexpr DWORD receive_timeout_ms{10000};
    constexpr int receive_buffer_size{4096};
    constexpr int max_header_bytes{20 * 1024};

    // Request struct returned by each incoming HTTP request
    struct Request {
        RequestLine request_line;
        std::vector<Header> headers;
        std::optional<std::string> body;
    };

    /// @brief Throws when a Winsock operation fails.
    /// @param result The result returned by the Winsock operation.
    /// @param method The name of the operation, used in the error message.
    void check_socket_error(int result, std::string method) {
        if(result == SOCKET_ERROR) {
            const int error{WSAGetLastError()};
            throw std::runtime_error(method + "() failed with error code: " + std::to_string(error));
        }
    }

    /// @brief Rejects the request if its receive deadline has expired.
    /// @param deadline The deadline by which the request must be received.
    void check_request_deadline(RequestClock::time_point deadline) {
        if(RequestClock::now() >= deadline) {
            throw std::runtime_error(
                "Request rejected: receive deadline expired"
            );
        }
    }

    /// @brief Configures the socket to wait only until the request deadline.
    /// @param connection The socket receiving the request.
    /// @param deadline The deadline by which the request must be received.
    void prepare_receive(
        const Socket& connection,
        RequestClock::time_point deadline
    ) {
        // Get current time
        const auto now = RequestClock::now();

        // Compare deadline with elapsed time, and reject if passed deadline
        if(now >= deadline) {
            throw std::runtime_error(
                "Request rejected: receive deadline expired"
            );
        }

        // Updates remaining time, rounding up to nearest millisecond
        const auto remaining =
            std::chrono::ceil<std::chrono::milliseconds>(
                deadline - now
            );

        // How long the next recv() can block while waiting for data
        const DWORD timeout_ms = static_cast<DWORD>(remaining.count());

        if(::setsockopt(
            connection.native_handle(),
            SOL_SOCKET,
            SO_RCVTIMEO,
            reinterpret_cast<const char*>(&timeout_ms),
            sizeof(timeout_ms)
        ) == SOCKET_ERROR) {
            throw std::runtime_error(
                "Request rejected: failed to configure receive timeout"
            );
        }

    }


    /// @brief Parses the HTTP header lines that follow the request line.
    /// @param headers The vector to which parsed headers are appended.
    /// @param incoming_bytes The received HTTP request bytes.
    /// @param headers_end_idx The index immediately after the header section.
    /// @param request_line_end_idx The index at the end of the request line.
    /// @return true if all headers are parsed successfully; otherwise, false.
    bool parse_remaining_request_headers(
        std::vector<Header>& headers,
        std::string incoming_bytes,
        std::size_t headers_end_idx,
        std::size_t request_line_end_idx
    ) noexcept {
        try {
            // Throws an error if there are no more headers following the first HTTP request line
            if(headers_end_idx <= request_line_end_idx) {
                throw std::invalid_argument("Invalid HTTP header: header cannot be empty");
            }

            const auto headers_start_idx = request_line_end_idx + 2; // +2 to pass CRLF control characters
            std::string headers_data{incoming_bytes.substr(headers_start_idx, headers_end_idx - headers_start_idx)};
            auto header_lines = std::views::split(headers_data, std::string_view{"\r\n"}); // Split on CRLF and collect headers

            // Iterate over split headers, processing per header line
            for(const auto& h : header_lines) {
                const std::string_view current_header{h.begin(), h.end()};
                headers.push_back(parse_header_line(current_header));
            }
        } catch(const std::exception& error) {
            std::cerr << "Encountered an error when parsing headers: " << error.what() << '\n';
            return false; // Return with failure status
        }

        return true;
    }


    /// @brief Parses the request body from buffered and subsequently received bytes.
    /// @param connection Client connection used to receive remaining body bytes.
    /// @param incoming_bytes Bytes already received, including the HTTP headers.
    /// @param buffer Temporary buffer used while receiving the request body.
    /// @param request_body Output string that receives the complete request body.
    /// @param expected_body_bytes Number of body bytes expected from the request.
    /// @param body_start_idx Offset at which the body begins in `incoming_bytes`.
    /// @return `true` when the complete body is received; otherwise `false`.
    bool parse_request_body(
        const Socket& connection,
        const std::string& incoming_bytes,
        char (&buffer)[receive_buffer_size],
        std::string& request_body,
        std::size_t expected_body_bytes,
        std::size_t body_start_idx,
        RequestClock::time_point deadline
    ) noexcept {
        try {
            // Throw error and return early if deadline elapsed
            check_request_deadline(deadline);

            if(expected_body_bytes == 0) {
                std::cout << "No body attached to current HTTP request.\n";
                return true;
            }

            // Append current body bytes previously received with HTTP headers
            request_body = incoming_bytes.substr(body_start_idx, expected_body_bytes);

            // Receive bytes from HTTP request until received sufficient body bytes or encountered an error
            while(request_body.size() < expected_body_bytes) {
                const auto remaining_bytes = expected_body_bytes - request_body.size();
                const auto bytes_to_read = std::min(remaining_bytes, static_cast<std::size_t>(receive_buffer_size));

                // Update receive deadline for receiving bytes for constructing request body
                prepare_receive(connection, deadline);

                int received_bytes {
                    ::recv(
                        connection.native_handle(),
                        buffer,
                        bytes_to_read,
                        0
                    )
                };

                if(received_bytes > 0) {
                    // Check elapsed time against deadline
                    check_request_deadline(deadline);

                    request_body.append(
                        buffer,
                        static_cast<std::size_t>(received_bytes)
                    );
                }

                // Request did not send enough bytes to complete body
                else if(received_bytes == 0) {
                    std::cerr << "Rejected request: client disconnected before the body was complete.\n";
                    return false; // Reject HTTP request with incomplete body
                }

                // Encountered some socket error
                else if (received_bytes == SOCKET_ERROR) {
                    std::cerr << "Rejected request: body receive failed "
                                << "(Winsock error " << WSAGetLastError() << ").\n";
                    return false;
                }

                // Catch any other unknown errors
                else {
                    std::cerr << "Rejected request: an unknown error has occurred.\n";
                    return false;
                }
            }

            // Check elapsed time against deadline
            check_request_deadline(deadline);

            // Report successful body completion.
            std::cout << "Request body complete. Expected: "
                      << expected_body_bytes
                      << " bytes; received: "
                      << request_body.size() << " bytes\n";

            return true; // Report success on receiving all body bytes
    
        } catch(const std::exception& error) {
            std::cerr << "Encountered an error: " << error.what() << '\n';
            return false;
        }
    }


    /// @brief Creates a TCP listener socket and binds it to the specified IPv4 address.
    /// @param bind_ip The IPv4 address on which the listener should accept connections.
    /// @param bind_port The TCP port on which the listener should accept connections.
    /// @return A socket bound to the specified address and port.
    Socket create_and_bind_listener_socket_to(
        std::string bind_ip="127.0.0.1",
        int bind_port=8080
    ) {
        // Create raw listener socket and store handle
        SOCKET raw_listener_handle{
            ::socket(
                AF_INET,
                SOCK_STREAM,
                IPPROTO_TCP
            )
        };

        if(raw_listener_handle == INVALID_SOCKET) {
            throw std::runtime_error("socket() failed with error code: " + std::to_string(WSAGetLastError()));
        }

        // Create listener socket wrapper
        Socket listener{raw_listener_handle};
        sockaddr_in address{};               // IPv4 socket address structure zero-initialized
        address.sin_family = AF_INET;        // Specify IPv4 address
        address.sin_port = htons(bind_port); // Set port and convert host integer byte order to network byte order (match endianness)
        
        // converts address to binary representation
        int bind_address{
            InetPtonA(
                AF_INET,            // Address family: IPv4
                bind_ip.c_str(),    // IP address to bind listener socket to (to listen on)
                &address.sin_addr   // Output buffer for binary address
            )
        };

        // Check for address conversion success
        if(bind_address != 1) {
            throw std::runtime_error("InetPtonA() failed to convert to binary IPv4 format, "
                "returning code: " + std::to_string(bind_address));
        }

        // Bind socket to local ip address and port
        check_socket_error(
            ::bind(
                listener.native_handle(),
                reinterpret_cast<const sockaddr*>(&address),
                static_cast<int>(sizeof(address))
            ),
            "bind"
        );

        // Return listener wrapper containing handle to listener socket
        return listener;
    }

    /// @brief Configure an accepted incoming connection.
    /// @param accepted_handle Native handle for the accepted client connection.
    /// @return A socket wrapping the configured client connection.
    Socket configure_new_connection(const SOCKET& accepted_handle) {

        if(accepted_handle == INVALID_SOCKET) {
            throw std::runtime_error(
                "accept() failed with error code: " + std::to_string(WSAGetLastError())
            );
        }

        // Wrap accepted handle
        Socket connection{accepted_handle};

        // Configure options on new socket: blocking ::recv() times out in 10 seconds
        check_socket_error(
            ::setsockopt(
                accepted_handle,                                    // Socket to modify
                SOL_SOCKET,                                         // Specify option belonging to socket layer
                SO_RCVTIMEO,                                        // Set receive timeout
                reinterpret_cast<const char*>(&receive_timeout_ms), // Timeout value: default=10000ms (address of value)
                static_cast<int>(sizeof(receive_timeout_ms))        // Size of option value (in this case, timeout)
            ),
            "setsockopt"
        );

        return connection;
    }
    
    /// @brief Receives and parses one HTTP request from the client connection.
    /// @param connection The client socket from which request bytes are received.
    /// @return The parsed request, or `std::nullopt` if the connection closes or the request is invalid.
    std::optional<Request> receive_request(const Socket& connection) {
        char buffer[receive_buffer_size]{};                                   // local buffer to store received raw character data
        std::string incoming_bytes;                                           // retain received bytes across multiple reads: used for constructing http headers
        const auto deadline = RequestClock::now() + std::chrono::seconds{10}; // Initialize response receive timeout (default=10 seconds)
        

        try {
            // Continuously accumulate bytes from incoming HTTP request
            while(true) {

                // Update receive deadline for receiving bytes + parsing response
                prepare_receive(connection, deadline);
                
                int received_bytes {
                    ::recv(
                        connection.native_handle(), // Socket to receive data from
                        buffer,                     // Destination buffer
                        sizeof(buffer),             // Maximum number of bytes to receive
                        0                           // Flags (0=default)
                    )
                };
    
                if(received_bytes > 0) {
                    check_request_deadline(deadline);

                    // Construct HTTP request and search for header terminator "\r\n\r\n"
                    incoming_bytes.append(buffer, static_cast<std::size_t>(received_bytes));
                    const auto headers_end_idx = incoming_bytes.find("\r\n\r\n");

                    if(headers_end_idx != std::string::npos) {
                        
                        // Compare up to header terminator start + 4 characters
                        if(headers_end_idx + 4 > max_header_bytes) {
                            std::cerr << "Rejected connection: HTTP headers exceed limit.\n";
                            break; // Close connection when exceeding header size
                        }

                        RequestLine request_line;
                        std::vector<Header> headers;
                        std::string body;

                        const auto request_line_end_idx = incoming_bytes.find("\r\n"); // finds end of first line

                        // Populate request line of HTTP request
                        try {
                            request_line = parse_request_line(incoming_bytes.substr(0, request_line_end_idx));
                            std::cout << ("Output: " + request_line.method + ' ' + request_line.target + ' ' + request_line.version) << '\n';
                        } catch(const std::exception& error) {
                            std::cerr << "Encountered an error when parsing request line: " << error.what() << '\n';
                            break; // Ends current receive_request() on error
                        }

                        // Parse and populate remaining HTTP request headers after request line
                        if(!parse_remaining_request_headers(headers, incoming_bytes, headers_end_idx, request_line_end_idx)) {
                            break; // Ends current receive_request() when failing to parse headers
                        }

                        // Validate populated headers and extract header values
                        // Currently retrieves Content-Length: number of body bytes to receive from HTTP request
                        try {
                            const auto request_framing = validate_request_headers(headers);
                            const auto expected_body_bytes = request_framing.content_length.value_or(0); // content_bytes is optional
                            const auto body_start_idx = headers_end_idx + 4; // +4 to pass CRLF control characters dividing header + body
                            
                            if(!parse_request_body(connection, incoming_bytes, buffer, body, expected_body_bytes, body_start_idx, deadline)) {
                                break; // Ends current receive_request() when failing to populate request body
                            }

                            // Check elapsed time against deadline
                            check_request_deadline(deadline);

                            // Return successfully parsed request
                            return Request{
                                request_line,
                                headers,
                                body
                            };
                        } catch(const std::exception& error) {
                            std::cerr << "Encountered an error when validating HTTP request headers: " << error.what() << '\n';
                            break;
                        }

                    } else if(incoming_bytes.size() >= max_header_bytes) {
                        std::cerr << "Rejected connection: HTTP headers exceed byte limit.\n";
                        break;
                    }
                } 
                
                // Connection closed by client
                else if(received_bytes == 0) {
                    break;
                }

                // SOCKET_ERROR: report error 
                else {
                    std::cerr << "recv() failed with error code: " + std::to_string(WSAGetLastError()) << '\n';
                    break;
                }
            }
        } catch(const std::exception& error) {
            std::cerr << "Rejected HTTP request: " << error.what() << '\n';
        }

        // Default return for all errors/invalid requests
        return std::nullopt;
    }
}

int main() {
    try {
        // Start windows socket library
        WinsockRuntime runtime;
        std::cout << "Winsock initialized.\n";

        /// Create listener socket
        Socket listener_socket{
            create_and_bind_listener_socket_to("127.0.0.1", 8080)
        };

        // Start listening on address and port via listener socket
        check_socket_error(
            ::listen(
                listener_socket.native_handle(), // listening socket handle
                SOMAXCONN                        // maximum pending connection queue size
            ),
            "listen"                             // label for check_socket_error
        );

        std::cout << "Listening on 127.0.0.1:8080.\n";
        

        // Receive and accept incoming connections
        // Currently closes accepted socket after receiving one request
        while(true) {

            // Block until accepting new incoming connection
            SOCKET accepted_handle{
                ::accept(
                    listener_socket.native_handle(), // listening socket
                    nullptr,                         // client_address
                    nullptr                          // address_length
                )
            };

            // Ensure that the connection was successful and set its options
            Socket connection{configure_new_connection(accepted_handle)};

            // Blocks until receiving an HTTP request on the connected socket
            std::optional<Request> parsed_request{receive_request(connection)};

            if(!parsed_request.has_value()) {
                std::cerr << "Failed to complete latest HTTP request.\n";
            }
        }

        // std::cout << "Cleaning up...\n";

    } catch (const std::exception& error) {
        std::cerr << "Encountered an error: " << error.what() << '\n';
        return 1;
    }

    return 0;
}