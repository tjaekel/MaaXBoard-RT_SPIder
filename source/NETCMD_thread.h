/*
 * NETCMD_thread.h
 *
 *  Created on: Dec 11, 2016
 *      Author: Torsten
 */

#ifndef NETCMD_THREAD_H_
#define NETCMD_THREAD_H_

#include "MEM_Pool.h"

#define EVENT_INT			1
#define	INT_EVENT_TIMEOUT	5000	/* 5 seconds to wait for INT Event */

#define NET_CMD_BUF_SIZE	(MEM_POOL_SEG_BYTES)

void netcmd_taskCreate(void);
void netcmd_SendINTEvent(int num);

#endif /* NETCMD_THREAD_H_ */
