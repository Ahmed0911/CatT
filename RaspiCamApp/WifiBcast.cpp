// (c)2015 befinitiv
/*
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; version 2.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License along
 *   with this program; if not, write to the Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "WifiBcast.h"
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "fec.h"



/* this is the template radiotap header we send packets out with */

static const u8 u8aRadiotapHeader[] = {

	0x00, 0x00, // <-- radiotap version
	0x0c, 0x00, // <- radiotap header lengt
	0x04, 0x80, 0x00, 0x00, // <-- bitmap
	0x22, 
	0x0, 
	0x18, 0x00 
};

/* Penumbra IEEE80211 header */

//the last byte of the mac address is recycled as a port number
#define SRC_MAC_LASTBYTE 15
#define DST_MAC_LASTBYTE 21

static u8 u8aIeeeHeader[] = {
	0x08, 0x01, 0x00, 0x00,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0x13, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x13, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x10, 0x86,
};


void set_port_no(uint8_t *pu, uint8_t port) {
	//dirty hack: the last byte of the mac address is the port number. this makes it easy to filter out specific ports via wireshark
	pu[sizeof(u8aRadiotapHeader) + SRC_MAC_LASTBYTE] = port;
	pu[sizeof(u8aRadiotapHeader) + DST_MAC_LASTBYTE] = port;
}


typedef struct {
	int seq_nr;
	int fd;
	int curr_pb;
	packet_buffer_t *pbl;
} fifo_t;

	

int packet_header_init(uint8_t *packet_header) {
			u8 *pu8 = packet_header;
			memcpy(packet_header, u8aRadiotapHeader, sizeof(u8aRadiotapHeader));
			pu8 += sizeof(u8aRadiotapHeader);
			memcpy(pu8, u8aIeeeHeader, sizeof (u8aIeeeHeader));
			pu8 += sizeof (u8aIeeeHeader);
					
			//determine the length of the header
			return pu8 - packet_header;
}

void fifo_init(fifo_t *fifo, int fifo_count, int block_size) {
	int i;

	for(i=0; i<fifo_count; ++i) {
		int j;

		fifo[i].seq_nr = 0;
		fifo[i].fd = -1;
		fifo[i].curr_pb = 0;
		fifo[i].pbl = lib_alloc_packet_buffer_list(block_size, MAX_PACKET_LENGTH);

		//prepare the buffers with headers
		for(j=0; j<block_size; ++j) {
			fifo[i].pbl[j].len = 0;
		}
	}

}

void fifo_open(fifo_t *fifo, int fifo_count) {
	int i;
	if(fifo_count > 1) {
		//new FIFO style
		
		//first, create all required fifos
		for(i=0; i<fifo_count; ++i) {
			char fn[256];
			sprintf(fn, FIFO_NAME, i);
			
			unlink(fn);
			if(mkfifo(fn, 0666) != 0) {
				fprintf(stderr, "Error creating FIFO \"%s\"\n", fn);
				exit(1);
			}
		}
		
		//second: wait for the data sources to connect
		for(i=0; i<fifo_count; ++i) {
			char fn[256];
			sprintf(fn, FIFO_NAME, i);
			
			printf("Waiting for \"%s\" being opened from the data source... \n", fn);			
			if((fifo[i].fd = open(fn, O_RDONLY)) < 0) {
				fprintf(stderr, "Error opening FIFO \"%s\"\n", fn);
				exit(1);
			}
			printf("OK\n");
		}
	}
	else {
		//old style STDIN input
		fifo[0].fd = STDIN_FILENO;
	}
}


void fifo_create_select_set(fifo_t *fifo, int fifo_count, fd_set *fifo_set, int *max_fifo_fd) {
	int i;

	FD_ZERO(fifo_set);
	
	for(i=0; i<fifo_count; ++i) {
		FD_SET(fifo[i].fd, fifo_set);

		if(fifo[i].fd > *max_fifo_fd) {
			*max_fifo_fd = fifo[i].fd;
		}
	}
}


