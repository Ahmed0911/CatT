#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <resolv.h>
#include <string.h>
#include <utime.h>
#include <unistd.h>
#include <getopt.h>
#include <pcap.h>
#include <endian.h>
#include <fcntl.h>
#include <time.h>
#include <sys/mman.h>
#include <string>


#define MAX_PACKET_LENGTH 4192
#define MAX_USER_PACKET_LENGTH 1450
#define MAX_DATA_OR_FEC_PACKETS_PER_BLOCK 32

#define FIFO_NAME "/tmp/fifo%d"
#define MAX_FIFOS 8


typedef unsigned int u32;
typedef unsigned short u16;
typedef unsigned char u8;
typedef u32 __le32;

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define	le16_to_cpu(x) (x)
#define	le32_to_cpu(x) (x)
#else
#define	le16_to_cpu(x) ((((x)&0xff)<<8)|(((x)&0xff00)>>8))
#define	le32_to_cpu(x) \
((((x)&0xff)<<24)|(((x)&0xff00)<<8)|(((x)&0xff0000)>>8)|(((x)&0xff000000)>>24))
#endif
#define	unlikely(x) (x)

#define	MAX_PENUMBRA_INTERFACES 8

typedef struct 
{
	uint32_t received_packet_cnt;
	uint32_t wrong_crc_cnt;
	int8_t current_signal_dbm;
} wifi_adapter_rx_status_t;

typedef struct 
{
	time_t last_update;
	uint32_t received_block_cnt;
	uint32_t damaged_block_cnt;
	uint32_t tx_restart_cnt;

	uint32_t wifi_adapter_cnt;
	wifi_adapter_rx_status_t adapter[MAX_PENUMBRA_INTERFACES];
} wifibroadcast_rx_status_t;

typedef struct 
{
	int valid;
	int crc_correct;
	uint32_t len; //this is the actual length of the packet stored in data
	uint8_t *data;
} packet_buffer_t;

typedef struct 
{
	int seq_nr;
	int curr_pb;
	packet_buffer_t *pbl;
} fifo_t;

//this sits at the payload of the wifi packet (outside of FEC)
typedef struct 
{
    uint32_t sequence_number;
} __attribute__((packed)) wifi_packet_header_t;

//this sits at the data payload (which is usually right after the wifi_packet_header_t) (inside of FEC)
typedef struct 
{
    uint32_t data_length;
} __attribute__((packed)) payload_header_t;

packet_buffer_t *lib_alloc_packet_buffer_list(size_t num_packets, size_t packet_length);



class WifiBcast
{
public:
	WifiBcast(std::string lanInterface);
	virtual ~WifiBcast();

	void SendData(uint8_t* buffer, uint32_t length);

private:
	pcap_t* _ppcap = NULL;

	uint32_t _paramTransmissionCount = 1;
    uint32_t _paramDataPacketsPerBlock = 8;
    uint32_t _paramFecPacketsPerBlock = 4;
	uint32_t _paramPacketLength = MAX_USER_PACKET_LENGTH;
	uint32_t _packetHeaderLength = 0;

    uint8_t _packetTransmitBuffer[MAX_PACKET_LENGTH];
	fifo_t _fifo[MAX_FIFOS];

	// statistics
	uint64_t _statSentPackets = 0;
};


