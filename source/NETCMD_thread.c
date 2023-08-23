/*
 * NETCMD_thread.c
 *
 *  Created on: Dec 11, 2016
 *      Author: Torsten
 */

/* Includes ------------------------------------------------------------------*/
#include <string.h>
#include <stdio.h>

#include "lwip/opt.h"
#include "lwip/arch.h"
#include "lwip/api.h"
#include "lwip/err.h"

#include "fsl_common.h"
#include "fsl_os_abstraction.h"
#include "task.h"

#include "NETCMD_thread.h"
#include "MEM_Pool.h"
#include "SPI.h"
////#include "I2C1_user.h"
////#include "UART.h"
#include "VCP_UART.h"
#include "cmd_dec.h"
////#include "SYS_config.h"
////#include "syserr.h"

////#define MAX_NUM_BYTES	255			/* for binary SPI and I2C */

//PCBs for all the UDP ports we need (open some or all for all potential audio streams)
struct udp_pcb *pcb;
BaseType_t REST_APIThreadID;

static int sEventFlag = 0;				//bit 0 or 1 set if waiting for event

int convertURL(char *out, const char *in, int maxLenOut);

static void NETCMDThread(void *pvParameters);
static int netcmd_server_serve(struct netconn *conn);
static int netcmd_spiTransaction(struct netconn *conn, int spiID, char *buf);
static int netcmd_spiRawTransaction(struct netconn *conn, int spiID, char *buf);
static int netcmd_spiTransactionFPGA_a(struct netconn *conn, char *buf);
static int netcmd_spiTransactionFPGA_A(struct netconn *conn, char *buf);
static int netcmd_i2cReadRawTransaction(struct netconn *conn, char *buf);
static int netcmd_i2cWriteRawTransaction(struct netconn *conn, char *buf);
static int netcmd_UARTtx(struct netconn *conn, char *buf);
static int netcmd_UARTrx(struct netconn *conn, char *buf);
static int netcmd_UARTflush(struct netconn *conn, char *buf);
static int netcmd_UARTtxASCII(struct netconn *conn, char *buf);
static int netcmd_UARTrxASCII(struct netconn *conn, char *buf);
static int netcmd_WaitForINT(int num);


/* for FPGA ADC value SPI transaction */
static unsigned char *FPGA_SPI_buf = NULL;
static int FPGA_SPI_idx = 0;

void SendPromptHTTPD(void)
{
	/* send prompt with End of Text EOT */
	UART_Send((const uint8_t *)"\003", 1, HTTPD_OUT);
}

void netcmd_taskCreate(void)
{
	REST_APIThreadID = xTaskCreate(NETCMDThread, "NETCMD", configMINIMAL_STACK_SIZE * 2, NULL, 2, NULL);
}

/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

/**
  * @brief serve tcp connection
  * @param conn: pointer on connection structure
  * @retval None
  */
