#include "net/network.hpp"
#include "net/socket.hpp"

#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>
#include <winsock2.h>
#include <ws2tcpip.h> // Address conversion functions

using RequestClock = std::chrono::steady_clock;

namespace {
    /// @brief Throws when a Winsock operation fails.
    /// @param result The result returned by the Winsock operation.
    /// @param method The name of the operation, used in the error message.
    void check_socket_error(int result, std::string method) {
        if(result == SOCKET_ERROR) {
            const int error{WSAGetLastError()};
            throw std::runtime_error(method + "() failed with error code: " + std::to_string(error));
        }
    }

    /// @brief Configure an accepted incoming connection.
    /// @param accepted_handle Native handle for the accepted client connection.
    /// @return A socket wrapping the configured client connection.
    Socket configure_new_connection(const SOCKET& accepted_handle) {

        if(accepted_handle == INVALID_SOCKET) {
            throw std::runtime_error(
                "accept() failed with error code: " + std::to_string(WSAGetLastError())
            );
        }

        // Wrap accepted handle
        Socket connection{accepted_handle};

        // Configure options on new socket: blocking ::recv() times out in 10 seconds
        check_socket_error(
            ::setsockopt(
                accepted_handle,                                    // Socket to modify
                SOL_SOCKET,                                         // Specify option belonging to socket layer
                SO_RCVTIMEO,                                        // Set receive timeout
                reinterpret_cast<const char*>(&default_timeout_ms), // Timeout value: default=10000ms (address of value)
                static_cast<int>(sizeof(default_timeout_ms))        // Size of option value (in this case, timeout)
            ),
            "setsockopt_recv"
        );

        // Configure options on new socket: blocking ::send() times out in 10 seconds
        check_socket_error(
            ::setsockopt(
                accepted_handle,                                    // Socket to modify
                SOL_SOCKET,                                         // Specify option belonging to socket layer
                SO_SNDTIMEO,                                        // Set send timeout
                reinterpret_cast<const char*>(&default_timeout_ms), // Timeout value: default=10000ms (address of value)
                static_cast<int>(sizeof(default_timeout_ms))        // Size of option value (in this case, timeout)
            ),
            "setsockopt_send"
        );

        return connection;
    }
}


/// @brief Creates a TCP listener socket and binds it to the specified IPv4 address.
/// @param bind_ip The IPv4 address on which the listener should accept connections.
/// @param bind_port The TCP port on which the listener should accept connections.
/// @return A socket bound to the specified address and port.
Socket create_and_bind_listener_socket_to(std::string bind_ip, int bind_port) {
    // Create raw listener socket and store handle
    SOCKET raw_listener_handle{
        ::socket(
            AF_INET,
            SOCK_STREAM,
            IPPROTO_TCP
        )
    };

    if(raw_listener_handle == INVALID_SOCKET) {
        throw std::runtime_error("socket() failed with error code: " + std::to_string(WSAGetLastError()));
    }

    // Create listener socket wrapper
    Socket listener{raw_listener_handle};
    sockaddr_in address{};               // IPv4 socket address structure zero-initialized
    address.sin_family = AF_INET;        // Specify IPv4 address
    address.sin_port = htons(bind_port); // Set port and convert host integer byte order to network byte order (match endianness)
    
    // converts address to binary representation
    int bind_address{
        InetPtonA(
            AF_INET,            // Address family: IPv4
            bind_ip.c_str(),    // IP address to bind listener socket to (to listen on)
            &address.sin_addr   // Output buffer for binary address
        )
    };

    // Check for address conversion success
    if(bind_address != 1) {
        throw std::runtime_error("InetPtonA() failed to convert to binary IPv4 format, "
            "returning code: " + std::to_string(bind_address));
    }

    // Bind socket to local ip address and port
    check_socket_error(
        ::bind(
            listener.native_handle(),
            reinterpret_cast<const sockaddr*>(&address),
            static_cast<int>(sizeof(address))
        ),
        "bind"
    );

    // Return listener wrapper containing handle to listener socket
    return listener;
}

/// @brief Starts listening for incoming TCP connections on the specified socket.
/// @param listener A bound socket to configure as a listening socket.
void start_listener(const Socket& listener) {
    // Start listening on address and port via listener socket
    check_socket_error(
        ::listen(
            listener.native_handle(), // listening socket handle
            SOMAXCONN                        // maximum pending connection queue size
        ),
        "listen"                             // label for check_socket_error
    );

    std::cout << "Listening on 127.0.0.1:8080.\n";
}

/// @brief Accepts and configures the next incoming TCP connection.
/// @param listener A listening socket that will accept the connection.
/// @return A socket wrapping the accepted and configured client connection.
Socket accept_incoming_connection(const Socket& listener) {
    // Block until accepting a new incoming connection.
    SOCKET accepted_handle{
        ::accept(
            listener.native_handle(), // listening socket
            nullptr,                  // client_address
            nullptr                   // address_length
        )
    };

    // Ensure that the connection was successful and set its options.
    Socket connection{configure_new_connection(accepted_handle)};
    return connection;
}



/// @brief Rejects the request if its receive deadline has expired.
/// @param deadline The deadline by which the request must be received.
void check_request_deadline(RequestClock::time_point deadline, std::string error_message) {
    if(RequestClock::now() >= deadline) {
        throw std::runtime_error(error_message);
    }
}

/// @brief Configures the socket to wait only until the request deadline.
/// @param connection The socket receiving the request.
/// @param deadline The deadline by which the request must be received.
void configure_receive(
    const Socket& connection,
    RequestClock::time_point deadline
) {
    // Get current time
    const auto now = RequestClock::now();

    // Compare deadline with elapsed time, and reject if passed deadline
    if(now >= deadline) {
        throw std::runtime_error(
            "Request rejected: receive deadline expired"
        );
    }

    // Updates remaining time, rounding up to nearest millisecond
    const auto remaining =
        std::chrono::ceil<std::chrono::milliseconds>(
            deadline - now
        );

    // How long the next recv() can block while waiting for data
    const DWORD timeout_ms = static_cast<DWORD>(remaining.count());

    if(::setsockopt(
        connection.native_handle(),
        SOL_SOCKET,
        SO_RCVTIMEO,
        reinterpret_cast<const char*>(&timeout_ms),
        sizeof(timeout_ms)
    ) == SOCKET_ERROR) {
        throw std::runtime_error(
            "Request rejected: failed to configure receive timeout"
        );
    }
}

/// @brief Configures the socket to send bytes only until the request deadline.
/// @param connection The socket receiving the request.
/// @param deadline The deadline by which the request must be sent.
void configure_send(
    const Socket& connection,
    RequestClock::time_point deadline
) {
    // Get current time
    const auto now = RequestClock::now();

    // Compare deadline with elapsed time, and reject if passed deadline
    if(now >= deadline) {
        throw std::runtime_error(
            "Request rejected: receive deadline expired"
        );
    }

    // Updates remaining time, rounding up to nearest millisecond
    const auto remaining =
        std::chrono::ceil<std::chrono::milliseconds>(
            deadline - now
        );

    // How long the next send() can block while waiting for data
    const DWORD timeout_ms = static_cast<DWORD>(remaining.count());
    
    if(::setsockopt(
        connection.native_handle(),
        SOL_SOCKET,
        SO_SNDTIMEO,
        reinterpret_cast<const char*>(&timeout_ms),
        sizeof(timeout_ms)
    ) == SOCKET_ERROR) {
        throw std::runtime_error(
            "Request rejected: failed to configure receive timeout"
        );
    }
}