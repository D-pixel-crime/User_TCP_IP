#include "utils.hpp"
#include "syshead.hpp"

extern int debug;

uint32_t sum_every_16_bits(std::span<const uint8_t> buff)
{
    uint32_t sum = 0;
    const uint16_t *temp = reinterpret_cast<const uint16_t *>(buff.data());
    size_t count = buff.size();

    while (count > 1)
    {
        sum += *temp++;
        count -= 2;
    }

    // In case count is odd.
    if (count > 0)
    {
        sum += *reinterpret_cast<const uint8_t *>(temp);
    }

    return sum;
}

uint16_t checksum(std::span<const uint8_t> buff, uint32_t start_sum = 0)
{
    uint32_t sum = start_sum;
    sum += sum_every_16_bits(buff);

    while (sum >> 16)
    {
        sum = (sum & 0xffff) + (sum >> 16);
    }

    return ~sum;
}

int get_address(std::string_view host, std::string_view port, struct sockaddr *addr)
{
    struct addrinfo hints, *res, *p;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    int status = getaddrinfo(std::string(host).c_str(), std::string(port).c_str(), &hints, &res);

    if (status != 0)
    {
        print_err("Failed to resolve address for {}: {}", host, gai_strerror(status));
        return -1;
    }

    int retCode = -1;
    for (p = res; p != nullptr; p = p->ai_next)
    {
        if (p->ai_family == AF_INET)
        {
            *addr = *p->ai_addr;
            retCode = 0;
            break;
        }
    }

    freeaddrinfo(res);
    return retCode;
}

uint32_t parse_ipv4_string(std::string_view addr)
{
    struct sockaddr_in sa{};

    inet_pton(AF_INET, std::string(addr).c_str(), &(sa.sin_addr));
    return sa.sin_addr.s_addr;
}