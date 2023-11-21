/*
*  Copyright (c) 2023, BAUD DATA COMMUNICATION Co., LTD
*  
*  Module : tftpd 
*  File   : tftpd.c
*  Author : Md Sazid Ahmed Tonmoy
*  Date   : November 20, 2023
*/

#include "tftpd.h"
#include "tftpd_task.c"
#include "tftpd_cmd.c"

void tftpd_init(void)
{
	TASK_ID task_id;
    struct soaddr_in serv_addr;
	uint32 bind_id, register_id;

	tftp_register_cmds();
	interface_set_showrunning_service(MODULE_TYPE_TFTPD, tftp_show_running);
	tftp_version_register();
	
	initialize_sessions();
	
    msgq_id = sys_msgq_create(256, Q_OP_FIFO);
    if (msgq_id == NULL)
	{
		syslog (LOG_ERR, "Failed to create message queue\n");
        return;
    }

    sock_id = so_socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_id < 0)
	{
        syslog (LOG_ERR, "Failed to create socket\n");
        return;
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(TFTP_PORT);

	bind_id = so_bind(sock_id, (struct soaddr *)&serv_addr, sizeof(serv_addr));
    if (bind_id < 0)
	{
        syslog (LOG_ERR, "Failed to bind socket\n");
        return;
    }

	register_id = socket_register(sock_id, (ULONG)msgq_id, 0);
    if (register_id != 0)
	{
        syslog (LOG_ERR, "Failed to register socket\n");
        return;
    }
	
    task_id = sys_task_spawn("TMMP", 128, 0, 131072, (FUNCPTR)tftpd_module_main_process, NULL, 0);
    if (task_id == (TASK_ID) SYS_ERROR)
       	syslog(LOG_WARNING, "Failed to spawn main process task\n");

    return;
}

