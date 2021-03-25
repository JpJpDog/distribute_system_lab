#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_mbuf.h>
#include <rte_mempool.h>

#define RX_RING_SIZE 1024
#define TX_RING_SIZE 1024
#define NUM_MBUFS 8191
#define MBUF_CACHE_SIZE 250
#define BURST_SIZE 32

static const struct rte_eth_conf port_conf_default = {
    .rxmode =
        {
            .max_rx_pkt_len = RTE_ETHER_MAX_LEN,
        },
};

/*
 * Initializes a given port using global settings and with the RX buffers
 * coming from the mbuf_pool passed as a parameter.
 */
static inline int port_init(uint16_t port, struct rte_mempool *mbuf_pool) {
  struct rte_eth_conf port_conf = port_conf_default;
  const uint16_t rx_rings = 1, tx_rings = 1;
  uint16_t nb_rxd = RX_RING_SIZE;
  uint16_t nb_txd = TX_RING_SIZE;
  int retval;
  uint16_t q;
  struct rte_eth_dev_info dev_info;
  struct rte_eth_txconf txconf;
  if (!rte_eth_dev_is_valid_port(port)) return -1;
  retval = rte_eth_dev_info_get(port, &dev_info);
  if (retval != 0) {
    printf("Error during getting device (port %u) info: %s\n", port,
           strerror(-retval));
    return retval;
  }
  // set port config according to device
  if (dev_info.tx_offload_capa & DEV_TX_OFFLOAD_MBUF_FAST_FREE) {
    port_conf.txmode.offloads |= DEV_TX_OFFLOAD_MBUF_FAST_FREE;
  }
  /* Configure the Ethernet device. */
  retval = rte_eth_dev_configure(port, rx_rings, tx_rings, &port_conf);
  if (retval != 0) return retval;
  // check and adjust rx and tx num according to device
  retval = rte_eth_dev_adjust_nb_rx_tx_desc(port, &nb_rxd, &nb_txd);
  if (retval != 0) return retval;
  /* Allocate and set up 1 RX queue per Ethernet port. */
  for (q = 0; q < rx_rings; q++) {
    retval = rte_eth_rx_queue_setup(
        port, q, nb_rxd, rte_eth_dev_socket_id(port), NULL, mbuf_pool);
    if (retval < 0) return retval;
  }
  txconf = dev_info.default_txconf;
  txconf.offloads = port_conf.txmode.offloads;
  /* Allocate and set up 1 TX queue per Ethernet port. */
  for (q = 0; q < tx_rings; q++) {
    retval = rte_eth_tx_queue_setup(port, q, nb_txd,
                                    rte_eth_dev_socket_id(port), &txconf);
    if (retval < 0) return retval;
  }
  /* Start the Ethernet port. */
  retval = rte_eth_dev_start(port);
  if (retval < 0) return retval;
  /* Display the port MAC address. */
  struct rte_ether_addr addr;
  retval = rte_eth_macaddr_get(port, &addr);
  if (retval != 0) return retval;
  printf("Port %u MAC: %02" PRIx8 " %02" PRIx8 " %02" PRIx8 " %02" PRIx8
         " %02" PRIx8 " %02" PRIx8 "\n",
         port, addr.addr_bytes[0], addr.addr_bytes[1], addr.addr_bytes[2],
         addr.addr_bytes[3], addr.addr_bytes[4], addr.addr_bytes[5]);
  /* Enable RX in promiscuous mode for the Ethernet device. */
  retval = rte_eth_promiscuous_enable(port);
  if (retval != 0) return retval;
  return 0;
}

#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_udp.h>

#include <algorithm>
#include <cstring>

int gen_data(char **out_data) {
  const int data_len = 20000;
  char *data = (char *)malloc(data_len);
  char *ch = "hi dpdk!";
  int ch_len = strlen(ch);
  int off = 0;
  while (off + ch_len < data_len) {
    strncpy(data + off, ch, ch_len);
    off += ch_len;
  }
  data[off] = '\0';
  *out_data = data;
  printf("## data_len %d\n", off + 1);
  return off + 1;
}

