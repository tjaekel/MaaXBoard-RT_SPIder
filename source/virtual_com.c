/*
 * Copyright (c) 2015 - 2016, Freescale Semiconductor, Inc.
 * Copyright 2016 - 2017 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <stdio.h>
#include <stdlib.h>
/*${standard_header_anchor}*/
#include "fsl_device_registers.h"
#include "fsl_debug_console.h"
#include "pin_mux.h"
#include "clock_config.h"
#include "board.h"

#include "globals.h"
#include "virtual_com.h"
#if (defined(FSL_FEATURE_SOC_SYSMPU_COUNT) && (FSL_FEATURE_SOC_SYSMPU_COUNT > 0U))
#include "fsl_sysmpu.h"
#endif /* FSL_FEATURE_SOC_SYSMPU_COUNT */

#if ((defined FSL_FEATURE_SOC_USBPHY_COUNT) && (FSL_FEATURE_SOC_USBPHY_COUNT > 0U))
#include "usb_phy.h"
#endif
#if defined(FSL_FEATURE_USB_KHCI_KEEP_ALIVE_ENABLED) && (FSL_FEATURE_USB_KHCI_KEEP_ALIVE_ENABLED > 0U) && \
    defined(USB_DEVICE_CONFIG_KEEP_ALIVE_MODE) && (USB_DEVICE_CONFIG_KEEP_ALIVE_MODE > 0U) &&             \
    defined(FSL_FEATURE_USB_KHCI_USB_RAM) && (FSL_FEATURE_USB_KHCI_USB_RAM > 0U)
extern uint8_t USB_EnterLowpowerMode(void);
#endif

#include <limits.h>				//ULONG_MAX

#include "VCP_UART.h"
#include "ITM_print.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/

/*******************************************************************************
 * Prototypes
 ******************************************************************************/
void BOARD_InitHardware(void);
void USB_DeviceClockInit(void);
void USB_DeviceIsrEnable(void);
#if USB_DEVICE_CONFIG_USE_TASK
void USB_DeviceTaskFn(void *deviceHandle);
#endif

void BOARD_DbgConsole_Deinit(void);
void BOARD_DbgConsole_Init(void);
usb_status_t USB_DeviceCdcVcomCallback(class_handle_t handle, uint32_t event, void *param);
usb_status_t USB_DeviceCallback(usb_device_handle handle, uint32_t event, void *param);

static void sendUSBEvent(void);

/*******************************************************************************
 * Variables
 ******************************************************************************/

static const char sWelcomeMessage[] = "\r\n*** SPIder version " VERSION_INFO " (" __DATE__ ") ***\r\n";

extern usb_device_endpoint_struct_t g_UsbDeviceCdcVcomDicEndpoints[];
extern usb_device_class_struct_t g_UsbDeviceCdcVcomConfig;
/* Data structure of virtual com device */
usb_cdc_vcom_struct_t s_cdcVcom;
////static char const *s_appName = "app task";

/* Line coding of cdc device */
USB_DMA_INIT_DATA_ALIGN(USB_DATA_ALIGN_SIZE)
static uint8_t s_lineCoding[LINE_CODING_SIZE] = {
    /* E.g. 0x00,0xC2,0x01,0x00 : 0x0001C200 is 115200 bits per second */
    (LINE_CODING_DTERATE >> 0U) & 0x000000FFU,
    (LINE_CODING_DTERATE >> 8U) & 0x000000FFU,
    (LINE_CODING_DTERATE >> 16U) & 0x000000FFU,
    (LINE_CODING_DTERATE >> 24U) & 0x000000FFU,
    LINE_CODING_CHARFORMAT,
    LINE_CODING_PARITYTYPE,
    LINE_CODING_DATABITS};

/* Abstract state of cdc device */
USB_DMA_INIT_DATA_ALIGN(USB_DATA_ALIGN_SIZE)
static uint8_t s_abstractState[COMM_FEATURE_DATA_SIZE] = {(STATUS_ABSTRACT_STATE >> 0U) & 0x00FFU,
                                                          (STATUS_ABSTRACT_STATE >> 8U) & 0x00FFU};

/* Country code of cdc device */
USB_DMA_INIT_DATA_ALIGN(USB_DATA_ALIGN_SIZE)
static uint8_t s_countryCode[COMM_FEATURE_DATA_SIZE] = {(COUNTRY_SETTING >> 0U) & 0x00FFU,
                                                        (COUNTRY_SETTING >> 8U) & 0x00FFU};

/* CDC ACM information */
USB_DMA_NONINIT_DATA_ALIGN(USB_DATA_ALIGN_SIZE) static usb_cdc_acm_info_t s_usbCdcAcmInfo;
/* Data buffer for receiving and sending*/
USB_DMA_NONINIT_DATA_ALIGN(USB_DATA_ALIGN_SIZE) static uint8_t s_currRecvBuf[DATA_BUFF_SIZE];
USB_DMA_NONINIT_DATA_ALIGN(USB_DATA_ALIGN_SIZE) static uint8_t s_currSendBuf[DATA_BUFF_SIZE];
volatile static uint32_t s_recvSize = 0;
volatile static uint32_t s_sendSize = 0;

