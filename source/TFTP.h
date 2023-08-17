/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __TFTP_H__
#define __TFTP_H__

////#include "fatfs.h"
#include <ff.h>
#include <diskio.h>
#include <lwip/tcpip.h>
#include <ethernetif.h>
////#include "app_ethernet.h"

#include "tftp_server.h"

extern const struct tftp_context TFTPctx;

void TFTP_Init(void);

#endif
