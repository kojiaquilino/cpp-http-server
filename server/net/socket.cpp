#include "socket.hpp"

#include <winsock.h>
#include <utility>

Socket::Socket(SOCKET handle) noexcept
    : handle_{handle} {};
    
Socket::~Socket() noexcept {
    closesocket(handle_);
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
    handle_ = std::exchange(other.handle_, INVALID_SOCKET);
    return *this;
}

