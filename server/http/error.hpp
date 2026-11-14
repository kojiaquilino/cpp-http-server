#pragma once

#include <stdexcept>
#include <string>
#include <utility>

class HTTPError : public std::runtime_error {
public:
    int status_code;
    std::string reason_phrase;

    HTTPError(int code, std::string reason, std::string message)
        : std::runtime_error{std::move(message)},
          status_code{code},
          reason_phrase{std::move(reason)} {}
};

namespace http_errors {

    inline HTTPError unknown_error(std::string message) {
        return {0, "Fatal error", std::move(message)};
    }

    inline HTTPError bad_request(std::string message) {
        return {400, "Bad Request", std::move(message)};
    }

    inline HTTPError not_found(std::string message) {
        return {404, "Not Found", std::move(message)};
    }

    inline HTTPError method_not_allowed(std::string message) {
        return {405, "Method Not Allowed", std::move(message)};
    }

    inline HTTPError request_timeout(std::string message) {
        return {408, "Request Timeout", std::move(message)};
    }

    inline HTTPError content_too_large(std::string message) {
        return {413, "Content Too Large", std::move(message)};
    }

    inline HTTPError uri_too_long(std::string message) {
        return {414, "URI Too Long", std::move(message)};
    }

    inline HTTPError unsupported_media_type(std::string message) {
        return {415, "Unsupported Media Type", std::move(message)};
    }

    inline HTTPError expectation_failed(std::string message) {
        return {417, "Expectation Failed", std::move(message)};
    }

    inline HTTPError unprocessable_content(std::string message) {
        return {422, "Unprocessable Content", std::move(message)};
    }

    inline HTTPError headers_too_large(std::string message) {
        return {431, "Request Header Fields Too Large", std::move(message)};
    }

    inline HTTPError internal_server_error(std::string message) {
        return {500, "Internal Server Error", std::move(message)};
    }

    inline HTTPError not_implemented(std::string message) {
        return {501, "Not Implemented", std::move(message)};
    }

    inline HTTPError service_unavailable(std::string message) {
        return {503, "Service Unavailable", std::move(message)};
    }

    inline HTTPError version_not_supported(std::string message) {
        return {505, "HTTP Version Not Supported", std::move(message)};
    }
}