/*
 * GPIO.c
 *
 *  Created on: Aug 10, 2023
 *      Author: tj925438
 */

#include "fsl_gpio.h"
#include "GPIO.h"

void GPIO_Init()
{
#ifndef SPI_NSS_HW
	/* SPI_CS0 */
	GPIO_PinWrite(GPIO11, 16U, 1);
	GPIO11->GDIR |= (1U << 16U); 			/*!< Enable GPIO */
#endif
	/* SPI_CS1 */
	GPIO_PinWrite(GPIO8, 16U, 1);
	GPIO8->GDIR |= (1U << 16U); 			/*!< Enable GPIO */

	/* GPIOs */
	GPIO_PinWrite(GPIO9, 30U, 0);
	GPIO9->GDIR |= (1U << 30U); 			/*!< Enable GPIO */
}

void SPI_SW_CS(int num, int state)
{
	if ( ! num)
	{
		if (state)
			GPIO_PortSet(GPIO11, 1U << 16U);
		else
			GPIO_PortClear(GPIO11, 1U << 16U);

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
	if (state)
		GPIO_PortSet(GPIO9, 1U << 30U);
	else
		GPIO_PortClear(GPIO9, 1U << 30U);
}
