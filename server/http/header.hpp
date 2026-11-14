#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>


constexpr std::size_t max_body_bytes{12 * 1024 * 1024}; // Byte limit for Content-Length
constexpr std::size_t header_count{5};                  // Number of available options for HeaderType

enum class HeaderType {
    Host,
    ContentLength,
    TransferEncoding,
    Expect,
    Other
};

// Represents an HTTP header as a name-value pair.
struct Header {
    std::string name;
    std::string value;
};

// Stores the framing information determined from an HTTP request's headers.
struct RequestFraming {
    std::optional<std::size_t> content_length;
};

// Parses a single HTTP header line into its name and value components.
Header parse_header_line(std::string_view line);

// Validates request headers and determines the request's framing information.
RequestFraming validate_request_headers(const std::vector<Header>& headers);