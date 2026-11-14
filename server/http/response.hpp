#pragma once

#include "http/error.hpp"

#include <string>

// Response struct constructed by HTTP server to send to client
// Represents the HTTP response data constructed by the server for a client.
struct Response {
    int status_code;                     // Result of request
    std::string reason_phrase;           // Accompanying description of status
    std::string content_type;            // Format of response body
    std::string body;                    // Content returned to client
    std::vector<Header> extra_headers{}; // Allowed methods for path + additional headers

    // Default constructor
    Response() = default;

    // Element-wise constructor
    Response(
        int status_code,
        std::string reason_phrase,
        std::string content_type,
        std::string body,
        std::vector<Header> extra_headers
    ) 
        : status_code{status_code},
          reason_phrase{reason_phrase},
          content_type{content_type},
          body{body},
          extra_headers{extra_headers} {}

    // Constructor for error responses
    Response(RequestError error, std::vector<std::string> allowed_methods = {})
        : status_code{error.status_code},
          reason_phrase{error.reason_phrase},
          content_type{"text/plain"},
          body{error.message},
          extra_headers{} {allowed_methods}
};

// Serializes an HTTP response into a string suitable for transmission to a client.
std::string serialize_response(const Response& response);