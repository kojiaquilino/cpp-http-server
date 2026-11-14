#pragma once

class NetworkRuntime
{
public:
    NetworkRuntime();           // Declare constructor
    ~NetworkRuntime() noexcept; // Declare destructor with noexcept promise

    // Prevent copying: each owner pairs one successful
    // WSAStartup call with one WSACleanup call.
    NetworkRuntime(const NetworkRuntime &) = delete;            // Disable copy constructor
    NetworkRuntime &operator=(const NetworkRuntime &) = delete; // Disable copy assignment operator
};