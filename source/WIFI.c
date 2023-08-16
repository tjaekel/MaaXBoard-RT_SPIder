/*
 * WIFI.c
 *
 *  Created on: Aug 15, 2023
 *      Author: tj
 */

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "event_groups.h"
#include "FreeRTOSConfig.h"
#include "globals.h"

portSTACK_TYPE *wifi_task_stack = NULL;
TaskHandle_t wifi_task_task_handler;

static QueueHandle_t wifi_commands_queue = NULL;
static QueueHandle_t wifi_response_queue = NULL;

static custom_wifi_instance_t t_wifi_cmd;

void WIFI_Init(void)
{
	wifi_commands_queue = xQueueCreate(10, sizeof(struct t_user_wifi_command));
	if (wifi_commands_queue != NULL)
	{
		vQueueAddToRegistry(wifi_commands_queue, "wifiQ");
	}

	wifi_response_queue = xQueueCreate(1, sizeof(uint8_t));
	/* Enable queue view in MCUX IDE FreeRTOS TAD plugin. */
	if (wifi_response_queue != NULL)
	{
		vQueueAddToRegistry(wifi_response_queue, "wResQ");
	}
	event_group_demo = xEventGroupCreate();

	////SDK_DelayAtLeastUs(1000000, SDK_DEVICE_MAXIMUM_CPU_CLOCK_FREQUENCY);

	/******** Freertos task declarations ********/
    ////os_setup_tick_function(vApplicationTickHook_lvgl);

#if defined(WIFI_EN) && (WIFI_EN==1)
    /* 2. Freertos task: wifi
     * @brief ssid scan, wifi connect to ssid (hardcoded), get wifi network info*/
	t_wifi_cmd.cmd_queue = &wifi_commands_queue;
	t_wifi_cmd.event_group_wifi = &event_group_demo;
	t_wifi_cmd.wifi_resQ = &wifi_response_queue;
    stat = xTaskCreate(wifi_task, "wifi", configMINIMAL_STACK_SIZE + 800, &t_wifi_cmd, tskIDLE_PRIORITY + 4, &wifi_task_task_handler);
    assert(pdPASS == stat);
#endif
}

