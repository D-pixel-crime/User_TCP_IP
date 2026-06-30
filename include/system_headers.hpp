#pragma once

// c headers having no implementation in cpp
#include <arpa/inet.h>
#include <fcntl.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/capability.h>
#include <sys/ioctl.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <algorithm> // IWYU pragma: keep
#include <cassert>
#include <condition_variable> // IWYU pragma: keep
#include <cstddef>            // IWYU pragma: keep
#include <cstdint>            // IWYU pragma: keep
#include <cstdlib>
#include <cstring>
#include <filesystem>   // IWYU pragma: keep
#include <format>       // IWYU pragma: keep
#include <functional>   // IWYU pragma: keep
#include <iostream>     // IWYU pragma: keep
#include <mutex>        // IWYU pragma: keep
#include <random>       // IWYU pragma: keep
#include <shared_mutex> // IWYU pragma: keep
#include <string>       // IWYU pragma: keep
#include <thread>       // IWYU pragma: keep