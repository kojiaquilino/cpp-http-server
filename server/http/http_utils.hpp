#pragma once

#include "http/header.hpp"

#include <algorithm>
#include <string>
#include <type_traits>

namespace HTTPUtils {
    // Convert enum class values into underlying integer
    template <typename T>
    constexpr auto to_int(T t) noexcept {
        return static_cast<std::underlying_type_t<T>>(t);
    }

    // header_name is lowercase by default
    constexpr HeaderType classify_header(std::string header_name) {
        using enum HeaderType;
        if(header_name == "host")               return Host;
        if(header_name == "content-length")     return ContentLength;
        if(header_name == "transfer-encoding")  return TransferEncoding;
        if(header_name == "expect")             return Expect;
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