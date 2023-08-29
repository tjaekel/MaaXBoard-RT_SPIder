/*
 * cmd_dec.c
 *
 *  Created on: Aug 8, 2023
 *      Author: tj925438
 */

#include <string.h>
#include "fsl_os_abstraction.h"
#include <task.h>

#include "globals.h"
#include "LED.h"
#include "SPI.h"
#include "GPIO.h"
#include "TEMP.h"
#include "HTTPD.h"
#include "MEM_Pool.h"
#include "VCP_UART.h"
#include "UDP.h"
#include "cmd_dec.h"

#include "ITM_print.h"

/* prototypes */
ECMD_DEC_Status CMD_NotImplemented(TCMD_DEC_Results *res, EResultOut out);

ECMD_DEC_Status CMD_help(TCMD_DEC_Results *res, EResultOut out);
ECMD_DEC_Status CMD_sysinfo(TCMD_DEC_Results *res, EResultOut out);
ECMD_DEC_Status CMD_syserr(TCMD_DEC_Results *res, EResultOut out);
ECMD_DEC_Status CMD_debug(TCMD_DEC_Results *res, EResultOut out);
ECMD_DEC_Status CMD_print(TCMD_DEC_Results *res, EResultOut out);
ECMD_DEC_Status CMD_repeat(TCMD_DEC_Results *res, EResultOut out);
ECMD_DEC_Status CMD_delay(TCMD_DEC_Results *res, EResultOut out);
ECMD_DEC_Status CMD_led(TCMD_DEC_Results *res, EResultOut out);
ECMD_DEC_Status CMD_fwreset(TCMD_DEC_Results *res, EResultOut out);
ECMD_DEC_Status CMD_sysinfo(TCMD_DEC_Results *res, EResultOut out);
ECMD_DEC_Status CMD_ipaddr(TCMD_DEC_Results *res, EResultOut out);

ECMD_DEC_Status CMD_pgpio(TCMD_DEC_Results *res, EResultOut out);

ECMD_DEC_Status CMD_rawspi(TCMD_DEC_Results *res, EResultOut out);
ECMD_DEC_Status CMD_spiclk(TCMD_DEC_Results *res, EResultOut out);

ECMD_DEC_Status CMD_umdir(TCMD_DEC_Results *res, EResultOut out);
ECMD_DEC_Status CMD_umprint(TCMD_DEC_Results *res, EResultOut out);

ECMD_DEC_Status CMD_test(TCMD_DEC_Results *res, EResultOut out);

ECMD_DEC_Status CMD_udpip(TCMD_DEC_Results *res, EResultOut out);
ECMD_DEC_Status CMD_udptest(TCMD_DEC_Results *res, EResultOut out);

ECMD_DEC_Status CMD_tasks(TCMD_DEC_Results *res, EResultOut out);

