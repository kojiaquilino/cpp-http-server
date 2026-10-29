#include "net/winsock_runtime.hpp"
#include <iostream>

int main() {
    try {
        WinsockRuntime runtime;
        std::cout << "Winsock initialized.\n";
    } catch (const std::exception& error) {
        std::cerr << "Encountered an error: " << error.what() << '\n';
    }

    return 0;
}