void tftpd_rrq_function(int session_idx)
{
	TIMER_USER_DATA timer_ud;
	msg_t new_msg_buf;
	struct soaddr_in new_serv_addr;
	struct soaddr_in new_clnt_address;
	int32 new_clnt_address_len;
	int32 new_sock_id = -1;
	int32 new_bind_id, new_register_id;
	int32 rv, rv_sock;
	int32 file_read = 0;
	int32 read = 0;
	char read_buffer[TFTP_BUFFER_SIZE];
	char ack_buffer[TFTP_BUFFER_SIZE];
	char oack_buffer[TFTP_BUFFER_SIZE];
	char data_buffer[TFTP_BUFFER_SIZE];
	char prev_buffer[TFTP_BUFFER_SIZE];
	char file_buffer[MAX_FILE_BUFFER_SIZE];
	char err_buffer[128];
	char option_name[8];
	char blksize[6];
	tftp_data *data;
	tftp_ack *ack;
	tftp_oack *oack;
	tftp_err *err;
	uint32 count;
	uint32 time_interval;
	uint32 read_buffer_size = 0;
	uint32 error_occur = 0;
	uint32 retransmission = 0;
	uint32 file_offset = 0;
	uint32 oack_flag = 0;
	uint32 oack_timer_flag = 0;

	data = (tftp_data *)data_buffer;
	ack = (tftp_ack *)ack_buffer;
	oack = (tftp_oack *)oack_buffer;
	err = (tftp_err *)err_buffer;

	time_interval = tickGet();
	
	if(sessions[session_idx].blk_flag == 1)
		count = 1;
	else
		count = 0;
	
	sessions[session_idx].new_msgq_id = sys_msgq_create(256, Q_OP_FIFO);
	if (sessions[session_idx].new_msgq_id == NULL) 
	{
		syslog (LOG_ERR, "Failed to create message queue for read request\n");
		close_session(session_idx);
		return;
	}

	new_sock_id = so_socket(AF_INET, SOCK_DGRAM, 0);
    if (new_sock_id < 0)
	{
        syslog (LOG_ERR, "Failed to create socket for read request\n");
		close_session(session_idx);
		sys_msgq_delete(sessions[session_idx].new_msgq_id);
        return;
    }

    memset(&new_serv_addr, 0, sizeof(new_serv_addr));
    new_serv_addr.sin_family = AF_INET;
    new_serv_addr.sin_port = 0;

	new_bind_id = so_bind(new_sock_id, (struct soaddr *)&new_serv_addr, sizeof(new_serv_addr));
    if (new_bind_id < 0)
	{
        syslog (LOG_ERR, "Failed to bind socket for read request\n");
		close_session(session_idx);
		so_close(new_sock_id);
		sys_msgq_delete(sessions[session_idx].new_msgq_id);
        return;
    }

	new_register_id = socket_register(new_sock_id, (ULONG)sessions[session_idx].new_msgq_id, 0);
    if (new_register_id != 0)
	{
        syslog (LOG_ERR, "Failed to register socket for read request\n");
		close_session(session_idx);
		so_close(new_sock_id);
		sys_msgq_delete(sessions[session_idx].new_msgq_id);
        return;
    }

	timer_ud.msg.qid = sessions[session_idx].new_msgq_id;
	timer_ud.msg.msg_buf[0] = MSG_TYPE_TIMER;
	sys_add_timer(TIMER_LOOP | TIMER_MSG_METHOD, &timer_ud, &sessions[session_idx].timer_id);
	
	if(!IsFileExist(sessions[session_idx].filename))
	{
		send_error_packet_to_client(session_idx, new_sock_id, FILE_NOT_FOUND, "File not found");
		close_session(session_idx);
		so_close(new_sock_id);
		sys_delete_timer(sessions[session_idx].timer_id);
		sys_msgq_delete(sessions[session_idx].new_msgq_id);
		return;
	}
	
	if(sessions[session_idx].blk_flag == 0)
	{
		oack->opcode = htons(TFTP_OACK_OPCODE);
		strcpy(option_name, "blksize");
		itoa(sessions[session_idx].blk_size, blksize, 10);
		memcpy(oack->option, option_name, strlen(option_name)+1);
		memcpy(oack->option+strlen(option_name)+1, blksize, strlen(blksize)+1);
		rv_sock = so_sendto(new_sock_id, oack_buffer, 2+strlen(option_name)+1+strlen(blksize)+1, 0, (struct soaddr *)&sessions[session_idx].clnt_address, sessions[session_idx].clnt_address_len);
		sys_start_timer(sessions[session_idx].timer_id, TIMER_RESOLUTION_S | MAX_TIMEOUT);
		if (rv_sock < 0)
		{
			send_error_packet_to_client(session_idx, new_sock_id, INVALID_OPTION, "An unrequested option");
			close_session(session_idx);
			so_close(new_sock_id);
			sys_stop_timer(sessions[session_idx].timer_id);
			sys_delete_timer(sessions[session_idx].timer_id);
			sys_msgq_delete(sessions[session_idx].new_msgq_id);
			return;
		}
		oack_flag = 1;
		oack_timer_flag = 1;
	}
	else
	{	
		memset(file_buffer, 0, sizeof(file_buffer));
		file_read = read_file_in_block(session_idx, file_offset, file_buffer);
		if(file_read < 0)
		{	
			send_error_packet_to_client(session_idx, new_sock_id, NOT_DEFINED, "A error in file read");
			close_session(session_idx);
			so_close(new_sock_id);
			sys_stop_timer(sessions[session_idx].timer_id);
			sys_delete_timer(sessions[session_idx].timer_id);
			sys_msgq_delete(sessions[session_idx].new_msgq_id);
			return;
		}
		file_offset++;
		if(file_read < sessions[session_idx].blk_size)
			read_buffer_size = file_read;
		else
			read_buffer_size = sessions[session_idx].blk_size;

		memcpy(read_buffer, &file_buffer+read, read_buffer_size);
		read = read + read_buffer_size;
		
		data->opcode = htons(TFTP_DATA_OPCODE);
		data->block_number = htons(count);
		memcpy(data->data_t, &read_buffer, read_buffer_size);
		rv_sock = so_sendto(new_sock_id, data_buffer, read_buffer_size + 4, 0, (struct soaddr *)&sessions[session_idx].clnt_address, sessions[session_idx].clnt_address_len);
		sys_start_timer(sessions[session_idx].timer_id, TIMER_RESOLUTION_S | MAX_TIMEOUT);
		if(rv_sock < 0)
		{
			send_error_packet_to_client(session_idx, new_sock_id, NOT_DEFINED, "A error in sending data");
			close_session(session_idx);
			so_close(new_sock_id);
			sys_stop_timer(sessions[session_idx].timer_id);
			sys_delete_timer(sessions[session_idx].timer_id);
			sys_msgq_delete(sessions[session_idx].new_msgq_id);
			return;
		}

		if(rv_sock < sessions[session_idx].blk_size + 4)
		{
			close_session(session_idx);
			so_close(new_sock_id);
			sys_stop_timer(sessions[session_idx].timer_id);
			sys_delete_timer(sessions[session_idx].timer_id);
			sys_msgq_delete(sessions[session_idx].new_msgq_id);
			return;
		}
	}

	while(1)
	{
		if(error_occur == 1)
		{
			memcpy(data->data_t, &prev_buffer, read_buffer_size);
			rv_sock = so_sendto(new_sock_id, data_buffer, read_buffer_size + 4, 0, (struct soaddr *)&sessions[session_idx].clnt_address, sessions[session_idx].clnt_address_len);
			sys_start_timer(sessions[session_idx].timer_id, TIMER_RESOLUTION_S | MAX_TIMEOUT);
			error_occur = 0;
		}
		
		rv = sys_msgq_receive(sessions[session_idx].new_msgq_id, (char *)&new_msg_buf, SYS_WAIT_FOREVER);
		if (rv != SYS_OK)
			continue;
		else
		{
			switch (new_msg_buf.msg_type)
			{
				case SOCKET_DATARCVD:
					rv_sock = so_recvfrom(new_sock_id, &ack_buffer, 4, 0, (struct soaddr *)&new_clnt_address, &new_clnt_address_len);
					
					if(oack_timer_flag == 1)
					{
						if(ntohs(ack->block_number) == 0)
						{
							oack_timer_flag = 0;
							sys_stop_timer(sessions[session_idx].timer_id);
						}
						else
							continue;
					}
					sys_stop_timer(sessions[session_idx].timer_id);

					if(ntohs(new_clnt_address.sin_port) !=  sessions[session_idx].clnt_address.sin_port)
					{
						memset(&err_buffer, 0, sizeof(err_buffer));
						err->opcode = htons(TFTP_ERR_OPCODE);
						err->error_code = htons(UNKNOWN_TID);
						strcpy(err->error_message, "Unknown transfer ID");

						rv_sock = so_sendto(new_sock_id, err_buffer, sizeof(err_buffer), 0, (struct soaddr *)&new_clnt_address, new_clnt_address_len);
						continue;
					}
					
					if(ntohs(ack->opcode) != TFTP_ACK_OPCODE)
					{
						send_error_packet_to_client(session_idx, new_sock_id, ILLEGAL_TFTP_OPERATION, "Illegal TFTP operation");
						close_session(session_idx);
						so_close(new_sock_id);
						sys_stop_timer(sessions[session_idx].timer_id);
						sys_delete_timer(sessions[session_idx].timer_id);
						sys_msgq_delete(sessions[session_idx].new_msgq_id);
						return;
					}
					else if(ntohs(ack->block_number) != count)
					{
						memcpy(prev_buffer, &read_buffer, read_buffer_size);
						error_occur  = 1;
						continue;
					}
					else
					{
						count++;
						retransmission = 0;
						if((oack_flag == 1) || (read >= MAX_FILE_BUFFER_SIZE))
						{
							memset(file_buffer, 0, sizeof(file_buffer));
							file_read = read_file_in_block(session_idx, file_offset, file_buffer);
							if(file_read < 0)
							{
								send_error_packet_to_client(session_idx, new_sock_id, NOT_DEFINED, "A error in file read");
								close_session(session_idx);
								so_close(new_sock_id);
								sys_stop_timer(sessions[session_idx].timer_id);
								sys_delete_timer(sessions[session_idx].timer_id);
								sys_msgq_delete(sessions[session_idx].new_msgq_id);
								return;
							}
							oack_flag = 0;
							file_offset++;
							read = 0;
						}


						if(sessions[session_idx].blk_flag == 0 && count == 0)
						{
							if(file_read < sessions[session_idx].blk_size)
									read_buffer_size = file_read;
							else
								read_buffer_size = sessions[session_idx].blk_size;
						}
						else
						{	
							if(file_read - read < sessions[session_idx].blk_size)
								read_buffer_size = file_read - read;
							else
								read_buffer_size = sessions[session_idx].blk_size;
						}
						
						
						memcpy(read_buffer, file_buffer+read, read_buffer_size);
						read = read + read_buffer_size;
						
						data->opcode = htons(TFTP_DATA_OPCODE);
						data->block_number = htons(count);
						memcpy(data->data_t, read_buffer, sessions[session_idx].blk_size);
						rv_sock = so_sendto(new_sock_id, data_buffer, read_buffer_size + 4, 0, (struct soaddr *)&sessions[session_idx].clnt_address, sessions[session_idx].clnt_address_len);
						sys_start_timer(sessions[session_idx].timer_id, TIMER_RESOLUTION_S | MAX_TIMEOUT);
						if(rv_sock <0)
						{
							send_error_packet_to_client(session_idx, new_sock_id, NOT_DEFINED, "A error in sending data");
							close_session(session_idx);
							so_close(new_sock_id);
							sys_stop_timer(sessions[session_idx].timer_id);
							sys_delete_timer(sessions[session_idx].timer_id);
							sys_msgq_delete(sessions[session_idx].new_msgq_id);
							return;
						}

						if(rv_sock < sessions[session_idx].blk_size + 4)
						{
							close_session(session_idx);
							so_close(new_sock_id);
							sys_stop_timer(sessions[session_idx].timer_id);
							sys_delete_timer(sessions[session_idx].timer_id);
							sys_msgq_delete(sessions[session_idx].new_msgq_id);
							return;
						}
						
					}
					
					break;
					
				case MSG_TYPE_TIMER:
					if(oack_timer_flag == 1)
					{
						if(retransmission < MAX_RETRIES)
						{
							rv_sock = so_sendto(new_sock_id, oack_buffer, 2+strlen(option_name)+1+strlen(blksize)+1, 0, (struct soaddr *)&sessions[session_idx].clnt_address, sessions[session_idx].clnt_address_len);
							retransmission++;
						}
						else
						{
							send_error_packet_to_client(session_idx, new_sock_id, NOT_DEFINED, "Not defined, Max retransmission reached");
							close_session(session_idx);
							so_close(new_sock_id);
							sys_stop_timer(sessions[session_idx].timer_id);
							sys_delete_timer(sessions[session_idx].timer_id);
							sys_msgq_delete(sessions[session_idx].new_msgq_id);
							return ;
						}
					}
					else
					{
						if(retransmission < MAX_RETRIES)
						{
							rv_sock = so_sendto(new_sock_id, data_buffer, read + 4, 0, (struct soaddr *)&sessions[session_idx].clnt_address, sessions[session_idx].clnt_address_len);
							retransmission++;
						}
						else
						{
							send_error_packet_to_client(session_idx, new_sock_id, NOT_DEFINED, "Not defined, Max retransmission reached");
							close_session(session_idx);
							so_close(new_sock_id);
							sys_stop_timer(sessions[session_idx].timer_id);
							sys_delete_timer(sessions[session_idx].timer_id);
							sys_msgq_delete(sessions[session_idx].new_msgq_id);
							return ;
						}
					}
					break;
					
				default:
					break;
				}					
		}

		if(tickGet() - time_interval >= MAX_DELAY_INTERVAL)
		{
			sys_task_delay(1);
			time_interval = tickGet();
		}
	}
	
	close_session(session_idx);
	so_close(new_sock_id);
	sys_stop_timer(sessions[session_idx].timer_id);
	sys_delete_timer(sessions[session_idx].timer_id);
	sys_msgq_delete(sessions[session_idx].new_msgq_id);
	return;
}

