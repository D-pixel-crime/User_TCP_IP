#include "../include/ipc.hpp"
#include "../include/socket.hpp"
#include <sys/stat.h>

inline constexpr int IPC_BUFF_LEN = 8192;
IntrusiveQueue<Ipc_Thread> sockets_queue(Ipc_Thread::getOffset__list_node());
std::mutex norm_mutex;

Ipc_Thread::Ipc_Thread(const int &_sock) : sock(_sock) {
  {
    std::unique_lock<std::mutex> lock(norm_mutex);
    sockets_queue.queue_add_tail(&list_node);
    socket_count++;
  }

  ipc_dbg("New IPC socket allocated.", this);
}

void ipc_free_thread(const int &sock) {
  {
    std::unique_lock<std::mutex> lock(norm_mutex);

    list_for_each_safe(
        sockets_queue.get_queue_list_head(), [&](list_head *pos) {
          Ipc_Thread *ipcth =
              list_entry<Ipc_Thread>(pos, Ipc_Thread::getOffset__list_node());

          if (ipcth->sock == sock) {
            sockets_queue.queue_del(&ipcth->list_node);
            ipc_dbg("IPC socket deleted", ipcth);
            close(ipcth->sock);
            delete ipcth;
            socket_count--;
            return true;
          }

          return false;
        });
  }
}

int ipc_try_send(const int &sockfd, const void *buff, const size_t &len) {
  return send(sockfd, buff, len, MSG_NOSIGNAL);
}

int ipc_write_rc(const int &sockfd, const pid_t &pid, const uint16_t &type,
                 int rc) {
  int resp_len = sizeof(Ipc_Msg) + sizeof(Ipc_Err);
  Ipc_Msg *response = reinterpret_cast<Ipc_Msg *>(new uint8_t[resp_len]);

  response->type = type;
  response->pid = pid;

  Ipc_Err err;

  if (rc < 0) {
    err.err = -rc;
    err.rc = -1;
  } else {
    err.err = 0;
    err.rc = rc;
  }

  std::memcpy(response->data, &err, sizeof(Ipc_Err));

  if (ipc_try_send(sockfd, reinterpret_cast<uint8_t *>(response), resp_len) ==
      -1) {
    throw std::runtime_error("ERR(ipc_try_send from ipc_write_rc): Error on "
                             "writing IPC Write Response.");
  }

  return 0;
}

int ipc_read(const int &sockfd, Ipc_Msg *msg) {
  Ipc_Read *requested = reinterpret_cast<Ipc_Read *>(msg->data);
  pid_t pid = msg->pid;
  int rlen = -1;

  std::vector<uint8_t> rbuff(requested->len);

  rlen = _read(pid, sockfd, rbuff.data(), requested->len);

  int resp_len =
      sizeof(Ipc_Msg) + sizeof(Ipc_Err) + sizeof(Ipc_Read) + std::max(0, rlen);
  Ipc_Msg *response = reinterpret_cast<Ipc_Msg *>(new uint8_t[resp_len]);
  Ipc_Err *error = reinterpret_cast<Ipc_Err *>(response->data);
  Ipc_Read *actual = reinterpret_cast<Ipc_Read *>(error->data);

  response->type = IPC_READ;
  response->pid = pid;

  error->rc = std::max(-1, rlen);
  error->err = std::abs(std::min(rlen, 0));

  actual->sockfd = requested->sockfd;
  actual->len = rlen;
  std::memcpy(actual->buff, rbuff.data(), std::max(rlen, 0));

  if (ipc_try_send(sockfd, reinterpret_cast<uint8_t *>(response), resp_len) ==
      -1) {
    print_err("ERR(ipc_read): Error on writing IPC Read Response.");
    return -1;
  }

  return 0;
}

