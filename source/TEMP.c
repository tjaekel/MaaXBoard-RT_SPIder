/*
 * TEMP.c
 *
 *  Created on: Aug 10, 2023
 *      Author: tj925438
 */

//#include "fsl_debug_console.h"
//#include "pin_mux.h"
//#include "clock_config.h"
#include "board.h"
#include "fsl_tempsensor.h"

#include "VCP_UART.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define DEMO_TEMP_SENSOR              TMPSNS
#define DEMO_TEMP_LOW_HIGH_IRQn       TMPSNS_LOW_HIGH_IRQn
#define DEMO_TEMP_PANIC_IRQn          TMPSNS_PANIC_IRQn
#define DEMO_TEMP_LOW_HIGH_IRQHandler TMPSNS_LOW_HIGH_IRQHandler
#define DEMO_TEMP_PANIC_IRQHandler    TMPSNS_PANIC_IRQHandler
#define DEMO_HIGH_ALARM_TEMP          75U			//27U


/*******************************************************************************
 * Prototypes
 ******************************************************************************/

/*******************************************************************************
 * Variables
 ******************************************************************************/

volatile bool temperatureReach = false;

/*******************************************************************************
 * Code
 ******************************************************************************/

void DEMO_TEMP_LOW_HIGH_IRQHandler(void)
{
    temperatureReach = true;

#if defined(FSL_FEATURE_TMPSNS_HAS_AI_INTERFACE) && FSL_FEATURE_TMPSNS_HAS_AI_INTERFACE
    /* Disable high temp interrupt to jump out dead loop */
    TMPSNS_DisableInterrupt(DEMO_TEMP_SENSOR, kTEMPSENSOR_HighTempInterruptStatusEnable);
#else
    /* Disable high temp interrupt and finish interrupt to jump out dead loop, because finish interrupt and
     * high/low/panic share the one IRQ number. */
    TMPSNS_DisableInterrupt(DEMO_TEMP_SENSOR,
                            kTEMPSENSOR_HighTempInterruptStatusEnable | kTEMPSENSOR_FinishInterruptStatusEnable);
#endif

    SDK_ISR_EXIT_BARRIER;
}

/*!
 * @brief Main function
 */
int TEMP_Init(void)
{
    tmpsns_config_t config;

    TMPSNS_GetDefaultConfig(&config);
    config.measureMode   = kTEMPSENSOR_ContinuousMode;
    config.frequency     = 0x03U;
    config.highAlarmTemp = DEMO_HIGH_ALARM_TEMP;

    TMPSNS_Init(DEMO_TEMP_SENSOR, &config);
    TMPSNS_StartMeasure(DEMO_TEMP_SENSOR);

    EnableIRQ(DEMO_TEMP_LOW_HIGH_IRQn);

    return 1;
}

int TEMP_Get(void)
{
	int temperature;

	/* Get current temperature */
	temperature = (int)(TMPSNS_GetCurrentTemperature(DEMO_TEMP_SENSOR) * 10);

	if (temperatureReach && (temperature - DEMO_HIGH_ALARM_TEMP > 0))
	{
	    temperatureReach = false;

	    /* Re-enable high temperature interrupt */
	    TMPSNS_EnableInterrupt(DEMO_TEMP_SENSOR, kTEMPSENSOR_HighTempInterruptStatusEnable);

	    print_log(DEBUG_OUT, "The chip temperature has reached high temperature that is %d.%d Celsius\r\n",
	                   temperature / 10, temperature % 10);
	 }

	 return temperature;
}
