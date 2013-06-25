/**
 *  @file logger.c
 */

#include <stdio.h>
#include <limits.h>
#include <errno.h>
#include <string.h>
#include <inttypes.h>
#include <time.h>
#include <arpa/inet.h>
#include "adis.h"
#include "logger.h"
#include "utils_sockets.h"
#include "net_addrs.h"

#define P_LIMIT 1500

static FILE *fp = NULL;
static char filename[50];
static char log_buffer[1500]; 		// Global so destructor can flush final data
static int log_buffer_size = 0;
static int sd;

// sequence number, each UDP packet gets a number
uint32_t sequence;


void logger_init() {

	// Show we're opening a file
	sprintf(filename, "logfile-%d.log", (int)time(NULL));
	fp = fopen(filename, "w+");
	if(!fp){
		fprintf (stderr, "disk logger: could not open file %s for writing: %s\n", filename, strerror(errno));
	}
	setbuf(fp, NULL);

	// Outgoing socket (WiFi)
	sd = get_send_socket();

	// Initilize sequence number
    sequence = 0;

	// Add sequence number to the first packet
	memcpy(&log_buffer[log_buffer_size], &sequence, sizeof(uint32_t));
	log_buffer_size += sizeof(uint32_t);

	// Print some debug
//	printf("Filling packet: ");
}


void logger_final() {
	if(log_buffer_size > 0){
		fwrite(log_buffer, sizeof(char), log_buffer_size, fp);
	}
	fclose(fp);
}


static void flush_log()
{
	// Send current buffer to disk
//	printf("\nDumping packet to disk and wifi.\n\n");
	// for the log file, convert the sequence number to a SEQN message
	packet_header header = { .ID="SEQN", .timestamp={0,0,0,0,0,0}, .data_length=htons(4) };
	fwrite(&header, 1, sizeof(packet_header), fp);
	fwrite(log_buffer, sizeof(char), log_buffer_size, fp);

	// Send current buffer to WiFi
	sendto_socket(sd, log_buffer, log_buffer_size, WIFI_IP, WIFI_PORT);

	// Reset buffer size
	log_buffer_size = 0;

	// Increment sequence number
	sequence++;

	// Write sequence number to head of next packet
	uint32_t s  = htonl(sequence);
	memcpy(&log_buffer[log_buffer_size], &s, sizeof(uint32_t));
	log_buffer_size += sizeof(uint32_t);

//	printf("Filling packet: ");
}

static void logg(void *data, size_t len)
{
	// Check size of buffer, if big enough, we can send packet
	if (log_buffer_size + len >= P_LIMIT)
		flush_log();

	// Copy data into packet buffer
	memcpy(log_buffer + log_buffer_size, data, len);
	log_buffer_size += len;
//	printf("-");
}

static void log_message(char *msg)
{
	int len = strlen(msg);
	if (log_buffer_size + sizeof(packet_header) + len > P_LIMIT)
		flush_log();

	packet_header header = { .ID = "MESG", .timestamp = {0,0,0,0,0,0}, .data_length=htons(len) };
	logg(&header, sizeof(packet_header));
	logg(&msg, len);
}

void log_getPositionData_adis(ADIS_packet *data) {
	logg(data, sizeof(ADIS_packet));
}

void log_getPositionData_gps(GPS_packet* data){
	// different GPS packets have different lengths
	logg(data, sizeof(packet_header) + htons(data->data_length));
}

void log_getData_mpu(MPU_packet* data){
	logg(data, sizeof(MPU_packet));
}
void log_getData_mpl(MPL_packet* data){
	logg(data, sizeof(MPL_packet));
}

void log_getSignalData_arm(char* code){
	log_message(code);
}
void log_getPositionData_rc(RollServo_adjustment* data){
	logg(data, sizeof(RollServo_adjustment));
}
void log_getSignalData_rs(char* code){
	log_message(code);
}

