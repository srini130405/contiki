#include "contiki-conf.h"
#include "contiki.h"

#include "net/netstack.h"
#include "net/mac/tsch/tsch.h"

#include "net/ip/uip.h"
#include "net/ip/uip-debug.h"
#include "net/ip/uip-udp-packet.h"

#include "rpl-tools.h"
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


static void
udp_rx_handler(void)
{
  if(uip_newdata()) {
    char buf[17];
    uint16_t len = uip_datalen();

    if(len > sizeof(buf) - 1) {
      len = sizeof(buf) - 1;
    }
    /* memcpy avoids unaligned access on the JN516x */
    memcpy(buf, uip_appdata, len);
    buf[len] = '\0';

uint16_t src[8];

memcpy(src, UIP_IP_BUF->srcipaddr.u8, sizeof(src));

printf("[COORD] UDP from %04x:%04x:%04x:%04x:"
       "%04x:%04x:%04x:%04x\n",
       uip_ntohs(src[0]),
       uip_ntohs(src[1]),
       uip_ntohs(src[2]),
       uip_ntohs(src[3]),
       uip_ntohs(src[4]),
       uip_ntohs(src[5]),
       uip_ntohs(src[6]),
       uip_ntohs(src[7]));
  printf(": value = %s (len=%u)\n", buf, uip_datalen());
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