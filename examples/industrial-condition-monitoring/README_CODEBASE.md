# Industrial Condition Monitoring — Codebase Overview

This document explains the current foundational firmware in `examples/industrial-condition-monitoring/`. It is intended for teammates who have not worked through the Contiki codebase before.

> **Current scope:** this is the networking foundation, not the finished industrial-monitoring application. The current node sends a simple integer value that increments by 10. Edge inference, abnormality alerts, and diagnostic-data transfer are planned application behavior to build on top of this foundation.

## 1. What the current code does

The project currently uses:

- **TSCH** for time-slotted, channel-hopping communication.
- **Orchestra** to create TSCH schedules autonomously based on network information.
- **RPL** to build an IPv6 routing topology over the wireless network.
- **IPv6 and UDP** to send application data to the coordinator.
- **A coordinator and one or more nodes** that can form a multi-hop network.

The current setup has been tested with a topology like this:

```text
Coordinator (RPL root)
       |
  Intermediate node
       |
     Leaf node
```

A node can send data to the coordinator through an intermediate node. In other words, the leaf does not have to communicate directly with the coordinator; RPL selects a parent and forwards packets along the route.

### Current application behavior

For now, the node's application payload is a simple test value that increments by **10** each time it sends a report. This makes it easy to see whether new packets are arriving and whether the values change as expected.

This is only a connectivity test. The intended industrial-monitoring behavior is:

1. **Normal operation:** process sensor measurements locally and periodically send compact features or status data.
2. **Abnormality detected at the edge:** send an alert to the coordinator.
3. **Diagnostic phase:** transmit additional diagnostic data while the machine is considered abnormal, subject to the network's capacity and scheduling policy.

The abnormality inference and diagnostic-transfer behavior are not implemented by the current increment-by-10 test payload.

## 2. Roles of the main components

### Coordinator

The coordinator starts the TSCH network as its coordinator and acts as the **RPL root**. It configures the network prefix `bbbb::/64`, uses the fixed global IPv6 address `bbbb::1`, and listens for application UDP packets on port **8185**.

The fixed address means the node firmware can send to the same coordinator address even if a different JN5168 dongle is used for the coordinator. Its link-local address still depends on the dongle's link-layer address.

### Node

A node joins the TSCH network by listening for Enhanced Beacons (EBs), synchronizes to the TSCH schedule, and participates in the RPL network. It sends its UDP test reports to `bbbb::1:8185` (IPv6 destination and UDP port).

The node's own global and link-local IPv6 addresses can contain an interface identifier derived from that node's link-layer address. This is expected; the coordinator is the endpoint we have explicitly fixed.

### TSCH, Orchestra, and RPL are different layers

- **TSCH** coordinates transmissions in time slots and changes radio channels according to a hopping sequence.
- **Orchestra** defines repeating slotframes and links used for beacons, shared traffic, and unicast traffic. It is the baseline scheduler in the current code.
- **RPL** builds the IPv6 routing topology and selects parent/forwarding relationships.
- **UDP** carries the application payload to the coordinator.

Joining TSCH and joining the RPL DAG are related but distinct events. The green LED in the current node application is turned on after the code observes that the node has joined an RPL DAG. It therefore indicates the application's **RPL-join condition**, not merely that an EB has been heard.

## 3. Building and flashing

Use [`README_BUILD.md`](README_BUILD.md) for the installation, build, flashing, and PuTTY setup instructions.

Build the two firmware images from the project directory:

```sh
make -f Makefile.coord
make -f Makefile.node
```

Flash the coordinator image to the coordinator dongle and the node image to each node dongle. Open the corresponding serial COM port in PuTTY at **1,000,000 baud, 8 data bits, no parity, 1 stop bit, and no flow control**.

## 4. Understanding PuTTY output

PuTTY displays serial messages printed by the firmware. The output mixes application messages (such as `[LEAF] Sending value = 10`) with lower-level TSCH and Orchestra debug messages. The two kinds of messages describe different layers of the same operation.

The logs below are real examples from the current setup. Some lines wrap or appear visually misaligned in PuTTY because several messages are printed close together; read each message by its text, not by its screen alignment.

### 4.1 Coordinator startup and Orchestra schedule

