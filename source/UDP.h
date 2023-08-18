/*
 * UDP.h
 *
 *  Created on: Aug 17, 2023
 *      Author: tj925438
 */

#ifndef UDP_H_
#define UDP_H_

void UDP_Init(void);
void netcmd_resetUDPcnt(void);
void netcmd_setUDPdest(unsigned long ipAddr);
int netcmd_sendUDP(int ch, char *b, unsigned int len, int flag);
void netcmd_getUDPStat(unsigned short *cnt);

#endif /* UDP_H_ */
