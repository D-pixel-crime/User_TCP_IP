#include "../include/network_device.hpp"
#include "../include/arp.hpp"
#include "../include/ethernet.hpp"
#include "../include/ip.hpp"
#include "../include/socket_buffer.hpp"
#include "../include/tuntap_interface.hpp"
#include "../include/utils.hpp"

extern int running;

Network_Device::Network_Device(const std::string &_addr,
                               const std::string &_hwaddr, uint32_t _mtu)
    : addr(parse_ipv4_string(_addr)), mtu(_mtu) {
  addr_len = sizeof(addr);
  auto parsedMac = parse_MAC_string(_hwaddr);
  if (parsedMac) {
    std::memcpy(hwaddr, parsedMac, sizeof(hwaddr));
  }
}

void network_device_init() {
  loopback = new Network_Device("127.0.0.1", "00:00:00:00:00:00", 1500);
  netdev = new Network_Device("10.0.0.10", "00:0c:29:6d:50:25", 1500);
}

int netdev_transmit(SkBuff *skb, uint8_t *dst_hw, const uint16_t &ethertype) {
  Eth_Hdr *hdr;
  int ret = 0;
  Network_Device *dev = skb->dev;

  skb->push(Eth_Hdr::getSize());

  hdr = reinterpret_cast<Eth_Hdr *>(skb->data);

  std::memcpy(hdr->dmac, dst_hw, sizeof(dev->hwaddr));
  std::memcpy(hdr->smac, dev->hwaddr, sizeof(dev->hwaddr));

  hdr->ethertype = htons(ethertype);
  eth_debug("Out", hdr);

  ret = tuntap_write(reinterpret_cast<uint8_t *>(skb->data), skb->len);

  return ret;
}

int netdev_receive(SkBuff *skb) {
  Eth_Hdr *hdr = reinterpret_cast<Eth_Hdr *>(skb);
  eth_debug("In", hdr);

  switch (hdr->ethertype) {
  case ETH_P_ARP:
    arp_rcv(skb);
    break;

  case ETH_P_IP:
    ip_rcv(skb);
    break;

  case ETH_P_IPV6:
    std::cout << "IPv6 Protocol not yet implemented!" << std::endl;
    break;

  default:
    print_err("Protocol-{} NOT supported!", hdr->ethertype);
    break;
  }

  return 0;
}

void *netdev_rx_loop() {
  while (running) {
    SkBuff *skb = new SkBuff(BUFFLEN);

    if (tuntap_read(reinterpret_cast<uint8_t *>(skb->data), BUFFLEN) < 0) {
      print_err("ERR(netdev_rx_loop): Reading from tuntap_read!");
      free_skb(skb);
      return nullptr;
    }

    netdev_receive(skb);
  }

  return nullptr;
}

Network_Device *netdev_get(const uint32_t &sip) {
  if (netdev->addr == sip) {
    return netdev;
  }
  return nullptr;
}

void free_netdev() {
  if (loopback) {
    delete loopback;
  }
  if (netdev) {
    delete netdev;
  }
}