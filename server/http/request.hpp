#pragma once

#include "http/error.hpp"
#include "http/header.hpp"
#include "http/request_line.hpp"
#include "net/socket.hpp"

#include <chrono>
#include <optional>
#include <string>
#include <vector>
#include <variant>

struct Request {
    RequestLine request_line;
    std::vector<Header> headers;
    std::optional<std::string> body;
};

struct RequestError {
    int status_code;
    std::string reason_phrase;
    std::string message;

    // Manually constructing RequestError
    RequestError(
        int status_code,
        std::string reason_phrase,
        std::string message
    )
        : status_code{status_code},
          reason_phrase{std::move(reason_phrase)},
          message{std::move(message)} {}

    // Construct a RequestError from an HTTP error
    RequestError(HTTPError error)
        : status_code{error.status_code},
          reason_phrase{error.reason_phrase},
          message{error.what()} {}
};


using RequestClock = std::chrono::steady_clock;
using RequestData = std::variant<Request, RequestError>;
    
constexpr int receive_buffer_size{4096};      // Preallocated receiving buffer size
constexpr int max_header_bytes{20 * 1024};    // Receive at most 20k bytes per request header
constexpr std::size_t max_chunk_bytes{65536}; // Send at most 64k bytes per send operation

// Parses and appends the remaining HTTP request headers.
void parse_remaining_request_headers(
    std::string incoming_bytes,
    std::size_t headers_end_idx,
    std::size_t request_line_end_idx,
    std::vector<Header>& headers
);

// Reads the request body until the expected number of bytes is collected.
void collect_request_body(
    const Socket& connection,
    const std::string& incoming_bytes,
    char (&buffer)[receive_buffer_size],
    std::size_t expected_body_bytes,
    std::size_t body_start_idx,
    RequestClock::time_point deadline,
    std::string& request_body
);

// Processes the request and returns either parsed data or a request error.
RequestData process_request(
    const Socket& connection,
    char (&buffer)[receive_buffer_size],
    std::string incoming_bytes,
    std::size_t headers_end_idx,
    RequestClock::time_point deadline
) noexcept;