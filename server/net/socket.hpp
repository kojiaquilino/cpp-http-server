#pragma once

#include "net/platform.hpp"

class Socket {
public:
    Socket() noexcept = default;
    explicit Socket(net::NativeSocket handle) noexcept; // Prevent implicit casts from Socket -> SOCKET object
    ~Socket() noexcept;

    // Disable copy constructor and copy assignment operator
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    // Enforce move semantics for transferring socket ownership
    Socket(Socket&& other) noexcept;
    Socket& operator=(Socket&& other) noexcept;

    // Member accessors
    net::NativeSocket native_handle() const noexcept; // return wrapped SOCKET object
    bool valid() const noexcept;           // return whether socket is still valid

private:
    net::NativeSocket handle_{net::invalid_socket}; // initialize as empty resource
};