static int netcmd_server_serve(struct netconn *conn)
{
  struct netbuf *inbuf;
  err_t recv_err;
  char *buf;
  uint16_t buflen, totalLen, expLen;
  //ATT: have an aligned buffer for SPI DMA !
  char *inpBuf;
  int retClose;

  retClose = 0;				/* dont't close at the end */

  //allocate buffer for Network packet
  inpBuf = MEM_PoolAlloc(NET_CMD_BUF_SIZE);

  if (inpBuf)
  {
	  /* Read the data from the port, blocking if nothing yet there.
     	 We assume the request (the part we care about) is in one netbuf */
	  recv_err = netconn_recv(conn, &inbuf);
	  expLen = 0;
	  totalLen = 0;

	  if (recv_err == ERR_OK)
	  {
		  /*
		   * support fragmented TCP/IP packets - ATT: we use our Length field
		   * for binary commands
		   */

		  netbuf_data(inbuf, (void**)&buf, &buflen);
		  /* copy data to local buffer */
		  memcpy(&inpBuf[totalLen], buf, buflen);
		  /* Delete the buffer (netconn_recv gives us ownership,
		     so we have to make sure to deallocate the buffer) */
		  netbuf_delete(inbuf);

		  ////if (gCFGparams.Debug & DBG_NETWORK)
		  {
			  int i;

			  print_log(DEBUG_OUT, "\r\n*N: buflen: %d\r\n", buflen);
			  print_log(DEBUG_OUT, "*N: buf[]:");
			  for (i = 0; (i < buflen) && (i < 16); i++)
				  print_log(DEBUG_OUT, " %2.2x", inpBuf[totalLen + i]);
			  print_log(DEBUG_OUT, "\r\n");
		  }

		  totalLen += buflen;

		  if (strncmp(inpBuf, "GET /", 5) == 0)
		  {
			  switch (inpBuf[5])
			  {
			  /* handle all binary commands for expecting length and more than one packet */
			  case '1' :
			  case '2' :
			  case 'd' :
			  case 'c' :
			  case 'a' :
			  case 'A' :
			  case 'i' :
			  case 'I' :
			  case 'U' :
			  case 'u' :
			  	  {
			  		  expLen  = (size_t)inpBuf[6] << 8;		//high part first
			  		  expLen |= (size_t)inpBuf[7];			//low part second - BIG ENDIAN

			  		  ////if (gCFGparams.Debug & DBG_NETWORK)
			  		  {
			  			  print_log(DEBUG_OUT, "*N: expLen req. %d\r\n", expLen);
			  		  }

			  		  //we have "GET /x<HI><LO>" - correct for it: +8 characters (to match with totalLen)
			  		  expLen += 8;

			  		  ////if (gCFGparams.Debug & DBG_NETWORK)
			  		  {
			  			  print_log(DEBUG_OUT, "*N: expLen is   %d\r\n", expLen);	//keep spaces for nice reading
			  		  }

			  		  /* just to make sure: MEM_Pool segment size minus GET /x and 2 bytes len */
			  		  if (expLen > (NET_CMD_BUF_SIZE -6 -2))
			  		  {
			  			  /* overwrite the max. length for SPI transaction - Big Endian */
			  			  inpBuf[6] = (char)((expLen -6 -2) >> 8);			//high part
			  			  inpBuf[7] = (char)(expLen -6 -2);					//low part
			  		  }
			      }
			  } //end switch
		  } //end if

		  /* ATT: only for binary commands! - we use expLen as indication */
		  while ((expLen) > totalLen)					//we have GET /x<HI><LO>, plus 8 bytes
		  {
			  recv_err = netconn_recv(conn, &inbuf);
			  if (recv_err != ERR_OK)
			  {
				  retClose = 1;
				  ////SYS_SetError(SYS_ERR_NETWORK);
				  goto NET_ERROR;								//network error, e.g. Python closed with hanging socket
			  }
			  netbuf_data(inbuf, (void**)&buf, &buflen);
			  //can buf also be 0? then buflen should be 0

			  /* copy data to local buffer */
			  /* make sure not to write beyond MEM_Pool segment size */
			  if ((totalLen + buflen) > (NET_CMD_BUF_SIZE))
				  buflen = NET_CMD_BUF_SIZE - totalLen;

			  /* copy data to local buffer */
			  memcpy(&inpBuf[totalLen], buf, buflen);
			  netbuf_delete(inbuf);			//delete inbuf
			  totalLen += buflen;

			  ////if (gCFGparams.Debug & DBG_NETWORK)
			  {
				  print_log(DEBUG_OUT, "*N: FRAG: len %d | total %d\r\n", expLen, totalLen);
			  }
		  }

		  ////if (gCFGparams.Debug & DBG_NETWORK)
		  {
			  print_log(DEBUG_OUT, "*N: len %d | total %d\r\n", expLen, totalLen);
		  }

		  if ((totalLen >=5) && (strncmp(inpBuf, "GET /", 5) == 0))
		  {
			  switch(inpBuf[5])
			  {
			  //ATT: make sure the bytes for SPI transactions are word aligned for DMA !
			  case '0' :
			  {
				  /* SPI transaction - in binary mode - to FPGA SPI */
				  netcmd_spiRawTransaction(conn, 1, &inpBuf[6]);
			  }
			  break;
			  case '1' :
			  {
				  /* SPI transaction - in binary mode - PRI */
				  netcmd_spiRawTransaction(conn, 2, &inpBuf[6]);
			  }
			  break;
			  case '2' :
			  {
				  /* SPI transaction - in binary mode - AUX */
				  netcmd_spiRawTransaction(conn, 3, &inpBuf[6]);
			  }
			  break;
			  case 'd' :
			  // /3 and /4 would be the other board
			  {
				  /* SPI transaction - in binary mode - DUMMY */
				  netcmd_spiRawTransaction(conn, 6, &inpBuf[6]);
			  }
			  break;
			  case 'a' :
			  {
				  /* SPI transaction - in binary mode - 1st FPGA ADC load */
				  netcmd_spiTransactionFPGA_a(conn, &inpBuf[6]);
			  }
			  break;
			  case 'A' :
			  {
				  /* SPI transaction - in binary mode - 2nd FPGA ADC load */
				  netcmd_spiTransactionFPGA_A(conn, &inpBuf[6]);
			  }
			  break;
			  case 'i' :
			  {
				  /* I2C Read transaction - in binary mode */
				  netcmd_i2cReadRawTransaction(conn, &inpBuf[6]);
			  }
			  break;
			  case 'I' :
			  {
				  /* I2C Write transaction - in binary mode */
				  netcmd_i2cWriteRawTransaction(conn, &inpBuf[6]);
			  }
			  break;
			  case '5' :
			  {
				  /* SPI2 transaction - in ASCII mode */
				  netcmd_spiTransaction(conn, 1, &inpBuf[6]);
			  }
			  break;
			  case '6' :
			  {
				  /* SPI4 transaction in ASCII mode */
				  netcmd_spiTransaction(conn, 2, &inpBuf[6]);
			  }
			  break;
			  case '7' :
			  {
				  /* SPI4 transaction in ASCII mode */
				  netcmd_spiTransaction(conn, 3, &inpBuf[6]);
			  }
			  break;
			  // 3+ and 4+ would be other board
			  case 'D' :
			  {
				  /* SPI4 transaction in ASCII mode - Dummy */
				  //netcmd_spiTransaction(conn, 6, &inpBuf[10]);
				  netcmd_spiTransaction(conn, 6, &inpBuf[6]);
			  }
			  break;

			  /*
			   * TODO: I2C ASCII Read + Write
			   */

			  /*
			   * ATT: because of this /U we cannot use any 'U...' as string command!!!!
			   * if you send UT -> it will hang!!!!
			   */
			  case 'U' :
			  {
				  /* UART Transmit transaction in binary mode */
				  netcmd_UARTtx(conn, &inpBuf[6]);
			  }
			  break;
			  case 'u' :
			  {
				  /* UART Reception transaction in binary mode */
				  netcmd_UARTrx(conn, &inpBuf[6]);
			  }
			  break;
			  case 'F' :
			  {
				  /* UART receiver flush - empty receiver inpBuffer */
				  netcmd_UARTflush(conn, &inpBuf[6]);
			  }
			  break;
			  case 'T' :
			  {
				  /* UART Transmit transaction in ASCII mode */
				  netcmd_UARTtxASCII(conn, &inpBuf[7]);
			  }
			  break;
			  case 'R' :
			  {
				  /* UART Transmit transaction in ASCII mode */
				  netcmd_UARTrxASCII(conn, &inpBuf[6]);
			  }
			  break;

			  /*
			   * TODO: GPIO, config, in, out
			   */

			  case 't' :
			  {
				  /* wait for INT0 */
				  if (netcmd_WaitForINT(0))
					  netconn_write(conn, "I0\003", 3, NETCONN_NOCOPY);
				  else
					  netconn_write(conn, "ER/003", 3, NETCONN_NOCOPY);
			  }
			  break;
			  case 'W' :
			  {
				  /* wait for INT1 */
				  if (netcmd_WaitForINT(1))
					  netconn_write(conn, "I1\003", 3, NETCONN_NOCOPY);
				  else
					  netconn_write(conn, "ER\003", 3, NETCONN_NOCOPY);
			  }
			  break;
			  case 'w' :
			  {
				  /* wait for INT2 */
				  if (netcmd_WaitForINT(2))
					  netconn_write(conn, "I2\003", 3, NETCONN_NOCOPY);
				  else
					  netconn_write(conn, "ER\003", 3, NETCONN_NOCOPY);
			  }
			  break;
			  case 'L' :
			  {
				  /* get the last UART log messages and send back to client */
				  char *resStr;
				  unsigned long len;
				  resStr = HTTP_GetOutBuffer(&len);
				  if (len == 0)
					  /* just to send anything back for TCP */
					  netconn_write(conn, "*\003", 2, NETCONN_NOCOPY);
				  else
					  netconn_write(conn, resStr, len, NETCONN_NOCOPY);
			  }
			  break;
			  case 'X' :
			  {
				  /* close the socket, break the endless loop on established TCP connection */
				  netconn_write(conn, "X\003", 2, NETCONN_NOCOPY);
				  retClose = 1;
			  }
			  break;
			  case 'x' :	//send response, close socket, do "fwreset"
			  {
				  extern ECMD_DEC_Status CMD_fwreset(TCMD_DEC_Results *res, EResultOut out);
				  netconn_write(conn, "x\003", 2, NETCONN_NOCOPY);
				  retClose = 1;
				  //but we have to close the socket here, "fwreset" does not come back
				  //no idea if still sent or closing socket kills the pending transfer?

				  netconn_close(conn);
				  /* delete connection */
				  netconn_delete(conn);

				  //wait a bit to let the network do the traffic
				  OSA_TimeDelay(500);

				  CMD_fwreset(NULL, DEBUG_OUT);
				  //we should not come back here
			  }
			  break;
			  case 'C' :
			  {
				  /*
				   * if we send from web browser: space is %20, ; remains ;
				   */
				  char *cmdStr = MEM_PoolAlloc(MEM_POOL_SEG_BYTES);
				  char *resStr;
				  if (cmdStr)
				  {
					  unsigned long len;

					  /* terminate ASCII string */
					  inpBuf[totalLen] = '\0';

					  /* terminate the string: search for HTTP and set NUL, when sent from Web Browser */
					  resStr = strstr(&inpBuf[6], " HTTP");
					  if (resStr)
					  {
						  *resStr = '\0';
					  }
					  convertURL(cmdStr, &inpBuf[6], MEM_POOL_SEG_BYTES);
					  /* call the command interpreter */
					  HTTP_OutBufferClear();

					  print_log(DEBUG_OUT, "*D: %d |%s|\r\n", strlen(cmdStr), cmdStr);

					  CMD_DEC_execute(cmdStr, HTTPD_OUT);
					  /* put a prompt into the output buffer */
					  SendPromptHTTPD();		//do not send to a web browser!

					  /* get the output of command decoder and fill into form */
					  resStr = HTTP_GetOutBuffer(&len);
					  if (len)
					  {
						  ////SCB_CleanDCache_by_Addr((uint32_t *)resStr, ((len+31)/32)*32);	//resStr should be 32word aligned
						  if (netconn_write(conn, resStr, len, NETCONN_NOCOPY) != ERR_OK)
						  {
							  ////SYS_SetError(SYS_ERR_NETWORK);
							  print_log(DEBUG_OUT, "*D: netconn_write FAIL\r\n");
						  }

						  /* do not send anything else here afterwards! */
					  }
					  MEM_PoolFree(cmdStr);
				  }
				  else
				  {
					  netconn_write(conn, "*E: out of memory\003", 18, NETCONN_NOCOPY);
				  }
			  }
			  break;
			  case 'c' :
			  {
				  //large ASCII command, in segments, with Binary Length
				  /*
				   * if we send from web browser: space is %20, ; remains ;
				   */
				  char *cmdStr = MEM_PoolAlloc(NET_CMD_BUF_SIZE);
				  char *resStr;
				  if (cmdStr)
				  {
					  unsigned long len;

					  /* terminate ASCII string */
					  inpBuf[totalLen] = '\0';
#if 1
					  /* should not happen, not possible from web browser */
					  /* terminate the string: search for HTTP and set NUL */
					  resStr = strstr(&inpBuf[8], " HTTP");
					  if (resStr)
						  *resStr = '\0';
					  convertURL(cmdStr, &inpBuf[8], NET_CMD_BUF_SIZE);
#endif
					  /* call the command interpreter */
					  HTTP_OutBufferClear();
					  //print_log(DEBUG_OUT, ">%s<\r\n", cmdStr);
					  CMD_DEC_execute(cmdStr, HTTPD_OUT);
					  /* put a prompt into the output buffer */
					  SendPromptHTTPD();

					  /* get the output of command decoder and fill into form */
					  resStr = HTTP_GetOutBuffer(&len);
					  if (len)
					  {
						  ////SCB_CleanDCache_by_Addr((uint32_t *)resStr, ((len+31)/32)*32);
						  if (netconn_write(conn, resStr, len, NETCONN_NOCOPY) != ERR_OK)
						  {
							  ////	SYS_SetError(SYS_ERR_NETWORK);
							  print_log(DEBUG_OUT, "*D: netconn_write FAIL\r\n");
						  }

						  /* do not send anything else here afterwards! */
					  }
					  MEM_PoolFree(cmdStr);
				  }
				  else
				  {
					  ////SYS_SetError(SYS_ERR_OUT_OF_MEM);
					  netconn_write(conn, "*E: out of memory\003", 18, NETCONN_NOCOPY);
				  }
			  }
			  break;
			  default :
				  if (netconn_write(conn, "OK\003", 3, NETCONN_NOCOPY) != ERR_OK)
				  {
					  ////	  SYS_SetError(SYS_ERR_NETWORK);
				  }
			  } //end switch

			  taskYIELD();
    	  } //end "GET /"
	  } //recv error
	  else
	  {
		  retClose = 1;
		  ////SYS_SetError(SYS_ERR_NETCLOSED);
		  ////if (gCFGparams.Debug & DBG_NETWORK)
		  {
			  print_log(DEBUG_OUT, "\r\n*N: recv error - connection closed? | recv_err: %x\r\n", recv_err);
		  }
	  }
  }
  else
  {
	  //out of memory
	  print_log(DEBUG_OUT, "*D: netcmd MEMPool error\r\n");
	  retClose = 1;
  }
NET_ERROR:
  if (inpBuf)
	  MEM_PoolFree(inpBuf);

  return retClose;
}