/* USB device class information */
static usb_device_class_config_struct_t s_cdcAcmConfig[1] = {{
    USB_DeviceCdcVcomCallback,
    0,
    &g_UsbDeviceCdcVcomConfig,
}};

/* USB device class configuration information */
static usb_device_class_config_list_struct_t s_cdcAcmConfigList = {
    s_cdcAcmConfig,
    USB_DeviceCallback,
    1,
};

#if defined(FSL_FEATURE_USB_KHCI_KEEP_ALIVE_ENABLED) && (FSL_FEATURE_USB_KHCI_KEEP_ALIVE_ENABLED > 0U) && \
    defined(USB_DEVICE_CONFIG_KEEP_ALIVE_MODE) && (USB_DEVICE_CONFIG_KEEP_ALIVE_MODE > 0U) &&             \
    defined(FSL_FEATURE_USB_KHCI_USB_RAM) && (FSL_FEATURE_USB_KHCI_USB_RAM > 0U)
volatile static uint8_t s_waitForDataReceive = 0;
volatile static uint8_t s_comOpen            = 0;
#endif

static char UARTbuffer[UART_LINE_LEN + 1];
char XPrintBuf[XPRINT_LEN];

/*******************************************************************************
 * Code
 ******************************************************************************/

void USB_OTG1_IRQHandler(void)
{
    USB_DeviceEhciIsrFunction(s_cdcVcom.deviceHandle);
}

#if 0
/* this is not needed for USB Device */
void USB_OTG2_IRQHandler(void)
{
    USB_DeviceEhciIsrFunction(s_cdcVcom.deviceHandle);
}
#endif

void USB_DeviceClockInit(void)
{
    uint32_t usbClockFreq;
    usb_phy_config_struct_t phyConfig = {
        BOARD_USB_PHY_D_CAL,
        BOARD_USB_PHY_TXCAL45DP,
        BOARD_USB_PHY_TXCAL45DM,
    };
    usbClockFreq = 24000000;
    if (CONTROLLER_ID == kUSB_ControllerEhci0)
    {
        CLOCK_EnableUsbhs0PhyPllClock(kCLOCK_Usbphy480M, usbClockFreq);
        CLOCK_EnableUsbhs0Clock(kCLOCK_Usb480M, usbClockFreq);
    }
    else
    {
        CLOCK_EnableUsbhs1PhyPllClock(kCLOCK_Usbphy480M, usbClockFreq);
        CLOCK_EnableUsbhs1Clock(kCLOCK_Usb480M, usbClockFreq);
    }
    USB_EhciPhyInit(CONTROLLER_ID, BOARD_XTAL0_CLK_HZ, &phyConfig);
}
void USB_DeviceIsrEnable(void)
{
    uint8_t irqNumber;

    uint8_t usbDeviceEhciIrq[] = USBHS_IRQS;
    irqNumber                  = usbDeviceEhciIrq[CONTROLLER_ID - kUSB_ControllerEhci0];

    /* Install isr, set priority, and enable IRQ. */
    NVIC_SetPriority((IRQn_Type)irqNumber, USB_DEVICE_INTERRUPT_PRIORITY);
    EnableIRQ((IRQn_Type)irqNumber);
}
#if USB_DEVICE_CONFIG_USE_TASK
void USB_DeviceTaskFn(void *deviceHandle)
{
    USB_DeviceEhciTaskFunction(deviceHandle);
}
#endif
/*!
 * @brief CDC class specific callback function.
 *
 * This function handles the CDC class specific requests.
 *
 * @param handle          The CDC ACM class handle.
 * @param event           The CDC ACM class event type.
 * @param param           The parameter of the class specific request.
 *
 * @return A USB error code or kStatus_USB_Success.
 */
