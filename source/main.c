/*
 * main.c
 *
 *  Created on: Aug 8, 2023
 *      Author: tj925438
 */

#include <stdio.h>
#include <stdlib.h>
/*${standard_header_anchor}*/
#include "fsl_device_registers.h"
#include "fsl_debug_console.h"
#include "pin_mux.h"
#include "clock_config.h"
#include "board.h"
#include "virtual_com.h"

#include "LED.h"
#include "SPI.h"
#include "GPIO.h"
#include "TEMP.h"
#include "HTTPD.h"
#include "MEM_Pool.h"
#include "VCP_UART.h"
#include "cmd_dec.h"

#ifndef APP_TASK_STACK_SIZE
#define APP_TASK_STACK_SIZE 5000L
#endif

static char const *s_appName = "CMDtask";

int USBH_Init(void);

/*!
 * @brief Application task function.
 *
 * This function runs the task for application.
 *
 * @return None.
 */

void APPTaskLoop(void)
{
	char *inP;

    while (1)
    {
    	CMD_printPrompt(UART_OUT);
    	inP = VCP_UART_getString();

    	CMD_DEC_execute(inP, UART_OUT);
    }
}

void APPTask(void *handle)
{
    USB_DeviceApplicationInit();

    SPI_setup();

#if USB_DEVICE_CONFIG_USE_TASK
    if (s_cdcVcom.deviceHandle)
    {
        if (xTaskCreate(USB_DeviceTask,                  /* pointer to the task                      */
                        (char const *)"usb device task", /* task name for kernel awareness debugging */
                        5000L / sizeof(portSTACK_TYPE),  /* task stack size                          */
                        s_cdcVcom.deviceHandle,          /* optional task startup argument           */
                        5,                               /* initial priority                         */
                        &s_cdcVcom.deviceTaskHandle      /* optional task handle to create           */
                        ) != pdPASS)
        {
            debug_log("usb device task create failed!\r\n");
            return;
        }
    }
#endif

    APPTaskLoop();
}

#if defined(__CC_ARM) || (defined(__ARMCC_VERSION)) || defined(__GNUC__)
int main(void)
#else
void main(void)
#endif
{
    BOARD_ConfigMPU();

    BOARD_InitPins();
    BOARD_BootClockRUN();
    BOARD_InitDebugConsole();

    MEM_PoolInit();
    LED_Init();
    GPIO_Init();
    TEMP_Init();

    if (xTaskCreate(APPTask,                                       /* pointer to the task                      */
                    s_appName,                                     /* task name for kernel awareness debugging */
                    APP_TASK_STACK_SIZE / sizeof(portSTACK_TYPE),  /* task stack size                          */
                    &s_cdcVcom,                                    /* optional task startup argument           */
                    4,                                             /* initial priority                         */
                    &s_cdcVcom.applicationTaskHandle               /* optional task handle to create           */
                    ) != pdPASS)
    {
        debug_log("app task create failed!\r\n");
#if (defined(__CC_ARM) || (defined(__ARMCC_VERSION)) || defined(__GNUC__))
        return 1;
#else
        return;
#endif
    }

    /* initialization with thread definitions */
#ifdef WITH_USB_MEMORY
    USBH_Init();
#endif
    HTTPD_Init();

    debug_log("starting RTOS...\r\n");
    vTaskStartScheduler();

#if (defined(__CC_ARM) || (defined(__ARMCC_VERSION)) || defined(__GNUC__))
    return 1;
#endif
}
