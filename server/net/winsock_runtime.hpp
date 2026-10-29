#pragma once

class WinsockRuntime
{
public:
    WinsockRuntime();           // Declare constructor
    ~WinsockRuntime() noexcept; // Declare destructor with noexcept promise

    // Disable forms of duplicating object:
    // should only be one existing socket at all times per socket
    WinsockRuntime(const WinsockRuntime &) = delete;            // Disable copy constructor
    WinsockRuntime &operator=(const WinsockRuntime &) = delete; // Disable copy assignment operator
};