usb_status_t USB_DeviceCdcVcomCallback(class_handle_t handle, uint32_t event, void *param)
{
    usb_status_t error = kStatus_USB_InvalidRequest;
#if ((defined USB_DEVICE_CONFIG_CDC_CIC_EP_DISABLE) && (USB_DEVICE_CONFIG_CDC_CIC_EP_DISABLE > 0U))
#else
    uint32_t len;
#endif
    uint8_t *uartBitmap;
    usb_cdc_acm_info_t *acmInfo = &s_usbCdcAcmInfo;
    usb_device_cdc_acm_request_param_struct_t *acmReqParam;
    usb_device_endpoint_callback_message_struct_t *epCbParam;
    acmReqParam = (usb_device_cdc_acm_request_param_struct_t *)param;
    epCbParam   = (usb_device_endpoint_callback_message_struct_t *)param;
    switch (event)
    {
        case kUSB_DeviceCdcEventSendResponse:
        {
            if ((epCbParam->length != 0) &&
                (0U == (epCbParam->length % g_UsbDeviceCdcVcomDicEndpoints[0].maxPacketSize)))
            {
                /* If the last packet is the size of endpoint, then send also zero-ended packet,
                 ** meaning that we want to inform the host that we do not have any additional
                 ** data, so it can flush the output.
                 */
                error = USB_DeviceCdcAcmSend(handle, USB_CDC_VCOM_BULK_IN_ENDPOINT, NULL, 0);
            }
            else if ((1U == s_cdcVcom.attach) && (1U == s_cdcVcom.startTransactions))
            {
                if ((epCbParam->buffer != NULL) || ((epCbParam->buffer == NULL) && (epCbParam->length == 0)))
                {
                    /* User: add your own code for send complete event */
                    /* Schedule buffer for next receive event */
                    error = USB_DeviceCdcAcmRecv(handle, USB_CDC_VCOM_BULK_OUT_ENDPOINT, s_currRecvBuf,
                                                 g_UsbDeviceCdcVcomDicEndpoints[1].maxPacketSize);
#if defined(FSL_FEATURE_USB_KHCI_KEEP_ALIVE_ENABLED) && (FSL_FEATURE_USB_KHCI_KEEP_ALIVE_ENABLED > 0U) && \
    defined(USB_DEVICE_CONFIG_KEEP_ALIVE_MODE) && (USB_DEVICE_CONFIG_KEEP_ALIVE_MODE > 0U) &&             \
    defined(FSL_FEATURE_USB_KHCI_USB_RAM) && (FSL_FEATURE_USB_KHCI_USB_RAM > 0U)
                    s_waitForDataReceive = 1;
                    USB0->INTEN &= ~USB_INTEN_SOFTOKEN_MASK;
#endif
                }
            }
            else
            {
            }
        }
        break;
        case kUSB_DeviceCdcEventRecvResponse:
        {
            if ((1U == s_cdcVcom.attach) && (1U == s_cdcVcom.startTransactions))
            {
                s_recvSize = epCbParam->length;
                error      = kStatus_USB_Success;

#if defined(FSL_FEATURE_USB_KHCI_KEEP_ALIVE_ENABLED) && (FSL_FEATURE_USB_KHCI_KEEP_ALIVE_ENABLED > 0U) && \
    defined(USB_DEVICE_CONFIG_KEEP_ALIVE_MODE) && (USB_DEVICE_CONFIG_KEEP_ALIVE_MODE > 0U) &&             \
    defined(FSL_FEATURE_USB_KHCI_USB_RAM) && (FSL_FEATURE_USB_KHCI_USB_RAM > 0U)
                s_waitForDataReceive = 0;
                USB0->INTEN |= USB_INTEN_SOFTOKEN_MASK;
#endif
                if (0U == s_recvSize)
                {
                    /* Schedule buffer for next receive event */
                    error = USB_DeviceCdcAcmRecv(handle, USB_CDC_VCOM_BULK_OUT_ENDPOINT, s_currRecvBuf,
                                                 g_UsbDeviceCdcVcomDicEndpoints[1].maxPacketSize);
#if defined(FSL_FEATURE_USB_KHCI_KEEP_ALIVE_ENABLED) && (FSL_FEATURE_USB_KHCI_KEEP_ALIVE_ENABLED > 0U) && \
    defined(USB_DEVICE_CONFIG_KEEP_ALIVE_MODE) && (USB_DEVICE_CONFIG_KEEP_ALIVE_MODE > 0U) &&             \
    defined(FSL_FEATURE_USB_KHCI_USB_RAM) && (FSL_FEATURE_USB_KHCI_USB_RAM > 0U)
                    s_waitForDataReceive = 1;
                    USB0->INTEN &= ~USB_INTEN_SOFTOKEN_MASK;
#endif
                }
            }

            sendUSBEvent();
        }
        break;
        case kUSB_DeviceCdcEventSerialStateNotif:
            ((usb_device_cdc_acm_struct_t *)handle)->hasSentState = 0;
            error                                                 = kStatus_USB_Success;
            break;
        case kUSB_DeviceCdcEventSendEncapsulatedCommand:
            break;
        case kUSB_DeviceCdcEventGetEncapsulatedResponse:
            break;
        case kUSB_DeviceCdcEventSetCommFeature:
            if (USB_DEVICE_CDC_FEATURE_ABSTRACT_STATE == acmReqParam->setupValue)
            {
                if (1U == acmReqParam->isSetup)
                {
                    *(acmReqParam->buffer) = s_abstractState;
                    *(acmReqParam->length) = sizeof(s_abstractState);
                }
                else
                {
                    /* no action, data phase, s_abstractState has been assigned */
                }
                error = kStatus_USB_Success;
            }
            else if (USB_DEVICE_CDC_FEATURE_COUNTRY_SETTING == acmReqParam->setupValue)
            {
                if (1U == acmReqParam->isSetup)
                {
                    *(acmReqParam->buffer) = s_countryCode;
                    *(acmReqParam->length) = sizeof(s_countryCode);
                }
                else
                {
                    /* no action, data phase, s_countryCode has been assigned */
                }
                error = kStatus_USB_Success;
            }
            else
            {
                /* no action, return kStatus_USB_InvalidRequest */
            }
            break;
        case kUSB_DeviceCdcEventGetCommFeature:
            if (USB_DEVICE_CDC_FEATURE_ABSTRACT_STATE == acmReqParam->setupValue)
            {
                *(acmReqParam->buffer) = s_abstractState;
                *(acmReqParam->length) = COMM_FEATURE_DATA_SIZE;
                error                  = kStatus_USB_Success;
            }
            else if (USB_DEVICE_CDC_FEATURE_COUNTRY_SETTING == acmReqParam->setupValue)
            {
                *(acmReqParam->buffer) = s_countryCode;
                *(acmReqParam->length) = COMM_FEATURE_DATA_SIZE;
                error                  = kStatus_USB_Success;
            }
            else
            {
                /* no action, return kStatus_USB_InvalidRequest */
            }
            break;
        case kUSB_DeviceCdcEventClearCommFeature:
            break;
        case kUSB_DeviceCdcEventGetLineCoding:
            *(acmReqParam->buffer) = s_lineCoding;
            *(acmReqParam->length) = LINE_CODING_SIZE;
            error                  = kStatus_USB_Success;
            break;
        case kUSB_DeviceCdcEventSetLineCoding:
        {
            if (1U == acmReqParam->isSetup)
            {
                *(acmReqParam->buffer) = s_lineCoding;
                *(acmReqParam->length) = sizeof(s_lineCoding);
            }
            else
            {
                /* no action, data phase, s_lineCoding has been assigned */
            }
            error = kStatus_USB_Success;
        }
        break;
        case kUSB_DeviceCdcEventSetControlLineState:
        {
            s_usbCdcAcmInfo.dteStatus = acmReqParam->setupValue;
            /* activate/deactivate Tx carrier */
            if (acmInfo->dteStatus & USB_DEVICE_CDC_CONTROL_SIG_BITMAP_CARRIER_ACTIVATION)
            {
                acmInfo->uartState |= USB_DEVICE_CDC_UART_STATE_TX_CARRIER;
            }
            else
            {
                acmInfo->uartState &= (uint16_t)~USB_DEVICE_CDC_UART_STATE_TX_CARRIER;
            }

            /* activate carrier and DTE. Com port of terminal tool running on PC is open now */
            if (acmInfo->dteStatus & USB_DEVICE_CDC_CONTROL_SIG_BITMAP_DTE_PRESENCE)
            {
                acmInfo->uartState |= USB_DEVICE_CDC_UART_STATE_RX_CARRIER;
            }
            /* Com port of terminal tool running on PC is closed now */
            else
            {
                acmInfo->uartState &= (uint16_t)~USB_DEVICE_CDC_UART_STATE_RX_CARRIER;
            }

            /* Indicates to DCE if DTE is present or not */
            acmInfo->dtePresent = (acmInfo->dteStatus & USB_DEVICE_CDC_CONTROL_SIG_BITMAP_DTE_PRESENCE) ? true : false;

            /* Initialize the serial state buffer */
            acmInfo->serialStateBuf[0] = NOTIF_REQUEST_TYPE;                /* bmRequestType */
            acmInfo->serialStateBuf[1] = USB_DEVICE_CDC_NOTIF_SERIAL_STATE; /* bNotification */
            acmInfo->serialStateBuf[2] = 0x00;                              /* wValue */
            acmInfo->serialStateBuf[3] = 0x00;
            acmInfo->serialStateBuf[4] = 0x00; /* wIndex */
            acmInfo->serialStateBuf[5] = 0x00;
            acmInfo->serialStateBuf[6] = UART_BITMAP_SIZE; /* wLength */
            acmInfo->serialStateBuf[7] = 0x00;
            /* Notify to host the line state */
            acmInfo->serialStateBuf[4] = acmReqParam->interfaceIndex;
            /* Lower byte of UART BITMAP */
            uartBitmap    = (uint8_t *)&acmInfo->serialStateBuf[NOTIF_PACKET_SIZE + UART_BITMAP_SIZE - 2];
            uartBitmap[0] = acmInfo->uartState & 0xFFu;
            uartBitmap[1] = (acmInfo->uartState >> 8) & 0xFFu;
#if ((defined USB_DEVICE_CONFIG_CDC_CIC_EP_DISABLE) && (USB_DEVICE_CONFIG_CDC_CIC_EP_DISABLE > 0U))
#else
            len = (uint32_t)(NOTIF_PACKET_SIZE + UART_BITMAP_SIZE);
            if (0U == ((usb_device_cdc_acm_struct_t *)handle)->hasSentState)
            {
                error = USB_DeviceCdcAcmSend(handle, USB_CDC_VCOM_INTERRUPT_IN_ENDPOINT, acmInfo->serialStateBuf, len);
                if (kStatus_USB_Success != error)
                {
                    debug_log("kUSB_DeviceCdcEventSetControlLineState error!");
                }
                ((usb_device_cdc_acm_struct_t *)handle)->hasSentState = 1;
            }
#endif
            /* Update status */
            if (acmInfo->dteStatus & USB_DEVICE_CDC_CONTROL_SIG_BITMAP_CARRIER_ACTIVATION)
            {
                /*  To do: CARRIER_ACTIVATED */
            }
            else
            {
                /* To do: CARRIER_DEACTIVATED */
            }

            if (1U == s_cdcVcom.attach)
            {
                s_cdcVcom.startTransactions = 1;
#if defined(FSL_FEATURE_USB_KHCI_KEEP_ALIVE_ENABLED) && (FSL_FEATURE_USB_KHCI_KEEP_ALIVE_ENABLED > 0U) && \
    defined(USB_DEVICE_CONFIG_KEEP_ALIVE_MODE) && (USB_DEVICE_CONFIG_KEEP_ALIVE_MODE > 0U) &&             \
    defined(FSL_FEATURE_USB_KHCI_USB_RAM) && (FSL_FEATURE_USB_KHCI_USB_RAM > 0U)
                s_waitForDataReceive = 1;
                USB0->INTEN &= ~USB_INTEN_SOFTOKEN_MASK;
                s_comOpen = 1;
                debug_log("USB_APP_CDC_DTE_ACTIVATED\r\n");
#endif
            }
            error = kStatus_USB_Success;
        }
        break;
        case kUSB_DeviceCdcEventSendBreak:
            break;
        default:
            break;
    }

    return error;
}

