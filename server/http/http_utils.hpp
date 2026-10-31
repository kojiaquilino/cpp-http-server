#pragma once

#include <string>

// Trimming a string

inline void ltrim(std::string& str) {
    str.erase(str.begin(), std::find_if(str.begin(), str.end(), [](unsigned char ch) {
        return !std::isspace(ch); // Find first nonspace character
    }));
}

inline void rtrim(std::string& str) {
    str.erase(std::find_if(str.rbegin(), str.rend(), [](unsigned char ch) {
        return !std::isspace(ch); // Find first nonspace character from the back
    }).base(), str.end());
}

inline void trim(std::string& str) {
    ltrim(str);
    rtrim(str);
}