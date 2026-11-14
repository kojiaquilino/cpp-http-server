#pragma once

#include <string>

// Response struct constructed by HTTP server to send to client
// Represents the HTTP response data constructed by the server for a client.
struct Response {
    int status_code;            // Result of request
    std::string reason_phrase;  // Accompanying description of status
    std::string content_type;   // Format of response body
    std::string body;           // Content returned to client
};

// Serializes an HTTP response into a string suitable for transmission to a client.
std::string serialize_response(const Response& response);