/*!
 * @brief USB device callback function.
 *
 * This function handles the usb device specific requests.
 *
 * @param handle          The USB device handle.
 * @param event           The USB device event type.
 * @param param           The parameter of the device specific request.
 *
 * @return A USB error code or kStatus_USB_Success.
 */
usb_status_t USB_DeviceCallback(usb_device_handle handle, uint32_t event, void *param)
{
    usb_status_t error = kStatus_USB_InvalidRequest;
    uint16_t *temp16   = (uint16_t *)param;
    uint8_t *temp8     = (uint8_t *)param;

    switch (event)
    {
        case kUSB_DeviceEventBusReset:
        {
            s_cdcVcom.attach               = 0;
            s_cdcVcom.currentConfiguration = 0U;
            error                          = kStatus_USB_Success;
#if (defined(USB_DEVICE_CONFIG_EHCI) && (USB_DEVICE_CONFIG_EHCI > 0U)) || \
    (defined(USB_DEVICE_CONFIG_LPCIP3511HS) && (USB_DEVICE_CONFIG_LPCIP3511HS > 0U))
            /* Get USB speed to configure the device, including max packet size and interval of the endpoints. */
            if (kStatus_USB_Success == USB_DeviceClassGetSpeed(CONTROLLER_ID, &s_cdcVcom.speed))
            {
                USB_DeviceSetSpeed(handle, s_cdcVcom.speed);
            }
#endif
        }
        break;
        case kUSB_DeviceEventSetConfiguration:
            if (0U == (*temp8))
            {
                s_cdcVcom.attach               = 0;
                s_cdcVcom.currentConfiguration = 0U;
                error                          = kStatus_USB_Success;
            }
            else if (USB_CDC_VCOM_CONFIGURE_INDEX == (*temp8))
            {
                s_cdcVcom.attach               = 1;
                s_cdcVcom.currentConfiguration = *temp8;
                error                          = kStatus_USB_Success;
                /* Schedule buffer for receive */
                USB_DeviceCdcAcmRecv(s_cdcVcom.cdcAcmHandle, USB_CDC_VCOM_BULK_OUT_ENDPOINT, s_currRecvBuf,
                                     g_UsbDeviceCdcVcomDicEndpoints[1].maxPacketSize);
            }
            else
            {
                /* no action, return kStatus_USB_InvalidRequest */
            }
            break;
        case kUSB_DeviceEventSetInterface:
            if (0U != s_cdcVcom.attach)
            {
                uint8_t interface        = (uint8_t)((*temp16 & 0xFF00U) >> 0x08U);
                uint8_t alternateSetting = (uint8_t)(*temp16 & 0x00FFU);
                if (interface == USB_CDC_VCOM_COMM_INTERFACE_INDEX)
                {
                    if (alternateSetting < USB_CDC_VCOM_COMM_INTERFACE_ALTERNATE_COUNT)
                    {
                        s_cdcVcom.currentInterfaceAlternateSetting[interface] = alternateSetting;
                        error                                                 = kStatus_USB_Success;
                    }
                }
                else if (interface == USB_CDC_VCOM_DATA_INTERFACE_INDEX)
                {
                    if (alternateSetting < USB_CDC_VCOM_DATA_INTERFACE_ALTERNATE_COUNT)
                    {
                        s_cdcVcom.currentInterfaceAlternateSetting[interface] = alternateSetting;
                        error                                                 = kStatus_USB_Success;
                    }
                }
                else
                {
                    /* no action, return kStatus_USB_InvalidRequest */
                }
            }
            break;
        case kUSB_DeviceEventGetConfiguration:
            if (NULL != param)
            {
                /* Get current configuration request */
                *temp8 = s_cdcVcom.currentConfiguration;
                error  = kStatus_USB_Success;
            }
            break;
        case kUSB_DeviceEventGetInterface:
            if (NULL != param)
            {
                /* Get current alternate setting of the interface request */
                uint8_t interface = (uint8_t)((*temp16 & 0xFF00U) >> 0x08U);
                if (interface < USB_CDC_VCOM_INTERFACE_COUNT)
                {
                    *temp16 = (*temp16 & 0xFF00U) | s_cdcVcom.currentInterfaceAlternateSetting[interface];
                    error   = kStatus_USB_Success;
                }
            }
            break;
        case kUSB_DeviceEventGetDeviceDescriptor:
            if (NULL != param)
            {
                error = USB_DeviceGetDeviceDescriptor(handle, (usb_device_get_device_descriptor_struct_t *)param);
            }
            break;
        case kUSB_DeviceEventGetConfigurationDescriptor:
            if (NULL != param)
            {
                error = USB_DeviceGetConfigurationDescriptor(handle,
                                                             (usb_device_get_configuration_descriptor_struct_t *)param);
            }
            break;
        case kUSB_DeviceEventGetStringDescriptor:
            if (NULL != param)
            {
                /* Get device string descriptor request */
                error = USB_DeviceGetStringDescriptor(handle, (usb_device_get_string_descriptor_struct_t *)param);
            }
            break;
        default:
            /* no action, return kStatus_USB_InvalidRequest */
            break;
    }

    return error;
}

