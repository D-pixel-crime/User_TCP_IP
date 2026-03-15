#pragma once

// c headers having no implementation in cpp
#include <sys/socket.h>

// cpp equivalent of c headers
#include <iostream>
#include <format>
#include <algorithm>
#include <span>
#include <vector>
#include <cstdint>
#include <string_view>
#include <source_location>

constexpr int CMD_BUF_LEN = 100;

template <typename... Args>
inline void print_debug(std::format_string<Args...> fmt, Args &&...args, const std::source_location loc = std::source_location::current())
{
    std::cout << std::format(fmt, std::forward<Args>(args)...) << std::format(" - {}:{}\n", loc.file_name(), loc.line()) << std::endl;
}

template <typename... Args>
inline void print_err(std::format_string<Args...> fmt, Args &&...args)
{
    std::cout << std::format(fmt, std::forward<Args>(args)...) << std::endl;
}

int run_cmd(const char *cmd, ...);

uint32_t sum_every_16_bits(const std::vector<uint8_t> &buff);

uint16_t checksum(const std::vector<uint8_t> &buff, int start_sum = 0);

int get_address(std::string_view host, std::string_view port, struct sockaddr *addr);

uint32_t parse_ipv4_string(std::string_view addr);