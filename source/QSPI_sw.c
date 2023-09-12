/*
 * QSPI_sw.c
 *
 *  Created on: Sep 11, 2023
 *      Author: tj925438
 */

/* QSPI in SW GPIO mode (as FastGPIO) */

#include <stdio.h>
#include <stdlib.h>
#include "fsl_gpio.h"

#include "QSPI_sw.h"

#include "MEMORY_attributes.h"

/* trimmed for 41 MHz, periodic SCLK
 * our scope has 500 MHz sampling rate, to 2ns error,
 * MCU runs with 1 GHz, assuming 2 cycles per __NOP() is also 2ns
 * It looks "perfect", the 'jitter' and inaccuracy comes from the scope!
 */

/* for SCLK low period */
#define NOP_DELAY 	__NOP();\
					__NOP();\
					__NOP();\
					__NOP();\
					__NOP();\
					__NOP();\
					__NOP();\
					__NOP();\
					__NOP();\
					__NOP();\
					__NOP();\
					__NOP();\
					__NOP();\
					__NOP();\
					__NOP();\
					__NOP();\
					__NOP();\
					__NOP();\
					__NOP()

/* for SCLK high period */
#define NOP_DELAY2 	__NOP();\
					__NOP();\
					__NOP();\
					__NOP();\
					__NOP();\
					__NOP();\
					__NOP();\
					__NOP();\
					__NOP();\
					__NOP();\
					__NOP();\
					__NOP();\
					__NOP();\
					__NOP();\
					__NOP();\
					__NOP();\
					__NOP()

/* last in loop high period */
#define NOP_DELAY2b __NOP();\
					__NOP();\
					__NOP();\
					__NOP();\
					__NOP();\
					__NOP();\
					__NOP();\
					__NOP();\
					__NOP();\
					__NOP()

/* last (2nd) delay on first byte, before entering loop */
#define NOP_DELAY3 	__NOP();\
					__NOP();\
					__NOP();\
					__NOP();\
					__NOP();\
					__NOP();\
					__NOP();\
					__NOP();\
					__NOP();\
					__NOP()

uint8_t GPIO_QSPIbuffer[16] = {0xA5, 0x5A, 0x12, 0x21, 0x44, 0x88, 0x77, 0x42, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC};

void QSPI_Init(void)
{
	/* set PCS signal high, all other low */
	/* optional: we could have all signals except PCS as inputs (tri-stated) */
	CM7_GPIO3->DR_SET = (1 << QSPI_PCSbit);
	CM7_GPIO3->DR_CLEAR = (1 << QSPI_SCLKbit) | ( 1 << QSPI_D0bit) | (1 << QSPI_D1bit) | (1 << QSPI_D2bit) | (1 << QSPI_D3bit);
}

#pragma GCC push_options
#pragma GCC optimize ("O3")

ITCM_CM7 void QSPI_SendWrite(uint8_t *buf, size_t numBytes)
{
	uint8_t b;
	uint32_t w;

	__asm volatile ( "cpsid i" ::: "memory" );
	__asm volatile ( "dsb" );
	__asm volatile ( "isb" );

	/* Set Direction of QSPI D0..D3 as output */


	/* 2. Set initial D0..D1 - PCS still high, SCLK starting low */
	b = *buf++;
	/* TODO: we could handle 3 bits at once */
	w  = (b & 0x10) << (QSPI_D0bit - 4);
	w |= (b & 0x20) << (QSPI_D1bit - 5);
	w |= (b & 0x40) << (QSPI_D2bit - 6);
	w |= (b & 0x80) << (QSPI_D3bit - 7);

	w |= 1 << QSPI_PCSbit;
	CM7_GPIO3->DR = w;
	////__NOP();

	/* Set and keep PCS low */
	w &= ~(1 << QSPI_PCSbit);
	CM7_GPIO3->DR = w;

	NOP_DELAY;

	w |= 1 << QSPI_SCLKbit;
	CM7_GPIO3->DR = w;

	/* keep D0..D1 stable half a clock period on SCLK high */
	NOP_DELAY2;

	/* set SCLK low (falling edge, change to next 4bit Dx) */
	w  = (b & 0x01) << (QSPI_D0bit - 0);
	w |= (b & 0x02) << (QSPI_D1bit - 1);
	w |= (b & 0x04) << (QSPI_D2bit - 2);
	w |= (b & 0x08) << (QSPI_D3bit - 3);
	CM7_GPIO3->DR = w;

	/* wait half a clock period with SCLK low */
	NOP_DELAY;

	/* SCLK high */
	w |= 1 << QSPI_SCLKbit;
	CM7_GPIO3->DR = w;

	NOP_DELAY3;
	numBytes--;

	while (numBytes--)
	{
		b = *buf++;
		w  = (b & 0x10) << (QSPI_D0bit - 4);
		w |= (b & 0x20) << (QSPI_D1bit - 5);
		w |= (b & 0x40) << (QSPI_D2bit - 6);
		w |= (b & 0x80) << (QSPI_D3bit - 7);

		CM7_GPIO3->DR = w;

		NOP_DELAY;

		w |= 1 << QSPI_SCLKbit;
		CM7_GPIO3->DR = w;

		NOP_DELAY2;

		w  = (b & 0x01) << (QSPI_D0bit - 0);
		w |= (b & 0x02) << (QSPI_D1bit - 1);
		w |= (b & 0x04) << (QSPI_D2bit - 2);
		w |= (b & 0x08) << (QSPI_D3bit - 3);
		CM7_GPIO3->DR = w;

		NOP_DELAY;

		w |= 1 << QSPI_SCLKbit;
		CM7_GPIO3->DR = w;

		NOP_DELAY2b;
	}

	/* Set PCS high */
	w |= 1 << QSPI_PCSbit;
	CM7_GPIO3->DR = w;

	/* change D0..D1 direction to input (tri-state) */

	__asm volatile ( "cpsie i" ::: "memory" );
}

#pragma GCC pop_options

void QSPI_Test(void)
{
	size_t i;

	QSPI_Init();
	QSPI_SendWrite(GPIO_QSPIbuffer, 16);
}
