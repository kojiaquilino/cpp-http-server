#pragma once

#include "http/header.hpp"
#include "net/socket.hpp"
#include "http/request_line.hpp"

#include <chrono>
#include <optional>
#include <string>
#include <vector>
#include <winsock2.h>

using RequestClock = std::chrono::steady_clock;

constexpr DWORD receive_timeout_ms{10000};    // Time out operations in 10 seconds
constexpr int receive_buffer_size{4096};      // Preallocated receiving buffer size
constexpr int max_header_bytes{20 * 1024};    // Receive at most 20k bytes per request header
constexpr std::size_t max_chunk_bytes{65536}; // Send at most 64k bytes per send operation

// Request struct returned by each incoming HTTP request
struct Request {
    RequestLine request_line;
    std::vector<Header> headers;
    std::optional<std::string> body;
};

bool parse_remaining_request_headers(
    std::string incoming_bytes,
    std::size_t headers_end_idx,
    std::size_t request_line_end_idx,
    std::vector<Header>& headers
) noexcept;

bool parse_request_body(
    const Socket& connection,
    const std::string& incoming_bytes,
    char (&buffer)[receive_buffer_size],
    std::size_t expected_body_bytes,
    std::size_t body_start_idx,
    RequestClock::time_point deadline,
    std::string& request_body
) noexcept;

std::optional<Request> process_request(
    const Socket& connection,
    char (&buffer)[receive_buffer_size],
    std::string incoming_bytes,
    std::size_t headers_end_idx,
    RequestClock::time_point deadline
);