const TCMD_DEC_Command Commands[] /*FASTRUN*/ = {
		{
				.cmd = (const char *)"help",
				.help = (const char *)"list of all defined commands or help for [cmd]",
				.func = CMD_help
		},
		{
				.cmd = (const char *)"sysinfo",
				.help = (const char *)"display version and system info [-m]",
				.func = CMD_sysinfo
		},
		{
				.cmd = (const char *)"syserr",
				.help = (const char *)"display sys error [-d]",
				.func = CMD_syserr
		},
		{
				.cmd = (const char *)"tasks",
				.help = (const char *)"display RTOS tasks",
				.func = CMD_tasks
		},
		{
				.cmd = (const char *)"debug",
				.help = (const char *)"set debug flags <val>",
				.func = CMD_debug
		},
		{
				.cmd = (const char *)"print",
				.help = (const char *)"print [-n] [rest of cmd]",
				.func = CMD_print
		},
		{
				.cmd = (const char *)"repeat",
				.help = (const char *)"repeat [-0|-n] <rest of line> endless or n times",
				.func = CMD_repeat
		},
	    {
				.cmd = (const char *)"delay",
				.help = (const char *)"delay [ms]",
				.func = CMD_delay
		},
		{
				.cmd = (const char *)"led",
				.help = (const char *)"led <0..2> [0|1]",
				.func = CMD_led
		},
		{
				.cmd = (const char *)"pgpio",
				.help = (const char *)"set GPIO <num> [0|1]",
				.func = CMD_pgpio
		},
		{
				.cmd = (const char *)"fwreset",
				.help = (const char *)"fwreset reboots FW",
				.func = CMD_fwreset
		},
		{
				.cmd = (const char *)"ipaddr",
				.help = (const char *)"display MCU IP address",
				.func = CMD_ipaddr
		},

		//USB memory stick
		{
				.cmd = (const char *)"umdir",
				.help = (const char *)"USB memory list directory [1:/dirname]",
				.func = CMD_umdir
		},
		{
				.cmd = (const char *)"umprint",
				.help = (const char *)"USB memory print file <1:/filename>",
				.func = CMD_umprint
		},
		{
				.cmd = (const char *)"udpip",
				.help = (const char *)"set udip destination <ipaddr>",
				.func = CMD_udpip
		},
		{
				.cmd = (const char *)"udptest",
				.help = (const char *)"send test data via UDP",
				.func = CMD_udptest
		},

		//SPI
		{
				.cmd = (const char *)"rawspi",
				.help = (const char *)"rawspi <byte ...>",
				.func = CMD_rawspi
		},
		{
				.cmd = (const char *)"spiclk",
				.help = (const char *)"spiclk <hz>",
				.func = CMD_spiclk
		},


		//Chip specific

		//other expert, test commands
		{
				.cmd = (const char *)"test",
				.help = (const char *)"expert test",
				.func = CMD_test
		},
};

/**
 * ----------------------------------------------------------------------------
 */

static unsigned int CMD_DEC_findCmd(const char *cmd, unsigned int len)
{
	size_t i;
	for (i = 0; i < (sizeof(Commands)/sizeof(TCMD_DEC_Command)); i++)
	{
		if (Commands[i].func == NULL)
			continue;
		/* if length does not match - keep going */
		if (len != (unsigned int)strlen(Commands[i].cmd))
			continue;
		if (strncmp(cmd, Commands[i].cmd, (size_t)len) == 0)
			return (unsigned int)i;
	}

	return (unsigned int)-1;
}

static void CMD_DEC_DecodeError(ECMD_DEC_Status err, EResultOut out)
{
	/*
	 * TODO: decode command errors - empty for now, silently ignored on errors
	 */
	(void)err;
	(void)out;

	return;
}