static inline char *safe_append_mbuf(struct rte_mbuf *mbuf, int len) {
  char *result = rte_pktmbuf_append(mbuf, len);
  if (result == NULL) {
    rte_exit(EXIT_FAILURE, "No enough sapce\n");
  }
  return result;
}

static inline struct rte_mbuf *safe_alloc_mbuf(struct rte_mempool *mbuf_pool) {
  struct rte_mbuf *mbuf = rte_pktmbuf_alloc(mbuf_pool);
  if (mbuf == NULL) {
    rte_exit(EXIT_FAILURE, "No enough mbuf!\n");
  }
  return mbuf;
}

static uint16_t checksum(const void *start, int len) {
  uint32_t sum = 0;
  uint16_t *ch = (uint16_t *)start;
  for (; len > 1; ch++, len -= 2) {
    sum += *ch;
    sum = (sum >> 16) + (sum & 0xffff);
  }
  if (len) {
    sum += *((uint8_t *)ch);
    sum = (sum >> 16) + (sum & 0xffff);
  }
  return (uint16_t)sum;
}

static void inline fill_ether_hdr(struct rte_ether_hdr *hdr) {
  struct rte_ether_addr s_addr = {{0x00, 0x0c, 0x29, 0x8e, 0xae, 0xf9}};
  struct rte_ether_addr d_addr = {{0x00, 0x0c, 0x29, 0x8e, 0xae, 0xf9}};
  hdr->s_addr = s_addr;
  hdr->d_addr = d_addr;
  hdr->ether_type = rte_cpu_to_be_16(0x0800);
}

static void inline fill_ipv4_hdr(struct rte_ipv4_hdr *hdr, int off, bool more,
                                 int len) {
  hdr->version_ihl = (4 << 4) + 5;  // ipv4, length 5 (*4)
  hdr->type_of_service = 0;
  hdr->total_length = rte_cpu_to_be_16(len);  // tcp 20
  hdr->packet_id = rte_cpu_to_be_16(111);     // set random
  uint16_t tmp = off >> 3;                    // first three bit is the flag
  if (more) tmp |= 0x2000;                    // set more flag if not end
  hdr->fragment_offset = rte_cpu_to_be_16(tmp);
  hdr->time_to_live = 64;
  hdr->next_proto_id = 17;  // udp:17 tcp:6
  hdr->hdr_checksum = rte_cpu_to_be_16(0);
  hdr->src_addr = rte_cpu_to_be_32(0x01010101);  // 1.1.1.1
  hdr->dst_addr = rte_cpu_to_be_32(0x01010101);  // 1.1.1.1
  hdr->hdr_checksum = rte_cpu_to_be_16(~checksum(hdr, sizeof(*hdr)));
}

static void inline fill_udp_hdr(struct rte_udp_hdr *hdr, int data_len,
                                uint16_t chksum) {
  hdr->dgram_cksum = rte_cpu_to_be_16(0);
  hdr->dgram_len = rte_cpu_to_be_16(data_len);
  hdr->dst_port = rte_cpu_to_be_16(80);
  hdr->src_port = rte_cpu_to_be_16(80);
  uint32_t chksum1 = (uint32_t)(~checksum(hdr, sizeof(*hdr))) + chksum;
  hdr->dgram_cksum =
      rte_cpu_to_be_16(~(uint16_t)((chksum1 >> 16) + (chksum1 & 0xffff)));
}

static inline void fill_hdr(struct rte_mbuf *mbuf, int off, bool more,
                            int data_len) {
  struct rte_ether_hdr *ether_hdr =
      (struct rte_ether_hdr *)safe_append_mbuf(mbuf, sizeof(*ether_hdr));
  fill_ether_hdr(ether_hdr);
  struct rte_ipv4_hdr *ipv4_hdr =
      (struct rte_ipv4_hdr *)safe_append_mbuf(mbuf, sizeof(*ipv4_hdr));
  fill_ipv4_hdr(ipv4_hdr, off, more, data_len + sizeof(struct rte_ipv4_hdr));
}

