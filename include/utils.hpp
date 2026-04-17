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

/**
 * Executes a shell command with dynamic formatting.
 * * @tparam 'Args' Variadic template parameter pack to handle any number/type of arguments.
 * @param fmt 'std::string_view' keeps non-owning reference to a string.
 * @param fmt Contains the command template (e.g., "ip link set {} up").
 * @param args The data to be injected into the {} placeholders.
 */
template <typename... Args>
// 'Args&&': universal refernce(T&&), can bind both lvalue, rvalue references.
int run_cmd(std::string_view fmt, Args &&...args)
{
    try
    {
        /*
        'std::make_format_args(args...)': Type-erases the arguments into a
          standardized container that the formatting engine can process.

        'std::vformat': The runtime version of std::format. It performs a
          RUNTIME-CHECK to ensure the no.of arguments fit.
         */
        std::string cmd = std::vformat(fmt, std::make_format_args(args...));

        if (debug)
        {
            print_debug("Running command: {}", cmd);
        }

        return system(cmd.c_str());
    }
    catch (const std::format_error &e)
    {
        std::cerr << "Format Error: " << e.what() << std::endl;
        return -1;
    }
    catch (const std::exception &e)
    {
        std::cerr << "General Error: " << e.what() << std::endl;
        return -1;
    }
}

/**
 * Prints a formatted debug message with source location information.
 * * @tparam 'Args' Variadic template parameter pack to handle any number/type of arguments.
 * @param fmt 'std::format_string' checks at COMPILE-TIME that the format string is valid for the provided arguments.
 */
template <typename... Args>
inline void print_debug(std::format_string<Args...> fmt, Args &&...args)
{
    const std::source_location loc = std::source_location::current();
    std::cout << std::format(fmt, std::forward<Args>(args)...) << std::format(" - {}:{}\n", loc.file_name(), loc.line()) << std::endl;
}

template <typename... Args>
inline void print_err(std::format_string<Args...> fmt, Args &&...args)
{
    std::cerr << std::format(fmt, std::forward<Args>(args)...) << std::endl;
}

/**
 * Calculates the sum of every 16 bits in a buffer.
 * @param buff The input buffer as a span of bytes.
 */
uint32_t sum_every_16_bits(std::span<const uint8_t> buff);

/**
 * Calculates the official RFC 1071 "Internet Checksum" for a given buffer.
 * @param buff The input buffer as a span of bytes.
 */
uint16_t checksum(std::span<const uint8_t> buff, uint32_t start_sum = 0);

/**
 * get_address: Resolves a hostname and port into a sockaddr structure.
 * * @param host: The hostname or IP address to resolve.
 * * @param port: The service name or port number to resolve.
 * * @param addr: Pointer to a sockaddr structure where the resolved address will be stored.
 */
int get_address(std::string_view host, std::string_view port, struct sockaddr *addr);

/**
 * Parses an IPv4 address string (e.g., "192.168.1.1") into a 32-bit integer.
 * @param addr The IPv4 address string to parse.
 */
uint32_t parse_ipv4_string(std::string_view addr);