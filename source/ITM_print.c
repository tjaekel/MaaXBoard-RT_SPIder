
#include "fsl_component_serial_manager.h"
#include "fsl_component_serial_port_internal.h"
#include "fsl_component_serial_port_swo.h"
#include "core_cm7.h"

static serial_port_swo_config_t swoConfig = {
		.clockRate = 132000000,
		.port = 0,
		.baudRate = 1000000,
		.protocol = kSerialManager_SwoProtocolNrz,
};

int ITM_Init(void)
{
    uint32_t prescaler;

#if 0
    assert((TPI->DEVID & (TPI_DEVID_NRZVALID_Msk | TPI_DEVID_MANCVALID_Msk)) != 0U);
#endif

    prescaler = swoConfig.clockRate / swoConfig.baudRate - 1U;

    /* enable the ITM and DWT units */
    CoreDebug->DEMCR = CoreDebug_DEMCR_TRCENA_Msk;

    if ((CoreDebug->DEMCR & CoreDebug_DEMCR_TRCENA_Msk) == 0U)
    {
        return 1;		//error
    }

    /* Lock access */
    ITM->LAR = 0xC5ACCE55U;
    /* Disable ITM */
    ITM->TER &= ~(1UL << swoConfig.port);
    ITM->TCR = 0U;
#if 0
    /* select SWO encoding protocol */
    TPI->SPPR = (uint32_t)swoConfig.protocol;
    /* select asynchronous clock prescaler */
    TPI->ACPR = prescaler & 0xFFFFU;
#endif
    /* allow unprivileged access */
    ITM->TPR = 0U;
    /* enable ITM */
    ITM->TCR = ITM_TCR_ITMENA_Msk | ITM_TCR_SYNCENA_Msk
#ifdef ITM_TCR_TraceBusID_Msk
               | ITM_TCR_TraceBusID_Msk
#elif defined(ITM_TCR_TRACEBUSID_Msk)
               | ITM_TCR_TRACEBUSID_Msk
#else
#endif
               | ITM_TCR_SWOENA_Msk | ITM_TCR_DWTENA_Msk;
    /* enable the port bits */
    ITM->TER = 1UL << swoConfig.port;

    return 0;		//error free
}

void ITM_PrintString(char *s)
{
	while (*s)
	{
		ITM_SendChar((uint32_t)*s++);
	}
}