static void NETCMDThread(void *pvParameters)
{
	  (void)pvParameters;

	  struct netconn *conn, *newconn;
	  err_t err, accept_err;

	  /* start Netbios to resolve host name */

	  print_log(DEBUG_OUT, "*D: NETCMDThread started...\r\n");

	  /* Create a new TCP connection handle */
	  conn = netconn_new(NETCONN_TCP);

	  if (conn != NULL)
	  {
		  print_log(DEBUG_OUT, "*D: netconn_new OK\r\n");
	    /* Bind to port 8080 (HTTP) with default IP address */
	    err = netconn_bind(conn, IP4_ADDR_ANY, 8080);

	    if (err == ERR_OK)
	    {
	    	print_log(DEBUG_OUT, "*D: netconn_bind OK\r\n");
	      /* Put the connection into LISTEN state */
	      netconn_listen(conn);

	      while(1)
	      {
	        /* accept any incoming connection */
	        accept_err = netconn_accept(conn, &newconn);
	        if(accept_err == ERR_OK)
	        {
	        	print_log(DEBUG_OUT, "*D: netconn_accept OK\r\n");
	        	do
	        	{
	        		/* serve connection */
	        		if (netcmd_server_serve(newconn))
	        			break;
	        		taskYIELD();
	        	} ////while (gCFGparams.NET_DHCP & CFG_NET_LET_TCP_OPEN);
	        	while (1);

	        	/* Close the connection (Web Browser closes also) */
	        	netconn_close(newconn);

	        	/* delete connection */
	        	netconn_delete(newconn);

	        	taskYIELD();
	        }
	        else
	        {
	        	////	SYS_SetError(SYS_ERR_NETWORK);
	        	print_log(DEBUG_OUT, "*D: netconn_accept FAILED\r\n");
	        }
	      }
	    }
	    else
	    {
	    	print_log(DEBUG_OUT, "*D: netconn_bind FAIL\r\n");
	    }
	  }
	  else
	  {
		  print_log(DEBUG_OUT, "*D: netconn_new FAIL\r\n");
	  }

	  /* all failed - kill ourself */
	  print_log(DEBUG_OUT, "*D: NETCMDThread killed\r\n");
	  vTaskDelete(NULL);
}

