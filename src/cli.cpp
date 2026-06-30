#include "../include/cli.hpp"
#include "../include/system_headers.hpp" // IWYU pragma: keep

bool debug = 0;

static void usage(std::string_view app) {
  std::cerr << "Usage: " << app << "\n\n"
            << "Linux TCP/IP stack over TUN/TAP.\n"
            << "Requires CAP_NET_ADMIN. See capabilities(7).\n"
            << "Options:\n"
            << "  -d Enable debug logging\n"
            << "  -h Show this help\n\n";

  std::exit(EXIT_FAILURE);
}

int parse_opts(int &argc, char **&argv) {
  int opt;

  while ((opt = getopt(argc, argv, "hd")) != -1) {
    switch (opt) {
    case 'd':
      debug = 1;
      break;

    case 'h':
    default:
      usage(argv[0]);
    }
  }

  argc -= optind;
  argv += optind;

  return optind;
}

void parse_cli(int &argc, char **&argv) { parse_opts(argc, argv); }