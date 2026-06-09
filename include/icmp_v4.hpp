#pragma once
#include "syshead.hpp"
#include "socket_buffer.hpp"

inline constexpr int ICMP_V4_REPLY = 0x00;
inline constexpr int ICMP_V4_DEST_UNREACHABLE = 0x03;
inline constexpr int ICMP_V4_SRC_QUENCH = 0x04;
inline constexpr int ICMP_V4_ECHO = 0x08;
inline constexpr int ICMP_V4_ROUTER_ADV = 0x09;
inline constexpr int ICMP_V4_ROUTER_SOL = 0x0a;
inline constexpr int ICMP_V4_ROUTER_TIMEOUT = 0x0b;
inline constexpr int ICMP_V4_ROUTER_MALFORMED = 0x0c;

class __attribute__((packed)) ICMPv4
{
public:
    uint8_t type;
    uint8_t code;
    uint16_t csum;
    uint8_t data[0];
};

class __attribute__((packed)) ICMPv4_Echo
{
public:
    uint16_t id;
    uint16_t seq;
    uint8_t data[0];
};

class __attribute__((packed)) ICMPv4_Dest_Unreachable
{
public:
    uint8_t unused;
    uint8_t len;
    uint16_t var;
    uint8_t data[0];
};

void icmpv4_incoming(SkBuff *skb);

void icmpv4_reply(SkBuff *skb);