```text
[COORD] Starting network...
[COORD] Initializing RPL/Orchestra/TSCH...
Orchestra: initializing rule 0
TSCH-schedule: add_slotframe 0 41
TSCH-schedule: add_link 0 1 2 24 0 255
Orchestra: initializing rule 1
TSCH-schedule: add_slotframe 1 11
TSCH-schedule: add_link 1 1 0 1 1 255
Orchestra: initializing rule 2
TSCH-schedule: add_slotframe 2 7
TSCH-schedule: add_link 2 7 0 0 2 255
Orchestra: initialization done
TSCH: starting as coordinator, PAN ID 5254, asn-0.0
TSCH: starting as coordinator
Server IPv6 addresses:
  bbbb::1
  fe80::215:8d00:57:f9bc
[COORD] Network initialized
[COORD] UDP listening on port 8185
...
[COORD] Ready to receive data.
```

Step by step:

1. **Application startup:** `[COORD] Starting network...` and `[COORD] Initializing RPL/Orchestra/TSCH...` mark the start of the coordinator's network initialization.
2. **Orchestra rules:** Orchestra initializes three scheduling rules. Each `add_slotframe` line creates a repeating slotframe, and each `add_link` line adds a scheduled link to a slotframe. The configured periods are 41, 11, and 7 timeslots. The numeric fields in `add_link` are internal TSCH schedule parameters; you normally do not need to decode every field to verify startup.
3. **TSCH coordinator:** `TSCH: starting as coordinator, PAN ID 5254` means TSCH has started as the coordinator for PAN ID `0x5254` (the log prints `5254` without the `0x` prefix).
4. **IPv6 configuration:** `bbbb::1` is the coordinator's fixed global IPv6 address. `fe80::...` is its link-local address, which depends on the dongle's link-layer address.
5. **UDP listener:** `[COORD] UDP listening on port 8185` means the application has opened its UDP listener. `[COORD] Ready to receive data.` is the application-level ready message.

The coordinator's `Default route: None` is expected here: it is the RPL root, so it does not need a default route toward another RPL parent.

### 4.2 Coordinator sending Enhanced Beacons (EBs)

```text
TSCH-queue: packet is added put_index=0, packet=4004d50
TSCH: enqueue EB packet 37 18
TSCH-queue: packet is removed, get_index=0
TSCH: {asn-0.18 link-0-41-24-0 ch-17} bc-0-0 37 tx 0, st 0-1
```

These lines show the coordinator preparing and transmitting an Enhanced Beacon (EB):

- **`enqueue EB packet 37 18`** — TSCH queues an EB packet. The numbers are internal packet/log fields; they are not the application value.
- **`packet is added` / `packet is removed`** — the packet is being placed in, and then taken from, a TSCH transmit queue.
- **`asn-0.18`** — the TSCH Absolute Slot Number (ASN), shown in the log's compact notation. ASN advances as the network's timeslots progress.
- **`link-0-41-24-0`** — identifies the scheduled link being used, including its slotframe-related fields.
- **`ch-17`** — the radio channel used for this transmission (channel 17 in this example).
- **`bc`** — broadcast link/traffic.
- **`tx`** — this device transmitted during the slot.

Later EB lines show `ch-23` and `ch-15` as well. That is expected: the custom EB hopping sequence uses channels 17, 23, and 15. The channel order depends on the ASN and channel-selection calculation, so it need not appear as a simple repeating 17 → 23 → 15 pattern.

### 4.3 The node scans, receives an EB, and associates

The node initially prints lines such as:

```text
[LEAF] Starting network...
Orchestra: initializing rule 0
...
Orchestra: initialization done
TSCH: scanning on channel 17
TSCH: starting as node
Server IPv6 addresses:
  fe80::215:8d00:57:f9f9
[LEAF] Network stack initialized
[LEAF] UDP destination: :8185
[LEAF] Waiting for RPL/TSCH association...
TSCH: scanning on channel 23
TSCH: scanning on channel 17
TSCH: scanning on channel 15
...
```

