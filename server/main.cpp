#include "http/error.hpp"
#include "net/network.hpp"
#include "http/response.hpp"
#include "net/socket.hpp"
#include "net/socket_io.hpp"
#include "net/winsock_runtime.hpp"

#include <iostream>
#include <optional>
#include <variant>

int main() {
    try {
        // Start windows socket library
        WinsockRuntime runtime;
        std::cout << "Winsock initialized.\n";

        // Create listener socket
        Socket listener_socket{
            create_and_bind_listener_socket_to("127.0.0.1", 8080)
        };

        start_listener(listener_socket);

        // Receive and accept incoming connections
        // Currently closes accepted socket after receiving one request
        while(true) {
            // Block until accepting new incoming connection
            Socket connection{accept_incoming_connection(listener_socket)};

            // Blocks until receiving an HTTP request on the connected socket
            RequestData request_result{receive_request(connection)};
            Response response;
            bool close_socket = false;

            std::visit([&](const auto& value) {
                using T = std::decay_t<decltype(value)>;

                if constexpr(std::is_same_v<T, Request>) {
                    response = {
                        200,
                        "OK",
                        "text/plain",
                        "Hello"
                    };
                } else if constexpr(std::is_same_v<T, RequestError>) {
                    auto request_error = static_cast<RequestError>(value);
                    
                    if(request_error.status_code == 0) {
                        std::cerr << "Encountered a fatal socket error: closing connection...\n";
                        close_socket = true;
                        return;
                    }

                    response = {
                        request_error.status_code,
                        request_error.reason_phrase,
                        "text/plain",
                        request_error.message
                    };

                } else {
                    std::cerr << "Unable to process request_result: exiting program...\n";
                    return 1;
                }
            }, request_result);

            // If encountered fatal socket error: close connection
            if(close_socket) {
                continue;
            }

            try {
                const auto bytes = serialize_response(response);
                send_response(connection, bytes);

            } catch(const std::exception& error) {
                std::cerr << "Response send failed: " << error.what() << '\n';
            }
        }

    } catch(const std::exception& error) {
        std::cerr << "Encountered an error: " << error.what() << '\n';
        return 1;
    }

    return 0;
}