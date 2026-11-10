#include "http/response.hpp"

#include <algorithm>
#include <format>
#include <stdexcept>
#include <string>

std::string serialize_response(const Response& response) {
    std::string serialized{};
    auto [status_code, reason_phrase, content_type, body] = response;
    const auto validate_content = [](const std::string& content) { // prevent control characters from corrupting response headers
        return std::all_of(
            content.begin(),
            content.end(),
            [](unsigned char c) {
                return c >= 32 && c != 127;
            }
        );
    };

    // Validate response parameters
    if(status_code < 100 || status_code > 599)                    throw std::invalid_argument("Invalid status code: outside range 100-599");
    if(reason_phrase.empty() || !validate_content(reason_phrase)) throw std::invalid_argument("Invalid reason phrase: contains invalid characters or is empty");
    if(content_type.empty()  || !validate_content(content_type))  throw std::invalid_argument("Invalid content type: contains invalid characters or is empty");


    // Begin constructing serialized response once validated

    // Append status line of response
    serialized.append(
        std::format("HTTP/1.1 {} {}\r\n", status_code, reason_phrase)
    );

    // Append headers: Content-Type, Content-Length, Connection
    serialized.append(
        std::format(
            "Content-Type: {}\r\n"
            "Content-Length: {}\r\n"
            "Connection: close\r\n",
            content_type, body.size() 
        )
    );

    // Divide headers and body of response
    serialized.append("\r\n");

    // Append body bytes to response
    serialized.append(body);

    return serialized;
}