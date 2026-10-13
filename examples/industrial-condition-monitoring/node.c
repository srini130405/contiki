#include "contiki-conf.h"
#include "contiki.h"

#include "net/netstack.h"
#include "net/mac/tsch/tsch.h"

#include "net/ip/uip.h"
#include "net/ip/uip-debug.h"
#include "net/ip/uip-udp-packet.h"
#include "net/rpl/rpl.h"

#include "rpl-tools.h"
#include "packet-format.h"
#include "sensor.h"
#include "node-id.h"
#include "leds.h"
#include "radio-time.h"
#include <stdio.h>
#include <string.h>


/* =========================================================
 * Configuration (override in project-conf.h)
 * ========================================================= */

#define UDP_PORT 8185

#define STARTUP_DELAY (7 * CLOCK_SECOND)

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


/* Window counter, shared by all packets of one window */
static uint8_t seq;


/* =========================================================
 * Packet senders
 * ========================================================= */

static void
fill_hdr(struct pkt_hdr *hdr, uint8_t type)
{
  hdr->type = type;
  hdr->seq = seq;
  hdr->node_id = node_id;
}

static void
send_rms(const uint16_t rms[3])
{
  struct pkt_rms pkt;

  memset(&pkt, 0, sizeof(pkt));
  fill_hdr(&pkt.hdr, PACKET_FORMAT_TYPE_RMS);
  pkt.x = rms[0];
  pkt.y = rms[1];
  pkt.z = rms[2];

  uip_udp_packet_send(udp_conn, &pkt, sizeof(pkt));
}

static void
send_alert(const uint16_t rms[3], uint8_t axes)
{
  struct pkt_alert pkt;

  memset(&pkt, 0, sizeof(pkt));
  fill_hdr(&pkt.hdr, PACKET_FORMAT_TYPE_ALERT);
  pkt.x = rms[0];
  pkt.y = rms[1];
  pkt.z = rms[2];
  pkt.axes = axes;

  uip_udp_packet_send(udp_conn, &pkt, sizeof(pkt));
}

#if APP_CONF_SEND_RAW_ON_ALERT
/* Send the whole current window as RAW chunks */
static void
send_raw_window(void)
{
  struct pkt_raw pkt;
  uint8_t chunk;

  for(chunk = 0; chunk < PACKET_FORMAT_RAW_TOTAL_CHUNKS; chunk++) {
    uint16_t first = chunk * PACKET_FORMAT_RAW_SAMPLES_PER_PACKET;
    uint8_t i;

    memset(&pkt, 0, sizeof(pkt));
    fill_hdr(&pkt.hdr, PACKET_FORMAT_TYPE_RAW);
    pkt.index = chunk;

    for(i = 0; i < PACKET_FORMAT_RAW_SAMPLES_PER_PACKET && first + i < WINDOW_SAMPLES; i++) {
      sensor_get_sample(first + i, pkt.xyz[i]);
    }
    pkt.count = i;

    uip_udp_packet_send(udp_conn, &pkt, sizeof(pkt));
  }
}
#endif /* APP_CONF_SEND_RAW_ON_ALERT */

/* Bitmask of axes whose RMS is over the threshold: bit0 x, bit1 y, bit2 z */
static uint8_t
axes_over_threshold(const uint16_t rms[3])
{
  uint8_t axes = 0;

#if APP_CONF_ALERT_RMS_THRESHOLD > 0
  if(rms[0] > APP_CONF_ALERT_RMS_THRESHOLD) {
    axes |= 1;
  }
  if(rms[1] > APP_CONF_ALERT_RMS_THRESHOLD) {
    axes |= 2;
  }
  if(rms[2] > APP_CONF_ALERT_RMS_THRESHOLD) {
    axes |= 4;
  }
#endif
  return axes;
}


PROCESS_THREAD(leaf_process, ev, data)
{
  static struct etimer startup_timer;
  static struct etimer send_timer;
  static struct etimer rms_timer;

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
   * ----------------------------e--------------------------- */


uip_ip6addr(&coordinator_ipaddr,
            0xbbbb, 0, 0, 0,
            0, 0, 0, 1);

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

  printf("\n[LEAF] Monitoring: %u ms window (%u samples) every %lu s, "
         "RMS report every %lu s, alert above %u mg\n",
         RMS_WINDOW_MS, WINDOW_SAMPLES,
         (unsigned long)(APP_CONF_ALERT_CHECK_INTERVAL / CLOCK_SECOND),
         (unsigned long)(APP_CONF_RMS_SEND_INTERVAL / CLOCK_SECOND),
         APP_CONF_ALERT_RMS_THRESHOLD);


  /* -------------------------------------------------------
   * Monitoring loop
   *
   * Every APP_CONF_ALERT_CHECK_INTERVAL: sample a window, compute RMS and send
   * an ALERT right away if any axis is over the threshold.
   * Every APP_CONF_RMS_SEND_INTERVAL: also send that window's RMS.
   * ------------------------------------------------------- */

  etimer_set(&send_timer, APP_CONF_ALERT_CHECK_INTERVAL);
  etimer_set(&rms_timer, APP_CONF_RMS_SEND_INTERVAL);
  static uint64_t report_count=0;

  while(1) {


    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&send_timer));
    etimer_reset(&send_timer);

    if(rpl_get_any_dag() != NULL) {
      uint16_t rms[3];
      uint8_t axes;

      sensor_sample_window();
      sensor_window_rms(rms);

      axes = axes_over_threshold(rms);
      if(axes) {
        printf("\n[LEAF] ALERT seq %u axes 0x%x: x=%u y=%u z=%u mg\n",
               seq, axes, rms[0], rms[1], rms[2]);
        send_alert(rms, axes);
#if APP_CONF_SEND_RAW_ON_ALERT
        send_raw_window();
#endif
      }

      if(etimer_expired(&rms_timer)) {
        etimer_reset(&rms_timer);
        printf("\n[LEAF] RMS seq %u: x=%u y=%u z=%u mg\n",
               seq, rms[0], rms[1], rms[2]);
        send_rms(rms);
      }

      seq++;
    } else {
      printf("[LEAF] No DAG yet, skipping send\n");
    }
    if((report_count++)%RADIO_REPORT_FREQUENCY==0)
      radio_time_report();
  }

  PROCESS_END();
}