void pb_transmit_packet(pcap_t *ppcap, int seq_nr, uint8_t *packet_transmit_buffer, int packet_header_len, const uint8_t *packet_data, int packet_length) {
    //add header outside of FEC
    wifi_packet_header_t *wph = (wifi_packet_header_t*)(packet_transmit_buffer + packet_header_len);
    wph->sequence_number = seq_nr;

    //copy data
    memcpy(packet_transmit_buffer + packet_header_len + sizeof(wifi_packet_header_t), packet_data, packet_length);

    int plen = packet_length + packet_header_len + sizeof(wifi_packet_header_t);
    int r = pcap_inject(ppcap, packet_transmit_buffer, plen);
    if (r != plen) {
        pcap_perror(ppcap, "Trouble injecting packet");
        exit(1);
    }
}




void pb_transmit_block(packet_buffer_t *pbl, pcap_t *ppcap, int *seq_nr, int port, int packet_length, uint8_t *packet_transmit_buffer, int packet_header_len, int data_packets_per_block, int fec_packets_per_block, int transmission_count) {
	int i;
	uint8_t *data_blocks[MAX_DATA_OR_FEC_PACKETS_PER_BLOCK];
	uint8_t fec_pool[MAX_DATA_OR_FEC_PACKETS_PER_BLOCK][MAX_USER_PACKET_LENGTH];
	uint8_t *fec_blocks[MAX_DATA_OR_FEC_PACKETS_PER_BLOCK];


	for(i=0; i<data_packets_per_block; ++i) {
		data_blocks[i] = pbl[i].data;
	}


	if(fec_packets_per_block) {
		for(i=0; i<fec_packets_per_block; ++i) {
			fec_blocks[i] = fec_pool[i];
		}

		fec_encode(packet_length, data_blocks, data_packets_per_block, (unsigned char **)fec_blocks, fec_packets_per_block);
	}

	uint8_t *pb = packet_transmit_buffer;
	set_port_no(pb, port);
	pb += packet_header_len;


	int x;
	for(x=0; x<transmission_count; ++x) {
		//send data and FEC packets interleaved
		int di = 0; 
		int fi = 0;
		int seq_nr_tmp = *seq_nr;
		while(di < data_packets_per_block || fi < fec_packets_per_block) {
			if(di < data_packets_per_block) {
				pb_transmit_packet(ppcap, seq_nr_tmp, packet_transmit_buffer, packet_header_len, data_blocks[di], packet_length);
				seq_nr_tmp++;
				di++;
			}

			if(fi < fec_packets_per_block) {
				pb_transmit_packet(ppcap, seq_nr_tmp, packet_transmit_buffer, packet_header_len, fec_blocks[fi], packet_length);
				seq_nr_tmp++;
				fi++;
			}	
		}
	}

	*seq_nr += data_packets_per_block + fec_packets_per_block;



	//reset the length back
	for(i=0; i< data_packets_per_block; ++i) {
			pbl[i].len = 0;
	}

}


