#include "socket.hpp"

#include <winsock2.h>
#include <utility>

Socket::Socket(SOCKET handle) noexcept
    : handle_{handle} {};
    
Socket::~Socket() noexcept {
    if(valid()) {
        closesocket(handle_);
    }
}

SOCKET Socket::native_handle() const noexcept {
    return handle_;
}

bool Socket::valid() const noexcept {
    return handle_ != INVALID_SOCKET;
}

Socket::Socket(Socket&& other) noexcept {
    handle_ = std::exchange(other.handle_, INVALID_SOCKET);
}

Socket& Socket::operator=(Socket&& other) noexcept {

    // Prevent self-moves
    if(this != &other) {
        if(valid()) {
            closesocket(handle_); // Close existing socket if still open
        }

        // Move other handle to current socket object, and invalidate other handle
        handle_ = std::exchange(other.handle_, INVALID_SOCKET);
    }
    return *this;
}