static void CDC_VCOM_FreeRTOSEnterCritical(uint32_t *sr)
{
    *sr = DisableGlobalIRQ();
    __ASM("CPSID i");
}

static void CDC_VCOM_FreeRTOSExitCritical(uint32_t sr)
{
    EnableGlobalIRQ(sr);
}

/*!
 * @brief Application initialization function.
 *
 * This function initializes the application.
 *
 * @return None.
 */
void USB_DeviceApplicationInit(void)
{
    USB_DeviceClockInit();
#if (defined(FSL_FEATURE_SOC_SYSMPU_COUNT) && (FSL_FEATURE_SOC_SYSMPU_COUNT > 0U))
    SYSMPU_Enable(SYSMPU, 0);
#endif /* FSL_FEATURE_SOC_SYSMPU_COUNT */

    s_cdcVcom.speed        = USB_SPEED_FULL;
    s_cdcVcom.attach       = 0;
    s_cdcVcom.cdcAcmHandle = (class_handle_t)NULL;
    s_cdcVcom.deviceHandle = NULL;

    if (kStatus_USB_Success != USB_DeviceClassInit(CONTROLLER_ID, &s_cdcAcmConfigList, &s_cdcVcom.deviceHandle))
    {
        debug_log("USB device init failed\r\n");
    }
    else
    {
        debug_log("USB device initialized\r\n");
        s_cdcVcom.cdcAcmHandle = s_cdcAcmConfigList.config->classHandle;
    }

    USB_DeviceIsrEnable();

    /*Add one delay here to make the DP pull down long enough to allow host to detect the previous disconnection.*/
    SDK_DelayAtLeastUs(5000, SDK_DEVICE_MAXIMUM_CPU_CLOCK_FREQUENCY);
    USB_DeviceRun(s_cdcVcom.deviceHandle);
}

