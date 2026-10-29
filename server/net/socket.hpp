#pragma once

#include <winsock.h>

class Socket {
public:
    Socket() noexcept = default;
    explicit Socket(SOCKET handle) noexcept; // Prevent implicit casts from Socket -> SOCKET object
    ~Socket() noexcept;

    // Disable copy constructor and copy assignment operator
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    // Enforce move semantics for transferring socket ownership
    Socket(Socket&& other) noexcept;
    Socket& operator=(Socket&& other) noexcept;

    // Member accessors
    SOCKET native_handle() const noexcept; // return wrapped SOCKET object
    bool valid() const noexcept;           // return whether socket is still valid

private:
    SOCKET handle_{INVALID_SOCKET}; // initialize as empty resource
};
