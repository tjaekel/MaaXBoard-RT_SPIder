/*
 * LED.h
 *
 *  Created on: Aug 8, 2023
 *      Author: tj925438
 */

#ifndef LED_H_
#define LED_H_

#include "board.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define LED_NUMBERS  3U
#define LED_1_INIT() USER_LED_INIT(LOGIC_LED_OFF)
#define LED_1_ON()   USER_LED_ON()
#define LED_1_OFF()  USER_LED_OFF()
#define LED_2_INIT() USER_LED_RED_INIT(LOGIC_LED_OFF)
#define LED_2_ON()   USER_LED_RED_ON()
#define LED_2_OFF()  USER_LED_RED_OFF()
#define LED_3_INIT() USER_LED_BLUE_INIT(LOGIC_LED_OFF)
#define LED_3_ON()   USER_LED_BLUE_ON()
#define LED_3_OFF()  USER_LED_BLUE_OFF()

/*******************************************************************************
 * Prototypes
 ******************************************************************************/
void LED_Init(void);
void LED_state(unsigned int num, unsigned int state);

#endif /* LED_H_ */
