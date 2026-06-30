#include "../include/system_headers.hpp" // IWYU pragma: keep

inline constexpr size_t MAX_HOSTNAME = 50;
inline constexpr size_t RLEN = 4096;

int get_address(const char *host, const char *port, sockaddr *addr) {
  addrinfo hints;
  addrinfo *res;

  std::memset(&hints, 0, sizeof(addrinfo));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  int s = getaddrinfo(host, port, &hints, &res);
  if (s != 0) {
    std::cerr << "ERR(getaddrinfo): " << gai_strerror(s) << '\n';
    exit(EXIT_FAILURE);
  }

  for (addrinfo *rp = res; rp; rp = rp->ai_next) {
    *addr = *rp->ai_addr;
    freeaddrinfo(res);
    return 0;
  }

  freeaddrinfo(res);
  return 1;
}

int main(int argc, char **argv) {
  if (argc != 3 || std::string_view(argv[1]).size() >= MAX_HOSTNAME) {
    std::cerr << "Curl called but HOST or PORT not given or invalid\n";
    return EXIT_FAILURE;
  }

  std::string_view host = argv[1];
  std::string_view port = argv[2];

  if (port.size() >= 6) {
    std::cerr << "Curl called but PORT malformed\n";
    return EXIT_FAILURE;
  }

  sockaddr addr;

  if (get_address(argv[1], argv[2], &addr)) {
    std::cerr << "Curl could not resolve hostname\n";
    return EXIT_FAILURE;
  }

  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock == -1) {
    perror("Socket creation failed");
    return EXIT_FAILURE;
  }

  if (fcntl(sock, F_SETFL, O_NONBLOCK) == -1) {
    perror("Setting socket nonblocking");
    close(sock);
    return EXIT_FAILURE;
  }

  if (connect(sock, &addr, sizeof(addr)) == -1) {
    if (errno != EINPROGRESS) {
      perror("Curl could not establish connection");
      close(sock);
      return EXIT_FAILURE;
    }
  }

  std::array<pollfd, 1> fds{};
  fds[0].fd = sock;
  fds[0].events = POLLOUT;

  int ret = poll(fds.data(), fds.size(), -1);
  if (ret < 1) {
    perror("Poll failed");
    close(sock);
    return EXIT_FAILURE;
  }

  assert(fds[0].revents & POLLOUT);

  std::string req = std::format("GET / HTTP/1.1\r\n"
                                "Host: {}:{}\r\n"
                                "Connection: close\r\n\r\n",
                                host, port);

  if (write(sock, req.c_str(), req.size()) !=
      static_cast<ssize_t>(req.size())) {
    perror("Write error");
    close(sock);
    return EXIT_FAILURE;
  }

  int rlen = 0;
  while (true) {
    fds[0].events = POLLIN;
    ret = poll(fds.data(), fds.size(), -1);

    if (ret < 0) {
      perror("Poll failed");
      close(sock);
      return EXIT_FAILURE;
    }

    if (fds[0].revents & POLLIN) {
      std::array<char, RLEN> buff{};

      if ((rlen = read(sock, buff.data(), buff.size())) == -1) {
        perror("Read error");
        close(sock);
        return EXIT_FAILURE;
      }

      if (!rlen) {
        break;
      }

      std::cout << std::string_view(buff.data(), rlen);
    }

    if (fds[0].revents & (POLLHUP | POLLERR)) {
      std::cerr << "POLLHUP/ERR received " << fds[0].revents << '\n';
      break;
    }
  }

  close(sock);
  return EXIT_SUCCESS;
}