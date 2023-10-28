/*
 * GPIO.c
 *
 *  Created on: Aug 10, 2023
 *      Author: tj925438
 */

#include "fsl_gpio.h"
#include "board.h"
#include "GPIO.h"

void GPIO_INT_Init(void);

void GPIO_Init()
{
#ifndef SPI_NSS_HW
	/* SPI_CS0 */
	GPIO_PinWrite(GPIO9, 24U, 1);
	GPIO9->GDIR |= (1U << 24U); 			/*!< Enable GPIO */
#endif
	/* SPI_CS1 */
	GPIO_PinWrite(GPIO8, 16U, 1);
	GPIO8->GDIR |= (1U << 16U); 			/*!< Enable GPIO */

	/* GPIOs */
	GPIO_PinWrite(GPIO9, 30U, 0);
	GPIO9->GDIR |= (1U << 30U); 			/*!< Enable GPIO */

	GPIO_INT_Init();
}

void SPI_SW_CS(int num, int state)
{
	if ( ! num)
	{
		if (state)
			GPIO_PortSet(GPIO9, 1U << 24U);
		else
			GPIO_PortClear(GPIO9, 1U << 24U);

		return;
	}

	/* SPI_CS1 */
	if (state)
		GPIO_PortSet(GPIO8, 1U << 16U);
	else
		GPIO_PortClear(GPIO8, 1U << 16U);
}

void GPIO_Set(int num, int state)
{
#if 0
	if (state)
		GPIO_PortSet(GPIO9, 1U << 30U);
	else
		GPIO_PortClear(GPIO9, 1U << 30U);
#else
	GPIO_PinWrite(GPIO9, 30, state);
#endif
}

uint32_t GPIO_Get(int num)
{
	return CM7_GPIO2->PSR;
}

/* user button using INT */

#define EXAMPLE_SW_GPIO         BOARD_USER_BUTTON_GPIO
#define EXAMPLE_SW_GPIO_PIN     BOARD_USER_BUTTON_GPIO_PIN
#define EXAMPLE_SW_IRQ          BOARD_USER_BUTTON_IRQ
#define EXAMPLE_GPIO_IRQHandler BOARD_USER_BUTTON_IRQ_HANDLER
#define EXAMPLE_SW_NAME         BOARD_USER_BUTTON_NAME

volatile uint32_t g_InputSignal = 0U;

void EXAMPLE_GPIO_IRQHandler(void)
{
    /* clear the interrupt status */
    GPIO_PortClearInterruptFlags(EXAMPLE_SW_GPIO, 1U << EXAMPLE_SW_GPIO_PIN);
    /* Change state of switch */
    g_InputSignal |= 0x80000000;
    SDK_ISR_EXIT_BARRIER;
}

void CM7_GPIO2_3_IRQHandler(void)
{
	uint32_t val;

    /* clear the interrupt status - read pin level to figure out which one */
	val = CM7_GPIO3->PSR;

	if ( ! (val & 0x20))
	{
		GPIO_PortClearInterruptFlags(CM7_GPIO3, 1U << 5U);
		g_InputSignal |= 0x1;
	}
	if ( ! (val & 0x100))
	{
		GPIO_PortClearInterruptFlags(CM7_GPIO3, 1U << 8U);
		g_InputSignal |= 0x2;
	}

	val = CM7_GPIO2->PSR;
	if ( ! (val & 0x00400000))
	{
		GPIO_PortClearInterruptFlags(CM7_GPIO2, 1U << 22U);
		g_InputSignal |= 0x4;
	}
    SDK_ISR_EXIT_BARRIER;
}

void GPIO_INT_Init(void)
{
	EnableIRQ(EXAMPLE_SW_IRQ);

	EnableIRQ(CM7_GPIO2_3_IRQn);

	CM7_GPIO3->IMR |= (1 << 8U);
	CM7_GPIO2->IMR |= (1 << 22U);
	CM7_GPIO3->IMR |= (1 << 5U);
}

uint32_t GPIO_INT_check(void)
{
	return g_InputSignal;
}

void GPIO_INT_clear(void)
{
	g_InputSignal = 0;
}
