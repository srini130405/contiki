#ifndef PROJECT_CONF_H_
#define PROJECT_CONF_H_

/* =========================================================
 * TSCH
 * ========================================================= */

#define WITH_TSCH 1
#define WITH_TSCH_SECURITY 0

#define TSCH_LOG_CONF_LEVEL 2

#undef IEEE802154_CONF_PANID
#define IEEE802154_CONF_PANID 0x5254

#undef TSCH_CONF_JOIN_MY_PANID_ONLY
#define TSCH_CONF_JOIN_MY_PANID_ONLY 1

/*
 * TSCH is started explicitly by rpl_tools_init().
 */
#undef TSCH_CONF_AUTOSTART
#define TSCH_CONF_AUTOSTART 0

#undef FRAME802154_CONF_VERSION
#define FRAME802154_CONF_VERSION FRAME802154_IEEE802154E_2012

/* =========================================================
 * TSCH hopping sequence
 * ========================================================= */

#undef TSCH_CONF_DEFAULT_HOPPING_SEQUENCE

#define TSCH_HOPPING_SEQUENCE_MY_SEQUENCE \
  (uint8_t[]){17, 23, 15, 25, 19, 11, 13, 21}

#define TSCH_CONF_EB_HOPPING_SEQUENCE \
    {17, 23, 15, 25, 19, 11, 13, 21}

#define TSCH_EB_HOPPING_SEQUENCE_LEN 8

#define TSCH_CONF_JOIN_HOPPING_SEQUENCE \
    (uint8_t[]){17, 23, 15, 25, 19, 11, 13, 21}

#define TSCH_CONF_DEFAULT_HOPPING_SEQUENCE \
  TSCH_HOPPING_SEQUENCE_MY_SEQUENCE

/* =========================================================
 * 6TiSCH Minimal
 *
 * Disabled because Orchestra creates the schedule.
 * ========================================================= */

#define TSCH_SCHEDULE_CONF_WITH_6TISCH_MINIMAL 0

#define TSCH_CONF_WITH_LINK_SELECTOR 1


/* =========================================================
 * Orchestra
 * ========================================================= */

#define TSCH_CALLBACK_NEW_TIME_SOURCE \
  orchestra_callback_new_time_source

#define TSCH_CALLBACK_PACKET_READY \
  orchestra_callback_packet_ready

#define NETSTACK_CONF_ROUTING_NEIGHBOR_ADDED_CALLBACK \
  orchestra_callback_child_added

#define NETSTACK_CONF_ROUTING_NEIGHBOR_REMOVED_CALLBACK \
  orchestra_callback_child_removed


/* EB slotframe */
#define ORCHESTRA_CONF_EBSF_PERIOD 41

/* Common shared slotframe */
#define ORCHESTRA_CONF_COMMON_SHARED_PERIOD 7

/* Unicast slotframe */
#define ORCHESTRA_CONF_UNICAST_PERIOD 11

/* Sender-based unicast slots */
#define ORCHESTRA_CONF_UNICAST_SENDER_BASED 1

/* Collision-free hash */
#define ORCHESTRA_CONF_COLLISION_FREE_HASH 1

#define ORCHESTRA_CONF_MAX_HASH \
  (ORCHESTRA_CONF_UNICAST_PERIOD - 1)


/* =========================================================
 * RPL
 * ========================================================= */

#define RPL_CALLBACK_PARENT_SWITCH \
  tsch_rpl_callback_parent_switch

#define RPL_CALLBACK_NEW_DIO_INTERVAL \
  tsch_rpl_callback_new_dio_interval

/* RPL Trickle timer */

#undef RPL_CONF_DIO_INTERVAL_MIN
#define RPL_CONF_DIO_INTERVAL_MIN 12

#undef RPL_CONF_DIO_INTERVAL_DOUBLINGS
#define RPL_CONF_DIO_INTERVAL_DOUBLINGS 2

/* RPL probing */

#define RPL_CONF_PROBING_INTERVAL \
  (3 * CLOCK_SECOND)

#define RPL_CONF_PROBING_EXPIRATION_TIME \
  (2 * 60 * CLOCK_SECOND)

/* IPv6 router */

#undef UIP_CONF_ROUTER
#define UIP_CONF_ROUTER 1

/* RPL storing mode */

#undef RPL_CONF_MOP
#define RPL_CONF_MOP RPL_MOP_STORING_NO_MULTICAST

/* Default link metric */

#undef RPL_CONF_INIT_LINK_METRIC
#define RPL_CONF_INIT_LINK_METRIC 2

#define RPL_CONF_MAX_INSTANCES 1
#define RPL_CONF_MAX_DAG_PER_INSTANCE 1


/* =========================================================
 * TSCH timing
 * ========================================================= */

#define TSCH_CONF_EB_PERIOD \
  (1 * CLOCK_SECOND)

#define TSCH_CONF_KEEPALIVE_TIMEOUT \
  (24 * CLOCK_SECOND)


