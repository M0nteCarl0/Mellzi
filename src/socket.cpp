#include "mellzi/socket.h"
#include <iostream>
#include <cstring>
#include <atomic>

#if defined(_WIN32)
    #pragma comment(lib, "ws2_32.lib")
#endif

namespace mellzi::net {

namespace {
    std::atomic<int> g_network_ref_count{0};
}

void init_network() {
#if defined(_WIN32)
    if (g_network_ref_count.fetch_add(1) == 0) {
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
            std::cerr << "[Mellzi::Net] WSAStartup failed\n";
        }
    }
#else
    g_network_ref_count.fetch_add(1);
#endif
}

void cleanup_network() {
#if defined(_WIN32)
    if (g_network_ref_count.fetch_sub(1) == 1) {
        WSACleanup();
    }
#else
    g_network_ref_count.fetch_sub(1);
#endif
}

void TcpStream::close() noexcept {
    if (sock_ != INVALID_SOCK) {
#if defined(_WIN32)
        closesocket(sock_);
#else
        ::close(sock_);
#endif
        sock_ = INVALID_SOCK;
    }
}

bool TcpStream::set_nodelay(bool enable) {
    if (sock_ == INVALID_SOCK) return false;
    int opt = enable ? 1 : 0;
    return ::setsockopt(sock_, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&opt), sizeof(opt)) == 0;
}

bool TcpStream::set_timeout(std::chrono::milliseconds timeout) {
    if (sock_ == INVALID_SOCK) return false;
#if defined(_WIN32)
    DWORD tv = static_cast<DWORD>(timeout.count());
    bool ok_rcv = ::setsockopt(sock_, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&tv), sizeof(tv)) == 0;
    bool ok_snd = ::setsockopt(sock_, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&tv), sizeof(tv)) == 0;
    return ok_rcv && ok_snd;
#else
    struct timeval tv;
    tv.tv_sec = static_cast<time_t>(timeout.count() / 1000);
    tv.tv_usec = static_cast<suseconds_t>((timeout.count() % 1000) * 1000);
    bool ok_rcv = ::setsockopt(sock_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) == 0;
    bool ok_snd = ::setsockopt(sock_, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) == 0;
    return ok_rcv && ok_snd;
#endif
}

bool TcpStream::set_buffer_sizes(int send_size, int recv_size) {
    if (sock_ == INVALID_SOCK) return false;
    bool ok_snd = ::setsockopt(sock_, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<const char*>(&send_size), sizeof(send_size)) == 0;
    bool ok_rcv = ::setsockopt(sock_, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<const char*>(&recv_size), sizeof(recv_size)) == 0;
    return ok_snd && ok_rcv;
}

int TcpStream::send(std::span<const uint8_t> data) {
    if (sock_ == INVALID_SOCK || data.empty()) return 0;
#if defined(_WIN32)
    int n = ::send(sock_, reinterpret_cast<const char*>(data.data()), static_cast<int>(data.size()), 0);
    return n == SOCKET_ERROR ? -1 : n;
#else
    ssize_t n = ::send(sock_, data.data(), data.size(), 0);
    return n < 0 ? -1 : static_cast<int>(n);
#endif
}

bool TcpStream::send_all(std::span<const uint8_t> data) {
    size_t sent = 0;
    while (sent < data.size()) {
        int n = send(data.subspan(sent));
        if (n <= 0) {
            return false;
        }
        sent += static_cast<size_t>(n);
    }
    return true;
}

int TcpStream::receive(std::span<uint8_t> buffer) {
    if (sock_ == INVALID_SOCK || buffer.empty()) return 0;
#if defined(_WIN32)
    int n = ::recv(sock_, reinterpret_cast<char*>(buffer.data()), static_cast<int>(buffer.size()), 0);
    return n == SOCKET_ERROR ? -1 : n;
#else
    ssize_t n = ::recv(sock_, buffer.data(), buffer.size(), 0);
    return n < 0 ? -1 : static_cast<int>(n);
#endif
}

bool TcpStream::receive_exact(std::span<uint8_t> buffer) {
    size_t received = 0;
    while (received < buffer.size()) {
        int n = receive(buffer.subspan(received));
        if (n <= 0) {
            return false;
        }
        received += static_cast<size_t>(n);
    }
    return true;
}

std::string TcpStream::peer_address() const {
    if (sock_ == INVALID_SOCK) return "";
    sockaddr_in addr{};
    socklen_t len = sizeof(addr);
    if (::getpeername(sock_, reinterpret_cast<sockaddr*>(&addr), &len) == 0) {
        char buf[INET_ADDRSTRLEN]{0};
        ::inet_ntop(AF_INET, &addr.sin_addr, buf, sizeof(buf));
        return std::string(buf);
    }
    return "";
}

