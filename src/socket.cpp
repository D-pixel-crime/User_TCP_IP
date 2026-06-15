#include "../include/socket.hpp"
#include "../include/intrusive_queue.hpp"
#include "../include/timer.hpp"

int sock_amount = 0;
IntrusiveQueue<Socket> sockets_queue(Socket::getOffset__list_node());
std::shared_mutex rw_mutex;

extern Net_Family inet;

std::vector<Net_Family *> families = []()
{
    std::vector<Net_Family *> temp(128, nullptr);
    temp[AF_INET] = &inet;
    return temp;
}(); // Immediately-executing lambda

/* Figure out a way to not shadow kernel file descriptors. */
Socket::Socket(pid_t _pid) : fd{4097}, pid{1}, refCnt{1}
{
    {
        std::unique_lock<std::shared_mutex> wr_lock(rw_mutex);
        fd++;
    }

    state = Socket_State::SS_UNCONNECTED;
    ops = nullptr;
    flags = O_RDWR;

    /*To be implemened
        wait_init(&sock->lock);
    */
}

int socket_rd_acquire(Socket *sock)
{
    try
    {
        sock->lock.lock();
        sock->refCnt++;
        return 0;
    }
    catch (const std::system_error &e)
    {
        return e.code().value();
    }
}

int socket_wr_acquire(Socket *sock)
{
    try
    {
        sock->lock.lock();
        sock->refCnt++;
        return 0;
    }
    catch (const std::system_error &e)
    {
        return e.code().value();
    }
}

int socket_release(Socket *sock)
{
    try
    {
        sock->refCnt--;
        if (!sock->refCnt)
        {
            sock->lock.unlock();
            delete sock;
        }
        else
        {
            sock->lock.unlock();
        }
        return 0;
    }
    catch (const std::system_error &e)
    {
        return e.code().value();
    }
}

int socket_free(Socket *sock)
{
    {
        std::unique_lock<std::shared_mutex> wr_lock(rw_mutex);
        socket_wr_acquire(sock);
        sockets_queue.queue_del(&sock->node);
        sock_amount--;
    }

    if (sock->ops)
    {
        sock->ops->free(sock);
    }

    /*To be implemented
        wait_free(&sock->sleep);
    */
    socket_release(sock);
}

Socket *socket_find(Socket *query)
{
    Socket *ret = nullptr;
    std::shared_lock<std::shared_mutex> rd_lock(rw_mutex);

    list_for_each(sockets_queue.get_queue_list_head(), [&](list_head *pos)
                  {
        Socket *sock = list_entry<Socket>(pos, Socket::getOffset__list_node());
        if(sock == query){
            ret = sock;
            return true;
        }

        return false; });

    return ret;
}

template <typename T>
void *socket_garbage_collect(T *arg)
{
    Socket *sock = socket_find(reinterpret_cast<Socket *>(arg));
    if (!sock)
    {
        return nullptr;
    }

    socket_free(sock);
    return nullptr;
}

int socket_delete(Socket *sock)
{
    if (sock->state == Socket_State::SS_DISCONNECTING)
    {
        return 0;
    }

    sock->state = Socket_State::SS_DISCONNECTING;
    Timer::create(10000, [sock]()
                  { socket_garbage_collect<Socket>(sock); }, Timer_Type::Oneshot);
}

void abort_sockets()
{
    list_for_each_safe(sockets_queue.get_queue_list_head(), [&](list_head *pos)
                       {
        Socket *sock = list_entry<Socket>(pos, Socket::getOffset__list_node());
        sock->ops->abort(sock); });
}

Socket *get_socket(const pid_t &_pid, const uint32_t &_fd)
{
    Socket *ret = nullptr;

    std::shared_lock<std::shared_mutex> rd_lock(rw_mutex);

    list_for_each(sockets_queue.get_queue_list_head(), [&](list_head *pos)
                  {
        Socket *sock = list_entry<Socket>(pos, Socket::getOffset__list_node());
        if(sock->pid == _pid && sock->fd == _fd){
            ret = sock;
            return true;
        }
        return false; });

    return ret;
}

