#include "http/error.hpp"
#include "http/request.hpp"
#include "http/request_line.hpp"
#include "http/router.hpp"

#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

void Router::register_handler(std::string method, std::string path, Handler handler) {

    // Check for invalid method string
    if(request_line::has_invalid_method_character(method)) {
        throw std::invalid_argument("Method contains invalid characters");
    }

    // Check for invalid path string
    if(request_line::has_invalid_target_character(path)) {
        throw std::invalid_argument("Path contains invalid characters");
    }

    // Check for prefixed '/' in path
    if(path.front() != '/') {
        throw std::invalid_argument("Requested path must start with '/'.");
    }

    // Check for invalid/empty handler function
    if(!handler) {
        throw std::invalid_argument{"Route handler cannot be empty."};
    }

    // Check for duplicate registration
    if(routes_.find(std::pair(method, path)) != routes_.end()) {
        throw std::invalid_argument{"Route is already registered."};
    }

    routes_[std::pair(method, path)] = handler;
    allowed_methods_[path].push_back(method);
}

Response Router::dispatch(const Request& request) const {
    auto [method, path, _] = request.request_line;
    
    // Check if path exists (has at least one associated handler)
    if(allowed_methods_.find(path) == allowed_methods_.end()) {
        const RequestError error{http_errors::not_found("Path does not exist.")};
        return {error};
    }

    // Check if path exists but method is disallowed
    if(routes_.find(std::pair(method, path)) == routes_.end()) {
        const auto allowed = allowed_methods_.at(path);
        const RequestError error{http_errors::method_not_allowed("The requested method is not allowed for this path.")};
        return {error, allowed};
    }

    // Retrieve endpoint handler
    const Handler handler = routes_.at(std::pair(method, path));
    
    try {
        return handler(request);
    }
    
    // for now, catch all server-side exceptions and return status code 500
    catch(const std::exception&) {
        const RequestError error{http_errors::internal_server_error("An error occurred when processing the request.")};
        return {error};
    }
}