uint16_t TcpStream::peer_port() const {
    if (sock_ == INVALID_SOCK) return 0;
    sockaddr_in addr{};
    socklen_t len = sizeof(addr);
    if (::getpeername(sock_, reinterpret_cast<sockaddr*>(&addr), &len) == 0) {
        return ntohs(addr.sin_port);
    }
    return 0;
}

std::unique_ptr<TcpStream> TcpStream::connect(std::string_view host, uint16_t port,
                                              std::chrono::milliseconds timeout) {
    init_network();

    std::string host_str(host);
    std::string port_str = std::to_string(port);

    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    addrinfo* result = nullptr;
    if (::getaddrinfo(host_str.c_str(), port_str.c_str(), &hints, &result) != 0 || !result) {
        return nullptr;
    }

    socket_t s = ::socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (s == INVALID_SOCK) {
        ::freeaddrinfo(result);
        return nullptr;
    }

    auto stream = std::make_unique<TcpStream>(s);

    // Set non-blocking to support connection timeout
#if defined(_WIN32)
    u_long mode = 1;
    ::ioctlsocket(s, FIONBIO, &mode);
#else
    int flags = ::fcntl(s, F_GETFL, 0);
    ::fcntl(s, F_SETFL, flags | O_NONBLOCK);
#endif

    int res = ::connect(s, result->ai_addr, static_cast<int>(result->ai_addrlen));
    ::freeaddrinfo(result);

    bool connected = false;
    if (res == 0) {
        connected = true;
    } else {
        fd_set write_fds;
        FD_ZERO(&write_fds);
        FD_SET(s, &write_fds);

        timeval tv;
        tv.tv_sec = static_cast<long>(timeout.count() / 1000);
        tv.tv_usec = static_cast<long>((timeout.count() % 1000) * 1000);

        int sel = ::select(static_cast<int>(s + 1), nullptr, &write_fds, nullptr, &tv);
        if (sel > 0 && FD_ISSET(s, &write_fds)) {
            int err = 0;
            socklen_t err_len = sizeof(err);
            if (::getsockopt(s, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&err), &err_len) == 0 && err == 0) {
                connected = true;
            }
        }
    }

    // Set back to blocking
#if defined(_WIN32)
    mode = 0;
    ::ioctlsocket(s, FIONBIO, &mode);
#else
    flags = ::fcntl(s, F_GETFL, 0);
    ::fcntl(s, F_SETFL, flags & ~O_NONBLOCK);
#endif

    if (!connected) {
        return nullptr;
    }

    stream->set_nodelay(true);
    stream->set_timeout(timeout);
    stream->set_buffer_sizes(16 * 1024 * 1024, 16 * 1024 * 1024);
    return stream;
}

void TcpListener::close() noexcept {
    if (sock_ != INVALID_SOCK) {
#if defined(_WIN32)
        closesocket(sock_);
#else
        ::close(sock_);
#endif
        sock_ = INVALID_SOCK;
    }
}

std::unique_ptr<TcpListener> TcpListener::bind(uint16_t port, std::string_view address, int backlog) {
    init_network();

    socket_t s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCK) {
        return nullptr;
    }

    int opt = 1;
    ::setsockopt(s, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (address.empty() || address == "0.0.0.0") {
        addr.sin_addr.s_addr = INADDR_ANY;
    } else {
        std::string addr_str(address);
        ::inet_pton(AF_INET, addr_str.c_str(), &addr.sin_addr);
    }

    if (::bind(s, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) != 0) {
#if defined(_WIN32)
        closesocket(s);
#else
        ::close(s);
#endif
        return nullptr;
    }

    if (::listen(s, backlog) != 0) {
#if defined(_WIN32)
        closesocket(s);
#else
        ::close(s);
#endif
        return nullptr;
    }

    return std::make_unique<TcpListener>(s);
}

std::unique_ptr<TcpStream> TcpListener::accept(std::chrono::milliseconds timeout) {
    if (sock_ == INVALID_SOCK) return nullptr;

    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(sock_, &read_fds);

    timeval tv;
    tv.tv_sec = static_cast<long>(timeout.count() / 1000);
    tv.tv_usec = static_cast<long>((timeout.count() % 1000) * 1000);

    int sel = ::select(static_cast<int>(sock_ + 1), &read_fds, nullptr, nullptr, &tv);
    if (sel <= 0 || !FD_ISSET(sock_, &read_fds)) {
        return nullptr;
    }

    sockaddr_in client_addr{};
    socklen_t client_len = sizeof(client_addr);
    socket_t client_sock = ::accept(sock_, reinterpret_cast<sockaddr*>(&client_addr), &client_len);
    if (client_sock == INVALID_SOCK) {
        return nullptr;
    }

    auto stream = std::make_unique<TcpStream>(client_sock);
    stream->set_nodelay(true);
    stream->set_buffer_sizes(16 * 1024 * 1024, 16 * 1024 * 1024);
    return stream;
}

} // namespace mellzi::net
