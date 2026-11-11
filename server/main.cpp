#define NOMINMAX

#include "http/response.hpp"
#include "net/network.hpp"
#include "net/socket.hpp"
#include "net/socket_io.hpp"
#include "net/winsock_runtime.hpp"

#include <iostream>
#include <optional>

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
            std::optional<Request> parsed_request{receive_request(connection)};

            if(!parsed_request.has_value()) {
                std::cerr << "Failed to complete latest HTTP request.\n";
                continue;
            }

            try {
                const Response response{
                    200,
                    "OK",
                    "text/plain",
                    "Hello"
                };

                const auto bytes = serialize_response(response);
                send_response(connection, bytes);
            } catch(const std::exception& error) {
                std::cerr << "Response send failed: " << error.what() << '\n';
            }
        }

        // std::cout << "Cleaning up...\n";

    } catch (const std::exception& error) {
        std::cerr << "Encountered an error: " << error.what() << '\n';
        return 1;
    }

    return 0;
}