void tftpd_wrq_function(int session_idx)
{
	TIMER_USER_DATA timer_ud;
	msg_t new_msg_buf;
	FCB_POINT *file = NULL;
	struct soaddr_in new_serv_addr;
	struct soaddr_in new_clnt_address;
	int32 new_clnt_address_len;
	int32 new_sock_id = -1;
	int32 new_bind_id, new_register_id;	
	int32 rv, rv_sock, rv_file;
	int32 write = 0;
	tftp_data *data;
	tftp_ack *ack;
	tftp_oack *oack;
	tftp_err *err;
	char ack_buffer[TFTP_BUFFER_SIZE];
	char data_buffer[TFTP_BUFFER_SIZE];
	char oack_buffer[TFTP_BUFFER_SIZE];
	char err_buffer[128];
	char option_name[8];
	char blksize[6];
	uint32 time_interval;
	uint32 count = 0; 
	uint32 prev_block = 0;
	uint32 retransmission = 0;
	uint32 oack_timer_flag = 0;

	ack = (tftp_ack *)ack_buffer;
	data = (tftp_data *)data_buffer;
	oack = (tftp_oack *)oack_buffer;
	err = (tftp_err *)err_buffer;

	time_interval = tickGet();

	sessions[session_idx].new_msgq_id = sys_msgq_create(256, Q_OP_FIFO);
	if (sessions[session_idx].new_msgq_id == NULL) 
	{
		syslog (LOG_ERR, "Failed to message queue for write request\n");
		close_session(session_idx);
		return;
	}

	new_sock_id = so_socket(AF_INET, SOCK_DGRAM, 0);
    if (new_sock_id < 0)
	{
        syslog (LOG_ERR, "Failed to create socket for write request\n");
		close_session(session_idx);
		sys_msgq_delete(sessions[session_idx].new_msgq_id);
        return;
    }

    memset(&new_serv_addr, 0, sizeof(new_serv_addr));
    new_serv_addr.sin_family = AF_INET;
    new_serv_addr.sin_port = 0;

	new_bind_id = so_bind(new_sock_id, (struct soaddr *)&new_serv_addr, sizeof(new_serv_addr));
    if (new_bind_id < 0)
	{
        syslog (LOG_ERR, "Failed to bind socket for write request\n");
		close_session(session_idx);
		so_close(new_sock_id);
		sys_msgq_delete(sessions[session_idx].new_msgq_id);
        return;
    }

	new_register_id = socket_register(new_sock_id, (ULONG)sessions[session_idx].new_msgq_id, 0);
    if (new_register_id != 0)
	{
        syslog (LOG_ERR, "Failed to register socket for write request\n");
		close_session(session_idx);
		so_close(new_sock_id);
		sys_msgq_delete(sessions[session_idx].new_msgq_id);
        return;
    }

	timer_ud.msg.qid = sessions[session_idx].new_msgq_id;
	timer_ud.msg.msg_buf[0] = MSG_TYPE_TIMER;
	sys_add_timer(TIMER_LOOP | TIMER_MSG_METHOD, &timer_ud, &sessions[session_idx].timer_id);
	
	rv_file = enter_filesys(OPEN_WRITE);
    if (rv_file != 0) 
	{
		send_error_packet_to_client(session_idx, new_sock_id, NOT_DEFINED, "A error in opening file system");
		close_session(session_idx);
		so_close(new_sock_id);
		sys_delete_timer(sessions[session_idx].timer_id);
		sys_msgq_delete(sessions[session_idx].new_msgq_id);
        return;
    }

	if(IsFileExist(sessions[session_idx].filename))
	{
		send_error_packet_to_client(session_idx, new_sock_id, FILE_ALREADY_EXISTS, "File Already exists");
		close_session(session_idx);
		exit_filesys(OPEN_WRITE);
		so_close(new_sock_id);
		sys_delete_timer(sessions[session_idx].timer_id);
		sys_msgq_delete(sessions[session_idx].new_msgq_id);
        return;
	}
	
	file = file_open(sessions[session_idx].filename, "w", NULL);
    if (file == NULL) 
	{
		send_error_packet_to_client(session_idx, new_sock_id, NOT_DEFINED, "A error in file open");
		close_session(session_idx);
		exit_filesys(OPEN_WRITE);
		so_close(new_sock_id);
		sys_delete_timer(sessions[session_idx].timer_id);
		sys_msgq_delete(sessions[session_idx].new_msgq_id);
        return;
    }
	if(sessions[session_idx].blk_flag == 0)
	{
		oack->opcode = htons(TFTP_OACK_OPCODE);
		strcpy(option_name, "blksize");
		itoa(sessions[session_idx].blk_size, blksize, 10);
		memcpy(oack->option, option_name, strlen(option_name)+1);
		memcpy(oack->option+strlen(option_name)+1, blksize, strlen(blksize)+1);
		rv_sock = so_sendto(new_sock_id, oack_buffer, 2+strlen(option_name)+1+strlen(blksize)+1, 0, (struct soaddr *)&sessions[session_idx].clnt_address, sessions[session_idx].clnt_address_len);
		sys_start_timer(sessions[session_idx].timer_id, TIMER_RESOLUTION_S | MAX_TIMEOUT);
		if (rv_sock < 0)
		{
			send_error_packet_to_client(session_idx, new_sock_id, INVALID_OPTION, "An unrequested option");
			close_session(session_idx);
			file_close(file);
			exit_filesys(OPEN_WRITE);
			so_close(new_sock_id);
			sys_delete_timer(sessions[session_idx].timer_id);
			sys_msgq_delete(sessions[session_idx].new_msgq_id);
			return;
		}
		else
		{
			prev_block = count;
			count++;
			oack_timer_flag = 1;
		}
	}
	else if(sessions[session_idx].blk_flag == 1)
	{
		ack->opcode = htons(TFTP_ACK_OPCODE);
		ack->block_number = htons(count);
		rv_sock = so_sendto(new_sock_id, ack_buffer, 4, 0, (struct soaddr *)&sessions[session_idx].clnt_address, sessions[session_idx].clnt_address_len);
		sys_start_timer(sessions[session_idx].timer_id, TIMER_RESOLUTION_S | MAX_TIMEOUT);
						
		if (rv_sock < 0)
		{
			send_error_packet_to_client(session_idx, new_sock_id, NOT_DEFINED, "A error in ack send");
			close_session(session_idx);
			file_close(file);
			exit_filesys(OPEN_WRITE);
			so_close(new_sock_id);
			sys_delete_timer(sessions[session_idx].timer_id);
			sys_msgq_delete(sessions[session_idx].new_msgq_id);
			return;
		}
		else
		{
			prev_block = count;
			count++;
		}
	}
	
	while (1) 
	{
		rv = sys_msgq_receive(sessions[session_idx].new_msgq_id, (char *)&new_msg_buf, SYS_WAIT_FOREVER);
		if (rv != SYS_OK) 
			continue;
		else 
		{
			switch (new_msg_buf.msg_type) 
			{
				case SOCKET_DATARCVD:
					rv_sock = so_recvfrom(new_sock_id, &data_buffer, sizeof(data_buffer), 0, (struct soaddr *)&new_clnt_address, &new_clnt_address_len);
					
					if(oack_timer_flag == 1)
					{
						if(ntohs(data->block_number) == 1)
						{
							oack_timer_flag = 0;
							sys_stop_timer(sessions[session_idx].timer_id);
						}
						else
							continue;
					}
					sys_stop_timer(sessions[session_idx].timer_id);
				
					if(ntohs(new_clnt_address.sin_port) !=  sessions[session_idx].clnt_address.sin_port)
					{
						memset(&err_buffer, 0, sizeof(err_buffer));
						err->opcode = htons(TFTP_ERR_OPCODE);
						err->error_code = htons(UNKNOWN_TID);
						strcpy(err->error_message, "Unknown transfer ID");

						rv_sock = so_sendto(new_sock_id, err_buffer, sizeof(err_buffer), 0, (struct soaddr *)&new_clnt_address, new_clnt_address_len);
						continue;
					}
				
					if (ntohs(data->opcode) != TFTP_DATA_OPCODE)
					{
						send_error_packet_to_client(session_idx, new_sock_id, ILLEGAL_TFTP_OPERATION, "Illegal TFTP operation");
						close_session(session_idx);
						file_close(file);
						exit_filesys(OPEN_WRITE);
						so_close(new_sock_id);
						sys_delete_timer(sessions[session_idx].timer_id);
						sys_msgq_delete(sessions[session_idx].new_msgq_id);
						return;
					} 
					
					if(ntohs(data->block_number) == prev_block+1)
					{
						retransmission = 0;

						write = file_write(file, data->data_t, rv_sock-4);
						if (write < 0)
						{
							send_error_packet_to_client(session_idx, new_sock_id, NOT_DEFINED, "A error in write file");
							close_session(session_idx);
							file_close(file);
							exit_filesys(OPEN_WRITE);
							so_close(new_sock_id);
							sys_delete_timer(sessions[session_idx].timer_id);
							sys_msgq_delete(sessions[session_idx].new_msgq_id);
							return;
						}

						ack->opcode = htons(TFTP_ACK_OPCODE);
						ack->block_number = htons(count);
						rv_sock = so_sendto(new_sock_id, ack_buffer, 4, 0, (struct soaddr *)&sessions[session_idx].clnt_address, sessions[session_idx].clnt_address_len);
						sys_start_timer(sessions[session_idx].timer_id, TIMER_RESOLUTION_S | MAX_TIMEOUT);
						
						if (rv_sock < 0)
						{
							send_error_packet_to_client(session_idx, new_sock_id, NOT_DEFINED, "A error in ack send");
							close_session(session_idx);
							file_close(file);
							exit_filesys(OPEN_WRITE);
							so_close(new_sock_id);
							sys_delete_timer(sessions[session_idx].timer_id);
							sys_msgq_delete(sessions[session_idx].new_msgq_id);
							return;
						}
						else
						{
							prev_block = count;
							count++;
						}
						
						if (write < sessions[session_idx].blk_size)
						{
							close_session(session_idx);
							file_close(file);
							exit_filesys(OPEN_WRITE);
							so_close(new_sock_id);
							sys_delete_timer(sessions[session_idx].timer_id);
							sys_msgq_delete(sessions[session_idx].new_msgq_id);
							return;
						}
					}
					else if(ntohs(data->block_number) == prev_block)
					{
						if (retransmission < MAX_RETRIES)
						{
							rv_sock = so_sendto(new_sock_id, ack_buffer, 4, 0, (struct soaddr *)&sessions[session_idx].clnt_address, sessions[session_idx].clnt_address_len);
							sys_start_timer(sessions[session_idx].timer_id, TIMER_RESOLUTION_S | MAX_TIMEOUT);
							retransmission++;
						} 
						else 
						{
							send_error_packet_to_client(session_idx, new_sock_id, NOT_DEFINED, "Not defined, Max retransmission reached");
							close_session(session_idx);
							file_close(file);
							exit_filesys(OPEN_WRITE);
							so_close(new_sock_id);
							sys_delete_timer(sessions[session_idx].timer_id);
							sys_msgq_delete(sessions[session_idx].new_msgq_id);
							return;
						}
					}
					break;
					
				case MSG_TYPE_TIMER:
					if(oack_timer_flag == 1)
					{
						if (retransmission < MAX_RETRIES)
						{
							rv_sock = so_sendto(new_sock_id, oack_buffer, 2+strlen(option_name)+1+strlen(blksize)+1, 0, (struct soaddr *)&sessions[session_idx].clnt_address, sessions[session_idx].clnt_address_len);
							sys_start_timer(sessions[session_idx].timer_id, TIMER_RESOLUTION_S | MAX_TIMEOUT);
							retransmission++;
						} 
						else 
						{
							send_error_packet_to_client(session_idx, new_sock_id, NOT_DEFINED, "Not defined, Max retransmission reached");
							close_session(session_idx);
							file_close(file);
							exit_filesys(OPEN_WRITE);
							so_close(new_sock_id);
							sys_delete_timer(sessions[session_idx].timer_id);
							sys_msgq_delete(sessions[session_idx].new_msgq_id);
							return;
						}
					}
					else
					{
						if (retransmission < MAX_RETRIES)
						{
							rv_sock = so_sendto(new_sock_id, ack_buffer, 4, 0, (struct soaddr *)&sessions[session_idx].clnt_address, sessions[session_idx].clnt_address_len);
							sys_start_timer(sessions[session_idx].timer_id, TIMER_RESOLUTION_S | MAX_TIMEOUT);
							retransmission++;
						} 
						else 
						{
							send_error_packet_to_client(session_idx, new_sock_id, NOT_DEFINED, "Not defined, Max retransmission reached");
							close_session(session_idx);
							file_close(file);
							exit_filesys(OPEN_WRITE);
							so_close(new_sock_id);
							sys_delete_timer(sessions[session_idx].timer_id);
							sys_msgq_delete(sessions[session_idx].new_msgq_id);
							return;
						}
					}
					break;
					
				default:
					break;
			}
		}
		
		if(tickGet() - time_interval >= MAX_DELAY_INTERVAL)
		{
			sys_task_delay(1);
			time_interval = tickGet();
		}
	}

	close_session(session_idx);
	file_close(file);
	exit_filesys(OPEN_WRITE);
	so_close(new_sock_id);
	sys_delete_timer(sessions[session_idx].timer_id);
	sys_msgq_delete(sessions[session_idx].new_msgq_id);
	return;
}

