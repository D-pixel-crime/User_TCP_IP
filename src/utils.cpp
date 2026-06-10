#include "../include/utils.hpp"
#include "../include/syshead.hpp"

int get_address(const std::string &host, const std::string &port, struct sockaddr *addr)
{
    struct addrinfo hints, *res, *p;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    int status = getaddrinfo(host.c_str(), port.c_str(), &hints, &res);

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

uint32_t parse_ipv4_string(const std::string &addr)
{
    struct sockaddr_in sa{};

    if (inet_pton(AF_INET, addr.c_str(), &(sa.sin_addr)) != 1)
    {
        throw std::runtime_error(std::format("ERR(utils): During parsing ip-{}.", addr));
    }

    return ntohl(sa.sin_addr.s_addr);
}

uint8_t *parse_MAC_string(const std::string &mac_addr)
{
    uint8_t ans[6];
    int i = 0, n = mac_addr.size(), k = 0;
    while (i < n)
    {
        uint8_t curr = static_cast<uint8_t>(stoi(mac_addr.substr(i, 2), nullptr, 16));
        ans[k] = curr;
        k++;
        i += 3;
    }

    return ans;
}