/*
 * convert URL into ASCII string for command interpreter:
 * SPACE -> + or %20
 * ; -> %3B
 * + -> %2B
 * # -> %23
 * " -> %22
 * LF -> %0A
 */
int convertURL(char *out, const char *in, int maxLenOut)
{
	int state = 0;
	int i = 0;
	char hexVal[3];

	while ((i < (maxLenOut - 1)) && (*in))
	{
		if (state == 0)
		{
			if (*in == '+')
			{
				*out++ = ' ';
				in++;
				i++;
				continue;
			}
			if (*in == '%')
			{
				in++;
				state = 1;
				continue;
			}

			/* all other characters take it as it is */
			*out++ = *in++;
			i++;
			continue;
		}

		if (state == 1)
		{
			hexVal[0] = *in++;
			state = 2;
			continue;
		}

		if (state == 2)
		{
			hexVal[1] = *in++;
			hexVal[2] = '\0';
			/* convert hex value (in ASCII, w/o 0x) into ASCII character value */
			*out++ = (char)strtol(hexVal, NULL, 16);
			i++;
			state = 0;
		}
	}

	/* NUL for end of string in out */
	*out = '\0';

	/* return the length of out string */
	return i;
}

static int netcmd_spiTransaction(struct netconn *conn, int spiID, char *buf)
{
	int res = 1;
#if 0
	char *cPtr;
	uint8_t *SPItxBuf;				//converted from ASCII to binary
	char *txBuf;					//for hex ASCII with space but without 0x: 3 characters each + 1 for NUL
	int num, numVals, txBufLen;
	int i, maxNumBytes;

	maxNumBytes = NET_CMD_BUF_SIZE / 3 - 1;
	SPItxBuf = (uint8_t *)MEM_PoolAlloc(NET_CMD_BUF_SIZE);
	if ( ! SPItxBuf)
		return 0;				//Out of memory
	txBuf = (char *)MEM_PoolAlloc(NET_CMD_BUF_SIZE);
	if ( ! txBuf)
	{
		MEM_PoolFree(SPItxBuf);
		return 0;				//Out of memory
	}

	/*
	 * substitute the '+' by spaces (they stand for spaces in URL)
	 * We send as: SPI0+val+val+val+... use '+' to separate in URL
	 */
	cPtr = buf;
	numVals = 0;

	/* now scan the values (assuming as bytes)*/
	for (i = 0; i < maxNumBytes; i++)
	{
		num = sscanf(cPtr, "%i", &res);
		if (num > 0)
		{
			*(SPItxBuf + i) = (uint8_t)res;
			numVals++;
			//search for the next
			while(*cPtr && (*cPtr != '+'))
				cPtr++;
			if (*cPtr == '+')
				cPtr++;
		}
		else
			break;
	}

	res = 0;
	if (numVals > 0)
	{
		uint8_t *SPIrxBuf;

		SPIrxBuf = MEM_PoolAlloc(NET_CMD_BUF_SIZE);
		if ( ! SPIrxBuf)
		{
			MEM_PoolFree(SPItxBuf);
			MEM_PoolFree(txBuf);
			return 0;
		}

		SPI_TransmitReceive(spiID, SPItxBuf, SPIrxBuf, (uint16_t)numVals);

		/* generate result */
		cPtr = txBuf;
		txBufLen = 0;
		for (i = 0; i < numVals; i++)
		{
			/* all in Hex! without 0x to save bandwidth */
			sprintf(cPtr, "%2.2X ", SPIrxBuf[i]);
			cPtr += 3;
			txBufLen += 3;
		}

		res = 1;
		netconn_write(conn, txBuf, (size_t)txBufLen, NETCONN_NOCOPY);

		/* ATT: we release the buffers - do we need COPY for the network stack? */
		MEM_PoolFree(SPItxBuf);
		MEM_PoolFree(SPIrxBuf);
		MEM_PoolFree(txBuf);
	}
#endif
	return res;
}

