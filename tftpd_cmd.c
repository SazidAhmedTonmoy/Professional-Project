/*
*  Copyright (c) 2023, BAUD DATA COMMUNICATION Co., LTD
*  
*  Module : tftpd 
*  File   : tftpd_cmd.c
*  Author : Md Sazid Ahmed Tonmoy
*  Date   : November 20, 2023
*/

static struct topcmds top_tftp_cmd[] = 
{
  { "tftp", cmdPref(PF_CMDNO, PF_CMDDEF, 0),
    IF_ANY, ~FG_GLOBAL, IF_NULL, FG_CONFIG, 
    cmd_conf_tftp, NULL, NULL, 0, 0,
    "tftp        -- TFTP configuration",
    "tftp        -- TFTP configuration tftp in chinese",
    NULLCHAR, NULLCHAR
  },
  { NULLCHAR }
};

struct cmds tftp_show_cmd[] = 
{
  { "tftp", MATCH_AMB, 0, 0, show_tftp, NULL, NULL, 0, 0,
    "tftp                 -- TFTP",
    "tftp                 -- TFTP",
    NULLCHAR, NULLCHAR
  },
  { NULLCHAR }
};

static struct cmds tftp_cmds[] = 
{
  { "server", MATCH_AMB, cmdPref(PF_CMDNO, PF_CMDDEF, 0), 0,
    cmd_conf_tftp_server, NULL, NULL, 2, 0, 
    "server        -- TFTP server configuration",
    "server        -- TFTP server configuration in chineses",
    NULLCHAR, NULLCHAR
  },
  { NULLCHAR }
};

static struct cmds tftp_server_cmds[] =
{
  { "enable", MATCH_AMB, cmdPref(PF_CMDNO, PF_CMDDEF, 0), 0,
    do_enable_tftp, NULL, NULL, cmdArgc(1, 1, 1), cmdArgc(1, 1, 1), 
    "enable               -- enable TFTP client",
    "enable               -- enable TFTP client in chinese",
    NULLCHAR, NULLCHAR
  },
  { "port", MATCH_AMB, cmdPref(PF_CMDNO, PF_CMDDEF, 0), 0,
    set_tftp_port, NULL, NULL, cmdArgc(2, 1, 1), cmdArgc(2, 1, 1),
    "port                  -- set TFTP port",
    "port                  -- set TFTP port in chinese",
  },
  { "retransmit", MATCH_AMB, cmdPref(PF_CMDNO, PF_CMDDEF, 0), 0,
    set_tftp_retransmit, NULL, NULL, cmdArgc(2, 1, 1), cmdArgc(3, 1, 1),
    "retransmit                  -- set TFTP retransmit",
    "retransmit                  -- set TFTP retransmit in chinese",
  },
  { NULLCHAR }
};

struct cmds tftp_show_sub_cmds[] = 
{
  { "server", MATCH_AMB, 0, 0, do_show_tftp_server, NULL, NULL, 0, 1,
    "server              -- session's statistics",
    "server              -- session's statistics in chinese",
    NULLCHAR, NULLCHAR
  },
  { NULLCHAR }
};


static int do_enable_tftp(int argc, char **argv, struct user *u)
{
	msg_t buf;
	if(IsNoPref(u) || IsDefPref(u))
	{
		buf.msg_type = TFTP_DISABLE;
		sys_msgq_send(msgq_id, (char *)&buf, Q_OP_NORMAL, SYS_NO_WAIT);
	}
	else
	{
		buf.msg_type = TFTP_ENABLE;
		sys_msgq_send(msgq_id, (char *)&buf, Q_OP_NORMAL, SYS_NO_WAIT);
	}
	
	return 0;
}

static int cmd_conf_tftp(int argc, char **argv, struct user *u)
{
	return subcmd(tftp_cmds, NULL, argc, argv, u);
}

static int cmd_conf_tftp_server(int argc, char **argv, struct user *u)
{
	return subcmd(tftp_server_cmds, NULL, argc, argv, u);
}

