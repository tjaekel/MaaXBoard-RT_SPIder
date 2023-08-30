/*
 * SPI.c
 *
 *  Created on: Aug 8, 2023
 *      Author: tj
 */

#include "fsl_device_registers.h"
#include "fsl_debug_console.h"
#include "fsl_lpspi.h"
#include "pin_mux.h"
#include "board.h"
#include "fsl_lpspi_edma.h"
#if defined(FSL_FEATURE_SOC_DMAMUX_COUNT) && FSL_FEATURE_SOC_DMAMUX_COUNT
#include "fsl_dmamux.h"
#endif

#include "fsl_common.h"
#include "FreeRTOS.h"
#include "task.h"

#include "SPI_slave.h"

void SPI_SW_CS(int num, int state);

/*******************************************************************************
 * Definitions
 ******************************************************************************/
/* Master related */
#define EXAMPLE_LPSPI_MASTER_BASEADDR              	(LPSPI4)					//was: (LPSPI1) - MaaXBoard has LPSPI4
#define EXAMPLE_LPSPI_MASTER_DMA_MUX_BASE          	(DMAMUX0)
#define EXAMPLE_LPSPI_MASTER_DMA_RX_REQUEST_SOURCE 	kDmaRequestMuxLPSPI4Rx		//was: kDmaRequestMuxLPSPI1Rx
#define EXAMPLE_LPSPI_MASTER_DMA_TX_REQUEST_SOURCE 	kDmaRequestMuxLPSPI4Tx		//was: kDmaRequestMuxLPSPI1Tx
#define EXAMPLE_LPSPI_MASTER_DMA_BASE              	(DMA0)
#define EXAMPLE_LPSPI_MASTER_DMA_RX_CHANNEL        	0U
#define EXAMPLE_LPSPI_MASTER_DMA_TX_CHANNEL        	1U

#define EXAMPLE_LPSPI_MASTER_PCS_FOR_INIT     		(kLPSPI_Pcs0)				//other PCS do not work!
#define EXAMPLE_LPSPI_MASTER_PCS_FOR_TRANSFER 		(kLPSPI_MasterPcs0)

#define LPSPI_MASTER_CLK_FREQ 						(CLOCK_GetFreqFromObs(CCM_OBS_LPSPI4_CLK_ROOT))

#define EXAMPLE_LPSPI_DEALY_COUNT 					0xFFFFFU
#define TRANSFER_SIZE     							1920U     				/* Transfer dataSize */
/* faster as 12000000 is not possible! */
#define TRANSFER_BAUDRATE 							12000000U 				/* Transfer bit rate - 10,000KHz */

/*******************************************************************************
 * Prototypes
 ******************************************************************************/
/* LPSPI user callback */
void LPSPI_MasterUserCallback(LPSPI_Type *base, lpspi_master_edma_handle_t *handle, status_t status, void *userData);

/*******************************************************************************
 * Variables
 ******************************************************************************/
#ifndef DUAL_SPI
AT_NONCACHEABLE_SECTION_INIT(uint8_t masterRxData[TRANSFER_SIZE]) = {0};
#endif
AT_NONCACHEABLE_SECTION_INIT(uint8_t masterTxData[TRANSFER_SIZE]) = {0};

AT_NONCACHEABLE_SECTION_INIT(lpspi_master_edma_handle_t g_m_edma_handle) = {0};
edma_handle_t lpspiEdmaMasterRxRegToRxDataHandle;
edma_handle_t lpspiEdmaMasterTxDataToTxRegHandle;

#if (defined(DEMO_EDMA_HAS_CHANNEL_CONFIG) && DEMO_EDMA_HAS_CHANNEL_CONFIG)
extern edma_config_t userConfig;
#else
edma_config_t userConfig = {0};
#endif

volatile bool isTransferCompleted  = false;
volatile uint32_t g_systickCounter = 20U;

lpspi_transfer_t masterXfer;

void LPSPI_MasterUserCallback(LPSPI_Type *base, lpspi_master_edma_handle_t *handle, status_t status, void *userData)
{
    if (status == kStatus_Success)
    {
#if 0
    	/* on Debug UART, as log */
        PRINTF("This is LPSPI master edma transfer completed callback\r\n");
#endif
    }

    SPI_SW_CS(0, 1);
    SPI_SW_CS(1, 1);
    /* TODO: use a semaphore or event */
    isTransferCompleted = true;
}

