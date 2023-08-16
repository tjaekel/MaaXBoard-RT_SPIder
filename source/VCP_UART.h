/*
 * VCP_UART.h
 *
 *  Created on: Aug 8, 2023
 *      Author: tj925438
 */

#ifndef VCP_UART_H_
#define VCP_UART_H_

#include <stdio.h>

#define UART_LINE_LEN		128
#define XPRINT_LEN			(80*128)			/* max. length print buffer/strings/lines */

typedef enum {
	UART_OUT,
	DEBUG_OUT,
	HTTPD_OUT,
	HTTPD_OUT_ONLY,
	SILENT,
} EResultOut;

extern char XPrintBuf[XPRINT_LEN];

void VCP_UART_putString(const char *s, EResultOut out);
char *VCP_UART_getString(void);
void UART_Send(const char* str, int chrs, EResultOut out);
int UART_getCharNW();

void HTTP_OutBufferClear(void);
char *HTTP_GetOutBuffer(unsigned long *l);

#define print_log(out, ...)		do {\
									if (out != SILENT) {\
										snprintf(XPrintBuf, XPRINT_LEN - 1, __VA_ARGS__);\
										VCP_UART_putString((char *)XPrintBuf, out);\
									}\
								} while(0)

#endif /* VCP_UART_H_ */
