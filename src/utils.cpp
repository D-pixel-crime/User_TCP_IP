#include "utils.hpp"
#include "syshead.hpp"

extern int debug;

/**
 * Calculates the sum of every 16 bits in a buffer.
 */
uint32_t sum_every_16_bits(const std::vector<uint8_t> &buff)
{
    uint32_t sum = 0;
    for (int i = 1; i < buff.size(); i += 2)
    {
        uint16_t curr = (static_cast<uint16_t>(buff[i - 1]) << 8) | buff[i];
        sum += curr;
    }

    if (buff.size() & 1)
    {
        sum += static_cast<uint16_t>(buff.back());
    }

    return sum;
}

/**
 * Calculates the official RFC 1071 "Internet Checksum" for a given buffer.
 */
uint16_t checksum(const std::vector<uint8_t> &buff, int start_sum = 0)
{
    uint32_t sum = start_sum;
    sum += sum_every_16_bits(buff);

    while (sum >> 16)
    {
        sum = (sum & 0xffff) + (sum >> 16);
    }

    return ~sum;
}

/**
 * get_address: Resolves a hostname and port into a sockaddr structure.
 * * @param host: The hostname or IP address to resolve.
 * * @param port: The service name or port number to resolve.
 * * @param addr: Pointer to a sockaddr structure where the resolved address will be stored.
 */
int get_address(std::string_view host, std::string_view port, struct sockaddr *addr)
{
    struct addrinfo hints, *res, *p;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    int s = getaddrinfo(std::string(host).c_str(), std::string(port).c_str(), &hints, &res);

    if (s != 0)
    {
        print_err("Failed to resolve address for {}: {}", host, gai_strerror(s));
        return -1;
    }

    for (p = res; p != nullptr; p = p->ai_next)
    {
        if (p->ai_family == AF_INET)
        {
            *addr = *p->ai_addr;
            freeaddrinfo(res);
            return 0;
        }
    }

    freeaddrinfo(res);
    return -1;
}

/**
 * Parses an IPv4 address string (e.g., "192.168.1.1") into a 32-bit integer.
 * @param addr The IPv4 address string to parse.
 */
uint32_t parse_ipv4_string(std::string_view addr)
{
    struct sockaddr_in sa{};

    inet_pton(AF_INET, addr.data(), &(sa.sin_addr));
    return sa.sin_addr.s_addr;
}