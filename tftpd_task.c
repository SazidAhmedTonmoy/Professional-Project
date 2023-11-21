/*
*  Copyright (c) 2023, BAUD DATA COMMUNICATION Co., LTD
*  
*  Module : tftpd 
*  File   : tftpd_task.c
*  Author : Md Sazid Ahmed Tonmoy
*  Date   : November 20, 2023
*/

void tftpd_module_main_process(void)
{
	TASK_ID task_id;
	struct soaddr_in serv_addr;
	tftp_rrq *req;
	tftp_err *err;
	int32 bind_id, register_id;
	int32 rv, rv_sock;
	uint32 session_idx;
	uint32 dup_clnt_flag = 0;
	uint32 args[4] = {-1, -1, -1, -1};
	char err_buffer[128];
	char req_buffer[TFTP_BUFFER_SIZE];
	char blksize_str[MAX_OPTION_VALUE_LENTH];

	req = (tftp_rrq *)req_buffer;
	err = (tftp_err *)err_buffer;

	while(1)
	{
		rv = sys_msgq_receive(msgq_id, (char *)&msg_buf, SYS_WAIT_FOREVER);
		if (rv != SYS_OK)
			continue;
		else
		{
			switch (msg_buf.msg_type)
			{
				case SOCKET_DATARCVD:
					clnt_address_len = sizeof(clnt_address);
					memset(req_buffer, 0, sizeof(req_buffer));
					rv_sock = so_recvfrom(sock_id, &req_buffer, sizeof(req_buffer), 0, (struct soaddr *)&clnt_address, &clnt_address_len);
					if(rv_sock <0)
						continue;
					
					dup_clnt_flag = client_address_check();
					if(dup_clnt_flag == 1)
					{
						dup_clnt_flag = 0;
						continue;
					}

					if(strlen(req->filename) == 0 || strlen(req->filename) > 256)
					{
						memset(&err_buffer, 0, sizeof(err_buffer));
						err->opcode = htons(TFTP_ERR_OPCODE);
						err->error_code = htons(NOT_DEFINED);
						strcpy(err->error_message, "File name invalid");

						rv_sock = so_sendto(sock_id, err_buffer, sizeof(err_buffer), 0, (struct soaddr *)&clnt_address, clnt_address_len);
						continue ;
					}

					parse_options_from_request(req_buffer, "blksize", blksize_str);
					
					if(ntohs(req->opcode) == TFTP_RRQ_OPCODE)
					{
						if(write_on == 1)
						{
							memset(&err_buffer, 0, sizeof(err_buffer));
							err->opcode = htons(TFTP_ERR_OPCODE);
							err->error_code = htons(ACCESS_VIOLATION);
							strcpy(err->error_message, "Access Violation");

							rv_sock = so_sendto(sock_id, err_buffer, sizeof(err_buffer), 0, (struct soaddr *)&clnt_address, clnt_address_len);
							continue ;
						}
						
						session_idx = get_a_session();
						if(session_idx < 0)
						{
							memset(&err_buffer, 0, sizeof(err_buffer));
							err->opcode = htons(TFTP_ERR_OPCODE);
							err->error_code = htons(ACCESS_VIOLATION);
							strcpy(err->error_message, "Access Violation");

							rv_sock = so_sendto(sock_id, err_buffer, sizeof(err_buffer), 0, (struct soaddr *)&clnt_address, clnt_address_len);
							continue ;
						}
						
						read_counter++;
						if(read_counter > 3)
						{
							read_counter--;
							memset(&err_buffer, 0, sizeof(err_buffer));
							err->opcode = htons(TFTP_ERR_OPCODE);
							err->error_code = htons(ALLOCATION_EXCEEDED);
							strcpy(err->error_message, "Allocation exceeded");

							rv_sock = so_sendto(sock_id, err_buffer, sizeof(err_buffer), 0, (struct soaddr *)&clnt_address, clnt_address_len);
							continue ;
						}
						
						args[0] = session_idx;
						
						sessions[session_idx].active = 1;
						sessions[session_idx].opcode = TFTP_RRQ_OPCODE;
						strcpy(sessions[session_idx].mode, req->mode);
						memcpy(&sessions[session_idx].clnt_address, &clnt_address, sizeof(clnt_address));
						sessions[session_idx].clnt_address_len = clnt_address_len;
						strcpy(sessions[session_idx].filename, req->filename);
						sessions[session_idx].blk_size = atoi(blksize_str);
						if(sessions[session_idx].blk_size > TFTP_MAX_BLOCK_SIZE)
						{
							sessions[session_idx].blk_size = TFTP_OFFER_BLOCK_SIZE;
						}
						
						if(sessions[session_idx].blk_size == 0)
						{
							sessions[session_idx].blk_size =  TFTP_BLOCK_SIZE;
							sessions[session_idx].blk_flag = 1;
						}
						
						task_id = sys_task_spawn("TRRQ", 128, 0, 262144, (FUNCPTR)tftpd_rrq_function, args, 0);
						if (task_id == (TASK_ID) SYS_ERROR)
							syslog(LOG_WARNING, "Failed to spawn read request task\n");
					}
					
					else if(ntohs(req->opcode) == TFTP_WRQ_OPCODE)
					{
						if(write_on == 1 || read_counter > 0)
						{
							memset(&err_buffer, 0, sizeof(err_buffer));
							err->opcode = htons(TFTP_ERR_OPCODE);
							err->error_code = htons(ACCESS_VIOLATION);
							strcpy(err->error_message, "Access Violation");

							rv_sock = so_sendto(sock_id, err_buffer, sizeof(err_buffer), 0, (struct soaddr *)&clnt_address, clnt_address_len);
							continue ;
						}
						
						session_idx = get_a_session();
						if(session_idx < 0)
						{
							memset(&err_buffer, 0, sizeof(err_buffer));
							err->opcode = htons(TFTP_ERR_OPCODE);
							err->error_code = htons(ACCESS_VIOLATION);
							strcpy(err->error_message, "Access Violation");

							rv_sock = so_sendto(sock_id, err_buffer, sizeof(err_buffer), 0, (struct soaddr *)&clnt_address, clnt_address_len);
							continue ;
						}
						
						write_on = 1;
						args[0] = session_idx;
						
						sessions[session_idx].active = 1;
						sessions[session_idx].opcode = TFTP_WRQ_OPCODE;
						strcpy(sessions[session_idx].mode, req->mode);
						memcpy(&sessions[session_idx].clnt_address, &clnt_address, sizeof(clnt_address));
						sessions[session_idx].clnt_address_len = clnt_address_len;
						strcpy(sessions[session_idx].filename, req->filename);
						sessions[session_idx].blk_size = atoi(blksize_str);
						if(sessions[session_idx].blk_size > TFTP_MAX_BLOCK_SIZE)
						{
							sessions[session_idx].blk_size = TFTP_OFFER_BLOCK_SIZE;
						}
						
						if(sessions[session_idx].blk_size == 0)
						{
							sessions[session_idx].blk_size =  TFTP_BLOCK_SIZE;
							sessions[session_idx].blk_flag = 1;
						}
						
						task_id = sys_task_spawn("TWRQ", 128, 0, 131072, (FUNCPTR)tftpd_wrq_function, args, 0);
						if (task_id == (TASK_ID) SYS_ERROR)
							syslog(LOG_WARNING, "Failed to spawn write request task\n");
					}
					else
					{
						memset(&err_buffer, 0, sizeof(err_buffer));
						err->opcode = htons(TFTP_ERR_OPCODE);
						err->error_code = htons(ILLEGAL_TFTP_OPERATION);
						strcpy(err->error_message, "Illegal TFTP operation");

						rv_sock = so_sendto(sock_id, err_buffer, sizeof(err_buffer), 0, (struct soaddr *)&clnt_address, clnt_address_len);
						continue;
					}
					break;

				case TFTP_ENABLE:
					if(tftp_enable_flag == 0)
					{
						tftp_enable_flag = 1;
						sock_id = so_socket(AF_INET, SOCK_DGRAM, 0);
					    if (sock_id < 0)
							continue;

					    memset(&serv_addr, 0, sizeof(serv_addr));
					    serv_addr.sin_family = AF_INET;
					    serv_addr.sin_port = htons(TFTP_PORT);

						bind_id = so_bind(sock_id, (struct soaddr *)&serv_addr, sizeof(serv_addr));
					    if (bind_id < 0)
							continue;

						register_id = socket_register(sock_id, (ULONG)msgq_id, 0);
					    if (register_id != 0)
							continue;
					}
					else if(tftp_enable_flag == 1)
					{
						vty_output("\ntftp server is already enable\n");
					}
					break;
					
				case TFTP_DISABLE:
					if(tftp_enable_flag == 1)
					{
						tftp_enable_flag = 0;
						socket_unregister(sock_id);
						so_close(sock_id);
					}
					else if(tftp_enable_flag == 0)
					{
						vty_output("\ntftp server is already disable\n");
					}
					break;
					
				default:
					break;
			}
		}
	}	
	
	return ;
}
