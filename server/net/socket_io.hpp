#pragma once

#include "http/header.hpp"
#include "http/request.hpp"
#include "net/socket.hpp"

#include <chrono>
#include <string_view>

using RequestClock = std::chrono::steady_clock;

void check_deadline(RequestClock::time_point deadline, const char* message);
void send_response(const Socket& connection, std::string_view bytes);
std::optional<Request> receive_request(const Socket& connection);