#include <stdint.h>
#include <sys/queue.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <inttypes.h>
#include <getopt.h>
#include <math.h>

#include <stdint.h>
#include <inttypes.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_cycles.h>
#include <rte_lcore.h>
#include <rte_mbuf.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_udp.h>
#include <pthread.h>
#include <string.h>
#include <rte_memzone.h>
#include <rte_errno.h>
#include <rte_hexdump.h>
#include <stdbool.h>
#include <getopt.h>

#include "frameheader.h"


#define JUMBO_FRAME_ELEMENT_SIZE 0x2600
#define NUM_MBUFS 1048575
#define MBUF_CACHE_SIZE 250
#define RX_RING_SIZE 1024
#define TX_RING_SIZE 8192
#define DEBUG 0
#define PERF_STATS 0
#define PKT_BUFFERS 10

struct config_struct config_;




static const struct rte_eth_conf port_conf_default = {
	.rxmode = { .max_lro_pkt_size = JUMBO_FRAME_ELEMENT_SIZE,
				.offloads =  RTE_ETH_TX_OFFLOAD_IPV4_CKSUM |
						RTE_ETH_TX_OFFLOAD_UDP_CKSUM }	
};

static void
print_ethaddr(const char *name, const struct rte_ether_addr *eth_addr)
{
    char buf[RTE_ETHER_ADDR_FMT_SIZE];
    rte_ether_format_addr(buf, RTE_ETHER_ADDR_FMT_SIZE, eth_addr);
    printf("%s%s", name, buf);
}


int l2_len = sizeof(struct rte_ether_hdr);
int l3_len = sizeof(struct rte_ipv4_hdr);
int len_4 = sizeof (struct rte_udp_hdr);
int len_5 = sizeof(struct x10g_hdr);
int frame_rate = 1000;
int prepare_pkts = 2;
int pkt_size = 8106;


#define FILTER_SIZE 5

void gaussian_blur(uint16_t* image) {
    uint16_t blurred_image[1000*1000];
    // define the filter matrix
    int filter[FILTER_SIZE][FILTER_SIZE] = {{1, 2, 1}, {2, 4, 2}, {1, 2, 1}};

    // iterate over the image
    for (int y = 0; y < 1000; y++) {
        for (int x = 0; x < 1000; x++) {
                // apply the filter
                int new_value = 0;
                for (int filter_y = 0; filter_y < FILTER_SIZE; filter_y++) {
                    for (int filter_x = 0; filter_x < FILTER_SIZE; filter_x++) {
                        int image_x = x - FILTER_SIZE/2 + filter_x;
                        int image_y = y - FILTER_SIZE/2 + filter_y;
                        if (image_x >= 0 && image_x < 1000 && image_y >= 0 && image_y < 1000) {
                            new_value += image[image_y*1000 + image_x] * filter[filter_y][filter_x];
                        }
                    }
                }
                blurred_image[y*1000 + x] = new_value / 16; // normalize the value
        }
    }
    memcpy(image, blurred_image, 1000*1000*sizeof(uint16_t));
}

uint16_t* create_image_with_circle() {
    uint16_t* image = malloc(1000 * 1000 * sizeof(uint16_t));
    int center_x = 500;
    int center_y = 500;
    int radius = 25;

    for (int y = 0; y < 1000; y++) {
        for (int x = 0; x < 1000; x++) {
            if ((x-center_x)*(x-center_x) + (y-center_y)*(y-center_y) <= radius*radius) {
                // This pixel is inside the circle, set its color to white
                *image = (uint16_t) rte_rand_max(10000) + 50000;
            } else {
                // This pixel is outside the circle, set its color to a random value
                *image = (uint16_t) rte_rand_max(10000);
            }
            image++;
        }
    }

    image = image - 1000*1000;
	gaussian_blur(image);
	return image;
}