static unsigned int CMD_DEC_decode(char *cmdStr, TCMD_DEC_Results *res)
{
	unsigned int i;
	int state;
	char *xCmdStr = cmdStr;
	char cmd_separator = CMD_DEC_SEPARATOR;

	/* set default results */
	res->cmd = NULL;
	res->cmdLen = 0;
	res->offset = 0;
	res->opt = NULL;
	res->str = NULL;
	res->ctl = 0;
	res->num = 0;
	/* set all values to 0 as default, if omitted but used */
	for (i = 0; i < CMD_DEC_NUM_VAL; i++)
		res->val[i] = 0L;

	state = 0;
	i = 0;

	if (*cmdStr == '|') {
		cmd_separator = 0xff;
		cmdStr++;
		xCmdStr = cmdStr;
		res->ctl = 1;
	}

	while ((*cmdStr != '\0') && (*cmdStr != '\r') && (*cmdStr != '\n') && (*cmdStr != cmd_separator) && (*cmdStr != CMD_DEC_COMMENT))
	{
		/* skip leading spaces, tabs */
		while ((*cmdStr == ' ') || (*cmdStr == '\t'))
		{
			if ((state == 1) || (state == 3))
			{
				state++;
			}
			if (state == 5)
				state--;

			cmdStr++;
		}

		switch (state)
		{
		case 0:	/* find command keyword	*/
			res->cmd = cmdStr;
			state = 1;
			break;
		case 1:	/* find end of keyword */
			if ((*cmdStr != '\0') && (*cmdStr != '\r') && (*cmdStr != '\n') && (*cmdStr != cmd_separator) && (*cmdStr != CMD_DEC_COMMENT))
			{
				cmdStr++;
				res->cmdLen++;
			}
			break;
		case 2:	/* check if option is there */
			if (*cmdStr == CMD_DEC_OPT_SIGN)
			{
				res->opt = cmdStr;
				state = 3;
			}
			else
				state = 4;
			break;
		case 3:	/* find end of option string */
			if ((*cmdStr != '\0') && (*cmdStr != '\r') && (*cmdStr != '\n') && (*cmdStr != cmd_separator) && (*cmdStr != CMD_DEC_COMMENT))
				cmdStr++;
			break;
		case 4: /* now we scan just values or option value */
			if (i < CMD_DEC_NUM_VAL)
			{
				if (i == 0)
					if ( ! res->str)
						res->str = cmdStr;
				/*
				 * ATTENTION: this sscanf just signed values (decimal or hex), values above
				 * 0x8000000 results in a wrong value!
				 */

				////fixed, and add '$' for user variable
				if (*cmdStr == '$')
				{
					i++;
				}
				else
				{
					if ((cmdStr[1] == 'x') || (cmdStr[1] == 'X'))
					{
						if (sscanf(cmdStr, "%lx", (unsigned long *)(&res->val[i])) == 1)
							i++;
					}
					else
					{
						if (sscanf(cmdStr, "%lu", (unsigned long *)(&res->val[i])) == 1)
							i++;
					}
				}

				res->num = i;
			}
			state = 5;
			break;
		case 5:	/* skip value characters */
			if ((*cmdStr != '\0') && (*cmdStr != '\r') && (*cmdStr != '\n') && (*cmdStr != cmd_separator) && (*cmdStr != CMD_DEC_COMMENT))
				cmdStr++;
			break;
		}
	} /* end while */

	if (*cmdStr == cmd_separator)
	{
		cmdStr++;
		res->offset = (unsigned long)(cmdStr - xCmdStr);
		return (unsigned int)(cmdStr - xCmdStr);
	}

	if (*cmdStr == CMD_DEC_COMMENT)
		*cmdStr = '\0';

	if (res->cmd)
		if (*res->cmd == '\0')
			res->cmd = NULL;

	if ((*cmdStr == '\r') || (*cmdStr == '\n'))
		*cmdStr = '\0';

	return 0;
}

ECMD_DEC_Status CMD_DEC_execute(char *cmd, EResultOut out)
{
	TCMD_DEC_Results *cmdRes;
	unsigned int res, offset, idx;

	cmdRes = (TCMD_DEC_Results *)MEM_PoolAlloc(sizeof(TCMD_DEC_Results));
	if ( ! cmdRes)
	{
		return CMD_DEC_OOMEM;
	}

	offset = 0;
	do
	{
		res = CMD_DEC_decode(cmd + offset, cmdRes);
		offset += res;

		if (cmdRes->cmd)
			if (*cmdRes->cmd != '\0')	/* ignore empty line, just spaces */
			{
				idx = CMD_DEC_findCmd(cmdRes->cmd, cmdRes->cmdLen);
				if (idx != (unsigned int)-1)
				{
					ECMD_DEC_Status err;

					err = Commands[idx].func(cmdRes, out);

					/* decode the err code */
					CMD_DEC_DecodeError(err, out);
				}
				else
				{
					UART_Send((const char *)"*E: unknown >", 13, out);
					UART_Send((const char *)cmdRes->cmd, cmdRes->cmdLen, out);
					UART_Send((const char *)"<", 1, out);
					UART_Send((const char *)"\r\n", 2, out);

					////SYSERR_Set(out, SYSERR_CMD);
				}
			}

		if (cmdRes->ctl == 1)
			break;

	} while (res);

	MEM_PoolFree(cmdRes);
	return CMD_DEC_OK;
}

