#pragma once

#include <cstdint>
#include <cstddef>
#include <atomic>
#include <span>
#include <string>
#include <string_view>
#include <chrono>
#include <system_error>
#include <memory>

#if defined(_WIN32)
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <winsock2.h>
    #include <ws2tcpip.h>
    using socket_t = SOCKET;
    inline constexpr socket_t INVALID_SOCK = INVALID_SOCKET;
#else
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <netinet/tcp.h>
    #include <arpa/inet.h>
    #include <netdb.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <errno.h>
    using socket_t = int;
    inline constexpr socket_t INVALID_SOCK = -1;
#endif

namespace mellzi::net {

// Global network runtime init/cleanup
void init_network();
void cleanup_network();

class SocketGuard {
public:
    SocketGuard() { init_network(); }
    ~SocketGuard() { cleanup_network(); }
};

class TcpStream {
public:
    TcpStream() noexcept : sock_(INVALID_SOCK) {}
    explicit TcpStream(socket_t sock) noexcept : sock_(sock) {}
    ~TcpStream() { close(); }

    TcpStream(const TcpStream&) = delete;
    TcpStream& operator=(const TcpStream&) = delete;

    TcpStream(TcpStream&& other) noexcept : sock_(other.sock_) {
        other.sock_ = INVALID_SOCK;
    }

    TcpStream& operator=(TcpStream&& other) noexcept {
        if (this != &other) {
            close();
            sock_ = other.sock_;
            other.sock_ = INVALID_SOCK;
        }
        return *this;
    }

    static std::unique_ptr<TcpStream> connect(std::string_view host, uint16_t port,
                                              std::chrono::milliseconds timeout = std::chrono::milliseconds(5000));

    [[nodiscard]] bool is_valid() const noexcept {
        return sock_ != INVALID_SOCK;
    }

    void close() noexcept;

    bool set_nodelay(bool enable);
    bool set_timeout(std::chrono::milliseconds timeout);
    bool set_buffer_sizes(int send_size, int recv_size);

    // Send data
    int send(std::span<const uint8_t> data);
    bool send_all(std::span<const uint8_t> data);

    // Receive data
    int receive(std::span<uint8_t> buffer);
    bool receive_exact(std::span<uint8_t> buffer);

    [[nodiscard]] socket_t native_handle() const noexcept { return sock_; }

    [[nodiscard]] std::string peer_address() const;
    [[nodiscard]] uint16_t peer_port() const;

private:
    socket_t sock_{INVALID_SOCK};
};

class TcpListener {
public:
    TcpListener() noexcept : sock_(INVALID_SOCK) {}
    explicit TcpListener(socket_t sock) noexcept : sock_(sock) {}
    ~TcpListener() { close(); }

    TcpListener(const TcpListener&) = delete;
    TcpListener& operator=(const TcpListener&) = delete;

    TcpListener(TcpListener&& other) noexcept : sock_(other.sock_) {
        other.sock_ = INVALID_SOCK;
    }

    TcpListener& operator=(TcpListener&& other) noexcept {
        if (this != &other) {
            close();
            sock_ = other.sock_;
            other.sock_ = INVALID_SOCK;
        }
        return *this;
    }

    static std::unique_ptr<TcpListener> bind(uint16_t port, std::string_view address = "0.0.0.0", int backlog = 10);

    [[nodiscard]] bool is_valid() const noexcept {
        return sock_ != INVALID_SOCK;
    }

    void close() noexcept;

    std::unique_ptr<TcpStream> accept(std::chrono::milliseconds timeout = std::chrono::milliseconds(1000));

    [[nodiscard]] socket_t native_handle() const noexcept { return sock_; }

private:
    socket_t sock_{INVALID_SOCK};
};

} // namespace mellzi::net
