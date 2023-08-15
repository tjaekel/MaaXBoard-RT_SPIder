/*
 * HTTPD.h
 *
 *  Created on: Aug 11, 2023
 *      Author: tj925438
 */

#ifndef HTTPD_H_
#define HTTPD_H_

#include "VCP_UART.h"

int HTTPD_Init(void);
char *HTTPD_GetIPAddress(EResultOut out);

#endif /* HTTPD_H_ */