/* helper function for help - keyword length */
static unsigned int CMD_keywordLen(const char *str)
{
	unsigned int l = 0;

	while (*str)
	{
		if ((*str == ';') || (*str == ' ') || (*str == '\r') || (*str == '\n'))
			break;
		str++;
		l++;
	}

	return l;
}

/* helper function for print - string length up to ';' or '#' */
static unsigned int CMD_lineLen(const char *str, int entireLine)
{
	unsigned int l = 0;
  char cmdDelimiter = CMD_DEC_SEPARATOR;

  if (entireLine)
    cmdDelimiter = 0xff;

	while (*str)
	{
		if ((*str == cmdDelimiter) || (*str == '#') || (*str == '\r') || (*str == '\n'))
			break;
		str++;
		l++;
	}

	return l;
}

/* helper function to find string <filename> after a value <IDX> */
const char * CMD_nextStr(const char *str)
{
	const char *resStr = NULL;		/* set to invalid result */

	/* find space after <IDX> */
	while (*str)
	{
		if ((*str == ' ') || (*str == '\r') || (*str == '\n'))
			break;
		str++;
	}

	/* find start of <filename> as not a space */
	while (*str)
	{
		if (*str != ' ')
		{
			resStr = str;
			break;
		}
		str++;
	}

	return resStr;
}

/* other helper functions */

void hex_dump(const unsigned char *ptr, int len, int mode, EResultOut out)
{
	int i = 0;
	int xLen = len;

#if 0
	if (ptr == NULL)
		return;
#endif
	if (len == 0)
		////return;
		len = 1;

	////if (out == UART_OUT)
	{
		print_log(out, "%08lx | ", (unsigned long)ptr);
		while (xLen > 0)
		{
			if (mode == 1)
			{
				//bytes
				//TODO: we could an ASCII character view
				print_log(out, (const char *)"%02X ", (int)*ptr);
				ptr++;
				xLen--;
				i++;
			}
			else
			if (mode == 2)
			{
				//short words, little endian
				print_log(out, (const char *)"%04X ", (int)(*ptr | (*(ptr + 1) << 8)));
				ptr += 2;
				xLen -= 2;
				i += 2;
			}
			else
			if (mode == 4)
			{
				//32bit words, little endian
				print_log(out, (const char *)"%08lX ", (unsigned long)(*ptr |
																	  (*(ptr + 1) << 8) |
																	  (*(ptr + 2) << 16) |
																	  (*(ptr + 3) << 24)
																	 ));
				ptr += 4;
				xLen -= 4;
				i += 4;
			}
			else
				break;		//just for now to make it safe

			if ((i % 16) == 0)
			{
#if 1
				{
					//optional: print ASCII characters
					int j;
					print_log(out, "  ");
					for (j = 16; j > 0; j--)
					{
						if ((*(ptr - j) >= ' ') && (*(ptr - j) < 0x7F))
							print_log(out, "%c", *(ptr - j));
						else
							print_log(out, ".");
					}
				}
#endif
				if (xLen)
					print_log(out, "\r\n%08lx | ", (unsigned long)ptr);
				else
					UART_Send((const char*)"\r\n", 2, out);
			}
		}

		if ((i % 16) != 0)
			UART_Send((const char *)"\r\n", 2, out);
	}
}

/* verify the option and get the SPI interface number - starting at 1 */
int CMD_getSPIoption(char *str)
{
  if (str) {
	  if (strncmp(str, "-A", 2) == 0)
		  return 1;
	  //dummy SPI
	  ////if (strncmp(str, "-D", 2) == 0)
	  ////	return 6;
  }

	return 0;		//-P is default
}

