/*
 * globals.h
 *
 *  Created on: May 27, 2023
 *      Author: tjaekel
 */

#ifndef GLOBALS_H_
#define GLOBALS_H_

#include "fsl_pit.h"
#if 0
#include "host_mouse.h"
#include "host_keyboard.h"
#endif
#include "FreeRTOS.h"
#include "queue.h"
#include "event_groups.h"
#include "fsl_lpuart_freertos.h"
/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define	VERSION_INFO	"2.1"

/* FreeS task can be enabled by setting 1, disabled by setting 0 */
#define WIFI_EN			0		//1 - TODO: does not work yet
#define ETH100MB_EN		1
#define ETH1GB_EN		1

#if 0
/* CLI definitions */
#define mouseLogEn		0
#define keyboardLogEn	1

#define DEMO_LPUART LPUART1
#define DEMO_LPUART_CLK_FREQ BOARD_DebugConsoleSrcFreq()
#define DEMO_LPUART_IRQn LPUART1_IRQn

#define USER_LED_GREEN_GPIO GPIO8
#define USER_LED_RED_GPIO GPIO9
#define USER_LED_BLUE_GPIO GPIO9

#define GREEN_PIN	28u
#define RED_PIN		7u
#define BLUE_PIN	9u
#endif

/* PIT Timer used for runtime analysis on Freertos */
#define PIT1_PERIPHERAL PIT1
/* Definition of clock source frequency. */
#define PIT1_CLK_FREQ 240000000UL
/* Definition of ticks count for channel 0 - deprecated. */
#define PIT1_0_TICKS 23999U
/* PIT1 interrupt vector ID (number) - deprecated. */
#define PIT1_0_IRQN PIT0_IRQn
/* PIT1 interrupt handler identifier - deprecated. */
#define PIT1_0_IRQHANDLER PIT0_IRQHandler
/* Definition of channel number for channel 0. */
#define PIT1_CHANNEL_0 kPIT_Chnl_0
/* Definition of ticks count for channel 0. */
#define PIT1_CHANNEL_0_TICKS 23999U
/* PIT1 interrupt vector ID (number). */
#define PIT1_IRQN PIT1_IRQn
/* PIT1 interrupt handler identifier. */
#define PIT1_IRQHANDLER PIT1_IRQHandler

// /* Function prototypes in main.c */
// void toggleRedButton(bool state);
// void toggleGreenButton(bool state);
// void toggleBlueButton(bool state);

#if 0
/* struct wrapper for freertos mouse task input parameter */
typedef struct
{
	usb_host_mouse_instance_t *host_hid_mouse;
	QueueHandle_t *hid_queue;
}custom_usb_host_mouse_instance_t;

/* struct wrapper for freertos keyboard task input parameter */
typedef struct
{
	usb_host_keyboard_instance_t *host_hid_keyboard;
	QueueHandle_t *hid_queue;
}custom_usb_host_keyboard_instance_t;

/* struct wrapper for freertos usb logger task input parameter */
typedef struct
{
	lpuart_rtos_handle_t *uart_handle;
	QueueHandle_t *hid_queue;
}custom_usb_log_instance_t;
#endif

#if (WIFI_EN == 1)
/* struct wrapper for freertos wifi task input parameter */
typedef struct
{
	QueueHandle_t *cmd_queue;
	QueueHandle_t *wifi_resQ;
	EventGroupHandle_t *event_group_wifi;
}custom_wifi_instance_t;

/* struct wrapper for freertos console task input parameter */
typedef struct
{
	lpuart_rtos_handle_t *uart_handle;
	QueueHandle_t *cmd_queue;
	QueueHandle_t *wifi_resQ;
	EventGroupHandle_t *event_group_wifi;
}custom_console_instance_t;

typedef enum
{
	WIFI_SCAN,
	WIFI_IP,
	WIFI_CONN
}wifi_cmd;

typedef enum
{
	GUI_TASK,
	CONSOLE_TASK
}requesting_task;

/*! wifi command structure for other tasks */
struct t_user_wifi_command
{
	wifi_cmd cmd_type;	/*!< ssid scan, ip info, connect external AP */
	requesting_task task_name;	/*!< task name */
	void *payload;		/* could be data about external AP info*/
};
#endif

#define WIFI_GUI_NDATA		(1UL<<0UL)
#define WIFI_CONSOLE_NDATA	(1UL<<1UL)
#define WIFI_RDY			(1UL<<2UL)
#define ETH_100m_RDY		(1UL<<3UL)

////extern uint8_t shared_buff[2048];

// ipv4 type for storing ip address
typedef struct
{
	uint32_t ip;
	uint32_t sub;
	uint32_t gw;
	bool connected;
	uint8_t eth;
}ip_ro_t;

#if 0
// mouse type
typedef struct
{
	int16_t x;
	int16_t y;
	uint8_t btn;
}mouse_t;
#endif

#endif /* GLOBALS_H_ */
