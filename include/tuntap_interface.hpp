#pragma once
#include <cstdint>
#include <string>

class TUNTAP_Interface {
public:
  inline static std::string tap_addr = "10.0.0.5";
  inline static std::string tap_route = "10.0.0.0/24";
  inline static bool initialised;
  inline static int tap_fd;
  inline static std::string dev;

  TUNTAP_Interface();

  int tap_alloc();
};

void tuntap_interface_init();

void free_tuntap_interface();

int tuntap_read(uint8_t *buffer, size_t len);

int tuntap_write(uint8_t *buffer, size_t len);