void CMD_printPrompt(EResultOut out)
{
	UART_Send((const char *)"> ", 2, out);
}

/**
 * ----------------------------------------------------------------------------
 */

ECMD_DEC_Status CMD_NotImplemented(TCMD_DEC_Results *res, EResultOut out) {
  (void)res;
  print_log(out, "*I: not implemented\r\n");

  return CMD_DEC_OK;
}

ECMD_DEC_Status CMD_help(TCMD_DEC_Results *res, EResultOut out)
{
	unsigned int idx;

#ifdef TRY
  print_log(UART_OUT, "XX: %lx | %lx\r\n", (unsigned long)&Commands, (unsigned long)Commands[0].help);
  f("STRING");
  f("STRING");
#endif

	if (res->str)
	{
		/* we have 'help <CMD>' - print just the single line help */
		idx = CMD_DEC_findCmd(res->str, CMD_keywordLen(res->str));
		if (idx != (unsigned int)-1)
			print_log(out, (const char *)"%-10s: %s\r\n", Commands[idx].cmd, Commands[idx].help);
		else
		{
			UART_Send((const char *)"*E: unknown\r\n", 13, out);
			return CMD_DEC_UNKNOWN;
		}
	}
	else
	{
		/* just 'help' - print entire list */
		for (idx = 0; (size_t)idx < (sizeof(Commands)/sizeof(TCMD_DEC_Command)); idx++)
			print_log(out, (const char *)"%-10s: %s\r\n", Commands[idx].cmd, Commands[idx].help);
	}

	return CMD_DEC_OK;
}

ECMD_DEC_Status CMD_syserr(TCMD_DEC_Results *res, EResultOut out)
{

  return CMD_DEC_OK;
}

ECMD_DEC_Status CMD_debug(TCMD_DEC_Results *res, EResultOut out) {
  (void)out;

  return CMD_DEC_OK;
}

ECMD_DEC_Status CMD_print(TCMD_DEC_Results *res, EResultOut out)
{
	if (res->str)
		UART_Send((const char *)res->str, CMD_lineLen(res->str, res->ctl), out);
	if (res->opt)
	{
		if (strncmp(res->opt, "-n", 2) == 0)
		{
			/* no newline to add */
		}
		else
			UART_Send((const char *)"\r\n", 2, out);
	}
	else
		UART_Send((const char *)"\r\n", 2, out);

	return CMD_DEC_OK;
}

ECMD_DEC_Status CMD_repeat(TCMD_DEC_Results *res, EResultOut out)
{
	int numRepeat = 1;			//default is 1
	int endLess = 0;
	char *keepStr;				  //remember the command(s) to repeat

	if (res->opt)
	{
		if (*res->opt == '-')
		{
			sscanf((res->opt + 1), (const char *)"%i", &numRepeat);
		}
	}

	if (numRepeat == 0)
	{
		endLess = 1;
		numRepeat = 1;
	}

	keepStr = res->str;			//remember the rest of string as commands(s)

	/* loop numRepeat times to repeat the commands(s) */
	do
	{
		int c;
		/* check UART reception - any received character will break the loop */
		if (c = UART_getCharNW())
		{
#if 0
			print_log(out, "*D: loop broken with '%0c'\r\n", c);
#endif
			break;				//break loop with any UART Rx received */
		}

		/* call the command interpreter - one recursive step deeper */
		CMD_DEC_execute(keepStr, out);

		/* if numRepeat is 0 - we do it endless */
		if (endLess)
			continue;
		else
			numRepeat--;
	} while (numRepeat);

	res->ctl = 1;				//break the outer command interpreter, we can have ';', done here

	return CMD_DEC_OK;
}

ECMD_DEC_Status CMD_delay(TCMD_DEC_Results *res, EResultOut out)
{
  (void) out;

  if (res->val[0])
	  OSA_TimeDelay((int)res->val[0]);

  return CMD_DEC_OK;
}

