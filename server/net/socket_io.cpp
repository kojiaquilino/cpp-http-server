#define NOMINMAX

#include "http/header.hpp"
#include "net/network.hpp"
#include "http/response.hpp"
#include "http/request_line.hpp"
#include "net/socket.hpp"
#include "net/socket_io.hpp"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <winsock2.h>

using RequestClock = std::chrono::steady_clock;

/// @brief Throws an exception if the specified deadline has expired.
/// @param deadline The time point by which the operation must complete.
/// @param message The error message to include in the exception.
void check_deadline(
    RequestClock::time_point deadline,
    std::string_view message
) {
    if(RequestClock::now() >= deadline) {
        throw http_errors::request_timeout(std::string{message});
    }
}

/// @brief Sends all response bytes to the client before the send deadline expires.
/// @param connection The client socket to which the response is sent.
/// @param bytes The response bytes to send.
void send_response(const Socket& connection, std::string_view bytes) {
    std::size_t sent_bytes{0};
    const auto deadline = RequestClock::now() + std::chrono::seconds{10}; // Initialize response send timeout (default=10 seconds)

    while(sent_bytes < bytes.size()) {
        const auto remaining = bytes.size() - sent_bytes;                               // Number of bytes remaining to send
        const auto chunk_size = static_cast<int>(std::min(remaining, max_chunk_bytes)); // Max number of bytes to send in next chunk (max=65536 bytes)
        const char* chunk_start = bytes.data() + sent_bytes;                            // Maintain pointer to start of data to send
        
        check_deadline(deadline, "Request rejected: send deadline expired.");
        configure_send(connection, deadline);

        const int successful_bytes{
            ::send(
                connection.native_handle(),
                chunk_start,
                chunk_size,
                0
            )
        };

        // Handle result based on number of bytes successfully sent to client
        if(successful_bytes > 0) {
            sent_bytes += static_cast<int>(successful_bytes);
        } else if(successful_bytes == 0) {
            throw std::runtime_error("Socket send made no progress");
        } else {
            const int error{WSAGetLastError()};

            // Interrupted/canceled function call (most likely by client)
            if(error == WSAEINTR) {
                continue;
            }

            throw std::runtime_error(
                "Socket send failed: " + std::to_string(error)
            );
        }
    }

    // Check deadline once more after final send: reject if deadline elapsed
    check_deadline(deadline, "Request rejected: send deadline expired.");
}


/// @brief Receives and parses one HTTP request from the client connection.
/// @param connection The client socket from which request bytes are received.
/// @return The parsed request, or `std::nullopt` if the connection closes or the request is invalid.
RequestData receive_request(const Socket& connection) noexcept {
    char buffer[receive_buffer_size]{};                                   // local buffer to store received raw character data
    std::string incoming_bytes;                                           // retain received bytes across multiple reads: used for constructing http headers
    const auto deadline = RequestClock::now() + std::chrono::seconds{10}; // Initialize response receive timeout (default=10 seconds)
    
    try {
        // Continuously accumulate bytes from incoming HTTP request
        while(true) {

            // Update receive deadline for receiving bytes + parsing response
            configure_receive(connection, deadline);
            
            int received_bytes {
                ::recv(
                    connection.native_handle(), // Socket to receive data from
                    buffer,                     // Destination buffer
                    sizeof(buffer),             // Maximum number of bytes to receive
                    0                           // Flags (0=default)
                )
            };

            if(received_bytes > 0) {
                check_deadline(deadline, "Request rejected: receive deadline expired.");

                // Check early if request line is too long, in order to potentially return
                // status code 414: URI Too Long early rather than waiting for 431
                const auto request_line_end = incoming_bytes.find("\r\n");

                if(request_line_end != std::string::npos) {
                    if(request_line_end > max_request_line_bytes) {
                        throw http_errors::uri_too_long(
                            "HTTP request line exceeds limit."
                        );
                    }
                    
                } else {
                    // A trailing '\r' might begin a delimiter completed by the next read: 
                    // do not count against budget
                    const auto minimum_line_length =
                        incoming_bytes.size()
                        - (!incoming_bytes.empty() && incoming_bytes.back() == '\r' ? 1 : 0);

                    if(minimum_line_length > max_request_line_bytes) {
                        throw http_errors::uri_too_long(
                            "HTTP request line exceeds limit."
                        );
                    }
                }

                // Construct HTTP request and search for header terminator "\r\n\r\n"
                incoming_bytes.append(buffer, static_cast<std::size_t>(received_bytes));
                const auto headers_end_idx = incoming_bytes.find("\r\n\r\n");

                if(headers_end_idx != std::string::npos) {
                    
                    // Compare up to header terminator start + 4 characters
                    if(headers_end_idx + 4 > max_header_bytes) {
                        std::cerr << "Rejected connection: HTTP headers exceed byte limit.\n";
                        throw http_errors::headers_too_large("HTTP headers exceed byte limit.");
                    }

                    // Returns processed request or std::nullopt on failure
                    return process_request(connection, buffer, incoming_bytes, headers_end_idx, deadline);

                } else if(incoming_bytes.size() >= max_header_bytes) {
                    std::cerr << "Rejected connection: HTTP headers exceed byte limit.\n";
                    throw http_errors::headers_too_large("HTTP headers exceed byte limit.");
                }
            } 
            
            // Connection closed by client
            else if(received_bytes == 0) {
                if(!incoming_bytes.empty()) {
                    throw http_errors::bad_request(
                        "Client stopped sending before HTTP headers were complete."
                    );
                }

                break; // No request bytes received; close without responding.
            }

            // SOCKET_ERROR: report error 
            else {
               const int error = WSAGetLastError();

                if(error == WSAEINTR) {
                    continue; // Retry using the original deadline.
                }

                if(error == WSAETIMEDOUT) {
                    throw http_errors::request_timeout(
                        "Timed out while receiving HTTP headers."
                    );
                }

                std::cerr << "recv() failed: Winsock error " << error << '\n';
                throw http_errors::unknown_error("Socket receive failed."); // Do not return an HTTP response in this case
            }
        }
    } catch(const HTTPError& http_error) {
        std::cerr << "Rejected connection: " << http_error.what() << '\n';
        return RequestError{http_error};

    } catch(const std::exception& error) {
        std::cerr << "Rejected HTTP request: " << error.what() << '\n';
    }

    // Default return for all errors/invalid requests
    return RequestError{http_errors::unknown_error("An unknown error occurred.")};
}