static inline int
port_init(struct rte_mempool *mbuf_pool)
{
	struct rte_eth_conf port_conf = port_conf_default;
	const uint16_t rx_rings = 1, tx_rings = 1;
	int retval;
	uint16_t q;


	retval = rte_eth_dev_configure(0, rx_rings, tx_rings, &port_conf);
	if (retval != 0)
		return retval;

	/* Allocate and set up 1 RX queue per Ethernet port. */
	for (q = 0; q < rx_rings; q++) {
		retval = rte_eth_rx_queue_setup(0, q, RX_RING_SIZE,
				rte_eth_dev_socket_id(0), NULL, mbuf_pool);
		if (retval < 0)
			return retval;
	}

	/* Allocate and set up 1 TX queue per Ethernet port. */
	for (q = 0; q < tx_rings; q++) {
		retval = rte_eth_tx_queue_setup(0, q, TX_RING_SIZE,
				rte_eth_dev_socket_id(0), NULL);
		if (retval < 0)
			return retval;
	}

	/* Start the Ethernet port. */
	retval = rte_eth_dev_start(0);
	if (retval < 0)
		return retval;

	

	struct rte_ether_addr addr;
	retval = rte_eth_macaddr_get(0, &addr);
	if (retval != 0)
		return retval;

	printf("Port %u MAC: %02" PRIx8 " %02" PRIx8 " %02" PRIx8
			   " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 "\n",
			0,
			addr.addr_bytes[0], addr.addr_bytes[1],
			addr.addr_bytes[2], addr.addr_bytes[3],
			addr.addr_bytes[4], addr.addr_bytes[5]);

	return 0;
}



/*  Parse the argument (num_pools) given in the command line of the application */
static int
vmdq_parse_args(int argc, char **argv)
{
	// Set default config values
	config_.interval = DEFAULT_INTERVAL;
	config_.starting_frame_number = DEFAULT_STARTING_FRAME_NUMBER;
	config_.number_of_frames = DEFAULT_NUMBER_OF_FRAMES;
	strncpy(config_.destination_ip_address, DEFAULT_DEST_IP_ADDR, sizeof(config_.destination_ip_address));
	strncpy(config_.destination_mac_address, DEFAULT_DEST_MAC_ADDR, sizeof(config_.destination_mac_address));
	config_.destination_port = DEFAULT_DEST_PORT;
	strncpy(config_.source_ip_address, DEFAULT_SOURCE_IP_ADDR, sizeof(config_.source_ip_address));
	strncpy(config_.source_mac_address, DEFAULT_SOURCE_MAC_ADDR, sizeof(config_.source_mac_address));
	config_.source_port = DEFAULT_SOURCE_PORT;
	config_.test_pattern_mode = DEFAULT_TEST_PATTERN_MODE;
	config_.drop_frames = DEFAULT_DROP_FRAMES;
	config_.drop_packets = DEFAULT_DROP_PACKETS;

	int opt;
	int option_index;
	unsigned i;

	const char *prgname = argv[0];
	static struct option long_option[] = {
		{"interval", required_argument, NULL, 0},
		{"start_frame", required_argument, NULL, 0},
		{"frames", required_argument, NULL, 0},
		{"dest_ip", required_argument, NULL, 0},
		{"dest_mac", required_argument, NULL, 0},
		{"src_ip", required_argument, NULL, 0},
		{"src_mac", required_argument, NULL, 0},
		{"drop_packet", required_argument, NULL, 0},
		{"drop_frame", required_argument, NULL, 0},
		{"src_port", required_argument, NULL, 0},
		{"dst_port", required_argument, NULL, 0},
		{"test_pattern", required_argument, NULL, 0},
		{"help", no_argument, NULL, 0},
		{NULL, 0, 0, 0}
	};

	while ((opt = getopt_long(argc, argv, "p:", long_option, &option_index)) != EOF) {
		switch (opt) {
		/* portmask */
		case 0:
			if (!strcmp(long_option[option_index].name, "help"))
			{
				printf("\n\n\n");
				printf("--interval : Time delay in seconds between frames \n");
				printf("--start_frame : Frame number to start sending from \n");
				printf("--frames : number of frames to send \n");
				printf("--dest_ip : Destination Ip address in the format xxx.xxx.xxx.xxx \n");
				printf("--dest_mac : Destination MAC address in the format XX:XX:XX:XX:XX:XX \n");
				printf("--dest_port : Destination port to use 0 - 65535 \n");
				printf("--src_ip : Source Ip address in the format xxx.xxx.xxx.xxx \n");
				printf("--src_mac : Source MAC address in the format XX:XX:XX:XX:XX:XX \n");
				printf("--src_port : Source port to use 0 - 65535 \n");
				printf("--drop_packet : A value between 0-100 of the chance to drop a packet \n");
				printf("--drop_frame : A value between 0-100 of the chance to drop packets in that frame \n");
				printf("--test_pattern : 1 - repeating test pattern, 0 - simulated beam \n");
				printf("--help : Display this message \n\n\n");
				rte_eal_cleanup();
				exit(0);
				
				
			}
			// Frame interval
			if (!strcmp(long_option[option_index].name, "interval"))
			{
				printf("Found interval argument: %s\n", optarg);
				config_.interval = floor(atof(optarg)*1000000);
			}
			// Starting frame number
			if (!strcmp(long_option[option_index].name, "start_frame"))
			{
				printf("Found start_frame argument: %s\n", optarg);
				config_.starting_frame_number = atoi(optarg);
			}
			// Number of frames
			if (!strcmp(long_option[option_index].name, "frames"))
			{
				printf("Found frames argument: %s\n", optarg);
				config_.number_of_frames = atoi(optarg);
			}
			// Destination IP Address
			if (!strcmp(long_option[option_index].name, "dest_ip"))
			{
				printf("Found dest_ip argument: %s\n", optarg);
				strncpy(config_.destination_ip_address, optarg, sizeof(config_.destination_ip_address));
			}
			// Destination Mac Address
			if (!strcmp(long_option[option_index].name, "dest_mac"))
			{
				printf("Found dest_mac argument: %s\n", optarg);
				strncpy(config_.destination_mac_address, optarg, sizeof(config_.destination_mac_address));
			}
			// Destination port
			if (!strcmp(long_option[option_index].name, "dest_port"))
			{
				printf("Found dest_port argument: %s\n", optarg);
				config_.destination_port = atoi(optarg);
			}
			// Source IP Address
			if (!strcmp(long_option[option_index].name, "src_ip"))
			{
				printf("Found src_ip argument: %s\n", optarg);
				strncpy(config_.source_ip_address, optarg, sizeof(config_.source_ip_address));
			}
			// Source Mac Address
			if (!strcmp(long_option[option_index].name, "src_mac"))
			{
				printf("Found src_mac argument: %s\n", optarg);
				strncpy(config_.source_mac_address, optarg, sizeof(config_.source_mac_address));
			}
			// Source port
			if (!strcmp(long_option[option_index].name, "src_port"))
			{
				printf("Found src_port argument: %s\n", optarg);
				config_.source_port = atoi(optarg);
			}
			// Test pattern mode
			if (!strcmp(long_option[option_index].name, "test_pattern"))
			{

				config_.test_pattern_mode = atoi(optarg);

				printf("Found test pattern argument: %ld\n", config_.test_pattern_mode);

				

			}
			// Drop packets
			if (!strcmp(long_option[option_index].name, "drop_packet"))
			{
				printf("Found destaddr argument: %s\n", optarg);
				config_.drop_packets = atoi(optarg);
			}
			// Drop whole frames
			if (!strcmp(long_option[option_index].name, "drop_frame"))
			{
				printf("Found destaddr argument: %s\n", optarg);
				config_.drop_frames = atoi(optarg);
			}
			break;

		default:
			return -1;
		}
	}


	return 0;
}