/*!
 * @brief USB task function.
 *
 * This function runs the task for USB device.
 *
 * @return None.
 */
#if USB_DEVICE_CONFIG_USE_TASK
void USB_DeviceTask(void *handle)
{
    while (1U)
    {
        USB_DeviceTaskFn(handle);
    }
}
#endif

static TaskHandle_t xTaskToNotify = NULL;

static void sendUSBEvent(void) {
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    /* At this point xTaskToNotify should not be NULL as a transmission was
       in progress. */
    configASSERT(xTaskToNotify != NULL );

    /* Notify the task that the transmission is complete. */
   vTaskNotifyGiveIndexedFromISR(xTaskToNotify, 0, &xHigherPriorityTaskWoken );

    /* If xHigherPriorityTaskWoken is now set to pdTRUE then a context switch
       should be performed to ensure the interrupt returns directly to the highest
       priority task.  The macro used for this purpose is dependent on the port in
       use and may be called portEND_SWITCHING_ISR(). */
    portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
}

/* -------- public API --------------- */

#define HTTP_OUT_BUF_SIZE		2048
static char sHTTPoutBuffer[HTTP_OUT_BUF_SIZE];
static unsigned long sHTTPoutIdx = 0;

void HTTP_OutBufferClear(void)
{
	sHTTPoutIdx = 0;
}

char *HTTP_GetOutBuffer(unsigned long *l)
{
	*l = sHTTPoutIdx;
	return sHTTPoutBuffer;
}

static void HTTP_PutOutBuffer(const char *s, unsigned long l)
{
	if ((sHTTPoutIdx + l) > HTTP_OUT_BUF_SIZE)
		l = HTTP_OUT_BUF_SIZE - sHTTPoutIdx;

	memcpy(&sHTTPoutBuffer[sHTTPoutIdx], s, l);
	sHTTPoutIdx += l;
}

