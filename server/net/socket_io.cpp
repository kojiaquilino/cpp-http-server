#define NOMINMAX

#include "http/header.hpp"
#include "net/network.hpp"
#include "http/request_line.hpp"
#include "net/socket.hpp"
#include "net/socket_io.hpp"

#include <chrono>
#include <iostream>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <winsock2.h>

using RequestClock = std::chrono::steady_clock;

namespace {
    void check_deadline(
        RequestClock::time_point deadline,
        const char* message
    ) {
        if(RequestClock::now() >= deadline) {
            throw std::runtime_error(message);
        }
    }

    /// @brief Parses the HTTP header lines that follow the request line.
    /// @param headers The vector to which parsed headers are appended.
    /// @param incoming_bytes The received HTTP request bytes.
    /// @param headers_end_idx The index immediately after the header section.
    /// @param request_line_end_idx The index at the end of the request line.
    /// @return true if all headers are parsed successfully; otherwise, false.
    bool parse_remaining_request_headers(
        std::string incoming_bytes,
        std::size_t headers_end_idx,
        std::size_t request_line_end_idx,
        std::vector<Header>& headers
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
        std::size_t expected_body_bytes,
        std::size_t body_start_idx,
        RequestClock::time_point deadline,
        std::string& request_body
    ) noexcept {
        try {
            // Throw error and return early if deadline elapsed
            check_deadline(deadline, "Request rejected: receive deadline expired");

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
                configure_receive(connection, deadline);

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
                    check_deadline(deadline, "Request rejected: receive deadline expired");

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
            check_deadline(deadline, "Request rejected: receive deadline expired");

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

    std::optional<Request> process_request(
        const Socket& connection,
        char (&buffer)[receive_buffer_size],
        std::string incoming_bytes,
        std::size_t headers_end_idx,
        RequestClock::time_point deadline
    ) {
       
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
            return std::nullopt; // Ends current process_request() on error
        }

        // Parse and populate remaining HTTP request headers after request line
        if(!parse_remaining_request_headers(incoming_bytes, headers_end_idx, request_line_end_idx, headers)) {
            return std::nullopt; // Ends current process_request() when failing to parse headers
        }

        // Validate populated headers and extract header values
        // Currently retrieves Content-Length: number of body bytes to receive from HTTP request
        try {
            const auto request_framing = validate_request_headers(headers);
            const auto expected_body_bytes = request_framing.content_length.value_or(0); // content_bytes is optional
            const auto body_start_idx = headers_end_idx + 4; // +4 to pass CRLF control characters dividing header + body
            
            if(!parse_request_body(connection, incoming_bytes, buffer, expected_body_bytes, body_start_idx, deadline, body)) {
                return std::nullopt; // Ends current process_request() when failing to populate request body
            }

            // Check elapsed time against deadline
            check_deadline(deadline, "Request rejected: receive deadline expired");

            // Return successfully parsed request
            return Request{
                request_line,
                headers,
                body
            };
        } catch(const std::exception& error) {
            std::cerr << "Encountered an error when validating HTTP request headers: " << error.what() << '\n';
            return std::nullopt;
        }
    }
}


// void send_response(const Socket& connection, std::string_view bytes) {
//     std::size_t sent_bytes{0};
//     const auto deadline = RequestClock::now() + std::chrono::seconds{10}; // Initialize response send timeout (default=10 seconds)

//     while(sent_bytes < bytes.size()) {
//         const auto remaining = bytes.size() - sent_bytes;

//         const auto chunk_size = static_cast<int>(
//             std::min(remaining, max_chunk_bytes)
//         );

//         // Maintain pointer to start of data to send
//         const char* chunk_start = bytes.data() + sent_bytes;
//     }
// }


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
                check_deadline(deadline, "Request rejected: receive deadline expired");

                // Construct HTTP request and search for header terminator "\r\n\r\n"
                incoming_bytes.append(buffer, static_cast<std::size_t>(received_bytes));
                const auto headers_end_idx = incoming_bytes.find("\r\n\r\n");

                if(headers_end_idx != std::string::npos) {
                    
                    // Compare up to header terminator start + 4 characters
                    if(headers_end_idx + 4 > max_header_bytes) {
                        std::cerr << "Rejected connection: HTTP headers exceed limit.\n";
                        break; // Close connection when exceeding header size
                    }

                    // Returns processed request or std::nullopt on failure
                    return process_request(connection, buffer, incoming_bytes, headers_end_idx, deadline);

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
