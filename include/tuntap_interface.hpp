#pragma once
#include <string>

class TUNTAP_Interface
{
public:
    static bool initialised;
    static int tun_fd;
    static std::string dev;
    std::string tap_addr = "10.0.0.5";
    std::string tap_route = "10.0.0.0/24";

    TUNTAP_Interface();

    int tap_alloc();
};

void tuntap_interface_init();

void free_tuntap_interface();