- **`TSCH: scanning on channel N`** — the joining node is listening on a channel while looking for a compatible EB. It scans the configured join hopping sequence, which in this project is `{17, 23, 15}`.
- **`Waiting for RPL/TSCH association...`** — the application is waiting for the network-join condition. The code checks periodically whether an RPL DAG is available; it does not mean the radio is continuously printing a separate association status.
- **`UDP destination: :8185`** — this particular print format is not showing the IPv6 destination correctly; it only visibly shows the port. The configured destination should be `bbbb::1`, and successful delivery should be verified at the coordinator.

The key transition in the supplied node log is:

```text
TSCH: association: received packet (37 bytes) on channel 15
TSCH: parse_eb: no schedule
TSCH: update time source: 0 -> 188
TSCH-schedule: add_link 0 2 2 24 0 255
TSCH-schedule: add_link 1 2 0 1 1 255
TSCH: association done, sec 0, PAN ID 5254, asn-0.605, jp 1, timeslot id 0, hopping id 0, slotframe len 0 with 0 links, from 00:15:8d:00:00:57:f9:bc
```

Read these messages in order:

1. **`association: received packet ... on channel 15`** — the node has received a 37-byte packet on channel 15 while scanning. In this context, it is the EB that allows it to discover the TSCH network.
2. **`parse_eb: no schedule`** — the received EB did not provide a schedule in the format this parser was looking for. Do not interpret this line alone as a failed join: the subsequent log explicitly reports association completion, and Orchestra can add its own links.
3. **`update time source: 0 -> 188`** — TSCH changes its selected time source from none (`0`) to neighbor short address `188`. In the tested topology, `188` is the intermediate node, so this indicates that the leaf is synchronizing through that neighbor.
4. **`TSCH-schedule: add_link ...`** — links are being added to the local schedule as the node joins and Orchestra applies its scheduling rules.
5. **`association done ... PAN ID 5254`** — TSCH association has completed. The message includes security status, PAN ID, ASN and other internal TSCH fields. `from ...:f9:bc` is the link-layer address associated with the EB/source shown by the log.
6. **`TSCH: received from 188 ...`** — the node is receiving a TSCH frame from neighbor `188`, further showing communication with the intermediate node.

The node then prints:

```text
[LEAF] Joined RPL DAG. Network status:
...
- Default route:
  -- fe80::215:8d00:57:f9bc (lifetime: 0 seconds)
...
[LEAF] Starting UDP transmission
```

**TSCH association and RPL DAG membership are separate milestones.** `association done` confirms TSCH has joined/synchronized with the TSCH network. `[LEAF] Joined RPL DAG` is printed later when the application observes an RPL DAG. The application then turns on the green LED. Thus, the green LED indicates the application's RPL-DAG-join condition, not simply the first EB reception.

The default route's link-local next hop is `fe80::215:8d00:57:f9bc` in this example. It identifies the next hop used for traffic routed outside the node itself; in the tested multi-hop topology, that address corresponds to the intermediate node's link-local address. The displayed `lifetime: 0 seconds` should not be treated as proof of failure by itself: confirm the actual traffic path and inspect the status-printing code if the lifetime seems inconsistent. Similarly, `Routing entries (0 in total)` does not by itself indicate a broken network; a default route can exist without explicit destination-specific entries.

### 4.4 Reading TSCH transmit/receive lines and understanding the link

Examples from the supplied logs include:

```text
TSCH: {asn-0.bd7 link-2-7-0-2 ch-23} uc-1-0 58 rx 249, edr 97
TSCH: {asn-0.c1a link-1-11-7-1 ch-25} uc-1-0 100 rx 249, edr 1
TSCH: {asn-0.c47 link-2-7-0-2 ch-23} bc-1-0 96 rx 249, edr 33
TSCH: {asn-0.cf6 link-1-11-7-1 ch-21} uc-1-0 74 tx 188, st 2-1
```

The important fields are:

- **`asn-...`** — the ASN, which identifies progress through TSCH timeslots.
- **`link-...`** — the scheduled TSCH link used for the slot. The slotframe and link fields identify the schedule entry; consult the TSCH schedule code if you need the exact meaning of every numeric component.
- **`ch-N`** — the radio channel used in that timeslot.
- **`bc` / `uc`** — broadcast or unicast traffic/link context.
- **`tx` / `rx`** — whether this device transmitted or received in the slot.
- **`188` / `249`** — short addresses of the neighbor involved in the transmission/reception, as printed by this log.
- **`dr` / `edr`** — timing-drift-related values reported by the TSCH log. These are diagnostic fields, not the application payload.
- **`st`** — a compact TSCH status field. Its numeric subfields are implementation-specific; use the TSCH log/source definitions when diagnosing a particular status value.