int ipc_write(const int &sockfd, Ipc_Msg *msg) {
  Ipc_Write *payload = reinterpret_cast<Ipc_Write *>(msg->data);
  pid_t pid = msg->pid;

  int rc = -1;
  int head = IPC_BUFF_LEN - sizeof(Ipc_Write) - sizeof(Ipc_Msg);

  std::vector<uint8_t> buf(payload->len);

  std::memcpy(buf.data(), payload->buff, std::min(payload->len, (size_t)head));

  // Guard for payload that is longer than initial IPC_BUFF_LEN
  if (payload->len > static_cast<size_t>(head)) {
    int tail = payload->len - head;
    int res = read(sockfd, &buf[head], tail);

    if (res == -1) {
      throw std::runtime_error("ERR(ipc_write): Read on IPC payload guard.");
    } else if (res != tail) {
      print_err(
          "ERR(ipc_write): Did not read exact payload amount in IPC Write.");
    }
  }

  rc = _write(pid, payload->sockfd, buf.data(), payload->len);

  return ipc_write_rc(sockfd, pid, IPC_WRITE, rc);
}

int ipc_connect(const int &sockfd, Ipc_Msg *msg) {
  Ipc_Connect *payload = reinterpret_cast<Ipc_Connect *>(msg->data);
  pid_t pid = msg->pid;

  sockaddr addr = payload->addr;
  int rc = _connect(pid, payload->sockfd, &addr, payload->addr_len);
  payload->addr = addr;

  return ipc_write_rc(sockfd, pid, IPC_CONNECT, rc);
}

int ipc_socket(const int &sockfd, Ipc_Msg *msg) {
  Ipc_Socket *sock = reinterpret_cast<Ipc_Socket *>(msg->data);
  pid_t pid = msg->pid;

  int rc = _socket(pid, sock->domain, sock->type, sock->protocol);

  return ipc_write_rc(sockfd, pid, IPC_SOCKET, rc);
}

int ipc_close(const int &sockfd, Ipc_Msg *msg) {
  Ipc_Close *payload = reinterpret_cast<Ipc_Close *>(msg->data);
  pid_t pid = msg->pid;

  int rc = _close(pid, payload->sockfd);

  rc = ipc_write_rc(sockfd, pid, IPC_CLOSE, rc);

  return rc;
}

int ipc_poll(const int &sockfd, Ipc_Msg *msg) {
  Ipc_Poll *data = reinterpret_cast<Ipc_Poll *>(msg->data);
  pid_t pid = msg->pid;
  int rc = -1;

  std::vector<pollfd> fds(data->nfds);

  for (nfds_t i = 0; i < data->nfds; i++) {
    fds[i].fd = data->fds[i].fd;
    fds[i].events = data->fds[i].events;
    fds[i].revents = data->fds[i].revents;
  }

  rc = _poll(pid, fds.data(), data->nfds, data->timeout);

  int resp_len =
      sizeof(Ipc_Msg) + sizeof(Ipc_Err) + sizeof(Ipc_Poll_Fd) * data->nfds;
  Ipc_Msg *response = reinterpret_cast<Ipc_Msg *>(new uint8_t[resp_len]);

  response->type = IPC_POLL;
  response->pid = pid;

  Ipc_Err err;

  if (rc < 0) {
    err.err = -rc;
    err.rc = -1;
  } else {
    err.err = 0;
    err.rc = rc;
  }

  std::memcpy(response->data, &err, sizeof(Ipc_Err));

  Ipc_Poll_Fd *polled = reinterpret_cast<Ipc_Poll_Fd *>(
      reinterpret_cast<Ipc_Err *>(response->data)->data);

  for (nfds_t i = 0; i < data->nfds; i++) {
    polled[i].fd = fds[i].fd;
    polled[i].events = fds[i].events;
    polled[i].revents = fds[i].revents;
  }

  if (ipc_try_send(sockfd, (char *)response, resp_len) == -1) {
    throw std::runtime_error(
        "ERR(ipc_err): Error on writing IPC Poll Response");
  }

  return 0;
}

int ipc_fcntl(const int &sockfd, Ipc_Msg *msg) {
  Ipc_Fcntl *fc = reinterpret_cast<Ipc_Fcntl *>(msg->data);
  pid_t pid = msg->pid;
  int rc = -1;

  switch (fc->cmd) {
  case F_GETFL:
    rc = _fcntl(pid, fc->sockfd, fc->cmd);
    break;
  case F_SETFL:
    rc = _fcntl(pid, fc->sockfd, fc->cmd, *(int *)fc->data);
    break;
  default:
    print_err("ERR(ipc_fcntl): IPC Fcntl cmd not supported-{}.", fc->cmd);
    rc = -EINVAL;
  }

  return ipc_write_rc(sockfd, pid, IPC_FCNTL, rc);
}