void VCP_UART_putString(const char *s, EResultOut out)
{
	/* TODO: we could check if the previous buffer was sent or how much is free */
	uint32_t size = 0;
	usb_status_t error = kStatus_USB_Error;

	if (out == UART_OUT)
	{
		while (*s)
		{
			if (size >= sizeof(s_currSendBuf))
				break;
			s_currSendBuf[size++] = *s++;
		}

		if ((1U == s_cdcVcom.attach) && (1U == s_cdcVcom.startTransactions))
		{
			error = USB_DeviceCdcAcmSend(s_cdcVcom.cdcAcmHandle, USB_CDC_VCOM_BULK_IN_ENDPOINT, s_currSendBuf, size);
			if (error != kStatus_USB_Success)
			{
				/* wait and try again:
				 * this is also used in VCP_UART_getSring(), wenn calling too fast again the USB send - it will fail!
				 */
				OSA_TimeDelay(1);
				USB_DeviceCdcAcmSend(s_cdcVcom.cdcAcmHandle, USB_CDC_VCOM_BULK_IN_ENDPOINT, s_currSendBuf, size);
			}
		}
		return;
	}

	if (out == DEBUG_OUT)
	{
		DbgConsole_Printf(s);
		return;
	}

	if (out == HTTPD_OUT)
	{
		HTTP_PutOutBuffer(s, strlen(s));
		return;
	}

	if (out == ITM_OUT)
	{
		ITM_PrintString(s);
		return;
	}
}

char *VCP_UART_getString(void)
{
	usb_status_t error = kStatus_USB_Error;
	uint32_t rxIdx = 0;
	uint32_t size;
	uint32_t ulNotifiedValue = 1;
	uint32_t usbOsaCurrentSr;
	static int firstSend = 0;

	xTaskToNotify = xTaskGetCurrentTaskHandle();

    while (1)
    {
    	xTaskNotifyWaitIndexed( 0,                /* Wait for 0th notification. */
    	                        0x00,             /* Don't clear any notification bits on entry. */
    	                        ULONG_MAX,        /* Reset the notification value to 0 on exit. */
    	                        &ulNotifiedValue, /* Notified value pass out in ulNotifiedValue. */
    	                        portMAX_DELAY );  /* Block indefinitely. */

        ////if ((1U == s_cdcVcom.attach) && (1U == s_cdcVcom.startTransactions))
        {
#if 1
        	//this needs at least one character received to work
        	if ( ! firstSend) {
        		firstSend = 1;

        		strcpy((char *)s_currSendBuf, sWelcomeMessage);
        		size = strlen(sWelcomeMessage);
        		USB_DeviceCdcAcmSend(s_cdcVcom.cdcAcmHandle, USB_CDC_VCOM_BULK_IN_ENDPOINT, s_currSendBuf, size);
        	}
#endif

            /* Enter critical can not be added here because of the loop */
            /* endpoint callback length is USB_CANCELLED_TRANSFER_LENGTH (0xFFFFFFFFU) when transfer is canceled */
            if ((0 != s_recvSize) && (USB_CANCELLED_TRANSFER_LENGTH != s_recvSize))
            {
                /* The operating timing sequence has guaranteed there is no conflict to access the s_recvSize between
                   USB ISR and this task. Therefore, the following code of Enter/Exit ctitical mode is useless, only to
                   mention users the exclusive access of s_recvSize if users implement their own
                   application referred to this SDK demo */
                CDC_VCOM_FreeRTOSEnterCritical(&usbOsaCurrentSr);
                if ((0U != s_recvSize) && (USB_CANCELLED_TRANSFER_LENGTH != s_recvSize))
                {
                	if (rxIdx >= UART_LINE_LEN)
                		size = UART_LINE_LEN - rxIdx;
                	else
                		size = s_recvSize;
                	memcpy(&UARTbuffer[rxIdx], s_currRecvBuf, size);
                	rxIdx += size;

                    /* Copy Buffer to Send Buff - for local echo */
                    memcpy(s_currSendBuf, s_currRecvBuf, s_recvSize);
                    s_sendSize = s_recvSize;
                    s_recvSize = 0;
                }
                CDC_VCOM_FreeRTOSExitCritical(usbOsaCurrentSr);
            }

            if (0U != s_sendSize)
            {
            	//echo back what we have received
                size = s_sendSize;
                s_sendSize    = 0;

#if 0
                //the DbgConsole_... works in parallel - LPUART1 is needed!
                DbgConsole_Putchar((int)s_currSendBuf[0]);
                if (s_currSendBuf[0] == '\r')
                	DbgConsole_Putchar((int)'\n');
#endif

                //add a NEWLINE if CARRIAGE RETURN was there
                if (s_currSendBuf[size - 1] == '\r') {
                	s_currSendBuf[size] = '\n';
                	size++;
                }

                /* handle BS */
                if (UARTbuffer[rxIdx - 1] == 0x08)
                {
                	rxIdx--;		/* BS is in buffer and counted, remove it */
                	if (rxIdx > 0)
                    {
                    	memcpy(s_currSendBuf, "\010 \010", 3);
                        error = USB_DeviceCdcAcmSend(s_cdcVcom.cdcAcmHandle, USB_CDC_VCOM_BULK_IN_ENDPOINT, s_currSendBuf, 3);
                        if (error != kStatus_USB_Success)
                        {
                        	/* Failure to send Data Handling code here */
                        }
                        rxIdx--;	/* remove the character before in buffer */
                     }
                	else
                	{
                		/* we have send something, otherwise a deadlock - nothing received anymore! */
                		memcpy(s_currSendBuf, " \010", 2);
                		error = USB_DeviceCdcAcmSend(s_cdcVcom.cdcAcmHandle, USB_CDC_VCOM_BULK_IN_ENDPOINT, s_currSendBuf, 2);
                		if (error != kStatus_USB_Success)
                		{
                			/* Failure to send Data Handling code here */
                		}
                	}
                	continue;
                 }

                /* remote echo */
                error = USB_DeviceCdcAcmSend(s_cdcVcom.cdcAcmHandle, USB_CDC_VCOM_BULK_IN_ENDPOINT, s_currSendBuf, size);
                if (error != kStatus_USB_Success)
                {
                    /* Failure to send Data Handling code here */
                }

                /* check if we have to finish loop, with CR or rxIdx overflows */
                if (rxIdx >= UART_LINE_LEN)
                {
                	UARTbuffer[rxIdx] = '\0';
                	return UARTbuffer;
                }

                /* end of input with Enter (CR) */
                if (UARTbuffer[rxIdx - 1] == '\r')
                {
                	UARTbuffer[rxIdx - 1] = '\0';		/* get rid of CR, we do not need */
                	return UARTbuffer;
                }


            }
#if defined(FSL_FEATURE_USB_KHCI_KEEP_ALIVE_ENABLED) && (FSL_FEATURE_USB_KHCI_KEEP_ALIVE_ENABLED > 0U) && \
    defined(USB_DEVICE_CONFIG_KEEP_ALIVE_MODE) && (USB_DEVICE_CONFIG_KEEP_ALIVE_MODE > 0U) &&             \
    defined(FSL_FEATURE_USB_KHCI_USB_RAM) && (FSL_FEATURE_USB_KHCI_USB_RAM > 0U)
            if ((s_waitForDataReceive))
            {
                if (s_comOpen == 1)
                {
                    /* Wait for all the packets been sent during opening the com port. Otherwise these packets may
                     * wake up the system.
                     */
                    debug_log("Waiting to enter lowpower ...\r\n");
                    for (uint32_t i = 0U; i < 16000000U; ++i)
                    {
                        __NOP(); /* delay */
                    }

                    s_comOpen = 0;
                }
                debug_log("Enter lowpower\r\n");
                BOARD_DbgConsole_Deinit();
                USB0->INTEN &= ~USB_INTEN_TOKDNEEN_MASK;
                if (SysTick->CTRL & SysTick_CTRL_ENABLE_Msk)
                {
                    SysTick->CTRL &= ~SysTick_CTRL_TICKINT_Msk;
                }
                USB_EnterLowpowerMode();

                if (SysTick->CTRL & SysTick_CTRL_ENABLE_Msk)
                {
                    SysTick->CTRL |= SysTick_CTRL_TICKINT_Msk;
                }
                s_waitForDataReceive = 0;
                USB0->INTEN |= USB_INTEN_TOKDNEEN_MASK;
                BOARD_DbgConsole_Init();
                debug_log("Exit  lowpower\r\n");
            }
#endif
        }
    }
}

