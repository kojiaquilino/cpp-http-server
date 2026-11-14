#pragma once

#include "http/request.hpp"
#include "net/socket.hpp"

#include <chrono>
#include <string_view>

using RequestClock = std::chrono::steady_clock;

// Validates the deadline and reports an error if the operation has expired.
void check_deadline(RequestClock::time_point deadline, std::string_view message);

// Sends a response payload through the specified socket connection.
void send_response(const Socket& connection, std::string_view bytes);

// Receives and parses an HTTP request from the specified socket connection.
RequestData receive_request(const Socket& connection) noexcept;