int ipc_getsockopt(const int &sockfd, Ipc_Msg *msg) {
  Ipc_Sock_Opt *opts = reinterpret_cast<Ipc_Sock_Opt *>(msg->data);

  pid_t pid = msg->pid;
  int rc = -1;

  socklen_t optlen_local = opts->optlen;
  rc = _getsockopt(pid, opts->fd, opts->level, opts->optname, opts->optval,
                   &optlen_local);
  opts->optlen = optlen_local;

  int resp_len =
      sizeof(Ipc_Msg) + sizeof(Ipc_Err) + sizeof(Ipc_Sock_Opt) + opts->optlen;
  Ipc_Msg *response = reinterpret_cast<Ipc_Msg *>(new uint8_t[resp_len]);

  response->type = IPC_GETSOCKOPT;
  response->pid = pid;

  Ipc_Err err;

  if (rc < 0) {
    err.err = -rc;
    err.rc = -1;
  } else {
    err.err = 0;
    err.rc = rc;
  }

  std::memcpy(response->data, &err, sizeof(Ipc_Err));

  Ipc_Sock_Opt *optres = reinterpret_cast<Ipc_Sock_Opt *>(
      reinterpret_cast<Ipc_Err *>(response->data)->data);

  optres->fd = opts->fd;
  optres->level = opts->level;
  optres->optname = opts->optname;
  optres->optlen = opts->optlen;
  std::memcpy(&optres->optval, opts->optval, opts->optlen);

  if (ipc_try_send(sockfd, (char *)response, resp_len) == -1) {
    throw std::runtime_error(
        "ERR(ipc_getsockopt): Error on writing IPC getsockopt Response.");
  }

  return rc;
}

int ipc_getpeername(const int &sockfd, Ipc_Msg *msg) {
  Ipc_Sock_Name *name = reinterpret_cast<Ipc_Sock_Name *>(msg->data);

  pid_t pid = msg->pid;
  int rc = -1;

  int resp_len = sizeof(Ipc_Msg) + sizeof(Ipc_Err) + sizeof(Ipc_Sock_Name);
  Ipc_Msg *response = reinterpret_cast<Ipc_Msg *>(new uint8_t[resp_len]);

  response->type = IPC_GETPEERNAME;
  response->pid = pid;

  Ipc_Sock_Name *nameres = reinterpret_cast<Ipc_Sock_Name *>(
      reinterpret_cast<Ipc_Err *>(response->data)->data);
  rc = _getpeername(pid, name->socket, (struct sockaddr *)nameres->sa_data,
                    &nameres->address_len);

  Ipc_Err err;

  if (rc < 0) {
    err.err = -rc;
    err.rc = -1;
  } else {
    err.err = 0;
    err.rc = rc;
  }

  std::memcpy(response->data, &err, sizeof(Ipc_Err));

  nameres->socket = name->socket;

  if (ipc_try_send(sockfd, (char *)response, resp_len) == -1) {
    throw std::runtime_error(
        "ERR(ipc_getpeername): Error on writing IPC getpeername Response.");
  }

  return rc;
}

int ipc_getsockname(const int &sockfd, Ipc_Msg *msg) {
  Ipc_Sock_Name *name = reinterpret_cast<Ipc_Sock_Name *>(msg->data);

  pid_t pid = msg->pid;
  int rc = -1;

  int resp_len = sizeof(Ipc_Msg) + sizeof(Ipc_Err) + sizeof(Ipc_Sock_Name);
  Ipc_Msg *response = reinterpret_cast<Ipc_Msg *>(new uint8_t[resp_len]);

  response->type = IPC_GETSOCKNAME;
  response->pid = pid;

  Ipc_Sock_Name *nameres = reinterpret_cast<Ipc_Sock_Name *>(
      reinterpret_cast<Ipc_Err *>(response->data)->data);
  rc = _getsockname(pid, name->socket, (struct sockaddr *)nameres->sa_data,
                    &nameres->address_len);

  Ipc_Err err;

  if (rc < 0) {
    err.err = -rc;
    err.rc = -1;
  } else {
    err.err = 0;
    err.rc = rc;
  }

  std::memcpy(response->data, &err, sizeof(Ipc_Err));

  nameres->socket = name->socket;

  if (ipc_try_send(sockfd, (char *)response, resp_len) == -1) {
    throw std::runtime_error(
        "ERR(ipc_getsockname): Error on writing IPC getsockname Response.");
  }

  return rc;
}

