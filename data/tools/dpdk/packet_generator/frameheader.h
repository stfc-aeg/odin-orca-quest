#ifndef _X10GPROTOCOL_H_
#define _X10GPROTOCOL_H_

#include <stdint.h>

#include <rte_byteorder.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PKTS_PER_FRAME 250
#define PKT_UDP_DATA_LEN 8064
#define PKT_PAYLOAD_LEN 8000

struct x10g_hdr {
    rte_be64_t frame_num;
    uint8_t markers;
    uint8_t _unused_1;
    uint8_t padding_bytes;
    uint8_t readout_lane;
    rte_be32_t packet_num;
    rte_be64_t padding[6];
} __rte_packed;


struct pkt_rx_header {
    uint64_t frame_number;
    uint32_t packets_received;
    uint32_t packets_dropped;
    uint8_t sof_marker_count;
    uint8_t eof_marker_count;
    uint64_t frame_start_clock_stamp;
	uint64_t frame_complete_clock_stamp;
	uint32_t frame_time_delta;
    uint8_t packet_markers[PKTS_PER_FRAME];
} __rte_packed;

struct config_struct {
    uint64_t interval;
	uint64_t starting_frame_number;
	uint64_t number_of_frames;
	char destination_ip_address[20];
	char destination_mac_address[20];
    uint16_t destination_port;
	char source_ip_address[20];
	char source_mac_address[20];
    uint16_t source_port;
	uint64_t test_pattern_mode;
	uint64_t drop_packets;
	uint64_t drop_frames;
} __rte_packed;



// Defaults

#define DEFAULT_INTERVAL 1000
#define DEFAULT_STARTING_FRAME_NUMBER 0
#define DEFAULT_NUMBER_OF_FRAMES 1000
#define DEFAULT_DEST_IP_ADDR "10.100.0.6"
#define DEFAULT_DEST_MAC_ADDR "08:c0:eb:f8:28:7c"
#define DEFAULT_DEST_PORT 1234
#define DEFAULT_SOURCE_IP_ADDR "10.100.0.5"
#define DEFAULT_SOURCE_MAC_ADDR "08:c0:eb:f8:28:6c"
#define DEFAULT_SOURCE_PORT 1234
#define DEFAULT_TEST_PATTERN_MODE 1
#define DEFAULT_DROP_PACKETS 0
#define DEFAULT_DROP_FRAMES 0



#ifdef __cplusplus
}
#endif

#endif // _X10GPROTOCOL_H_