ECMD_DEC_Status CMD_led(TCMD_DEC_Results *res, EResultOut out)
{
  (void)out;

  LED_state(res->val[0], res->val[1]);

  return CMD_DEC_OK;
}

ECMD_DEC_Status CMD_pgpio(TCMD_DEC_Results *res, EResultOut out)
{
  (void)out;

  GPIO_Set(res->val[0], res->val[1]);

  return CMD_DEC_OK;
}

ECMD_DEC_Status CMD_fwreset(TCMD_DEC_Results *res, EResultOut out) {
  (void)res;
  (void)out;

  NVIC_SystemReset();     //CMSIS file used

  return CMD_DEC_OK;
}

ECMD_DEC_Status CMD_sysinfo(TCMD_DEC_Results *res, EResultOut out)
{
	(void)res;
	int temp;
	int inUse, watermark, max;

	MEM_PoolCounters(&inUse, &watermark, &max);
	print_log(out, "Version         : %s\r\n", VERSION_INFO);
	print_log(out, "MEM Pool        : %d | %d | %d\r\n", inUse, watermark, max);
	print_log(out, "MCU core clock  : %d [Hz]\r\n", SystemCoreClock);
	temp = TEMP_Get();
	print_log(out, "MCU temperature : %d.%d [C]\r\n", temp / 10, temp % 10);
	return CMD_DEC_OK;
}

/* --------------------------------------------------------------------------- */

unsigned char spiTx[CMD_DEC_NUM_VAL];
unsigned char spiRx[CMD_DEC_NUM_VAL];

ECMD_DEC_Status CMD_rawspi(TCMD_DEC_Results *res, EResultOut out)
{
  /* TODO: change to MEM_Pool */
  int i;

  for (i = 0; i < res->num; i++)
	  spiTx[i] = (unsigned char)res->val[i];

  SPI_transaction(spiTx, spiRx, i);

  ////hex_dump(spiRx, i, 1, out);
  for (i = 0; i < res->num; i++)
	  print_log(out, "%02x ", spiRx[i]);
  UART_Send((const char *)"\r\n", 2, out);

  return CMD_DEC_OK;
}

ECMD_DEC_Status CMD_spiclk(TCMD_DEC_Results *res, EResultOut out)
{
	(void)out;

	SPI_SetClock(res->val[0]);

	return CMD_DEC_OK;
}
/* -------------------------------------------------------------------------- */

extern int USBH_Available(void);
extern int USBH_ListRootDirectory(char *f, EResultOut out);
extern int USBH_PrintFile(char *f, EResultOut out);

ECMD_DEC_Status CMD_umdir(TCMD_DEC_Results *res, EResultOut out)
{
	if (USBH_Available())
		USBH_ListRootDirectory(res->str, out);

	return CMD_DEC_OK;
}

ECMD_DEC_Status CMD_umprint(TCMD_DEC_Results *res, EResultOut out)
{
	if (USBH_Available())
		USBH_PrintFile(res->str, out);

	return CMD_DEC_OK;
}

ECMD_DEC_Status CMD_ipaddr(TCMD_DEC_Results *res, EResultOut out)
{
	(void)res;
#ifdef WITH_HTTPD_SERVER
	HTTPD_GetIPAddress(out);
#endif

	return CMD_DEC_OK;
}

//#ifdef SDRAM_TEST
/* SDRAM test */
uint32_t sdRAM[1024] __attribute__((section(".data.$BOARD_SDRAM")));
#define SDRAM_SIZE_WORDS	(0x02000000 / sizeof(uint32_t))
//#endif