static int netcmd_spiRawTransaction(struct netconn *conn, int spiID, char *buf)
{
#if 0
	/*
	 * This is in binary, a sequence of binary bytes - the first two bytes specify the length in bytes
	 */
	//use MEMPool
	//ATT: we do not know how large the SPIrxBuf should be - we do not specify the length
	//of the *but which is the Tx buffer
	//ATT: SPIrxBuf must be +2 bytes larger, we insert LEN as 16bit word at the first location
	uint8_t *SPIrxBuf;
	size_t numBytes;

	SPIrxBuf = (uint8_t *)buf;

	numBytes  = (size_t)*buf++ << 8;		//high part first
	numBytes |= (size_t)*buf++;				//low part second - BIG ENDIAN

	////SPIrxBuf = MEM_PoolAlloc(numBytes + 2);
	////if ( ! SPIrxBuf)
	////	return 0;

	SPI_TransmitReceive(spiID, (uint8_t *)buf, SPIrxBuf + 2, (uint16_t)numBytes);
	SPIrxBuf[0] = (uint8_t)(numBytes >> 8);			//Hi, Big Endian
	SPIrxBuf[1] = (uint8_t)(numBytes & 0xFF);		//Lo
	netconn_write(conn, SPIrxBuf, numBytes + 2, NETCONN_NOCOPY);

	/* we had to use NETCONN_COPY, the buffer is released to early, potentially */

	////MEM_PoolFree(SPIrxBuf);
#endif
	return 1;
}

