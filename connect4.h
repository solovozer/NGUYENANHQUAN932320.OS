#define _POSIX_C_SOURCE 200809L
#include <cerrno>
#include <csignal>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>


static constexpr int DEFAULT_PORT = 12345;
static constexpr std::size_t BUF_SIZE = 4096;

static volatile sig_atomic_t hup_received = 0;

extern "C" void sighup_handler(int /*signum*/) {
    const char msg[] = "sighup handler: received SIGHUP\n";
    ssize_t _ = write(STDOUT_FILENO, msg, sizeof(msg) - 1);
    (void)_;
    hup_received = 1;
}

int make_listen_socket(int port) {
    int lf = ::socket(AF_INET, SOCK_STREAM, 0);
    if (lf < 0) {
        std::perror("socket");
        return -1;
    }

    int on = 1;
    if (setsockopt(lf, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) {
        std::perror("setsockopt");
        ::close(lf);
        return -1;
    }

    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(static_cast<uint16_t>(port));

    if (bind(lf, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::perror("bind");
        ::close(lf);
        return -1;
    }

    if (listen(lf, 16) < 0) {
        std::perror("listen");
        ::close(lf);
        return -1;
    }

    return lf;
}

void Test2(int argc, char* argv[]) {
    int port = DEFAULT_PORT;
    if (argc >= 2) {
        try {
            int p = std::stoi(argv[1]);
            if (p > 0 && p <= 65535) port = p;
        }
        catch (...) {
        }
    }

    int listenfd = make_listen_socket(port);
    if (listenfd < 0) return EXIT_FAILURE;

    std::cout << "Listening on port " << port << " (fd " << listenfd << ")\n";

    struct sigaction sa;
    std::memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sighup_handler;
    sa.sa_flags = 0; // do not set SA_RESTART intentionally
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGHUP, &sa, nullptr) < 0) {
        std::perror("sigaction");
        ::close(listenfd);
        return EXIT_FAILURE;
    }

    sigset_t block_mask, orig_mask;
    sigemptyset(&block_mask);
    sigaddset(&block_mask, SIGHUP);
    if (sigprocmask(SIG_BLOCK, &block_mask, &orig_mask) < 0) {
        std::perror("sigprocmask");
        ::close(listenfd);
        return EXIT_FAILURE;
    }

    int clientfd = -1;

    for (;;) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(listenfd, &readfds);
        int maxfd = listenfd;
        if (clientfd >= 0) {
            FD_SET(clientfd, &readfds);
            if (clientfd > maxfd) maxfd = clientfd;
        }
        int nfds = maxfd + 1;

        int ready = pselect(nfds, &readfds, nullptr, nullptr, nullptr, &orig_mask);
        if (ready < 0) {
            if (errno == EINTR) {
            }
            else {
                std::perror("pselect");
                break;
            }
        }

        if (hup_received) {
            std::cout << "Main: detected SIGHUP (hup_received). Handling in main thread.\n";
            hup_received = 0;
        }

        if (FD_ISSET(listenfd, &readfds)) {
            sockaddr_in peer;
            socklen_t plen = sizeof(peer);
            int newfd = accept(listenfd, reinterpret_cast<sockaddr*>(&peer), &plen);
            if (newfd < 0) {
                std::perror("accept");
            }
            else {
                char peerstr[INET_ADDRSTRLEN] = { 0 };
                inet_ntop(AF_INET, &peer.sin_addr, peerstr, sizeof(peerstr));
                std::cout << "New connection from " << peerstr << ":" << ntohs(peer.sin_port)
                    << " (fd " << newfd << ")\n";

                if (clientfd < 0) {
                    clientfd = newfd;
                }
                else {
                    std::cout << "Already have a client; closing new connection (fd " << newfd << ")\n";
                    ::close(newfd);
                }
            }
        }
        if (clientfd >= 0 && FD_ISSET(clientfd, &readfds)) {
            std::vector<char> buf(BUF_SIZE);
            ssize_t n = ::read(clientfd, buf.data(), static_cast<size_t>(buf.size()));
            if (n < 0) {
                std::perror("read from client");
                ::close(clientfd);
                clientfd = -1;
            }
            else if (n == 0) {
                std::cout << "Client (fd " << clientfd << ") closed connection\n";
                ::close(clientfd);
                clientfd = -1;
            }
            else {
                std::cout << "Received " << n << " bytes from client (fd " << clientfd << ")\n";
            }
        }
    }

    if (clientfd >= 0) ::close(clientfd);
    ::close(listenfd);
    return 0;
}