ECMD_DEC_Status CMD_test(TCMD_DEC_Results *res, EResultOut out)
{
//#ifdef SDRAM_TEST
	uint32_t *p = sdRAM;
	int i, j;

	print_log(UART_OUT, "*D: SDRAM addr: %lx\r\n", p);
	/* let's check if our sdRAM was initialized */
	for (i = 0; i < 8; i++)
	{
		print_log(UART_OUT, "*D: %0lx ", *p++);
	}
	print_log(UART_OUT, "\r\n");

	/* let's check what if and how initialized outside out sdRAM */
	p = &sdRAM[1024];
	for (i = 0; i < 8; i++)
	{
		print_log(UART_OUT, "*D: %0lx ", *p++);
	}
	print_log(UART_OUT, "\r\n");

	p = sdRAM;
	for (i = 0; i < (sizeof(sdRAM) / sizeof(uint32_t)); i++)
	{
		*p++ = i;
	}
	p = sdRAM;
	j = 0;
	for (i = 0; i < (sizeof(sdRAM) / sizeof(uint32_t)); i++)
	{
		if (*p != i)
		{
			print_log(UART_OUT, "*D: SDRAM mismatch: %lx <> %lx\r\n", *p, i);
			j++;
		}
		p++;
		if (j > 10)
			break;
	}
	if ( !j )
		print_log(UART_OUT, "*D: SDRAM match\r\n");

	/* test the last location */
	p = sdRAM;
	p += SDRAM_SIZE_WORDS - 1;

	*p = 0x12345678;
	print_log(UART_OUT, "*D: SDRAM loc: %lx = %lx\r\n", p, *p);
//#endif

	ITM_PrintString("hello from SWO\r\n");
	ITM_PrintChar('!');
	print_log(ITM_OUT, "\r\nHi!\r\n");

	return CMD_DEC_OK;
}

ECMD_DEC_Status CMD_udpip(TCMD_DEC_Results *res, EResultOut out)
{
	(void)out;
	unsigned long ip[4];
	unsigned long ipAddr;

	sscanf(res->str, "%lu.%lu.%lu.%lu", &ip[0], &ip[1], &ip[2], &ip[3]);
	ipAddr  = ip[0];
	ipAddr |= ip[1] << 8;
	ipAddr |= ip[2] << 16;
	ipAddr |= ip[3] << 24;

	netcmd_setUDPdest(ipAddr);

	return CMD_DEC_OK;
}

ECMD_DEC_Status CMD_udptest(TCMD_DEC_Results *res, EResultOut out)
{
	//(void)out;

	int i;
	unsigned long startTick, endTick;
	char *p;

	startTick = OSA_TimeGetMsec();
	//test: let it run for 10 ms (ticks)
	i = 0;

	p = (char *)MEM_PoolAlloc(MEM_POOL_SEG_BYTES);
	if ( !p )
		return CMD_DEC_OOMEM;

	/* this FAILS: we lose packets! (non-blocking send), just 85 from 537 generated packets! */
	do
	{
		/**
		 * ATTENTION: the maximum packet length we can send is 2952 bytes!
		 * everything larger is not received by host!
		 */
		if (res->val[0] == 0)
			netcmd_sendUDP(0, p, 512, 1);
		else
			netcmd_sendUDP(0, p, res->val[0], 1);
		i++;
		//wait 1 ms
		OSA_TimeDelay(1);
		endTick = OSA_TimeGetMsec();
	} while (endTick < (startTick + 10));

	print_log(out, (const char *)"time diff: %d [ms] | %d times\r\n", (int)(endTick - startTick), i);

	MEM_PoolFree(p);

	return CMD_DEC_OK;
}

ECMD_DEC_Status CMD_tasks(TCMD_DEC_Results *res, EResultOut out)
{
	(void)res;

	uint8_t *buf;
	buf = MEM_PoolAlloc(MEM_POOL_SEG_BYTES);

	if (buf)
	{
		*buf = '\0';
		vTaskList(buf);
		UART_Send(buf, (int)strlen((const char *)buf), out);

		MEM_PoolFree(buf);
		return CMD_DEC_OK;
	}
	else
		return CMD_DEC_OOMEM;
}
