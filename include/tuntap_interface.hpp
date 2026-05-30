#pragma once
#include <string>

class TUNTAP_Interface
{
public:
    static bool initialised;
    static int tap_fd;
    static std::string dev;
    std::string tap_addr = "10.0.0.5";
    std::string tap_route = "10.0.0.0/24";

    TUNTAP_Interface();

    int tap_alloc();
};

void tuntap_interface_init();

void free_tuntap_interface();

int tuntap_read(uint8_t *buffer, size_t len);

int tuntap_write(uint8_t *buffer, size_t len);