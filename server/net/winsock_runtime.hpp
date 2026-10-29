#pragma once

class WinsockRuntime
{
public:
    WinsockRuntime();           // Declare constructor
    ~WinsockRuntime() noexcept; // Declare destructor with noexcept promise

    // Prevent copying: each owner pairs one successful
    // WSAStartup call with one WSACleanup call.
    WinsockRuntime(const WinsockRuntime &) = delete;            // Disable copy constructor
    WinsockRuntime &operator=(const WinsockRuntime &) = delete; // Disable copy assignment operator
};