/* =========================================================
 * IPv6 / 6LoWPAN
 * ========================================================= */

/*
 * Enable 6LoWPAN fragmentation.
 */
#undef SICSLOWPAN_CONF_FRAG
#define SICSLOWPAN_CONF_FRAG 1

/*
 * IPv6 requires a 1280-byte buffer.
 */
#undef UIP_CONF_BUFFER_SIZE
#define UIP_CONF_BUFFER_SIZE 1280

/*
 * UDP connection table.
 */
#undef UIP_CONF_UDP_CONNS
#define UIP_CONF_UDP_CONNS 8

/*
 * No IPv6 fragmentation/reassembly.
 */
#undef UIP_CONF_IPV6_REASSEMBLY
#define UIP_CONF_IPV6_REASSEMBLY 0

/*
 * 6LoWPAN fragment timeout.
 */
#undef SICSLOWPAN_CONF_MAXAGE
#define SICSLOWPAN_CONF_MAXAGE 10


/* =========================================================
 * IPv6 Neighbor Discovery
 * ========================================================= */

#undef UIP_CONF_ND6_SEND_NS
#define UIP_CONF_ND6_SEND_NS 0

#undef UIP_CONF_ND6_SEND_RA
#define UIP_CONF_ND6_SEND_RA 0


/* =========================================================
 * TCP
 * ========================================================= */

#undef UIP_CONF_TCP
#define UIP_CONF_TCP 0


/* =========================================================
 * IPv6 tables
 * ========================================================= */

#undef UIP_CONF_DS6_ADDR_NBU
#define UIP_CONF_DS6_ADDR_NBU 1

#undef UIP_CONF_FWCACHE_SIZE
#define UIP_CONF_FWCACHE_SIZE 1

#undef UIP_CONF_MAX_ROUTES
#define UIP_CONF_MAX_ROUTES 28

#undef NBR_TABLE_CONF_MAX_NEIGHBORS
#define NBR_TABLE_CONF_MAX_NEIGHBORS 8


/* =========================================================
 * UDP
 * ========================================================= */

#undef UIP_CONF_UDP_CHECKSUMS
#define UIP_CONF_UDP_CHECKSUMS 1


/* =========================================================
 * TSCH / packet queues
 * ========================================================= */

#undef QUEUEBUF_CONF_NUM
#define QUEUEBUF_CONF_NUM 32

#undef TSCH_QUEUE_CONF_NUM_PER_NEIGHBOR
#define TSCH_QUEUE_CONF_NUM_PER_NEIGHBOR 32

#undef TSCH_CONF_DEQUEUED_ARRAY_SIZE
#define TSCH_CONF_DEQUEUED_ARRAY_SIZE 32

#undef TSCH_QUEUE_CONF_MAX_NEIGHBOR_QUEUES
#define TSCH_QUEUE_CONF_MAX_NEIGHBOR_QUEUES 8


/* =========================================================
 * MAC / RDC
 * ========================================================= */

#undef NETSTACK_CONF_MAC
#define NETSTACK_CONF_MAC tschmac_driver

#undef NETSTACK_CONF_RDC
#define NETSTACK_CONF_RDC nordc_driver


/* =========================================================
 * Link-layer security
 * ========================================================= */

/*
 * We are not using a LLSEC layer for this experiment.
 */
#undef NETSTACK_CONF_LLSEC
#define NETSTACK_CONF_LLSEC nullsec_driver


/* =========================================================
 * TSCH security
 * ========================================================= */

#if WITH_TSCH_SECURITY

#define LLSEC802154_CONF_ENABLED 1

#define TSCH_CONF_JOIN_SECURED_ONLY 0

#undef LLSEC802154_CONF_USES_EXPLICIT_KEYS
#define LLSEC802154_CONF_USES_EXPLICIT_KEYS 1

#undef LLSEC802154_CONF_USES_FRAME_COUNTER
#define LLSEC802154_CONF_USES_FRAME_COUNTER 0

#endif /* WITH_TSCH_SECURITY */


/* =========================================================
 * JN516x UART
 * ========================================================= */

#undef UART_HW_FLOW_CTRL
#define UART_HW_FLOW_CTRL 0

#undef UART_XONXOFF_FLOW_CTRL
#define UART_XONXOFF_FLOW_CTRL 1

#undef UART_BAUD_RATE
#define UART_BAUD_RATE UART_RATE_1000000


/* =========================================================
 * Exception handling
 * ========================================================= */

/*
 * Keep the behavior from the JN516x common configuration.
 */
#undef EXCEPTION_STALLS_SYSTEM
#define EXCEPTION_STALLS_SYSTEM 1


/* =========================================================
 * CoAP / REST
 *
 * Not required by our UDP experiment, so deliberately omitted.
 * ========================================================= */


/* =========================================================
 * Contiki version
 * ========================================================= */

#undef CONTIKI_VERSION_STRING
#define CONTIKI_VERSION_STRING "Contiki 3.x"


#endif /* PROJECT_CONF_H_ */