For example, `uc-... tx 188` means the node is transmitting over a unicast link to neighbor short address `188`. In the tested topology, a leaf may send its packet to intermediate node `188`; the intermediate then forwards it toward the coordinator. A TSCH `tx` line confirms a transmission attempt, not necessarily end-to-end UDP delivery.

The coordinator's `TSCH: received from 249 ...` lines indicate that it has received TSCH frames from neighbor short address `249`. They can include routing/control traffic as well as frames related to application traffic; not every TSCH frame is a UDP report.

### 4.5 Following the application value from sender to receiver

On the node, the application-level message looks like:

```text
[LEAF] Sending value = 10
...
[LEAF] Sending value = 20
```

These are the simple test values generated by the current application. Each value increments by 10. The application queues a UDP datagram for the configured coordinator destination; TSCH then schedules the underlying radio transmissions. In a multi-hop setup, the first wireless hop may be to the intermediate node rather than directly to the coordinator.

On the coordinator, the important lines are:

```text
[COORD] UDP from bbbb:0000:0000:0000:0215:8d00:0057:f9f9
: value = 10 (len=2)
...
[COORD] UDP from bbbb:0000:0000:0000:0215:8d00:0057:f9f9
: value = 20 (len=2)
```

- **`[COORD] UDP from ...`** — the coordinator's UDP receive callback has received a datagram and prints the sender's IPv6 address. This is the node's address, not the coordinator's destination address.
- **`value = 10` / `value = 20`** — the application payload decoded by the coordinator. Seeing the values increase confirms that the application data is reaching the coordinator.
- **`len=2`** — the payload length reported by the application is two bytes, consistent with the current integer test value's encoding.

The lower-level lines around these messages (for example, `TSCH: received from 249` or `TSCH: {asn-...} ... rx ...`) show radio/link activity. The `[COORD] UDP ... value = ...` message is the stronger end-to-end check: it confirms the packet reached the coordinator's application layer and the payload was decoded.

Some TSCH queue messages, such as `send packet to 255`, refer to lower-layer broadcast/control traffic and are **not** the same as the application's UDP destination `bbbb::1`. Use the `[LEAF] Sending value = ...` and `[COORD] UDP ... value = ...` messages to follow the application payload.

### 4.6 Quick interpretation checklist

- **Repeated `scanning on channel ...`** — the node is still searching for a compatible EB.
- **`association: received packet`** — an EB-sized packet was received while scanning.
- **`association done`** — TSCH association completed.
- **`update time source ...`** — TSCH selected or changed its synchronization neighbor.
- **`Joined RPL DAG` plus green LED** — the application observed RPL DAG membership.
- **`[LEAF] Sending value = N`** — the node application is sending a test value.
- **`[COORD] UDP from ... value = N`** — the coordinator application received and decoded that value.
- **TSCH `tx` only** — a transmission occurred or was attempted at the link layer; it does not alone prove end-to-end delivery.

If the node joins but the coordinator does not print UDP values, check the node's configured IPv6 destination (`bbbb::1`), UDP port (`8185`), RPL parent/default route, and application send path.

## 5. Important `project-conf.h` settings

The project configuration overrides defaults in Contiki and TSCH/Orchestra. Change these settings deliberately and test both network formation and packet delivery after each change.

### Channel hopping sequences

The current setup uses three related but distinct configuration concepts:

| Setting | Purpose |
|---|---|
| `TSCH_CONF_DEFAULT_HOPPING_SEQUENCE` | The normal TSCH hopping sequence used for regular TSCH links, including data traffic. The current sequence contains eight channels: `{17, 23, 15, 25, 19, 11, 13, 21}`. |
| `TSCH_CONF_JOIN_HOPPING_SEQUENCE` | The sequence a joining node scans while looking for EBs. It is restricted to `{17, 23, 15}` so the node searches the three channels used by the EB sequence. |
| Custom EB hopping sequence in `tsch.c` | The project modification that selects `{17, 23, 15}` specifically for advertising/EB links, while keeping the normal eight-channel sequence for other link types. |

