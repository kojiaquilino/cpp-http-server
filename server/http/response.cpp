#include "http/http_utils.hpp"
#include "http/response.hpp"

#include <algorithm>
#include <format>
#include <stdexcept>
#include <string>

std::string serialize_response(const Response& response) {
    std::string serialized{};
    std::vector<Header> parsed_extra_headers{};
    auto [status_code, reason_phrase, content_type, body, extra_headers] = response;
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

    // Parsing headers from string representations
    for(const auto& string_header : extra_headers) {
        try {
            const auto parsed_header = parse_header_line(string_header);
            
            // If parsed header is a duplicate header (of a reserved type), throw HTTPError
            const auto normalized_name = HTTPUtils::lowercase(parsed_header.name);

            if (std::find(
                    reserved_headers.begin(),
                    reserved_headers.end(),
                    normalized_name
                ) != reserved_headers.end()) {
                throw http_errors::bad_request("Cannot contain duplicate " + parsed_header.name + " header.");
            }

            parsed_extra_headers.push_back(parsed_header);
            
        } catch(const HTTPError&) {
            throw;
        }
    }

    // Adding extra headers to serialized response
    for(const auto& header: parsed_extra_headers) {
        serialized += header.name + ": " + header.value + "\r\n";
    }
    
    serialized.append("\r\n");  // Separates headers and body
    serialized.append(body);    // Append body to response

    return serialized;
}