int UART_getCharNW()
{
	int c;
	uint32_t usbOsaCurrentSr;

	if ((0 != s_recvSize) && (USB_CANCELLED_TRANSFER_LENGTH != s_recvSize))
	{
		CDC_VCOM_FreeRTOSEnterCritical(&usbOsaCurrentSr);
	    if ((0U != s_recvSize) && (USB_CANCELLED_TRANSFER_LENGTH != s_recvSize))
	    {
	    	c = (int)s_currRecvBuf[0];
	    	CDC_VCOM_FreeRTOSExitCritical(usbOsaCurrentSr);

	    	/* optional: consume from command line, otherwise not echoed but part of command string */
	    	s_recvSize--;

	    	return c;
	    }
	}

	taskYIELD();
	return 0;
}

void UART_Send(const char* str, int chrs, EResultOut out)
{
	uint32_t size = chrs;
	usb_status_t error = kStatus_USB_Error;

	if (out == UART_OUT)
	{
		memcpy(s_currSendBuf, str, size);

		if ((1U == s_cdcVcom.attach) && (1U == s_cdcVcom.startTransactions))
		{
			error = USB_DeviceCdcAcmSend(s_cdcVcom.cdcAcmHandle, USB_CDC_VCOM_BULK_IN_ENDPOINT, s_currSendBuf, size);
			if (error != kStatus_USB_Success)
			{
				/* wait and try again:
				 * this is also used in VCP_UART_getSring(), wenn calling too fast again the USB send - it will fail!
				 */
				OSA_TimeDelay(1);
				USB_DeviceCdcAcmSend(s_cdcVcom.cdcAcmHandle, USB_CDC_VCOM_BULK_IN_ENDPOINT, s_currSendBuf, size);
			}
		}
		return;
	}

	if (out == HTTPD_OUT)
	{
		HTTP_PutOutBuffer(str, chrs);
		return;
	}

	if (out == DEBUG_OUT)
	{
		/* not used here */
		return;
	}
}