void send_error_packet_to_client(int index, int new_sock_id, uint16 error_code, const char* error_message)
{	
	tftp_err *err;
	int32 rv_sock;
	char err_buffer[128];

	memset(&err_buffer, 0, sizeof(err_buffer));
	err = (tftp_err *)err_buffer;
	
	err->opcode = htons(TFTP_ERR_OPCODE);
	err->error_code = htons(error_code);
	strcpy(err->error_message, error_message);

	rv_sock = so_sendto(new_sock_id, err_buffer, sizeof(err_buffer), 0, (struct soaddr *)&sessions[index].clnt_address, sessions[index].clnt_address_len);		
}

uint32 client_address_check(void)
{
	uint32 checker;
	for(checker = 0; checker < MAX_SESSIONS; checker++)
	{
		if((clnt_address.sin_port == sessions[checker].clnt_address.sin_port) && (clnt_address.sin_addr.s_addr == sessions[checker].clnt_address.sin_addr.s_addr))
		{
			return 1;
		}
	}
	return 0;
}

uint32 read_file_in_block(int idx, int offset, char *buf)
{
	FCB_POINT *file;
	uint32 read;
	int32 next_offset = offset*MAX_FILE_BUFFER_SIZE;

	while(1)
	{
		if(FILE_NOERROR == enter_filesys(OPEN_READ))
			break;
		else
			sys_task_delay(5);
	}
	
	file = file_open(sessions[idx].filename, "r", NULL);
	if(file == NULL)
	{
		exit_filesys(OPEN_READ);
		return -1;
	}
	
	file_seek(file, next_offset, FROM_HEAD);
	read =file_read(file, buf, MAX_FILE_BUFFER_SIZE);
	file_close(file);
	exit_filesys(OPEN_READ);
	return read;
	
}

