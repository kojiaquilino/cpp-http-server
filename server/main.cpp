#include "http/error.hpp"
#include "http/router.hpp"
#include "net/network.hpp"
#include "http/response.hpp"
#include "net/socket.hpp"
#include "net/socket_io.hpp"
#include "net/network_runtime.hpp"

#include <iostream>
#include <optional>
#include <variant>

int main() {
    try {
        // Start windows socket library
#ifdef _WIN32
        NetworkRuntime runtime;
        std::cout << "Winsock initialized.\n";
#endif
        // Create HTTP router
        Router router;

        // Create listener socket
        Socket listener_socket{
            create_and_bind_listener_socket_to("127.0.0.1", 8080)
        };

        start_listener(listener_socket);

        const auto test_handler = [](const Request&) {
            return Response{200, "OK", "text/plain", "Hello :)"};
        };

        // Register temporary routes
        router.register_handler("GET", "/", test_handler);

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

                    const auto request = static_cast<Request>(value);

                    try {
                        response = router.dispatch(request);
                    } catch(const HTTPError& http_error) {
                        response = Response{RequestError{http_error}};
                    } catch(const std::exception& error) {
                        std::cerr << "An unknown exception occurred: " << error.what() << '\n';
                        response = Response{
                            RequestError{http_errors::internal_server_error(
                                "An unknown error occurred while processing the request."
                            )}
                        };
                    }

                } else if constexpr(std::is_same_v<T, RequestError>) {
                    const auto request_error = static_cast<RequestError>(value);
                    
                    if(request_error.status_code == 0) {
                        std::cerr << "Encountered a fatal socket error: closing connection...\n";
                        close_socket = true;
                        return;
                    }

                    response = {request_error};

                } else {
                    std::cerr << "Unable to process request_result.\n";
                }
            }, request_result);

            // If encountered fatal socket error: close connection
            if(close_socket) {
                continue;
            }

            std::string bytes;

            try {
                bytes = serialize_response(response); // Serialize response to return
            } catch(const std::exception& error) {
                std::cerr << "Response serialization failed: " << error.what() << '\n';
                bytes = serialize_response(Response{
                    RequestError{
                        http_errors::internal_server_error(
                            "The server could not construct a valid response."
                        )
                    }
                });
            }

            try {
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