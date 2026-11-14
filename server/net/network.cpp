#include "http/error.hpp"
#include "net/network.hpp"
#include "net/platform.hpp"
#include "net/socket.hpp"

#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>
#include <cerrno>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#endif

using RequestClock = std::chrono::steady_clock;

namespace {

    // Calls platform-specific socket error function
    int last_socket_error() noexcept {
#ifdef _WIN32
        return WSAGetLastError();
#else
        return errno;
#endif
    }

    // Checks if a network operation returned a socket error or not.
    // Throws a std::runtime_error if it did return a socket error
    void check_socket_error(int result, const std::string& operation) {
        if(result == -1) {
            const int error{last_socket_error()};

            throw SocketError{
                operation + "() failed with error code: " + std::to_string(error)
            };
        }
    }

    // Configures socket operation timeout
    // Changes based on platform
    void set_socket_timeout(
        const Socket& connection,
        int option,
        std::chrono::milliseconds remaining
    ) {
#ifdef _WIN32
        const DWORD timeout{
            static_cast<DWORD>(remaining.count())
        };

        const int result{
            ::setsockopt(
                connection.native_handle(),                 // Socket to modify
                SOL_SOCKET,                                 // Specify option belonging to socket layer
                option,                                     // Set option's timeout
                reinterpret_cast<const char*>(&timeout),    // Timeout value: default=10000ms (address of value)
                static_cast<int>(sizeof(timeout))           // Size of option value (in this case, timeout)
            )
        };
#else
        timeval timeout{};
        timeout.tv_sec =
            static_cast<decltype(timeout.tv_sec)>(
                remaining.count() / 1000
            );
        timeout.tv_usec =
            static_cast<decltype(timeout.tv_usec)>(
                (remaining.count() % 1000) * 1000
            );

        const int result{
            ::setsockopt(
                connection.native_handle(),             // Socket to modify
                SOL_SOCKET,                             // Specify option belonging to socket layer
                option,                                 // Set option's timeout
                &timeout,                               // Timeout value: default=10000ms (address of value)
                static_cast<socklen_t>(sizeof(timeout)) // Size of option value (in this case, timeout)
            )
        };
#endif
        check_socket_error(result, "setsockopt");
    }



    // Configures an accepted incoming connection with corresponding 
    // timeouts for send and receive socket operations.
    Socket configure_new_connection(const net::NativeSocket& accepted_handle) {

        if(accepted_handle == INVALID_SOCKET) {
            throw std::runtime_error(
                "accept() failed with error code: " + std::to_string(WSAGetLastError())
            );
        }

        // Wrap accepted handle
        Socket connection{accepted_handle};
        set_socket_timeout(connection, SO_RCVTIMEO, default_timeout); // configure recv()'s timeout
        set_socket_timeout(connection, SO_SNDTIMEO, default_timeout); // configure send()'s timeout
        return connection;
    }
}


Socket create_and_bind_listener_socket_to(std::string bind_ip, int bind_port) {
    // Create raw listener socket and store handle
    net::NativeSocket raw_listener_handle{
        ::socket(
            AF_INET,
            SOCK_STREAM,
            IPPROTO_TCP
        )
    };

    if(raw_listener_handle == net::invalid_socket) {
        throw std::runtime_error("socket() failed with error code: " + std::to_string(last_socket_error()));
    }

    // Create listener socket wrapper
    Socket listener{raw_listener_handle};
    sockaddr_in address{};               // IPv4 socket address structure zero-initialized
    address.sin_family = AF_INET;        // Specify IPv4 address
    address.sin_port = htons(bind_port); // Set port and convert host integer byte order to network byte order (match endianness)
    
// Converts address to binary representation (conversion is platform-specific)
// Used for validating address to bind socket to
#ifdef _WIN32
    const int converted_bind_address{
        ::InetPtonA(
            AF_INET,            // Address family: IPv4
            bind_ip.c_str(),    // IP address to bind listener socket to (to listen on)
            &address.sin_addr   // Output buffer for binary address
        )
    };
#else
    const int converted_bind_address{
        ::inet_pton(
            AF_INET,            // Address family: IPv4
            bind_ip.c_str(),    // IP address to bind listener socket to (to listen on)
            &address.sin_add    // Output buffer for binary address
        )
    };
#endif
    if(converted_bind_address != 1) {
        throw std::invalid_argument{"Invalid IPv4 bind address."};
    }

    // Check for address conversion success
    if(converted_bind_address != 1) {
        throw std::runtime_error("InetPtonA() failed to convert to binary IPv4 format, "
            "returning code: " + std::to_string(converted_bind_address));
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

Socket accept_incoming_connection(const Socket& listener) {
    // Block until accepting a new incoming connection.
    net::NativeSocket accepted_handle{
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

void check_request_deadline(RequestClock::time_point deadline, std::string error_message) {
    if(RequestClock::now() >= deadline) {
        throw std::runtime_error(error_message);
    }
}

void configure_receive(
    const Socket& connection,
    RequestClock::time_point deadline
) {
    // Get current time
    const auto now = RequestClock::now();

    // Compare deadline with elapsed time, and reject if passed deadline
    if(now >= deadline) {
        throw http_errors::request_timeout(
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
        throw SocketError(
            "Request rejected: failed to configure receive timeout"
        );
    }
}

void configure_send(
    const Socket& connection,
    RequestClock::time_point deadline
) {
    // Get current time
    const auto now = RequestClock::now();

    // Compare deadline with elapsed time, and reject if passed deadline
    if(now >= deadline) {
        throw http_errors::request_timeout(
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
        throw SocketError(
            "Request rejected: failed to configure send timeout"
        );
    }
}