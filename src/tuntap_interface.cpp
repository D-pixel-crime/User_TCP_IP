#include "../include/tuntap_interface.hpp"
#include "../include/system_headers.hpp" // IWYU pragma: keep
#include "../include/utils.hpp"

/**
 * Brings network-interface online by @param dev
 */
static int set_interface_up(const std::string &dev) {
  return run_cmd("ip link set dev {} up", dev);
}

/**
 * Binds @param dev to @param addr, so that the network-interface can be used
 * for routing
 */
static int set_interface_address(const std::string &dev,
                                 const std::string &addr) {
  return run_cmd("ip addr add dev {} local {}", dev, addr);
}

/**
 * Sets up routing for @param dev to route traffic to @param route
 */
static int set_interface_route(const std::string &dev,
                               const std::string &route) {
  return run_cmd("ip route add dev {} {}", dev, route);
}

inline int TUNTAP_Interface::tap_alloc() {
  ifreq ifr;
  int fd, err;

  if ((fd = open("/dev/net/tun", O_RDWR)) < 0) {
    print_err("Cannot open TUN/TAP dev\nMake sure one exists with 'mknod "
              "/dev/net/tun c 10 200'\n");
    std::exit(EXIT_FAILURE);
  }

  CLEAR(&ifr);

  ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
  if (!dev.empty()) {
    std::strncpy(ifr.ifr_name, dev.c_str(), IFNAMSIZ);
  }

  if ((err = ioctl(fd, TUNSETIFF, (void *)&ifr)) < 0) {
    print_err("ERR(tap_alloc): Could not ioctl tun/tap interface");
    close(fd);
    std::exit(EXIT_FAILURE);
  }

  dev.assign(ifr.ifr_name, 0, IFNAMSIZ);
  return fd;
}

TUNTAP_Interface::TUNTAP_Interface() {
  if (!initialised) {
    initialised = true;
    dev = std::string(IFNAMSIZ, '\0');

    tap_fd = tap_alloc();

    if (set_interface_up(dev)) {
      print_err("ERR(TUNTAP_Interface): Could not set interface {} up", dev);
      std::exit(EXIT_FAILURE);
    }

    if (set_interface_route(dev, tap_route)) {
      print_err(
          "ERR(TUNTAP_Interface): Could not set route {} for interface {}",
          tap_route, dev);
      std::exit(EXIT_FAILURE);
    }

    if (set_interface_address(dev, tap_addr)) {
      print_err(
          "ERR(TUNTAP_Interface): Could not set address {} for interface {}",
          tap_addr, dev);
      std::exit(EXIT_FAILURE);
    }
  }
}

TUNTAP_Interface *tuntap_interface = nullptr;

void tuntap_interface_init() { tuntap_interface = new TUNTAP_Interface(); }

void free_tuntap_interface() {
  if (TUNTAP_Interface::initialised) {
    close(TUNTAP_Interface::tap_fd);
    TUNTAP_Interface::initialised = false;
  }

  if (tuntap_interface) {
    delete tuntap_interface;
    tuntap_interface = nullptr;
  }
}

int tuntap_read(uint8_t *buffer, size_t len) {
  if (!TUNTAP_Interface::initialised) {
    print_err("TUN/TAP Interface not yet initialised!");
    std::exit(EXIT_FAILURE);
  }

  return read(TUNTAP_Interface::tap_fd, buffer, len);
}

int tuntap_write(uint8_t *buffer, size_t len) {
  if (!TUNTAP_Interface::initialised) {
    print_err("TUN/TAP Interface not yet initialised!");
    std::exit(EXIT_FAILURE);
  }

  return write(TUNTAP_Interface::tap_fd, buffer, len);
}