void SPI_setup(uint32_t baudrate)
{
    uint32_t srcClock_Hz;
    lpspi_master_config_t masterConfig;

    /*DMA Mux setting and EDMA init*/
#if defined(FSL_FEATURE_SOC_DMAMUX_COUNT) && FSL_FEATURE_SOC_DMAMUX_COUNT
    /* DMA MUX init*/
    DMAMUX_Init(EXAMPLE_LPSPI_MASTER_DMA_MUX_BASE);

    DMAMUX_SetSource(EXAMPLE_LPSPI_MASTER_DMA_MUX_BASE, EXAMPLE_LPSPI_MASTER_DMA_RX_CHANNEL,
                         EXAMPLE_LPSPI_MASTER_DMA_RX_REQUEST_SOURCE);
    DMAMUX_EnableChannel(EXAMPLE_LPSPI_MASTER_DMA_MUX_BASE, EXAMPLE_LPSPI_MASTER_DMA_RX_CHANNEL);

    DMAMUX_SetSource(EXAMPLE_LPSPI_MASTER_DMA_MUX_BASE, EXAMPLE_LPSPI_MASTER_DMA_TX_CHANNEL,
                         EXAMPLE_LPSPI_MASTER_DMA_TX_REQUEST_SOURCE);
    DMAMUX_EnableChannel(EXAMPLE_LPSPI_MASTER_DMA_MUX_BASE, EXAMPLE_LPSPI_MASTER_DMA_TX_CHANNEL);
#endif
    /* EDMA init*/
#if (!defined(DEMO_EDMA_HAS_CHANNEL_CONFIG) || (defined(DEMO_EDMA_HAS_CHANNEL_CONFIG) && !DEMO_EDMA_HAS_CHANNEL_CONFIG))
    /*
     * userConfig.enableRoundRobinArbitration = false;
     * userConfig.enableHaltOnError = true;
     * userConfig.enableContinuousLinkMode = false;
     * userConfig.enableDebugMode = false;
     */
    EDMA_GetDefaultConfig(&userConfig);
#endif
    EDMA_Init(EXAMPLE_LPSPI_MASTER_DMA_BASE, &userConfig);

    /*Master config*/
    LPSPI_MasterGetDefaultConfig(&masterConfig);

    masterConfig.cpol       = kLPSPI_ClockPolarityActiveHigh;
    masterConfig.cpha       = kLPSPI_ClockPhaseFirstEdge;
    masterConfig.direction  = kLPSPI_LsbFirst;

    masterConfig.baudRate 	= baudrate;
    masterConfig.whichPcs 	= EXAMPLE_LPSPI_MASTER_PCS_FOR_INIT;

    srcClock_Hz = LPSPI_MASTER_CLK_FREQ;
    LPSPI_MasterInit(EXAMPLE_LPSPI_MASTER_BASEADDR, &masterConfig, srcClock_Hz);

    /*Set up lpspi master*/
    memset(&(lpspiEdmaMasterRxRegToRxDataHandle), 0, sizeof(lpspiEdmaMasterRxRegToRxDataHandle));
    memset(&(lpspiEdmaMasterTxDataToTxRegHandle), 0, sizeof(lpspiEdmaMasterTxDataToTxRegHandle));

    EDMA_CreateHandle(&(lpspiEdmaMasterRxRegToRxDataHandle), EXAMPLE_LPSPI_MASTER_DMA_BASE,
                          EXAMPLE_LPSPI_MASTER_DMA_RX_CHANNEL);
    EDMA_CreateHandle(&(lpspiEdmaMasterTxDataToTxRegHandle), EXAMPLE_LPSPI_MASTER_DMA_BASE,
                          EXAMPLE_LPSPI_MASTER_DMA_TX_CHANNEL);
#if defined(FSL_FEATURE_EDMA_HAS_CHANNEL_MUX) && FSL_FEATURE_EDMA_HAS_CHANNEL_MUX
    DMA_SetChannelMux(EXAMPLE_LPSPI_MASTER_DMA_BASE, EXAMPLE_LPSPI_MASTER_DMA_TX_CHANNEL,
                           DEMO_LPSPI_TRANSMIT_EDMA_CHANNEL);
    EDMA_SetChannelMux(EXAMPLE_LPSPI_MASTER_DMA_BASE, EXAMPLE_LPSPI_MASTER_DMA_RX_CHANNEL,
                           DEMO_LPSPI_RECEIVE_EDMA_CHANNEL);
#endif
    LPSPI_MasterTransferCreateHandleEDMA(EXAMPLE_LPSPI_MASTER_BASEADDR, &g_m_edma_handle, LPSPI_MasterUserCallback,
                                             NULL, &lpspiEdmaMasterRxRegToRxDataHandle,
                                             &lpspiEdmaMasterTxDataToTxRegHandle);

    SPI_SW_CS(0, 1);
    SPI_SW_CS(1, 1);

#ifdef DUAL_SPI
    SPI_SlaveInit();
#endif
}

int SPI_transaction(unsigned char *tx, unsigned char *rx, size_t len)
{
	/* TODO: use a semaphore and avoid interrupting calls */
	memcpy(masterTxData, tx, len);

    /*Start master transfer*/
    masterXfer.txData   = masterTxData;
#ifndef DUAL_SPI
    masterXfer.rxData   = masterRxData;
#else
    masterXfer.rxData   = NULL;
#endif
    masterXfer.dataSize = len;
    masterXfer.configFlags = EXAMPLE_LPSPI_MASTER_PCS_FOR_TRANSFER | kLPSPI_MasterByteSwap | kLPSPI_MasterPcsContinuous;

    isTransferCompleted = false;

#ifdef DUAL_SPI
    /* prepare SPI slave */
    SPI_SlaveTransfer(len);
#endif

    LPSPI_MasterTransferEDMA(EXAMPLE_LPSPI_MASTER_BASEADDR, &g_m_edma_handle, &masterXfer);
    SPI_SW_CS(0, 0);
    SPI_SW_CS(1, 0);

#ifndef DUAL_SPI
    /* Wait until transfer completed */
    while (!isTransferCompleted)
    {
    	/* give other threads a chance during DMA */
    	taskYIELD();
    }

    memcpy(rx, masterRxData, len);
#else
    SPI_SlaveCompletion(rx, len);
#endif

    return 1;
}

void SPI_SetClock(uint32_t baudrate)
{
	uint32_t srcClock_Hz;
	lpspi_master_config_t masterConfig;

	LPSPI_Deinit(EXAMPLE_LPSPI_MASTER_BASEADDR);
	SPI_setup(baudrate);
}

uint32_t SPI_GetClock(void)
{
	return LPSPI_GetBaudrate();
}
