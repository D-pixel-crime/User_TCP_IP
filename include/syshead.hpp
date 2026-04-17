#pragma once

// c headers having no implementation in cpp
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <poll.h>
#include <netdb.h>
#include <sys/prctl.h>
#include <sys/capability.h>

// cpp equivalent of c headers
#include <cstdint>
#include <cstdlib>
#include <cstddef>
#include <iostream>
#include <string>
#include <format>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <filesystem>