The EB and join sequences need to overlap: a joining node must listen on channels on which the coordinator actually transmits EBs. In this project, the EB and join sequences both use channels 17, 23, and 15.

**Why keep separate sequences?** This was introduced to reduce the channel-search space during network joining while preserving the larger hopping sequence for normal TSCH communication. The observed association-time measurements are preliminary; they are not a guarantee that joining will always be faster in every RF environment.

The custom EB sequence is implemented in the TSCH source code, rather than being controlled only by `project-conf.h`. If you change it, update the join sequence as well and verify the channel-selection logs.

### Orchestra slotframe periods

The current configuration includes:

```c
/* EB slotframe */
#define ORCHESTRA_CONF_EBSF_PERIOD 41

/* Common shared slotframe */
#define ORCHESTRA_CONF_COMMON_SHARED_PERIOD 7

/* Unicast slotframe */
#define ORCHESTRA_CONF_UNICAST_PERIOD 11

/* Sender-based unicast slots */
#define ORCHESTRA_CONF_UNICAST_SENDER_BASED 1
```

A **slotframe** is a repeating set of TSCH timeslots. Its period controls how often the schedule pattern repeats; these values are in slotframe timeslots, **not seconds**.

- **`ORCHESTRA_CONF_EBSF_PERIOD 41`** — the EB slotframe repeats over a period of 41 timeslots.
- **`ORCHESTRA_CONF_COMMON_SHARED_PERIOD 7`** — the common shared slotframe repeats every 7 timeslots. Shared cells allow eligible nodes to contend for transmissions.
- **`ORCHESTRA_CONF_UNICAST_PERIOD 11`** — the unicast slotframe repeats every 11 timeslots.
- **`ORCHESTRA_CONF_UNICAST_SENDER_BASED 1`** — selects Orchestra's sender-based unicast scheduling behavior. The unicast transmission cell is derived from the sender under Orchestra's scheduling rules.

The periods are not seconds, and they should not be interpreted as “send every 41/7/11 seconds.” Actual transmission opportunities depend on the schedule, link type, queue state, traffic, and channel offset. The current setup uses Orchestra as a baseline; a later criticality-aware scheduler may need to change how extra diagnostic traffic receives transmission opportunities.

### RPL probing

```c
#define RPL_CONF_PROBING_INTERVAL \
  (3 * CLOCK_SECOND)

#define RPL_CONF_PROBING_EXPIRATION_TIME \
  (2 * 60 * CLOCK_SECOND)
```

- **Probing interval: 3 seconds** — the configured interval between RPL link-probing operations, expressed in Contiki clock ticks.
- **Probing expiration time: 2 minutes** — the configured expiration interval associated with probing/neighbour information.

Probing helps RPL assess neighbour/link status. These settings affect routing maintenance, not the TSCH channel-hopping sequence. The exact effect also depends on the Contiki version and the code paths enabled in this build.

### TSCH beacon and keepalive timing

```c
#define TSCH_CONF_EB_PERIOD \
  (1 * CLOCK_SECOND)

#define TSCH_CONF_KEEPALIVE_TIMEOUT \
  (24 * CLOCK_SECOND)
```

- **`TSCH_CONF_EB_PERIOD` — 1 second:** configures the interval between EB transmissions in this project. EBs advertise the TSCH network and help joining nodes discover and synchronize with it.
- **`TSCH_CONF_KEEPALIVE_TIMEOUT` — 24 seconds:** configures the TSCH keepalive timeout. Keepalives help maintain the relationship with a time source; a timeout can trigger recovery/reassociation behavior depending on the TSCH implementation.

Shorter EB intervals can make opportunities to discover the network arrive more frequently, but they also increase beacon traffic and radio activity. Keepalive and EB settings should be evaluated together with reliability, power use, and the intended deployment.

## 6. Development advice

- Keep the working coordinator/node build as a baseline before changing TSCH, Orchestra, or RPL.
- Test both single-hop and multi-hop packet delivery after network-layer changes.
- Distinguish **EB discovery**, **TSCH synchronization/association**, **RPL DAG membership**, and **successful UDP delivery** when debugging.
- Record changes to hopping sequences and slotframe periods in the project documentation so all teammates test the same configuration.
