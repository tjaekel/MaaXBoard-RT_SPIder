/*
 * SPI.h
 *
 *  Created on: Aug 8, 2023
 *      Author: tj
 */

#ifndef SPI_H_
#define SPI_H_

void SPI_setup(void);
int SPI_transaction(unsigned char *tx, unsigned char *rx, size_t len);

#endif /* SPI_H_ */
