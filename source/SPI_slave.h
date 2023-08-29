/*
 * SPI_slave.h
 *
 *  Created on: Aug 28, 2023
 *      Author: tj
 */

#ifndef SPI_SLAVE_H_
#define SPI_SLAVE_H_

int SPI_SlaveInit(void);
int SPI_SlaveTransfer(size_t bytes);
int SPI_SlaveCompletion(uint8_t *rxBuffer, size_t bytes);

#endif /* SPI_SLAVE_H_ */