static inline void send_all(struct rte_mbuf **mbufs, int burst_n) {
  printf("%d\n", burst_n);
  int send_n = 0;
  while (send_n < burst_n) {
    send_n += rte_eth_tx_burst(0, 0, mbufs + send_n, burst_n - send_n);
  }
}

static void udp_send(const char *data, int data_len,
                     struct rte_mempool *mbuf_pool) {
  const int MTU = 1496;
  const int ether_ip_hdr_size = sizeof(rte_ether_hdr) + sizeof(rte_ipv4_hdr);
  if (MTU + ether_ip_hdr_size > rte_pktmbuf_data_room_size(mbuf_pool)) {
    rte_exit(EXIT_FAILURE, "one pkt larger than mbuf data size\n");
  }
  struct rte_mbuf **mbufs = (struct rte_mbuf **)malloc(
      sizeof(*mbufs) *
      ((data_len + sizeof(rte_udp_hdr)) / (MTU - ether_ip_hdr_size) + 1));
  int off = 0, burst_l = 0;
  struct rte_mbuf *mbuf = safe_alloc_mbuf(mbuf_pool);
  int ip_data_len = std::min(data_len + (int)sizeof(struct rte_udp_hdr), MTU);
  fill_hdr(mbuf, 0, data_len + sizeof(struct rte_udp_hdr) > MTU, ip_data_len);
  struct rte_udp_hdr *udp_hdr =
      (struct rte_udp_hdr *)safe_append_mbuf(mbuf, sizeof(*udp_hdr));
  uint32_t chksum = checksum(udp_hdr - 8, 8) + checksum(data, data_len);
  chksum = (chksum >> 16) + (chksum & 0xffff);
  fill_udp_hdr(udp_hdr, data_len, chksum);
  char *data_dst = safe_append_mbuf(mbuf, ip_data_len - sizeof(*udp_hdr));
  memcpy(data_dst, data, ip_data_len);
  off += ip_data_len;
  mbufs[burst_l++] = mbuf;
  while (off < data_len) {
    mbuf = safe_alloc_mbuf(mbuf_pool);
    ip_data_len = std::min(data_len - off, MTU);
    fill_hdr(mbuf, off, data_len - off > MTU, ip_data_len);
    data_dst = safe_append_mbuf(mbuf, ip_data_len);
    memcpy(data_dst, data + off, ip_data_len);
    off += ip_data_len;
    mbufs[burst_l++] = mbuf;
  }
  send_all(mbufs, burst_l);
  free(mbufs);
}

int main(int argc, char **argv) {
  // init eal envirnment
  int ret = rte_eal_init(argc, argv);
  if (ret < 0) {
    rte_exit(EXIT_FAILURE, "Error with EAL initialization\n");
  }
  argc -= ret;
  argv += ret;
  unsigned nb_ports = rte_eth_dev_count_avail();
  // init rte_mempool to alloc mbuf, each port has a pool with NUM_MBUFS size,
  struct rte_mempool *mbuf_pool = rte_pktmbuf_pool_create(
      "MBUF_POOL", NUM_MBUFS * nb_ports, MBUF_CACHE_SIZE, 0,
      RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
  if (mbuf_pool == NULL) {
    rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");
  }
  // init every port by port_init
  uint16_t portid;
  RTE_ETH_FOREACH_DEV(portid) {
    if (port_init(portid, mbuf_pool) != 0) {
      rte_exit(EXIT_FAILURE, "Cannot init port %" PRIu16 "\n", portid);
    }
  }
  char *data;
  int len = gen_data(&data);
  udp_send(data, len, mbuf_pool);
  return 0;
}