static int netcmd_i2cReadRawTransaction(struct netconn *conn, char *buf)
{
#if 0
	/*
	 * This is in binary, a sequence of binary bytes - the first two bytes specify the length in bytes
	 */
	//use MEMPool
	uint8_t *I2CrxBuf;
	int numBytes;
	uint16_t addr;
	int ret;
	uint16_t numRegs;

	numBytes  = *buf++ << 8;		//high part first
	numBytes |= *buf++;				//low part second - BIG ENDIAN

	I2CrxBuf = MEM_PoolAlloc(numBytes + 2);
	if ( ! I2CrxBuf)
		return 0;

	if (numBytes > 2)
	{
		//numBytes is length of entire transaction
		addr  = (uint16_t)((uint16_t)*buf++ << 8);	//Big Endian
		addr |= (uint16_t)*buf++;					//slave addr: potentially as 10bit

		numRegs = *buf;								//max. 255 possible

		//do I2C read now
		ret = I2C1_Read(addr, I2CrxBuf + 2, numRegs);
		if (ret == 0)
			ret = numRegs;
		else
			ret = 0;
	}
	else
		ret = 0;

	//send response
	*(I2CrxBuf + 0) = (uint8_t)(ret >> 8);		//high length
	*(I2CrxBuf + 1) = (uint8_t)(ret & 0xFF);	//low length, - BIG ENDIAN
	netconn_write(conn, I2CrxBuf, (size_t)ret + 2, NETCONN_NOCOPY);

	MEM_PoolFree(I2CrxBuf);
#endif
	return 1;
}

static int netcmd_i2cWriteRawTransaction(struct netconn *conn, char *buf)
{
#if 0
	/*
	 * This is in binary, a sequence of binary bytes - the first two bytes specify the length in bytes
	 */
	/* remark: we could reuse buf for the response - but we need a pointer variable, same size */
	uint8_t resBuf[4];
	int numBytes;
	uint16_t addr;
	int numRegs, ret;

	numBytes  = *buf++ << 8;		//Big Endian, Hi
	numBytes |= *buf++;				//Lo

	if (numBytes > 2)
	{
		//numBytes is length of entire transaction
		addr  = (uint16_t)((uint16_t)*buf++ << 8);
		addr |= (uint16_t)*buf++;				//slave addr: potentially as 10bit - BIG ENDIAN

		//buf points to the bytes to write

		numRegs = (uint8_t)(numBytes - 2);

		ret = I2C1_Write(addr, (uint8_t *)buf, (uint16_t)numRegs);
		if (ret == 0)
			ret = numRegs;
		else
			ret = 0;
	}
	else
		ret = 0;

	//send response as: MSG_LEN (2 bytes) | VALUE_NUM_WRITTEN
	resBuf[0] = 0;
	resBuf[1] = 2;				//2 bytes following for written number of bytes, Big Endian
	resBuf[2] = (uint8_t)((uint32_t)ret >> 8);					//high length
	resBuf[3] = (uint8_t)((uint32_t)ret & (uint32_t)0xFF);		//low length - BIG ENDIAN
	netconn_write(conn, resBuf, 4, NETCONN_NOCOPY);		//XXXX
#endif
	return 1;
}

