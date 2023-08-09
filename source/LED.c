/*
 * LED.c
 *
 *  Created on: Aug 8, 2023
 *      Author: tj925438
 */

#include "LED.h"

void LED_Init(void)
{
    LED_1_INIT();
    LED_2_INIT();
    LED_3_INIT();
}

void LED_state(unsigned int num, unsigned int state)
{
	switch (num)
	{
	case 0 : 	if (state)
					LED_1_ON();
			 	else
			 		LED_1_OFF();
			 	break;
	case 1 : 	if (state)
					LED_2_ON();
				else
					LED_2_OFF();
				break;
	case 2 : 	if (state)
					LED_3_ON();
				else
					LED_3_OFF();
				break;
	default : break;
	}
}
