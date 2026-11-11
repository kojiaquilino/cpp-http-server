#pragma once

#include "net/socket.hpp"

#include <chrono>
#include <string>

using RequestClock = std::chrono::steady_clock;

constexpr DWORD default_timeout_ms{10000};

Socket create_and_bind_listener_socket_to(std::string bind_ip="127.0.0.1", int bind_port=8080);
void start_listener(const Socket& listener);
Socket accept_incoming_connection(const Socket& listener);
void check_request_deadline(RequestClock::time_point deadline, std::string error_message);
void configure_receive(const Socket& connection, RequestClock::time_point deadline);
void configure_send(const Socket& connection, RequestClock::time_point deadline);