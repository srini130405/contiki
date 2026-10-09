#include "contiki-conf.h"
#include "contiki.h"

#include "net/netstack.h"
#include "net/mac/tsch/tsch.h"

#include "net/ip/uip.h"
#include "net/ip/uip-debug.h"
#include "net/ip/uip-udp-packet.h"
#include "net/rpl/rpl.h"

#include "rpl-tools.h"
#include "node-id.h"
#include "leds.h"
#include <stdio.h>
#include <string.h>


/* =========================================================
 * Configuration
 * ========================================================= */

#define UDP_PORT 8185

#define STARTUP_DELAY (7 * CLOCK_SECOND)

/* Send one value every 5 seconds */
#define SEND_INTERVAL (5 * CLOCK_SECOND)

/* How often we check whether RPL has joined a DAG */
#define JOIN_CHECK_INTERVAL (1 * CLOCK_SECOND)


/*
 * Coordinator IPv6 address, built at runtime.
 *
 * NOTE: do NOT use a static initializer like {{ 0xbbbb, ... }}.
 * uip_ipaddr_t is a union whose first member is uint8_t u8[16], so the
 * 16-bit values get truncated to single bytes and the address is garbage.
 * uip_ip6addr() fills the u16[] members correctly.
 *
 * Coordinator link-layer address 00:15:8d:00:00:57:f9:f9
 *   -> IID with U/L bit flipped: 0215:8d00:0057:f9f9
 */
static uip_ipaddr_t coordinator_ipaddr;


PROCESS(leaf_process, "RPL/TSCH Orchestra Leaf");
AUTOSTART_PROCESSES(&leaf_process);


/* UDP connection */
static struct uip_udp_conn *udp_conn;


PROCESS_THREAD(leaf_process, ev, data)
{
  static struct etimer startup_timer;
  static struct etimer send_timer;

  static int value = 0;

  PROCESS_BEGIN();

  printf("\n");
  printf("========================================\n");
  printf(" RPL + TSCH + Orchestra Leaf\n");
  printf("========================================\n");

  printf("[LEAF] Node ID: %u\n", node_id);

  printf("[LEAF] Waiting 7 seconds...\n");
  leds_arch_init();
  etimer_set(&startup_timer, STARTUP_DELAY);
  PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&startup_timer));


  /* -------------------------------------------------------
   * Start as RPL leaf
   * ------------------------------------------------------- */

  printf("\n[LEAF] Starting network...\n");
  rpl_tools_init(NULL);
  printf("[LEAF] Network stack initialized\n");


  /* -------------------------------------------------------
   * UDP connection
   * ------------------------------------------------------- */

  uip_ip6addr(&coordinator_ipaddr,
              0xbbbb, 0x0000, 0x0000, 0x0000,
              0x0215, 0x8d00, 0x0057, 0xf9f9);

  udp_conn = udp_new(&coordinator_ipaddr, UIP_HTONS(UDP_PORT), NULL);

  if(udp_conn == NULL) {
    printf("[LEAF] ERROR: udp_new() failed\n");
    PROCESS_EXIT();
  }

  udp_bind(udp_conn, UIP_HTONS(UDP_PORT));

  printf("[LEAF] UDP destination: ");
  PRINT6ADDR(&coordinator_ipaddr);
  printf(":%u\n", UDP_PORT);


  /* -------------------------------------------------------
   * Wait until RPL has actually joined a DAG
   * (instead of a fixed 15 s delay)
   * ------------------------------------------------------- */

  printf("[LEAF] Waiting for RPL/TSCH association...\n");

  etimer_set(&send_timer, JOIN_CHECK_INTERVAL);
  while(rpl_get_any_dag() == NULL) {
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&send_timer));
    etimer_reset(&send_timer);
  }

  printf("\n[LEAF] Joined RPL DAG. Network status:\n");
  print_network_status();

  leds_arch_set(LEDS_GREEN);

  printf("\n[LEAF] Starting UDP transmission\n");


  /* -------------------------------------------------------
   * Periodic UDP transmission
   * ------------------------------------------------------- */

  etimer_set(&send_timer, SEND_INTERVAL);

  while(1) {

    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&send_timer));

    if(rpl_get_any_dag() != NULL) {
      char message[16];
      int len;

      value += 10;

      len = snprintf(message, sizeof(message), "%d", value);

      printf("\n[LEAF] Sending value = %s\n", message);

      uip_udp_packet_send(udp_conn, message, len);
    } else {
      printf("[LEAF] No DAG yet, skipping send\n");
    }

    etimer_reset(&send_timer);
  }

  PROCESS_END();
}