WifiBcast::WifiBcast(std::string lanInterface)
{
char szErrbuf[PCAP_ERRBUF_SIZE];
	int i;
	char fBrokenSocket = 0;
	int pcnt = 0;
	time_t start_time;
    uint8_t packet_transmit_buffer[MAX_PACKET_LENGTH];
	size_t packet_header_length = 0;
	fd_set fifo_set;
	int max_fifo_fd = -1;
	fifo_t fifo[MAX_FIFOS];

	int param_transmission_count = 1;
    int param_data_packets_per_block = 8;
    int param_fec_packets_per_block = 4;
	int param_packet_length = MAX_USER_PACKET_LENGTH;
	int param_port = 0;
	int param_min_packet_length = 0;
	int param_fifo_count = 1;

    packet_header_length = packet_header_init(packet_transmit_buffer);
	fifo_init(fifo, param_fifo_count, param_data_packets_per_block);
	fifo_open(fifo, param_fifo_count);
	fifo_create_select_set(fifo, param_fifo_count, &fifo_set, &max_fifo_fd);


	//initialize forward error correction
	fec_init();


	// open the interface in pcap
	szErrbuf[0] = '\0';
	_ppcap = pcap_open_live(lanInterface.c_str(), 800, 1, 20, szErrbuf);
	if (_ppcap == NULL) 
	{
		fprintf(stderr, "Unable to open interface %s in pcap: %s\n", lanInterface.c_str(), szErrbuf);
		return;
	}


	pcap_setnonblock(_ppcap, 0, szErrbuf);


	start_time = time(NULL);
	while (!fBrokenSocket) 
	{
		fd_set rdfs;
		int ret;

		rdfs = fifo_set;

		//wait for new data on the fifos
		ret = select(max_fifo_fd + 1, &rdfs, NULL, NULL, NULL);

		if(ret < 0) 
		{
			perror("select");
			return;
		}

		//cycle through all fifos and look for new data
		for(i=0; i<param_fifo_count && ret; ++i) 
		{
			if(!FD_ISSET(fifo[i].fd, &rdfs)) 
			{
				continue;
			}

			ret--;

			packet_buffer_t *pb = fifo[i].pbl + fifo[i].curr_pb;
			
			//if the buffer is fresh we add a payload header
			if(pb->len == 0) 
			{
				pb->len += sizeof(payload_header_t); //make space for a length field (will be filled later)
			}

			//read the data
			int inl = read(fifo[i].fd, pb->data + pb->len, param_packet_length - pb->len);
			if(inl < 0 || inl > param_packet_length-pb->len)
			{
				perror("reading stdin");
				return;
			}

			if(inl == 0) 
			{
				//EOF
				fprintf(stderr, "Warning: Lost connection to fifo %d. Please make sure that a data source is connected\n", i);
				usleep(1e5);
				continue;
			}

			pb->len += inl;
			
			//check if this packet is finished
			if(pb->len >= param_min_packet_length) 
			{
				payload_header_t *ph = (payload_header_t*)pb->data;
				ph->data_length = pb->len - sizeof(payload_header_t); //write the length into the packet. this is needed since with fec we cannot use the wifi packet lentgh anymore. We could also set the user payload to a fixed size but this would introduce additional latency since tx would need to wait until that amount of data has been received
				pcnt++;

				//check if this block is finished
				if(fifo[i].curr_pb == param_data_packets_per_block-1) 
				{
					pb_transmit_block(fifo[i].pbl, _ppcap, &(fifo[i].seq_nr), i+param_port, param_packet_length, packet_transmit_buffer, packet_header_length, param_data_packets_per_block, param_fec_packets_per_block, param_transmission_count);
					fifo[i].curr_pb = 0;
				}
				else 
				{
					fifo[i].curr_pb++;
				}
			}
		}


		if(pcnt % 128 == 0) 
		{
			printf("%d data packets sent (interface rate: %.3f)\n", pcnt, 1.0 * pcnt / param_data_packets_per_block * (param_data_packets_per_block + param_fec_packets_per_block) / (time(NULL) - start_time));
		}

	}


	printf("Broken socket\n");
}
	
WifiBcast::~WifiBcast()
{

}

void WifiBcast::SendData(uint8_t* buffer, uint32_t length)
{

}


// LIB Stuff
void lib_init_packet_buffer(packet_buffer_t *p) {
	assert(p != NULL);

	p->valid = 0;
	p->crc_correct = 0;
	p->len = 0;
	p->data = NULL;
}

void lib_alloc_packet_buffer(packet_buffer_t *p, size_t len) {
	assert(p != NULL);
	assert(len > 0);

	p->len = 0;
	p->data = (uint8_t*)malloc(len);
}

void lib_free_packet_buffer(packet_buffer_t *p) {
	assert(p != NULL);

	free(p->data);
	p->len = 0;
}

packet_buffer_t *lib_alloc_packet_buffer_list(size_t num_packets, size_t packet_length) {
	packet_buffer_t *retval;
	int i;

	assert(num_packets > 0 && packet_length > 0);

	retval = (packet_buffer_t *)malloc(sizeof(packet_buffer_t) * num_packets);
	assert(retval != NULL);

	for(i=0; i<num_packets; ++i) {
		lib_init_packet_buffer(retval + i);
		lib_alloc_packet_buffer(retval + i, packet_length);
	}

	return retval;
}

void lib_free_packet_buffer_list(packet_buffer_t *p, size_t num_packets) {
	int i;

	assert(p != NULL && num_packets > 0);

	for(i=0; i<num_packets; ++i) {
		lib_free_packet_buffer(p+i);
	}

	free(p);
}