static int netcmd_UARTtx(struct netconn *conn, char *buf)
{
#if 0
	/* binary mode: it starts with number of bytes to send */
	/* Remark: we could reuse buf for the response */
	int numBytes;
	uint8_t resBuf[4];

	numBytes  = *buf++ << 8;
	numBytes |= *buf++;

	UART4_Tx((const uint8_t *)buf, numBytes);

	/* generate a non-empty response message */
	//send response as: MSG_LEN (2 bytes) | VALUE_NUM_WRITTEN
	resBuf[0] = 0;
	resBuf[1] = 2;					//2 bytes following for written number of bytes, Big Endian
	resBuf[2] = (uint8_t)((uint32_t)numBytes >> 8);				//high part
	resBuf[3] = (uint8_t)((uint32_t)numBytes & (uint32_t)0xFF);	//low part - BIG ENDIAN
	netconn_write(conn, resBuf, 4, NETCONN_NOCOPY);
#endif
	return 1;
}

static int netcmd_UARTrx(struct netconn *conn, char *buf)
{
	(void)buf;
#if 0
	/* binary mode: no parameters, the return is with number of bytes received */
	int l;
	uint8_t *resStr;

	resStr = MEM_PoolAlloc(MEM_POOL_SEG_SIZE);
	if (resStr)
	{
		l = UART4_GetRx(resStr + 2);
		/* TODO: similar binary response! */
		*(resStr + 0) = (uint8_t)(l >> 8);		//length - Big Endian
		*(resStr + 1) = (uint8_t)(l & 0xFF);
		netconn_write(conn, resStr, (size_t)l + 2, NETCONN_NOCOPY);
		MEM_PoolFree(resStr);
	}
	else
		/* similar binary response: length is 0 */
		netconn_write(conn, "\000\000", 2, NETCONN_NOCOPY);
#endif
	return 1;
}

static int netcmd_UARTflush(struct netconn *conn, char *buf)
{
	(void)buf;
#if 0
	UART4_Flush();

	/* generate a non-empty response message */
	netconn_write(conn, "OK\003", 3, NETCONN_NOCOPY);
#endif
	return 1;
}

static int netcmd_UARTtxASCII(struct netconn *conn, char *buf)
{
#if 0
	char *txStr;
	unsigned int numBytes;

	txStr = strstr(buf, " HTTP");
	if (txStr)
		*txStr = '\0';

	txStr = MEM_PoolAlloc(MEM_POOL_SEG_SIZE);
	if (txStr)
	{
		numBytes = convertURL(txStr, buf, MEM_POOL_SEG_SIZE);
		UART4_Tx((const uint8_t *)txStr, numBytes);

		MEM_PoolFree(txStr);
	}
	netconn_write(conn, "OK\003", 3, NETCONN_NOCOPY);
#endif
	return 1;
}

static int netcmd_UARTrxASCII(struct netconn *conn, char *buf)
{
#if 0
	(void)buf;
	char *rxStr;
	int l;

	rxStr = MEM_PoolAlloc(MEM_POOL_SEG_SIZE);
	if (rxStr)
	{
		l = UART4_GetRx((uint8_t *)rxStr);
		netconn_write(conn, rxStr, (size_t)l, NETCONN_NOCOPY);
		MEM_PoolFree(rxStr);
	}
	netconn_write(conn, "\r\n\003", 3, NETCONN_NOCOPY);
#endif
	return 1;
}

static int netcmd_WaitForINT(int num)
{
	////osEvent INTEvent;

	switch (num)
	{
	case 2:
		sEventFlag |= 0x4;
		break;
	case 1:
		sEventFlag |= 0x2;
		break;
	default:
		sEventFlag |= 0x1;
	}

	////INTEvent = osSignalWait(EVENT_INT, INT_EVENT_TIMEOUT);

	////if (INTEvent.value.signals == EVENT_INT)
	////	return 1;
	////else
		return 0;
}

void netcmd_SendINTEvent(int num)
{
	switch (num)
	{
	case 2:
		if (sEventFlag & 0x4)
		{
			sEventFlag &= ~0x4;							/* clear flag */
			////osSignalSet(REST_APIThreadID, EVENT_INT);	/* send event */
		}
		break;
	case 1:
		if (sEventFlag & 0x2)
		{
			sEventFlag &= ~0x2;							/* clear flag */
			////osSignalSet(REST_APIThreadID, EVENT_INT);	/* send event */
		}
		break;
	default:
		if (sEventFlag & 0x1)
		{
			sEventFlag &= ~0x1;							/* clear flag */
			////osSignalSet(REST_APIThreadID, EVENT_INT);	/* send event */
		}
	}
}

static int netcmd_spiTransactionFPGA_a(struct netconn *conn, char *buf)
{
#if 0
	//first packet with ADC values for FPGA ADC
	//format:  LEN_HI LEN_LO BLKADDR EncodedVal ... EncodedVal
	//- it allocates buffer for a 32KB transaction and keeps it
	//- it waits for "GET /A" to finish and fire transaction
	//- a new "GET /a" will overwrite buffer
	//- EncodedVal : 3x 5bit values packed in a 16bit word
	//- fix for FPGA ADC SPI (SPI channel 1)

	int len;
	/*
	 * ATTENTION: is it DMA aligned with using BLKADDR??
	 */

	if ( ! FPGA_SPI_buf)
	{
		FPGA_SPI_buf = MEM_PoolAlloc(NET_CMD_BUF_SIZE);
		if ( ! FPGA_SPI_buf)
		{
			netconn_write(conn, "ER\003", 3, NETCONN_NOCOPY);
			return 0;				//ERROR: out of memory
		}
	}

	//if buffer is already allocated - a "GET /a" was already there - overwrite all
	len  = buf[0] << 8;					//BIG ENDIAN length
	len |= buf[1];

	memcpy(FPGA_SPI_buf, &buf[2], len);	//without the length itself, it has BLKADDR_LO and _HI as first
	FPGA_SPI_idx = len;					//index to second part

	netconn_write(conn, "OK\003", 3, NETCONN_NOCOPY);
#endif
	return 1;
}

