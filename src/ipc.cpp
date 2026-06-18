#include "../include/ipc.hpp"
#include "../include/socket.hpp"

inline constexpr int IPC_BUFF_LEN = 8192;
static int socket_count = 0;
IntrusiveQueue<Ipc_Thread> sockets_queue(Ipc_Thread::getOffset__list_node());
std::mutex norm_mutex;

Ipc_Thread::Ipc_Thread(const int &_sock) : sock(_sock)
{
    {
        std::lock_guard<std::mutex> lock(norm_mutex);
        sockets_queue.queue_add_tail(&list_node);
        socket_count++;
    }

    /*To be implemented:
        ipc_dbg("New IPC socket allocated", th);
    */
}

void ipc_free_thread(const int &sock)
{
    {
        std::lock_guard<std::mutex> lock(norm_mutex);

        list_for_each_safe(sockets_queue.get_queue_list_head(), [&](list_head *pos)
                           {
            Ipc_Thread* ipcth = list_entry<Ipc_Thread>(pos, Ipc_Thread::getOffset__list_node());

            if(ipcth->sock == sock){
                sockets_queue.queue_del(&ipcth->list_node);
                /*To be implemented:
                    ipc_dbg("IPC socket deleted", th);
                */
               close(ipcth->sock);
               delete ipcth;
               socket_count--;
               return true;
            }

            return false; });
    }
}

int ipc_try_send(const int &sockfd, const void *buff, const size_t &len)
{
    return send(sockfd, buff, len, MSG_NOSIGNAL);
}

int ipc_write_rc(const int &sockfd, const pid_t &pid, const uint16_t &type, int rc)
{
    int resp_len = sizeof(Ipc_Msg) + sizeof(Ipc_Err);
    Ipc_Msg *response = reinterpret_cast<Ipc_Msg *>(new uint8_t[resp_len]);

    if (!response)
    {
        print_err("ERR(ipc_write_rc): Failed memory allocation for IPC Write Response.");
        return -1;
    }

    response->type = type;
    response->pid = pid;

    Ipc_Err err;

    if (rc < 0)
    {
        err.err = -rc;
        err.rc = -1;
    }
    else
    {
        err.err = 0;
        err.rc = rc;
    }

    std::memcpy(response->data, &err, sizeof(Ipc_Err));

    if (ipc_try_send(sockfd, reinterpret_cast<uint8_t *>(response), resp_len) == -1)
    {
        throw std::runtime_error("ERR(ipc_write_rc): Error on writing IPC Write Response.");
        return -1;
    }

    return 0;
}

int ipc_read(const int &sockfd, Ipc_Msg *msg)
{
    Ipc_Read *requested = reinterpret_cast<Ipc_Read *>(msg->data);
    pid_t pid = msg->pid;
    int rlen = -1;
    uint8_t rbuff[requested->len];
    std::memset(rbuff, 0, requested->len);

    rlen = _read(pid, sockfd, rbuff, requested->len);

    int resp_len = sizeof(Ipc_Msg) + sizeof(Ipc_Err) + sizeof(Ipc_Read) + std::max(0, rlen);
    Ipc_Msg *response = reinterpret_cast<Ipc_Msg *>(new uint8_t[resp_len]);
    Ipc_Err *error = reinterpret_cast<Ipc_Err *>(response->data);
    Ipc_Read *actual = reinterpret_cast<Ipc_Read *>(error->data);

    if (!response)
    {
        print_err("ERR(ipc_read): Failed memory allocation for IPC Read Response.");
        return -1;
    }

    response->type = IPC_READ;
    response->pid = pid;

    error->rc = std::max(-1, rlen);
    error->err = std::abs(std::min(rlen, 0));

    actual->sockfd = requested->sockfd;
    actual->len = rlen;
    std::memcpy(actual->buff, rbuff, std::max(rlen, 0));

    if (ipc_try_send(sockfd, reinterpret_cast<uint8_t *>(response), resp_len) == -1)
    {
        print_err("ERR(ipc_read): Error on writing IPC Read Response.");
        return -1;
    }

    return 0;
}

int ipc_write(const int &sockfd, Ipc_Msg *msg)
{
    Ipc_Write *payload = reinterpret_cast<Ipc_Write *>(msg->data);
    pid_t pid = msg->pid;

    int rc = -1;
    int head = IPC_BUFF_LEN - sizeof(Ipc_Write) - sizeof(Ipc_Msg);

    uint8_t buf[payload->len];

    std::memset(buf, 0, payload->len);
    std::memcpy(buf, payload->buff, std::max(payload->len, (size_t)head));

    // Guard for payload that is longer than initial IPC_BUFF_LEN
    if (payload->len > head)
    {
        int tail = payload->len - head;
        int res = read(sockfd, &buf[head], tail);

        if (res == -1)
        {
            throw std::runtime_error("ERR(ipc_write): Read on IPC payload guard.");
            return -1;
        }
        else if (res != tail)
        {
            print_err("ERR(ipc_write): Did not read exact payload amount in IPC Write.");
        }
    }

    rc = _write(pid, payload->sockfd, buf, payload->len);

    return ipc_write_rc(sockfd, pid, IPC_WRITE, rc);
}

int ipc_connect(const int &sockfd, Ipc_Msg *msg)
{
    Ipc_Connect *payload = reinterpret_cast<Ipc_Connect *>(msg->data);
    pid_t pid = msg->pid;

    int rc = _connect(pid, payload->sockfd, &payload->addr, payload->addr_len);

    return ipc_write_rc(sockfd, pid, IPC_CONNECT, rc);
}

int ipc_socket(const int &sockfd, Ipc_Msg *msg)
{
    Ipc_Socket *sock = reinterpret_cast<Ipc_Socket *>(msg->data);
    pid_t pid = msg->pid;

    int rc = _socket(pid, sock->domain, sock->type, sock->protocol);

    return ipc_write_rc(sockfd, pid, IPC_SOCKET, rc);
}

int ipc_close(const int &sockfd, Ipc_Msg *msg)
{
    Ipc_Close *payload = reinterpret_cast<Ipc_Close *>(msg->data);
    pid_t pid = msg->pid;

    int rc = _close(pid, payload->sockfd);

    rc = ipc_write_rc(sockfd, pid, IPC_CLOSE, rc);

    return rc;
}