/*
 * GPIO.h
 *
 *  Created on: Aug 10, 2023
 *      Author: tj925438
 */

#ifndef GPIO_H_
#define GPIO_H_

void GPIO_Init();
void GPIO_Set(int num, int state);
uint32_t GPIO_Get(int num);

void GPIO_INT_Init(void);
uint32_t GPIO_INT_check(void);
void GPIO_INT_clear(void);

#endif /* GPIO_H_ */
