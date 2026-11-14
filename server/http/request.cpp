#define NOMINMAX

#include "http/error.hpp"
#include "http/header.hpp"
#include "http/request.hpp"
#include "http/request_line.hpp"
#include "net/network.hpp"
#include "net/socket.hpp"
#include "net/socket_io.hpp"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>
#include <winsock2.h>

using RequestClock = std::chrono::steady_clock;

void parse_remaining_request_headers(
    std::string incoming_bytes,
    std::size_t headers_end_idx,
    std::size_t request_line_end_idx,
    std::vector<Header>& headers
) {
    try {
        // Throws an error if there are no more headers following the first HTTP request line
        if(headers_end_idx <= request_line_end_idx) {
            throw http_errors::bad_request("Missing required Host header.");
        }

        const auto headers_start_idx = request_line_end_idx + 2; // +2 to pass CRLF control characters
        std::string headers_data{incoming_bytes.substr(headers_start_idx, headers_end_idx - headers_start_idx)};
        auto header_lines = std::views::split(headers_data, std::string_view{"\r\n"}); // Split on CRLF and collect headers

        // Iterate over split headers, processing per header line
        for(const auto& h : header_lines) {
            const std::string_view current_header{h.begin(), h.end()};
            headers.push_back(parse_header_line(current_header));
        }
    } catch(const HTTPError&) {
        throw; // propagate error up to caller
    } catch(const std::exception& error) { // Handle all other exceptions
        std::cerr << "Encountered an internal error when parsing headers: " << error.what() << '\n';
        throw http_errors::internal_server_error("Unexpected failure while parsing HTTP headers.");
    }
}

void collect_request_body(
    const Socket& connection,
    const std::string& incoming_bytes,
    char (&buffer)[receive_buffer_size],
    std::size_t expected_body_bytes,
    std::size_t body_start_idx,
    RequestClock::time_point deadline,
    std::string& request_body
) {
    // Throw error and return early if deadline elapsed
    check_deadline(deadline, "Request rejected: receive deadline expired");

    if(expected_body_bytes == 0) {
        std::cout << "No body attached to current HTTP request.\n";
        return;
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
            throw http_errors::bad_request("Client stopped sending before the request body was complete.");
        }

        // Encountered some socket error
        else if (received_bytes == SOCKET_ERROR) {
            const int error = WSAGetLastError();

        if(error == WSAEINTR) {
            continue; // Retry using the original deadline.
        }

        if(error == WSAETIMEDOUT) {
            throw http_errors::request_timeout("Timed out while receiving the request body.");
        }

        std::cerr << "Body receive failed: Winsock error " << error << '\n';
        throw http_errors::unknown_error("Socket receive failed."); // Do not return an HTTP response in this case
    }

    // Check elapsed time against deadline
    check_deadline(deadline, "Request rejected: receive deadline expired");

    // Report successful body completion.
    std::cout << "Request body complete. Expected: "
              << expected_body_bytes
              << " bytes; received: "
              << request_body.size() << " bytes\n";
    }
}

RequestData process_request(
    const Socket& connection,
    char (&buffer)[receive_buffer_size],
    std::string incoming_bytes,
    std::size_t headers_end_idx,
    RequestClock::time_point deadline
) noexcept {
    
    RequestLine request_line;
    std::vector<Header> request_headers;
    std::string request_body;

    const auto request_line_end_idx = incoming_bytes.find("\r\n"); // finds end of first line

    // Populate request line of HTTP request
    try {
        request_line = parse_request_line(incoming_bytes.substr(0, request_line_end_idx));

        // Check if the method provided in `request_line` is a supported method
        if(std::find(supported_methods.begin(), supported_methods.end(), request_line.method) == supported_methods.end()) {
            throw http_errors::not_implemented("This server supports only GET and POST.");
        }

        std::cout << ("Output: " + request_line.method + ' ' + request_line.target + ' ' + request_line.version) << '\n';

        // Parse and populate remaining HTTP request headers after request line
        parse_remaining_request_headers(
            incoming_bytes,
            headers_end_idx,
            request_line_end_idx,
            request_headers
        );

        // Validate populated headers and extract headers from HTTP request to populate `request_headers`
        // Currently retrieves Content-Length: number of body bytes to receive from HTTP request
        const auto request_framing = validate_request_headers(request_headers);
        const auto expected_body_bytes = request_framing.content_length.value_or(0); // content_bytes is optional
        const auto body_start_idx = headers_end_idx + 4; // +4 to pass CRLF control characters dividing header + body
        
        // Attempt to parse and collect HTTP request body bytes in `request_body`
        collect_request_body(
            connection,
            incoming_bytes,
            buffer,
            expected_body_bytes,
            body_start_idx,
            deadline,
            request_body
        );

        // Check elapsed time against deadline
        check_deadline(deadline, "Request rejected: receive deadline expired");

        // Return successfully parsed request
        return Request{
            request_line,
            request_headers,
            request_body
        };

    } catch(const HTTPError& http_error) {
        std::cerr << "Encountered an error when processing HTTP request: " << http_error.what() << '\n';
        return RequestError{http_error};
    } catch(const std::exception& error) {
        std::cerr << "Encountered an unknown error when processing HTTP request: " << error.what() << '\n';
        return RequestError{http_errors::internal_server_error("Unknown server error when processing HTTP request")};
    }
}

