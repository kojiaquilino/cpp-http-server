#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

enum class HeaderType {
    Host,
    ContentLength,
    TransferEncoding,
    Other
};

// Number of available options for HeaderType
inline constexpr std::size_t header_count{4};

struct Header {
    std::string name;
    std::string value;
};

struct RequestFraming {
    std::optional<std::size_t> content_length;
};

Header parse_header_line(std::string_view line);

RequestFraming validate_request_headers(const std::vector<Header>& headers);