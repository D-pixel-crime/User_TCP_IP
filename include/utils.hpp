#pragma once

// c headers having no implementation in cpp
#include <sys/socket.h>

// cpp equivalent of c headers
#include <cstdint>
#include <format>
#include <iostream>
#include <source_location>
#include <string_view>

extern bool debug;

inline constexpr int CMD_BUF_LEN = 100;

/**
 * Macro to clear a structure by setting all its bytes to zero.
 * * @param 'T' The structure variable to be cleared.
 */
template <typename T> void CLEAR(T *obj) {
  if (obj) {
    memset(obj, 0, sizeof(T));
  }
}

/**
 * Prints a formatted debug message with source location information.
 */
inline void
print_debug(std::string_view msg,
            const std::source_location loc = std::source_location::current()) {
  std::cout << msg << " "
            << std::format(" - {}:{}\n", loc.file_name(), loc.line())
            << std::endl;
}

template <typename... Args>
inline void print_err(std::format_string<Args...> fmt, Args &&...args) {
  std::cerr << std::format(fmt, std::forward<Args>(args)...) << std::endl;
}

/**
 * Executes a shell command with dynamic formatting.
 * * @tparam 'Args' Variadic template parameter pack to handle any number/type
 * of arguments.
 * @param fmt 'std::string_view' keeps non-owning reference to a string.
 * @param fmt Contains the command template (e.g., "ip link set {} up").
 * @param args The data to be injected into the {} placeholders.
 */
template <typename... Args>
// 'Args&&': universal reference(T&&), can bind both lvalue, rvalue references.
int run_cmd(std::string_view fmt, Args &&...args) {
  try {
    /*
    'std::make_format_args(args...)': Type-erases the arguments into a
      standardized container that the formatting engine can process.

    'std::vformat': The runtime version of std::format. It performs a
      RUNTIME-CHECK to ensure the no.of arguments fit.
     */
    std::string cmd = std::vformat(fmt, std::make_format_args(args...));

    if (debug) {
      print_debug(std::format("Running command: {}", cmd));
    }

    return system(cmd.c_str());
  } catch (const std::format_error &e) {
    std::cerr << "Format Error: " << e.what() << std::endl;
    return -1;
  } catch (const std::exception &e) {
    std::cerr << "General Error: " << e.what() << std::endl;
    return -1;
  }
}

/**
 * Calculates the sum of every 16 bits in a buffer.
 * @param buff Pointer to the start of the buffer.
 * @param count The size of the buffer in bytes.
 */
template <typename T> uint32_t sum_every_16_bits(const T *buff, size_t count) {
  uint32_t ans = 0;
  const uint16_t *temp = reinterpret_cast<const uint16_t *>(buff);

  while (count > 1) {
    ans += *temp++;
    count -= 2;
  }

  if (count > 0) {
    ans += *reinterpret_cast<const uint8_t *>(temp);
  }

  return ans;
}

/**
 * @brief Calculates the official RFC 1071 "Internet Checksum" for a given
 * buffer.
 * @param buff Pointer to the start of the buffer.
 * @param count The size of the buffer in bytes.
 * @param start_sum An optional initial sum to cascade checksums (defaults to
 * 0).
 */
template <typename T>
uint16_t checksum(const T *buff, const size_t count,
                  const uint32_t start_sum = 0) {
  uint32_t ans = start_sum;
  ans += sum_every_16_bits(buff, count);

  while (ans >> 16) {
    ans = (ans & 0xffff) + (ans >> 16);
  }

  return static_cast<uint16_t>(~ans);
}

/**
 * get_address: Resolves a hostname and port into a sockaddr structure.
 * * @param host: The hostname or IP address to resolve.
 * * @param port: The service name or port number to resolve.
 * * @param addr: Pointer to a sockaddr structure where the resolved address
 * will be stored.
 */
int get_address(const std::string &host, const std::string &port,
                struct sockaddr *addr);

/**
 * Parses an IPv4 address string (e.g., "192.168.1.1") into a 32-bit integer.
 * @param addr The IPv4 address string to parse.
 */
uint32_t parse_ipv4_string(const std::string &addr);

/**
 * Parses a MAC address string (e.g., "00:00:01:02:03:05") into a 32-bit
 * integer.
 * @param mac_addr The MAC address string to parse.
 */
uint8_t *parse_MAC_string(const std::string &mac_addr);