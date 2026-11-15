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
    if(method.empty()) {
        throw std::invalid_argument("Method cannot be empty.");
    } else if(request_line::has_invalid_method_character(method)) {
        throw std::invalid_argument("Method contains invalid characters.");
    }

    // Check for invalid path string
    if(path.empty()) {
        throw std::invalid_argument("Path cannot be empty.");
    } else if(request_line::has_invalid_target_character(path) || path.find('?') != std::string::npos) {
        throw std::invalid_argument("Path contains invalid characters.");
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
    auto target_path = path.substr(0, path.find('?'));

    // Check if path exists (has at least one associated handler)
    if(allowed_methods_.find(target_path) == allowed_methods_.end()) {
        const RequestError error{http_errors::not_found("Path does not exist.")};
        return Response{error};
    }

    // Check if path exists but method is disallowed
    if(routes_.find(std::pair(method, target_path)) == routes_.end()) {
        const auto allowed = allowed_methods_.at(target_path);
        const RequestError error{http_errors::method_not_allowed("The requested method is not allowed for this path.")};
        return Response{error, allowed};
    }

    // Retrieve endpoint handler
    const Handler handler = routes_.at(std::pair(method, target_path));
    
    try {
        return handler(request);
    } catch(const HTTPError&) {
        throw;
    } catch(const std::exception&) {
        const RequestError error{http_errors::internal_server_error("An error occurred when processing the request.")};
        return Response{error};
    }
}

