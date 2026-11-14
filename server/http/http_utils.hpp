#pragma once

#include "http/header.hpp"

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>
#include <type_traits>

namespace HTTPUtils {
    inline std::string lowercase(std::string_view value) {
        std::string result{value};

        for(auto& character : result) {
            character = static_cast<char>(
                std::tolower(static_cast<unsigned char>(character))
            );
        }

        return result;
    }

    // Convert enum class values into underlying integer
    template <typename T>
    constexpr auto to_int(T t) noexcept {
        return static_cast<std::underlying_type_t<T>>(t);
    }

    // Header names are compared case-insensitively
    inline HeaderType classify_header(std::string_view header_name) {
        const auto normalized_name = lowercase(header_name);
        using enum HeaderType;
        if(normalized_name == "host")              return Host;
        if(normalized_name == "content-length")    return ContentLength;
        if(normalized_name == "transfer-encoding") return TransferEncoding;
        if(normalized_name == "allow")             return Allow;
        if(normalized_name == "expect")            return Expect;
        return HeaderType::Other; // generic default header
    }

    // Convert enum to string
    constexpr std::string header_to_string(HeaderType header) {
        switch(header) {
            using enum HeaderType;
            case Host:              return "Host";
            case ContentLength:     return "Content-Length";
            case TransferEncoding:  return "Transfer-Encoding";
            case Expect:            return "Expect";
        }
        return "Other";
    }

    // Trimming a string
    inline void ltrim(std::string& str) {
        str.erase(str.begin(), std::find_if(str.begin(), str.end(), [](unsigned char ch) {
            return ch != ' ' && ch != '\t'; // Find first nonspace character
        }));
    }

    inline void rtrim(std::string& str) {
        str.erase(std::find_if(str.rbegin(), str.rend(), [](unsigned char ch) {
            return ch != ' ' && ch != '\t'; // Find first nonspace character from the back
        }).base(), str.end());
    }

    inline void trim(std::string& str) {
        ltrim(str);
        rtrim(str);
    }
}