Socket *socket_lookup(const uint16_t &remote_port, const uint16_t &local_port)
{
    Socket *ret = nullptr;
    Sock *sk = nullptr;

    {
        std::shared_lock<std::shared_mutex> rd_lock(rw_mutex);

        list_for_each(sockets_queue.get_queue_list_head(), [&](list_head *pos)
                      {
        Socket *sock = list_entry<Socket>(pos, Socket::getOffset__list_node());
        if(!sock || !sock->sk){
            return false;
        }

        sk = sock->sk;
        if(sk->sport == local_port && sk->dport == remote_port){
            ret = sock;
            return true;
        }

        return false; });
    }

    return ret;
}

void socket_debug()
{
    std::shared_lock<std::shared_mutex> rd_lock(rw_mutex);

    list_for_each(sockets_queue.get_queue_list_head(), [&](list_head *pos)
                  {
        Socket *sock = list_entry<Socket>(pos, Socket::getOffset__list_node());
        socket_rd_acquire(sock);
        socket_dbg(sock, "");
        socket_release(sock); });
}

int _socket(const pid_t &pid, const int &domain, const int &type, const int &protocol)
{
    Socket *sock = nullptr;
    if (!(sock = new Socket(pid)))
    {
        print_err("ERR(_socket): Couldn't allocate socket.");
        return -1;
    }

    sock->type = type;

    Net_Family *family = families[domain];

    if (!family)
    {
        print_err("ERR(_socket): Unsupported Domain.");
        socket_free(sock);
        return -1;
    }

    if (family->create(sock, protocol))
    {
        print_err("ERR(_socket): Domain Creation Failed.");
        socket_free(sock);
        return -1;
    }

    {
        std::unique_lock<std::shared_mutex> wr_lock(rw_mutex);

        sockets_queue.queue_add_tail(&sock->node);
        sock_amount++;

        socket_rd_acquire(sock);
    }

    int fd = sock->fd;
    socket_release(sock);

    return fd;
}

int _connect(const pid_t &pid, const int &sockfd, const sockaddr *addr, const socklen_t &addrlen)
{
    Socket *sock;
    if (!(sock = get_socket(pid, sockfd)))
    {
        print_err("ERR(_connect): Unable to find socket-fd-{} for connection-pid-{}.", sockfd, pid);
        return -1;
    }

    socket_wr_acquire(sock);
    int rc = sock->ops->connect(sock, addr, addrlen, 0);
    switch (rc)
    {
    case -EINVAL:
    case -EAFNOSUPPORT:
    case -ECONNREFUSED:
    case -ETIMEDOUT:
        socket_release(sock);
        socket_free(sock);
        break;

    default:
        socket_release(sock);
    }

    return rc;
}

int _write(const pid_t &pid, const int &sockfd, const void *buff, const unsigned int &count)
{
    Socket *sock;
    if (!(sock = get_socket(pid, sockfd)))
    {
        print_err("ERR(_write): Unable to find socket-fd-{} for connection-pid-{}.", sockfd, pid);
        return -1;
    }

    socket_wr_acquire(sock);
    int rc = sock->ops->write(sock, buff, count);
    socket_release(sock);

    return rc;
}

int _read(const pid_t &pid, const int &sockfd, void *buff, const unsigned int &count)
{
    Socket *sock;
    if (!(sock = get_socket(pid, sockfd)))
    {
        print_err("ERR(_read): Unable to find socket-fd-{} for connection-pid-{}.", sockfd, pid);
        return -1;
    }

    socket_wr_acquire(sock);
    int rc = sock->ops->read(sock, buff, count);
    socket_release(sock);

    return rc;
}

int _close(const pid_t &pid, const int &sockfd)
{
    Socket *sock;
    if (!(sock = get_socket(pid, sockfd)))
    {
        print_err("ERR(_close): Unable to find socket-fd-{} for connection-pid-{}.", sockfd, pid);
        return -1;
    }

    socket_wr_acquire(sock);
    int rc = sock->ops->close(sock);
    socket_release(sock);

    return rc;
}