static int set_tftp_port(int argc, char **argv, struct user *u)
{
	msg_t buf;
	int32 rc;
	int32 port;
	struct parameter param;
	
	memset(&param, 0,sizeof(struct parameter));
	param.type = ARG_INT;
	if(IsNoPref(u) || IsDefPref(u))
	{
		buf.msg_type = TFTP_DISABLE;
		sys_msgq_send(msgq_id, (char *)&buf, Q_OP_NORMAL, SYS_NO_WAIT);
		
		TFTP_PORT = DEFAULT_TFTP_PORT;

		buf.msg_type = TFTP_ENABLE;
		sys_msgq_send(msgq_id, (char *)&buf, Q_OP_NORMAL, SYS_NO_WAIT);
	}
	else 
	{
		if(argc != 2)
			return 0;

		if(argv[1][0] == '?')
		{
			if (IsChinese(u))
				vty_output("  tftp port range (chinese)    -- 1 to 65353\n");
			else
				vty_output("  tftp port range    -- 1 to 65353\n");
			
			return 0;
		}

		if((rc = getparameter(argc, argv, u, &param)))
		{
			return rc;
		}
			
		port = param.value.v_int;

		if(port >= 1 && port < 65354)
		{
			buf.msg_type = TFTP_DISABLE;
			sys_msgq_send(msgq_id, (char *)&buf, Q_OP_NORMAL, SYS_NO_WAIT);
			
			TFTP_PORT = port;

			buf.msg_type = TFTP_ENABLE;
			sys_msgq_send(msgq_id, (char *)&buf, Q_OP_NORMAL, SYS_NO_WAIT);
		}
		else
			vty_output("tftp port out of range 0-65353\n");
	}
	return 0;
}

static int set_tftp_retransmit(int argc, char **argv, struct user *u)
{
	int32 rc;
	int32 timeout, retry;
	struct parameter param;
	
	memset(&param, 0,sizeof(struct parameter));
	param.type = ARG_INT;
	if(IsNoPref(u) || IsDefPref(u))
	{
		MAX_TIMEOUT = DEFAULT_MAX_TIMEOUT;
		MAX_RETRIES = DEFAULT_MAX_RETRIES;
	}
	else
	{		
		if(argv[1][0] == '?')
		{
			if (IsChinese(u))
				vty_output("  Timeout value range (in chinese)   -- 1-255\n");
			else
				vty_output("  Timeout value range   -- 1-255\n");
			
			return 0;
		}

		if(argc == 3 && argv[2][0] == '?')
		{
			if (IsChinese(u))
				vty_output("  Retry value range (in chinese)   -- 1-6\n");
			else
				vty_output("  Retry value range   -- 1-6\n");
			
			return 0;
		}

		if((rc = getparameter(argc--, argv++, u, &param)))
			return rc;
		timeout = param.value.v_int;
		
		if((rc = getparameter(argc, argv, u, &param)))
			return rc;
		retry = param.value.v_int;

		if(timeout >= 1 && timeout < 256)
			MAX_TIMEOUT = timeout;
		else
			vty_output("timeout value out of range 1-255\n");

		if(retry >= 1 && retry < 7)
			MAX_RETRIES = retry;
		else
			vty_output("retry value out of range 1-6\n");
	}

	return 0;
}

int show_tftp(int argc, char **argv, struct user *u)
{
	return subcmd(tftp_show_sub_cmds, NULL, argc, argv, u);
}

static int do_show_tftp_server(int argc, char **argv, struct user *u)
{
	int32 i, counter = 0;
	
	vty_printf("tftp session's statistics:\n");
	vty_printf("!\n!\n");
	vty_printf("status:\n!\n");
	(tftp_enable_flag == 1) ? vty_printf("tftp enable: Yes\n") : vty_printf("tftp enable: No\n");
	vty_printf("tftp server port %d\n", TFTP_PORT);
	vty_printf("tftp server timeout %d\n", MAX_TIMEOUT);
	vty_printf("tftp server retry %d\n", MAX_RETRIES);
	vty_printf("!\n!\n!\n!");
	vty_printf("\nActive session status:\n");
	vty_printf("!\n");
	for(i = 0; i<MAX_SESSIONS; i++)
	{
		if(sessions[i].active == 1)
		{
			vty_printf("Session no	: %d\n", i+1);
			if(sessions[i].opcode == TFTP_RRQ_OPCODE)
				vty_printf("Session type:  Read\n");
			else if(sessions[i].opcode == TFTP_WRQ_OPCODE)
				vty_printf("Session type: Write\n");
			vty_printf("--------------------------------------\n");
			vty_printf("Filename	: %s\n", sessions[i].filename);
			vty_printf("Block size	: %d\n", sessions[i].blk_size);
			vty_printf("IP Address	: %s\n", ip_ntoa(sessions[i].clnt_address.sin_addr.s_addr));
			vty_printf("Port		: %d\n", ntohs(sessions[i].clnt_address.sin_port));
			vty_printf("!\n!\n");
			counter++;
		}
	}
	if(counter == 0)
		vty_printf("No session is running now\n!\n!\n");
	
	vty_printf_end(1);
	return 0;
}


void tftp_register_cmds(void)
{
	registercmd(top_tftp_cmd);
	register_subcmd("show", 0, IF_NULL, FG_ENABLE, tftp_show_cmd);
	
	return;
}
