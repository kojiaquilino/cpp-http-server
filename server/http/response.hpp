#pragma once

#include <string>

// Response struct constructed by HTTP server to send to client
struct Response {
    int status_code;            // Result of request
    std::string reason_phrase;  // Accompanying description of status
    std::string content_type;   // Format of resposne body
    std::string body;           // Content returned to client
};

std::string serialize_response(const Response& response);