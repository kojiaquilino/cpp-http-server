#pragma once

#include "net/socket.hpp"

#include <chrono>
#include <string>

using RequestClock = std::chrono::steady_clock;

inline constexpr std::chrono::milliseconds default_timeout{10000};

// Creates a socket, binds it to the specified address, and returns it.
Socket create_and_bind_listener_socket_to(std::string bind_ip="127.0.0.1", int bind_port=8080);

// Starts listening for incoming connections on the specified socket.
void start_listener(const Socket& listener);

// Accepts and returns the next incoming client connection.
Socket accept_incoming_connection(const Socket& listener);

// Checks whether the request deadline has expired and reports the provided error.
void check_request_deadline(RequestClock::time_point deadline, std::string error_message);

// Configures the connection to enforce the specified receive deadline.
void configure_receive(const Socket& connection, RequestClock::time_point deadline);

// Configures the connection to enforce the specified send deadline.
void configure_send(const Socket& connection, RequestClock::time_point deadline);