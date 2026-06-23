#pragma once
#include "intrusive_queue.hpp"
#include "syshead.hpp"

class Ipc_Thread;

inline void ipc_dbg(std::string_view msg, Ipc_Thread *ipcth)
{
    print_debug(std::format("IPC sockets count-{}, current sock-{}, tid {}: {}", socket_count, ipcth->sock, ipcth->id, msg));
}

inline constexpr int IPC_SOCKET = 0x0001;
inline constexpr int IPC_CONNECT = 0x0002;
inline constexpr int IPC_WRITE = 0x0003;
inline constexpr int IPC_READ = 0x0004;
inline constexpr int IPC_CLOSE = 0x0005;
inline constexpr int IPC_POLL = 0x0006;
inline constexpr int IPC_FCNTL = 0x0007;
inline constexpr int IPC_GETSOCKOPT = 0x0008;
inline constexpr int IPC_SETSOCKOPT = 0x0009;
inline constexpr int IPC_GETPEERNAME = 0x000A;
inline constexpr int IPC_GETSOCKNAME = 0x000B;

class Ipc_Thread
{
public:
    list_head list_node;
    int sock;
    std::thread::id id;

    static size_t getOffset__list_node()
    {
        return offsetof(Ipc_Thread, list_node);
    }

    Ipc_Thread(const int &_sock);
};

class __attribute__((packed)) Ipc_Msg
{
public:
    uint16_t type;
    pid_t pid;
    uint8_t data[];
};

class __attribute__((packed)) Ipc_Err
{
public:
    int rc;
    int err;
    uint8_t data[];
};

class __attribute__((packed)) Ipc_Socket
{
public:
    int domain;
    int type;
    int protocol;
};

class __attribute__((packed)) Ipc_Connect
{
public:
    int sockfd;
    sockaddr addr;
    socklen_t addr_len;
};

class __attribute__((packed)) Ipc_Write
{
public:
    int sockfd;
    size_t len;
    uint8_t buff[];
};

class __attribute__((packed)) Ipc_Read
{
public:
    int sockfd;
    size_t len;
    uint8_t buff[];
};

class __attribute__((packed)) Ipc_Close
{
public:
    int sockfd;
};

class __attribute__((packed)) Ipc_Poll_Fd
{
public:
    int fd;
    short int events;
    short int revents;
};

class __attribute__((packed)) Ipc_Poll
{
public:
    nfds_t nfds;
    int timeout;
    Ipc_Poll_Fd fds[];
};

class __attribute__((packed)) Ipc_Fcntl
{
public:
    int sockfd;
    int cmd;
    uint8_t data[];
};

class __attribute__((packed)) Ipc_Sock_Opt
{
public:
    int fd;
    int level;
    int optname;
    socklen_t optlen;
    uint8_t optval[];
};

class Ipc_Sock_Name
{
public:
    int socket;
    socklen_t address_len;
    uint8_t sa_data[128];
};