void parse_options_from_request(char* req_buffer, const char* option_name, char* option_value)
{
	tftp_rrq* req = (tftp_rrq *)req_buffer;
	option_value[0] = '\0';

	size_t rrq_pkt_len = 2 + strlen(req->filename) + 1 + strlen(req->mode);
	char* option_start = ((char *)req) + rrq_pkt_len;

	while(option_start < ((char *)req) + TFTP_BUFFER_SIZE)
	{
		if(option_start[0] == '\0')
			break;
		if(strcasecmp(option_start, option_name) == 0)
		{
			strcpy(option_value, option_start + strlen(option_name) + 1);
			break;
		}
		option_start += strlen(option_start)+1;
	}
}

void reverse(char str[], int length) 
{
    uint32 start = 0;
    uint32 end = length - 1;
	
    while (start < end) 
	{
        char temp = str[start];
        str[start] = str[end];
        str[end] = temp;
        start++;
        end--;
    }
}

char* itoa(int num, char* str, int base) 
{
    uint32 i = 0;
    uint32 isNegative = 0;

    if (num == 0) 
	{
        str[i++] = '0';
        str[i] = '\0';
        return str;
    }

    if (num < 0 && base == 10) 
	{
        isNegative = 1;
        num = -num;
    }

    while (num != 0) 
	{
        int rem = num % base;
        str[i++] = (rem > 9) ? (rem - 10) + 'a' : rem + '0';
        num = num / base;
    }

    if (isNegative)
        str[i++] = '-';

    str[i] = '\0';

    reverse(str, i);

    return str;
}

