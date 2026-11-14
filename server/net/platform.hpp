#pragma once

#ifdef _WIN32
    #include <winsock2.h>
#else
    #include <sys/socket.h>
    #include <unistd.h>
#endif

namespace net {
#ifdef _WIN32 // Windows platform
    using NativeSocket = SOCKET;
    inline constexpr NativeSocket invalid_socket{INVALID_SOCKET};
#else // macOS, Linux platforms
    using NativeSocket = int;
    inline constexpr NativeSocket invalid_socket{-1};
#endif
}