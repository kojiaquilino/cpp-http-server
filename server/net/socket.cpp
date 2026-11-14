#include "socket.hpp"
#include "net/platform.hpp"

#ifndef _WIN32
#include <unistd.h> // provides close() for POSIX sockets
#endif

#include <cstdint>
#include <utility>

Socket::Socket(net::NativeSocket handle) noexcept
    : handle_{handle} {};
    
Socket::~Socket() noexcept {
    if(valid()) {
#ifdef _WIN32
        ::closesocket(handle_);
#else
        ::close(handle_);
#endif
    }
}

net::NativeSocket Socket::native_handle() const noexcept {
    return handle_;
}

bool Socket::valid() const noexcept {
    return handle_ != net::invalid_socket;
}

Socket::Socket(Socket&& other) noexcept {
    handle_ = std::exchange(other.handle_, net::invalid_socket);
}

Socket& Socket::operator=(Socket&& other) noexcept {

    // Prevent self-moves
    if(this != &other) {
        if(valid()) {
#ifdef _WIN32
            ::closesocket(handle_);
#else
            ::close(handle_);
#endif
    }

        // Move other handle to current socket object, and invalidate other handle
        handle_ = std::exchange(other.handle_, net::invalid_socket);
    }
    return *this;
}

