/*
 * UDP.c
 *
 *  Created on: Aug 17, 2023
 *      Author: tj925438
 */

#include "lwip/opt.h"
#include "lwip/arch.h"
#include "lwip/api.h"
#include "lwip/err.h"

//UDP sender
static struct netconn *connUDP;
static ip_addr_t udpIPdest;
static unsigned short udpPktNum[2] = {0,0};

void UDP_Init(void)
{
	/* Create UDP connection handle */
	connUDP = netconn_new(NETCONN_UDP);
	netconn_set_nonblocking(connUDP, 0);
}

void netcmd_resetUDPcnt(void)
{
	udpPktNum[0] = 0;
	udpPktNum[1] = 0;
	udpPktNum[2] = 0;
}

void netcmd_setUDPdest(unsigned long ipAddr)
{
	if (1)
		udpIPdest.addr = ipAddr;
	else
		udpIPdest.addr = 0;

	udpPktNum[0] = 0;
	udpPktNum[1] = 0;
}

int netcmd_sendUDP(int ch, char *b, unsigned int len, int flag)
{
	/**
	 * ATT: we append 1 character after the buffer! make sure it is 1 byte longer
	 */
	err_t err;
	struct netbuf *nb;
	if (udpIPdest.addr != 0)
	{
		if (flag)
			memset(b, udpPktNum[ch] & 0xFF, len);

		//set the packet sequence as two bytes as packet start
		*(b + 0) = (char)udpPktNum[ch];
		*(b + 1) = (char)(udpPktNum[ch] >> 8);	//Little Endian
		/* ATT: cache coherent with ETH_DMA!!! - or set via MPU write-through */
		SCB_CleanDCache_by_Addr((uint32_t *)b, (int32_t)(((len + 32)/32) * 32));

		//for testing: set end marker in packet
		////*(b + len) = ~udpPktNum[ch];

		nb = netbuf_new();
		netbuf_ref(nb, b, len);

		switch (ch)
		{
		case 0 : err = netconn_sendto(connUDP, nb, &udpIPdest, 8081); break;
		case 1 : err = netconn_sendto(connUDP, nb, &udpIPdest, 8082); break;
		default : err = ERR_VAL;
		}
		////if (err != ERR_OK)
		////	SYS_SetError(SYS_ERR_NETWORK);

		udpPktNum[ch]++;

		netbuf_delete(nb);

		return (int)err;
	}
	//not set IP destination, "closed"
	//TODO: a separate syserr code
	////SYS_SetError(SYS_ERR_UDPDEST);
	return -100;		/* special, to separate from err codes */
}

void netcmd_getUDPStat(unsigned short *cnt)
{
	*cnt++ = udpPktNum[0];			//port 8081
	*cnt++ = udpPktNum[1];			//port 8082
}
