#pragma once

#include "http/header.hpp"
#include "http/request_line.hpp"
#include "net/socket.hpp"

#include <string_view>

constexpr DWORD receive_timeout_ms{10000};
constexpr int receive_buffer_size{4096};
constexpr int max_header_bytes{20 * 1024};
constexpr std::size_t max_chunk_bytes{65536}; // Send at most 64k bytes per send operation

// Request struct returned by each incoming HTTP request
struct Request {
    RequestLine request_line;
    std::vector<Header> headers;
    std::optional<std::string> body;
};


void send_response(const Socket& connection, std::string_view bytes);
std::optional<Request> receive_request(const Socket& connection);