static int netcmd_spiTransactionFPGA_A(struct netconn *conn, char *buf)
{
	int err = 1;
#if 0
#define SPI_CHUNK_LEN		(2*16*1024)		//16KWords chunks, 16bit words

	//second packet with ADC values for FPGA ADC
	//format:  LEN_HI LEN_LO EncodedVal ... EncodedVal
	//- it appends it to a _a packet - must be there before (otherwise no buffers)
	//- it finishes all and send the WBLK with 32K words to FPGA ADC
	//- before, it expands the value bytes encoded as 2x5bit ADC in 16bit word

	int len, i;
	uint16_t blkAddr;
	uint8_t *spiTx, *spiRx;			/* buffer for SPI transaction */

	err = 1;						//0 is ER, no success, 1 is OK

	//check if we have already allocated the buffer with "GET /a"
	if ( ! FPGA_SPI_buf)
	{
		netconn_write(conn, "ER\003", 3, NETCONN_NOCOPY);
		return 0;					//ERROR
	}

	//place the next after the first, finish and fire the SPI transaction,
	//release the buffer
	len  = buf[0] << 8;					//BIG ENDIAN length
	len |= buf[1];						//ATT: just 1/2 due to 2x5bit in 16bit

	memcpy(&FPGA_SPI_buf[FPGA_SPI_idx], &buf[2], len);
	FPGA_SPI_idx += len;				//the total length, still with BLKADDR

	//Now we have: BLKADDR_LO and _HI (Little Endian) and all the 2*5bit values
	//extract the values and create a SPI WBLK with 32KB (16KWord) values

	//do the SPI transaction
	blkAddr  = FPGA_SPI_buf[0];			//little endian
	blkAddr |= FPGA_SPI_buf[1];

	if (gCFGparams.Debug & DBG_NETWORK)
		print_log(DEBUG_OUT, "*N: ADC: len: %d | IDX: 0x%2.2x | buf[]: %2.2x %2.2x %2.2x %2.2x\r\n", FPGA_SPI_idx-2, blkAddr, FPGA_SPI_buf[2], FPGA_SPI_buf[3], FPGA_SPI_buf[4], FPGA_SPI_buf[5]);

	//we can check the len for reasonable size
	len = (FPGA_SPI_idx -2) * 2;						//we have 1/2 of total SPI size plus 2 bytes for blkAddr
	if (len > (SPI_CHUNK_LEN + 2))
		len = (SPI_CHUNK_LEN + 2);

	spiTx = MEM_PoolAlloc(len);
	if (spiTx)
	{
		*(spiTx + 0) = (uint8_t)0x01;			//WRBLK command
		*(spiTx + 1) = (uint8_t)blkAddr;		//IDX into command

		//expand the 2x5bit values
		for (i = 0; i < ((len -2) / 2); i++)
		{
			*(spiTx + 2 + 2*i) = FPGA_SPI_buf[2 + i];	//low part
			*(spiTx + 3 + 2*i) = 0;						//not used upper part
		}

		if (gCFGparams.Debug & DBG_VERBOSE)
		{
			print_log(DEBUG_OUT, (const char *)"*D: WBLK | IDX: 0x%02x bytes: %d\r\n", blkAddr, len);
		}

		/* EARLY buffer release: now we can release the TCP input buffers - does not work, we use later again! */
		MEM_PoolFree(FPGA_SPI_buf);
		FPGA_SPI_buf= NULL;

		//now allocate dummy buffer for Rx
		spiRx = MEM_PoolAlloc(len);
		if (spiRx)
		{
			SPI_TransmitReceive(1, spiTx, spiRx, len +2);		//just on FPGA ADC SPI: len is payload, +2 for command
			MEM_PoolFree(spiRx);
		}
		else
			err = 0;			//FAIL, error

		MEM_PoolFree(spiTx);
	}
	else
		err = 0;				//FAIL, error

	//release the buffer - already done by "early release"
	if (FPGA_SPI_buf)
		MEM_PoolFree(FPGA_SPI_buf);
	FPGA_SPI_buf= NULL;

	if (err)
		netconn_write(conn, "OK\003", 3, NETCONN_NOCOPY);
	else
		netconn_write(conn, "ER\003", 3, NETCONN_NOCOPY);
#endif
	return err;
}
