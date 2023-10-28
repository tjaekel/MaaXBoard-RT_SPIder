/*
 * QSPI_sw.h
 *
 *  Created on: Sep 11, 2023
 *      Author: tj925438
 */

#ifndef QSPI_SW_H_
#define QSPI_SW_H_

#define	QSPI_SCLKbit		23
#define QSPI_PCSbit			24			//always a driven output
#define	QSPI_D0bit			25
#define	QSPI_D1bit			26
#define	QSPI_D2bit			27
#define	QSPI_D3bit			18

#define	QSPI_DIRmaskOut		0x0F840000	//set output direction
#define QSPI_DIRmaskIn		0x01000000	//all input, except PCS

void BOARD_InitSWQSPIPins(void);	//configure LPSPI2 as QGPIOs for SW_QSPI
void QSPI_Test(void);

#endif /* QSPI_SW_H_ */