uint64_t
rdtsc(){
	return rte_get_tsc_cycles();
}

/* The main function, which does initialization and calls the per-lcore
 * functions.
 */
int
main(int argc, char *argv[])
{
	int i = 0, p = 0;
	struct rte_ether_hdr *eth_hdr;
	struct rte_udp_hdr *udp_hdr;
    struct rte_mempool *mbuf_pool;
	struct x10g_hdr *x10g_h;
	uint16_t *data_ptr;
	struct rte_ipv4_hdr *ip_hdr;
    uint16_t portid = 0;
	unsigned int lcores_count;
	const unsigned ring_size = 1024;
	const unsigned flags = 0;
	unsigned lcore_id = 0;
	unsigned int core_count;
	void *ptrMemZone;
	FILE* f;
	struct rte_ether_addr *dest_mac_address;
	struct rte_ether_addr *eth_addr;

	int nb_tx = 0;



/* 
		Initialize the Environment Abstraction Layer (EAL)
		This receives the Command line options before the -- separation on command line
	*/
	int ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Error with EAL initialization\n");

	argc -= ret;
	argv += ret;

	/*
		Print the passed params to the app, minus the parameters needed for eal init process
	*/
	for(i=0; i<argc;i++){
		printf("argc:%d, argv:%s\n", i, argv[i]);
	}

	/* parse app arguments */
	ret = vmdq_parse_args(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Invalid arguments\n");

	/* 
		Create a new mempool in memory to hold the packets, this is a wrapper around the underlying mempool function
		This is 
	*/
	mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL", 32767, MBUF_CACHE_SIZE, RTE_MBUF_PRIV_ALIGN, JUMBO_FRAME_ELEMENT_SIZE, rte_socket_id());
	/*
		check if it has been created
	*/
	if (mbuf_pool == NULL)
		rte_exit(EXIT_FAILURE, "Cannot create mbuf pool, check permissions\n");


	if (port_init(mbuf_pool) != 0)
		rte_exit(EXIT_FAILURE, "Cannot init port %"PRIu8 "\n", 0);

	printf("opening data file\n");


	struct rte_mbuf *pkt[PKT_BUFFERS][PKTS_PER_FRAME];


	int center_x = 500;
	int center_y = 500;
	int radius = 100;
	
	uint32_t buf;
	
	for (int b = 0; b < PKT_BUFFERS; b++)
	{
		uint8_t* image_data = (uint8_t*) create_image_with_circle();
		uint16_t packet_data = 0;
		for (i=0;i<PKTS_PER_FRAME;i++)
		{
			
			pkt[b][i] = rte_pktmbuf_alloc(mbuf_pool);
			pkt[b][i]->pkt_len = pkt_size;
			pkt[b][i]->data_len = pkt_size;
			
			eth_hdr = rte_pktmbuf_mtod(pkt[b][i],struct rte_ether_hdr*);
			
			ip_hdr = (struct rte_ipv4_hdr *) ((char *)eth_hdr + l2_len);

			udp_hdr = (struct rte_udp_hdr *) ((char *)eth_hdr + l2_len + l3_len); 
			
			x10g_h = (struct x10g_hdr *) ((char *)eth_hdr + l2_len + l3_len + len_4);
			
			data_ptr = (uint16_t *) ((char *) x10g_h + len_5);
			
			//Set the Start or End of frame markers
			if (i == 0 ){
				//start of frame here
				x10g_h->markers = 0x80;

			}else if (( i + 1 ) % PKTS_PER_FRAME == 0){
				//end of frame here
				x10g_h->markers = 0x40;
			}

			x10g_h->frame_num = rte_bswap64(config_.starting_frame_number + b);
			x10g_h->packet_num = rte_bswap32(i);
			rte_ether_unformat_addr(config_.source_mac_address, (void*) &eth_hdr->src_addr);

			rte_ether_unformat_addr(config_.destination_mac_address, (void*) &eth_hdr->dst_addr);

			inet_pton(AF_INET, config_.destination_ip_address, &buf);

			ip_hdr->dst_addr = buf;

			inet_pton(AF_INET, config_.source_ip_address, &buf);

			ip_hdr->src_addr = buf;


			eth_hdr->ether_type = 8;
			udp_hdr->dst_port = rte_bswap16(config_.destination_port);
			udp_hdr->src_port = rte_bswap16(config_.source_port);
			udp_hdr->dgram_len = rte_bswap16(8064);

			ip_hdr->fragment_offset = 0;
			ip_hdr->ihl = 5;
			ip_hdr->next_proto_id = 17;
			ip_hdr->packet_id = 29692;
			
	
			ip_hdr->time_to_live = 128;
			ip_hdr->total_length = 0;
			ip_hdr->type_of_service = 0;
			ip_hdr->version = 4;
			ip_hdr->version_ihl = 69;
			uint64_t counter = 0;
			uint64_t total_value = 0;

			//printf("frame mode: %ld\n", config_.test_pattern_mode);

			if ( config_.test_pattern_mode == 0)
			{
				//printf("PACKET incrementing\n");
				for(int index = 0; index < 4000; index++)
					{
						*data_ptr = packet_data;
						data_ptr++;
						packet_data++;
						if (packet_data == 65535)
							packet_data = 0;
					}
			}
			else if (config_.test_pattern_mode == 1)
			{
				//printf("PACKET fake detector\n");
				rte_memcpy(data_ptr, image_data + (i * 8000), 8000);
			}
			else if (config_.test_pattern_mode == 2)
			{
				//printf("PACKET 0's\n");
				for(int index = 0; index < 4000; index++)
				{
					*data_ptr = 0;
					data_ptr++;
					packet_data++;
					if (packet_data == 65535)
						packet_data = 0;
				}
			}

			data_ptr = (uint16_t *) ((char *) x10g_h + len_5);
			

			if(i == 0)
			{
				for(int index = 0; index < 1000; index ++)
				{
					*data_ptr = (uint16_t) config_.starting_frame_number + b;
				
					data_ptr++;
				}
			}else if (i == PKTS_PER_FRAME)
			{
				for(int index = 0; index < 1000; index ++)
				{
					*(data_ptr + 7000) = (uint16_t) config_.starting_frame_number + b;
				
					data_ptr++;
				}
			}
			
			
			if (DEBUG){
				printf("PACKET INFO: \n");
				printf("%X\n", ip_hdr->dst_addr);
				printf("%X\n", ip_hdr->fragment_offset);
				printf("%X\n", ip_hdr->hdr_checksum);
				printf("%X\n", ip_hdr->ihl);
				printf("%X\n", ip_hdr->next_proto_id);
				printf("%X\n", ip_hdr->packet_id);
				printf("%X\n", ip_hdr->src_addr);
				printf("%X\n", ip_hdr->time_to_live);
				printf("%X\n", ip_hdr->total_length);
				printf("%X\n", ip_hdr->type_of_service);
				printf("%X\n", ip_hdr->version);
				printf("%X\n", ip_hdr->version_ihl);
				printf("end of packet info\n\n");

				printf("PACKET INFO: \n");
				printf("%X\n", udp_hdr->dgram_cksum);
				printf("%X\n", udp_hdr->dgram_len);
				printf("%X\n", udp_hdr->dst_port);
				printf("%X\n", udp_hdr->src_port);
				printf("end of packet info\n\n");

				printf("PACKET INFO: \n");
				print_ethaddr("dst ", &eth_hdr->dst_addr);
				printf("\n%X\n", eth_hdr->ether_type);
				print_ethaddr("src ", &eth_hdr->src_addr);
				printf("\nend of packet info\n\n");
				printf("size of packet: %d", rte_pktmbuf_pkt_len(pkt[b][i]));
			}
			
			//printf("Packet %d crafted.\n", i);
		}
	}
	printf("Crafted all %d packets for TX!\n", i);

	int count = 0;


	uint64_t now;
	uint64_t last;

	getchar();

	last = rte_get_tsc_cycles();
	uint64_t ticks_per_sec = rte_get_tsc_hz();

	uint64_t total_packets_sent = 0; 
	uint64_t total_packets_dropped = 0;
	int which_buf = 0;

	while(true){
		nb_tx = 0;

		if (config_.drop_frames <= (rte_rand_max(100) + 1))
		{
			// Dont drop any packets
			while (nb_tx != PKTS_PER_FRAME)
			{
				
				nb_tx = nb_tx + rte_eth_tx_burst(0, 0, (pkt[which_buf] + nb_tx), (PKTS_PER_FRAME - nb_tx) );
			}
			total_packets_sent += nb_tx;
		}
		else
		{
			// Drop packets
			while (nb_tx != PKTS_PER_FRAME)
			{
				if (config_.drop_packets <= (rte_rand_max(100) + 1))
				{
					nb_tx = nb_tx + rte_eth_tx_burst(0, 0, (pkt[which_buf] + nb_tx), 1 );
					total_packets_sent++;
				}
				else
				{
					nb_tx++;
					total_packets_dropped++;
				}
			}
		}

		
		which_buf++;

		if (which_buf == PKT_BUFFERS)
		{
			which_buf = 0;
		}

		rte_delay_us(config_.interval);
		config_.starting_frame_number++;
		
		count++;
		if (count % 1000 == 0)
			printf("Frame %ld sent!\n", config_.starting_frame_number);
			if (count == config_.number_of_frames)
			{
				float time_taken = (float)(rte_get_tsc_cycles() - last) / ticks_per_sec;
				printf("Sent %ld packets in %f seconds! At data rate of %f Gb/s\n", total_packets_sent,time_taken, (float)( count / time_taken * 0.016212) );
				if (total_packets_dropped)
				{
					printf("Dropped %ld packets or %f percent \n", total_packets_dropped, (float) total_packets_dropped / (total_packets_sent+total_packets_dropped) * 100);
				}
				rte_eal_cleanup();
				exit(0);
			}

		for (i = 0; i < PKTS_PER_FRAME; i++)
		{
			eth_hdr = rte_pktmbuf_mtod(pkt[which_buf][i], struct rte_ether_hdr*);
			x10g_h = (struct x10g_hdr*)((char*)eth_hdr + l2_len + l3_len + len_4);
			x10g_h->frame_num = rte_bswap64(config_.starting_frame_number);
			data_ptr = (uint16_t*)((char*)x10g_h + len_5);

			if (i == 0) // First packet
			{
				for (int index = 0; index < 1000; index++)
				{
					*data_ptr = (uint16_t)config_.starting_frame_number;
					data_ptr++;
				}
			}
		}
	}
}