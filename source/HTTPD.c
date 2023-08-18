/*
 * HTTPD.c
 *
 *  Created on: Aug 14, 2023
 *      Author: tj925438
 */

#include "FreeRTOSConfig.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "event_groups.h"
#include "assert.h"

#include "lwip/tcpip.h"

#include "TFTP.h"
#include "UDP.h"
#include "NETCMD_thread.h"
#include "VCP_UART.h"

#define	HTTPD_PRIORITY 	1
#define HTTPD_STACKSIZE	512

#ifndef MDNS_HOSTNAME
#define MDNS_HOSTNAME 		"maaxboard"
#define MDNS_HOSTNAME_1G 	"maaxboardg"
#endif

extern void dual_eth_configuration();
extern void eth_100m_task(void *pvParameters);
extern void eth_1g_task(void *pvParameters);

extern void http_server_socket_init(void);

//extern void http_server_enable_mdns(struct netif *netif, const char *mdns_hostname);
extern void http_server_enable_mdns(struct netif *netif, const char *mdns_hostname, struct netif *netif2, const char *mdns_hostname2);
extern void http_server_print_ip_cfg(EResultOut out, struct netif *netif, int num);

extern struct netif netif_100m;
extern struct netif netif_1g;

static EventGroupHandle_t event_group_demo; /*!< Freertos eventgroup for wifi, ethernet connectivity tasks */

static void main_task(void *arg)
{
    (void)arg;

	////tcpip_init(NULL, NULL);

#if 0
    http_server_enable_mdns(&netif_100m, MDNS_HOSTNAME);
    ////http_server_print_ip_cfg(&netif_100m);					//with DHCP - it is still zero
#endif

#if 0
    /* it works only with one interface, not both */
    http_server_enable_mdns(&netif_1g, MDNS_HOSTNAME_1G);
    ////http_server_print_ip_cfg(&netif_1g);
#endif

    http_server_enable_mdns(&netif_100m, MDNS_HOSTNAME, &netif_1g, MDNS_HOSTNAME_1G);

    http_server_socket_init();

    TFTP_Init();

    UDP_Init();

    netcmd_taskCreate();

    vTaskDelete(NULL);
}

void HTTPD_Init(void)
{
	BaseType_t stat;

	/* must be called before starting ethernet tasks. */
	dual_eth_configuration();

	event_group_demo = xEventGroupCreate();

#if defined(ETH100MB_EN) && (ETH100MB_EN==1)
	/* 3. Freertos task: eth_100Mb
	 * @brief dhcp client running to get ip address */
	stat = xTaskCreate(eth_100m_task, "eth_100Mb", configMINIMAL_STACK_SIZE + 200, &event_group_demo, 3, NULL);
	assert(pdPASS == stat);
#endif

#if defined(ETH1GB_EN) && (ETH1GB_EN==1)
	/* 4. Freertos task: eth_1Gb
	 * @brief dhcp client running to get ip address */
	stat = xTaskCreate(eth_1g_task, "eth_1Gb", configMINIMAL_STACK_SIZE + 200, &event_group_demo, 3, NULL);
	assert(pdPASS == stat);
#endif

    /* create server task in RTOS */
    if (xTaskCreate(main_task, "main", HTTPD_STACKSIZE, NULL, HTTPD_PRIORITY, NULL) != pdPASS)
    {
        PRINTF("main(): Task creation failed.", 0);
        ////__BKPT(0);
        while (1) {}
    }
}

char *HTTPD_GetIPAddress(EResultOut out)
{
	http_server_print_ip_cfg(out, &netif_100m, 0);
	http_server_print_ip_cfg(out, &netif_1g, 1);

	return "undef";
}