void tftp_version_register(void)
{
	ver_list.module_type = MODULE_TYPE_TFTPD;
	ver_list.version = ((TFTP_MAJOR_VERSION<<16)|(TFTP_MINOR_VERSION<<8)|TFTP_REVSION_VERSION);
	strcpy(ver_list.module_name, "tftpd");
	strcpy(ver_list.module_description, "tftpd server -- 20/11/2023");
	ver_list.next = NULL;

	register_module_version(&ver_list);
}

INT32 tftp_show_running(DEVICE_ID diID)
{
	int32 rc = 0;
	
	if (diID == 0 && tftp_enable_flag == 1)
	{
		vty_printf("tftp server enable\n");
		rc++;
		if(TFTP_PORT != DEFAULT_TFTP_PORT)
		{
			vty_printf("tftp server port %d\n", TFTP_PORT);
			rc++;
		}
		if(MAX_TIMEOUT != DEFAULT_MAX_TIMEOUT && MAX_RETRIES != DEFAULT_MAX_RETRIES)
		{
			vty_printf("tftp server retransmit %d %d\n", MAX_TIMEOUT, MAX_RETRIES);
			rc++;
		}
		
		if (rc)
			return INTERFACE_GLOBAL_SUCCESS;
		else
			return INTERFACE_DEVICE_ERROR_EMPTYCONFIGURATION;
	}
	
	return INTERFACE_GLOBAL_SUCCESS;
}

void initialize_sessions(void)
{
	uint32 i;
	
	for(i = 0; i<MAX_SESSIONS; i++)
	{
		sessions[i].active = 0;
		sessions[i].new_msgq_id = 0;
		sessions[i].timer_id = 0;
		memset(&sessions[i].clnt_address, 0, sizeof(sessions[i].clnt_address));
		sessions[i].clnt_address_len = 0;
		memset(&sessions[i].filename, 0, sizeof(sessions[i].filename));
		sessions[i].blk_size = 0;
		sessions[i].blk_flag = 0;
	}
}

int get_a_session(void)
{
	uint32 i; 
	for(i = 0; i<MAX_SESSIONS; i++)
	{
		if(!sessions[i].active)
		{
			return i;
		}
	}
	return -1;
}

void close_session(int idx)
{
	if(read_counter > 0 || write_on == 0)
		read_counter--;
	write_on = 0;
	sessions[idx].active = 0;
	memset(&sessions[idx].clnt_address, 0, sizeof(sessions[idx].clnt_address));
	sessions[idx].clnt_address_len = 0;
	memset(&sessions[idx].filename, 0, sizeof(sessions[idx].filename));
	sessions[idx].blk_size = 0;
	sessions[idx].blk_flag = 0;
}


