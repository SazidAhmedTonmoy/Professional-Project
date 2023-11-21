/*
*  Copyright (c) 2023, BAUD DATA COMMUNICATION Co., LTD
*  
*  Module : tftpd 
*  File   : tftpd.h
*  Author : Md Sazid Ahmed Tonmoy
*  Date   : November 20, 2023
*/

#ifndef _TFTPD_H
#define _TFTPD_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "libsys/vos/vos_task.h"
#include "libsys/vos/vos_msgq.h"
#include "ip/socket.h"
#include "ip/sockdefs.h"
#include "ip/errno.h"
#include "ip/msg.h"
#include "ip/inet.h"
#include "ip/in.h"
#include "ip/netdb.h"
#include "libfile/file_sys.h"
#include "time/time.h"
#include "libsys/misc.h"
#include "libsys/timer.h"
#include "libsys/verctl.h"

#include <libcmd/cmdparse.h>
#include <libcmd/argparse.h>
#include <libcmd/cmderror.h>
#include <ip/libnet.h>
#include <ip/netdb.h>
#include <ip/resolv.h>
#ifdef INCLUDE_REDN
#include <libredn/redn.h>
#include <libredn/redn_argu.h>
#endif


#define MSG_TYPE_MSG_Q 			1
#define MSG_TYPE_TIMER 			2
#define TFTP_ENABLE 			11
#define TFTP_DISABLE 			12

#define DEFAULT_TFTP_PORT 		69
#define DEFAULT_MAX_TIMEOUT 	5
#define DEFAULT_MAX_RETRIES 	3

#define TFTP_BLOCK_SIZE 		512
#define TFTP_OFFER_BLOCK_SIZE	4096
#define TFTP_BUFFER_SIZE 		10000
#define MAX_SESSIONS 			3
#define MAX_OPTION_LENGTH 		32
#define MAX_OPTION_VALUE_LENTH 	32
#define TFTP_MIN_BLOCK_SIZE 	8
#define TFTP_MAX_BLOCK_SIZE 	65535
#define MAX_FILE_BUFFER_SIZE 	131072
#define MAX_DELAY_INTERVAL		50

#define TFTP_MAJOR_VERSION  	1
#define TFTP_MINOR_VERSION  	2
#define TFTP_REVSION_VERSION  	15

#define TFTP_RRQ_OPCODE 		1
#define TFTP_WRQ_OPCODE 		2
#define TFTP_DATA_OPCODE 		3
#define TFTP_ACK_OPCODE 		4
#define TFTP_ERR_OPCODE 		5
#define TFTP_OACK_OPCODE 		6

#define NOT_DEFINED				0
#define FILE_NOT_FOUND			1
#define ACCESS_VIOLATION		2
#define ALLOCATION_EXCEEDED		3
#define ILLEGAL_TFTP_OPERATION	4
#define UNKNOWN_TID				5
#define FILE_ALREADY_EXISTS		6
#define NO_SUCH_USER			7
#define INVALID_OPTION			8


typedef struct {
	uint16 opcode;
	char filename[0];
	char mode[0];
} __attribute__((packed)) tftp_rrq;

typedef struct {
    uint16 opcode;
    char filename[0];
    char mode[0];
} __attribute__((packed)) tftp_wrq;

typedef struct {
	uint16 opcode;
	uint16 block_number;
	char data_t[0];
} __attribute__((packed)) tftp_data;

typedef struct {
	uint16 opcode;
	uint16 block_number;
} __attribute__((packed)) tftp_ack;

typedef struct {
	uint16 opcode;
	uint16 error_code;
	char error_message[0];
} __attribute__((packed)) tftp_err;

typedef struct {
	uint16 opcode;
	char option[0];
} __attribute__((packed)) tftp_oack;

typedef struct{
	unsigned long msg_type;
	unsigned long count;
	unsigned long reserved1;
	unsigned long reserved2;
} msg_t;

typedef struct {
	uint32 active;
	uint32 opcode;
	char filename[256];
	char mode[10];
	MSG_Q_ID new_msgq_id;
	unsigned long timer_id;
	struct soaddr_in clnt_address;
	uint32 clnt_address_len;
	uint32 blk_size;
	uint32 blk_flag;
} tftpd_sessions;

MSG_Q_ID msgq_id;
msg_t msg_buf;
int32 sock_id = -1;
tftpd_sessions sessions[MAX_SESSIONS];
struct version_list ver_list;
struct soaddr_in clnt_address;
uint32 clnt_address_len;
uint32 write_on = 0;
uint32 read_counter = 0;
uint32 tftp_enable_flag = 1;
uint32 TFTP_PORT = DEFAULT_TFTP_PORT;
uint32 MAX_TIMEOUT = DEFAULT_MAX_TIMEOUT;
uint32 MAX_RETRIES = DEFAULT_MAX_RETRIES;


void tftp_register_cmds(void);
static int cmd_conf_tftp_server(int argc, char **argv, struct user *u);
static int cmd_conf_tftp(int argc, char **argv, struct user *u);
int show_tftp(int argc, char **argv, struct user *u);
static int do_enable_tftp(int argc, char **argv, struct user *u);
static int set_tftp_port(int argc, char **argv, struct user *u);
static int set_tftp_retransmit(int argc, char **argv, struct user *u);
static int do_show_tftp_server(int argc, char **argv, struct user *u);

extern unsigned long Print(char *format, ...);
void tftpd_module_main_process(void);
void tftpd_rrq_function(int session_idx);
void tftpd_wrq_function(int session_idx);
void send_error_packet_to_client(int index, int new_sock_id, uint16 error_code, const char* error_message);
uint32 client_address_check(void);
void parse_options_from_request(char *req_buffer, const char* option_name, char* option_value);
uint32 read_file_in_block(int idx, int offset, char *buf);
void tftp_version_register(void);
INT32 tftp_show_running(DEVICE_ID diID);
void reverse(char str[], int length);
char* itoa(int num, char* str, int base);
int get_a_session(void);
void initialize_sessions(void);
void close_session(int idx);

#endif