int demux_ipc_socket_call(const int &sockfd, uint8_t *cmd_buff,
                          const int &blen) {
  Ipc_Msg *msg = reinterpret_cast<Ipc_Msg *>(cmd_buff);

  switch (msg->type) {
  case IPC_SOCKET:
    return ipc_socket(sockfd, msg);
    break;
  case IPC_CONNECT:
    return ipc_connect(sockfd, msg);
    break;
  case IPC_WRITE:
    return ipc_write(sockfd, msg);
    break;
  case IPC_READ:
    return ipc_read(sockfd, msg);
    break;
  case IPC_CLOSE:
    return ipc_close(sockfd, msg);
    break;
  case IPC_POLL:
    return ipc_poll(sockfd, msg);
    break;
  case IPC_FCNTL:
    return ipc_fcntl(sockfd, msg);
    break;
  case IPC_GETSOCKOPT:
    return ipc_getsockopt(sockfd, msg);
  case IPC_GETPEERNAME:
    return ipc_getpeername(sockfd, msg);
  case IPC_GETSOCKNAME:
    return ipc_getsockname(sockfd, msg);
  default:
    print_err("ERR(demux_ipc_socket_call): No such IPC type-{}.", msg->type);
    break;
  };

  return 0;
}

void *socket_ipc_open(void *args) {
  int blen = IPC_BUFF_LEN;
  int sockfd = *reinterpret_cast<int *>(args);

  std::vector<uint8_t> buff(blen);
  int rc = -1;

  while ((rc = read(sockfd, buff.data(), blen)) > 0) {
    rc = demux_ipc_socket_call(sockfd, buff.data(), blen);

    if (rc == -1) {
      print_err("ERR(socket_ipc_open): Error on demuxing IPC socket call.");
      close(sockfd);
      return NULL;
    };
  }

  ipc_free_thread(sockfd);

  if (rc == -1) {
    throw std::runtime_error("ERR(socket_ipc_open): Socket IPC Read.");
  }

  return nullptr;
}

void start_ipc_listener() {
  int fd, rc, datasock;
  sockaddr_un un;
  constexpr char sockname[] = "/tmp/synack.socket";

  unlink(sockname);

  if (strnlen(sockname, sizeof(un.sun_path)) == sizeof(un.sun_path)) {
    print_err("ERR(start_ipc_listener): Path for UNIX socket is too long.");
    exit(-1);
  }

  if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
    throw std::runtime_error(
        "ERR(start_ipc_listener): IPC listener UNIX socket.");
  }

  memset(&un, 0, sizeof(struct sockaddr_un));
  un.sun_family = AF_UNIX;
  std::strncpy(un.sun_path, sockname, sizeof(un.sun_path) - 1);

  rc = bind(fd, (const struct sockaddr *)&un, sizeof(struct sockaddr_un));

  if (rc == -1) {
    throw std::runtime_error("ERR(start_ipc_listener): IPC bind.");
    exit(EXIT_FAILURE);
  }

  rc = listen(fd, 20);

  if (rc == -1) {
    perror("IPC listen");
    exit(EXIT_FAILURE);
  }

  if (chmod(sockname, S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP |
                          S_IXGRP | S_IROTH | S_IWOTH | S_IXOTH) == -1) {
    throw std::runtime_error(
        "ERR(start_ipc_listener): Chmod on lvl-ip IPC UNIX socket failed.");
  }

  while (true) {
    datasock = accept(fd, NULL, NULL);
    if (datasock == -1) {
      throw std::runtime_error("ERR(start_ipc_listener): IPC accept.");
      exit(EXIT_FAILURE);
    }

    Ipc_Thread *ipcth = new Ipc_Thread(datasock);

    std::thread t(socket_ipc_open, &ipcth->sock);
    ipcth->id = t.get_id();
    t.detach();
  }

  close(fd);
  unlink(sockname);
}