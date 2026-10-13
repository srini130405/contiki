#include "contiki-conf.h"
#include "contiki.h"

#include "net/netstack.h"
#include "net/mac/tsch/tsch.h"

#include "net/ip/uip.h"
#include "net/ip/uip-debug.h"
#include "net/ip/uip-udp-packet.h"
#include "net/packetbuf.h"
#include "net/linkaddr.h"

#include "rpl-tools.h"
#include "packet-format.h"
#include "node-id.h"

#include <stdio.h>
#include <string.h>


#define UDP_PORT 8185
#define STARTUP_DELAY (7 * CLOCK_SECOND)


PROCESS(coordinator_process, "RPL/TSCH Coordinator");
AUTOSTART_PROCESSES(&coordinator_process);

static struct uip_udp_conn *udp_conn;

#ifndef UIP_IP_BUF
#define UIP_IP_BUF ((struct uip_ip_hdr *)&uip_buf[UIP_LLH_LEN])
#endif


/* Received packet, copied out of uip_buf so the structs are aligned */
static union {
  struct pkt_hdr hdr;
  struct pkt_rms rms;
  struct pkt_alert alert;
  struct pkt_raw raw;
} pkt;


static void
print_packet(uint16_t len)
{
  uint8_t i;

  switch(pkt.hdr.type) {
  case PACKET_FORMAT_TYPE_RMS:
    printf("[COORD] RMS node %u seq %u: x=%u y=%u z=%u mg\n",
           pkt.hdr.node_id, pkt.hdr.seq, pkt.rms.x, pkt.rms.y, pkt.rms.z);
    break;

  case PACKET_FORMAT_TYPE_ALERT:
    printf("[COORD] ALERT node %u seq %u axes 0x%x: x=%u y=%u z=%u mg\n",
           pkt.hdr.node_id, pkt.hdr.seq, pkt.alert.axes,
           pkt.alert.x, pkt.alert.y, pkt.alert.z);
    break;

  case PACKET_FORMAT_TYPE_RAW:
    printf("[COORD] RAW node %u seq %u chunk %u (%u samples)\n",
           pkt.hdr.node_id, pkt.hdr.seq, pkt.raw.index, pkt.raw.count);
    for(i = 0; i < pkt.raw.count && i < PACKET_FORMAT_RAW_SAMPLES_PER_PACKET; i++) {
      printf("  %u: %d %d %d\n",
             pkt.raw.index * PACKET_FORMAT_RAW_SAMPLES_PER_PACKET + i,
             pkt.raw.xyz[i][0], pkt.raw.xyz[i][1], pkt.raw.xyz[i][2]);
    }
    break;

  default:
    printf("[COORD] unknown packet type %u (len=%u)\n", pkt.hdr.type, len);
    break;
  }
}


static void
udp_rx_handler(void)
{
  if(uip_newdata()) {
    uint16_t len = uip_datalen();
    const linkaddr_t *hop;
    uint16_t relay_id;

    if(len > sizeof(pkt)) {
      len = sizeof(pkt);
    }
    /* memcpy avoids unaligned access on the JN516x */
    memset(&pkt, 0, sizeof(pkt));
    memcpy(&pkt, uip_appdata, len);

#if 1
    {
      uint16_t src[8];

      memcpy(src, UIP_IP_BUF->srcipaddr.u8, sizeof(src));
      printf("[COORD] UDP from %04x:%04x:%04x:%04x:"
             "%04x:%04x:%04x:%04x (len=%u)\n",
             uip_ntohs(src[0]), uip_ntohs(src[1]),
             uip_ntohs(src[2]), uip_ntohs(src[3]),
             uip_ntohs(src[4]), uip_ntohs(src[5]),
             uip_ntohs(src[6]), uip_ntohs(src[7]), len);
    }
#endif

    /*
     * Last hop: the UDP callback runs synchronously from the radio input
     * path, so packetbuf still holds the received frame's MAC source.
     * Node ID = last 2 bytes of the MAC, same as node_id on that node.
     */
    hop = packetbuf_addr(PACKETBUF_ADDR_SENDER);
    relay_id = (hop->u8[LINKADDR_SIZE - 2] << 8) | hop->u8[LINKADDR_SIZE - 1];

    printf("[COORD] RX from node %u via node %u%s (len=%u)\n",
           pkt.hdr.node_id, relay_id,
           relay_id == pkt.hdr.node_id ? " (direct)" : "", len);

    print_packet(len);
  }
}


PROCESS_THREAD(coordinator_process, ev, data)
{
  static struct etimer startup_timer;
  uip_ipaddr_t prefix;

  PROCESS_BEGIN();

  printf("\n");
  printf("========================================\n");
  printf(" RPL + TSCH + Orchestra Coordinator\n");
  printf("========================================\n");
  printf("[COORD] Node ID: %u\n", node_id);
  printf("[COORD] Waiting 7 seconds...\n");

  etimer_set(&startup_timer, STARTUP_DELAY);
  PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&startup_timer));

  printf("\n[COORD] Starting network...\n");

  uip_ip6addr(&prefix, 0xbbbb, 0, 0, 0, 0, 0, 0, 0);

  printf("[COORD] Initializing RPL/Orchestra/TSCH...\n");
  rpl_tools_init(&prefix);
  printf("[COORD] Network initialized\n");

  udp_conn = udp_new(NULL, 0, NULL);

  if(udp_conn == NULL) {
    printf("[COORD] ERROR: udp_new() failed\n");
    PROCESS_EXIT();
  }

  udp_bind(udp_conn, UIP_HTONS(UDP_PORT));

  printf("[COORD] UDP listening on port %u\n", UDP_PORT);

  printf("\n[COORD] Local network status:\n");
  print_network_status();

  printf("\n[COORD] Ready to receive data.\n");

  while(1) {
    PROCESS_WAIT_EVENT();

    if(ev == tcpip_event) {
      udp_rx_handler();
    }
  }

  PROCESS_END();
}