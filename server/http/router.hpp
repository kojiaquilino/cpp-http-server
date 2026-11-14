#pragma once

#include <string>
#include <functional>
#include <unordered_map>
#include <utility>

#include "http/response.hpp"
#include "http/request.hpp"

using Handler = std::function<Response(const Request&)>;

class Router {
public:
    // Registers a handler for handling incoming HTTP requests to the provided endpoint
    void register_handler(std::string method, std::string path, Handler handler);

    // Queries the backend via the HTTP server for a response given a client request
    Response dispatch(const Request& request) const;

private:
    // {method, path} -> handler function
    std::unordered_map<std::pair<std::string, std::string>, Handler> routes_;
    std::unordered_map<std::string, std::vector<std::string>> allowed_methods_;
};