/*
 created by Deqing Sun for use with CH55xduino
 Adapted for CH552 pedal: HID boot requests, dynamic string pointer, bounded
 descriptors/configuration, reset mailboxes. See PROVENANCE.md and LICENSE.
 */

// clang-format off
#include "USBhandler.h"
#include "USBconstant.h"
// clang-format on

// Keyboard functions:

// clang-format off
__xdata __at (EP0_ADDR) uint8_t Ep0Buffer[8];     
__xdata __at (EP1_ADDR) uint8_t Ep1Buffer[128];
__xdata __at (EP2_ADDR) uint8_t Ep2Buffer[128];
// clang-format on

#if (EP2_ADDR + 128) > USER_USB_RAM
#error "This example needs more USB ram. Increase this setting in menu."
#endif

__data uint16_t SetupLen;
__data uint8_t SetupReq;
volatile __xdata uint8_t UsbConfig;

const uint8_t *__data pDescr;
static __data uint8_t ControlLedPending;

volatile uint8_t usbMsgFlags = 0; // uint8_t usbMsgFlags copied from VUSB

inline void NOP_Process(void) {}

void USB_EP0_SETUP(void) {
  __data uint8_t len = USB_RX_LEN;
  if (len == (sizeof(USB_SETUP_REQ))) {
    SetupLen = ((uint16_t)UsbSetupBuf->wLengthH << 8) | (UsbSetupBuf->wLengthL);
    len = 0; // Default is success and upload 0 length
    SetupReq = UsbSetupBuf->bRequest;
    ControlLedPending = 0;
    usbMsgFlags = 0;
    if ((UsbSetupBuf->bRequestType & USB_REQ_TYP_MASK) !=
        USB_REQ_TYP_STANDARD) // Not standard request
    {

      // here is the commnunication starts, refer to usbFunctionSetup of USBtiny
      // or usb_setup in usbtiny

      switch ((UsbSetupBuf->bRequestType & USB_REQ_TYP_MASK)) {
      case USB_REQ_TYP_VENDOR: {
        switch (SetupReq) {
        default:
          len = 0xFF; // command not supported
          break;
        }
        break;
      }
      case USB_REQ_TYP_CLASS: {
        /* Keyboard interface only, no report IDs in either protocol. Raw HID
         * uses the separate interrupt OUT endpoint, never unbounded EP0 data. */
        if (UsbSetupBuf->wIndexH != 0 || UsbSetupBuf->wIndexL != 0) {
          len = 0xFF;
          break;
        }
        switch (SetupReq) {
        case 0x01: /* GET_REPORT: one 8-byte keyboard input report */
          if (UsbSetupBuf->bRequestType != 0xA1 ||
              UsbSetupBuf->wValueH != 1 || UsbSetupBuf->wValueL != 0) {
            len = 0xFF;
            break;
          }
          len = SetupLen < 8 ? SetupLen : 8;
          for (__data uint8_t i = 0; i < len; ++i)
            Ep0Buffer[i] = UsbKeyboardReport[i];
          SetupLen = 0;
          /* Reuse the descriptor IN continuation for a required terminating
           * zero-length packet when the host requested more than 8 bytes. */
          SetupReq = USB_GET_DESCRIPTOR;
          break;
        case 0x02: /* GET_IDLE */
        case 0x03: /* GET_PROTOCOL */
          if (UsbSetupBuf->bRequestType != 0xA1 ||
              UsbSetupBuf->wValueH != 0 || UsbSetupBuf->wValueL != 0) {
            len = 0xFF;
            break;
          }
          Ep0Buffer[0] = SetupReq == 2 ? UsbIdle : UsbProtocol;
          len = SetupLen ? 1 : 0;
          break;
        case 0x09: /* SET_REPORT: one byte keyboard LEDs */
          if (UsbSetupBuf->bRequestType != 0x21 || SetupLen != 1 ||
              UsbSetupBuf->wValueH != 2 || UsbSetupBuf->wValueL != 0) {
            len = 0xFF;
            break;
          }
          ControlLedPending = 1;
          break;
        case 0x0A: /* SET_IDLE */
          if (UsbSetupBuf->bRequestType != 0x21 || SetupLen != 0 ||
              UsbSetupBuf->wValueL != 0) {
            len = 0xFF;
            break;
          }
          UsbIdle = UsbSetupBuf->wValueH;
          break;
        case 0x0B: /* SET_PROTOCOL: boot/report share the same 8-byte format */
          if (UsbSetupBuf->bRequestType != 0x21 || SetupLen != 0 ||
              UsbSetupBuf->wValueH != 0 || UsbSetupBuf->wValueL > 1) {
            len = 0xFF;
            break;
          }
          UsbProtocol = UsbSetupBuf->wValueL;
          break;
        default:
          len = 0xFF; // command not supported
          break;
        }
        break;
      }
      default:
        len = 0xFF; // command not supported
        break;
      }

    } else // Standard request
    {
      switch (SetupReq) // Request ccfType
      {
      case USB_GET_DESCRIPTOR:
        switch (UsbSetupBuf->wValueH) {
        case 1: // Device Descriptor
          pDescr = (const uint8_t *)&DeviceDescriptor;
          len = sizeof(USB_Descriptor_Device_t);
          break;
        case 2: // Configure Descriptor
          pDescr = (const uint8_t *)&ConfigurationDescriptor;
          len = sizeof(USB_Descriptor_Configuration_t);
          break;
        case 3:
          if (UsbSetupBuf->wValueL == 0) {
            pDescr = LanguageDescriptor;
          } else if (UsbSetupBuf->wValueL == 1) {
            pDescr = (const uint8_t *)ManufacturerDescriptor;
          } else if (UsbSetupBuf->wValueL == 2) {
            pDescr = (const uint8_t *)ProductDescriptor;
          } else if (UsbSetupBuf->wValueL == 3) {
            pDescr = (const uint8_t *)SerialDescriptor;
          } else {
            len = 0xff;
            break;
          }
          len = pDescr[0];
          break;
        case 0x22:
          if (UsbSetupBuf->wValueL == 0 && UsbSetupBuf->wIndexH == 0) {
            if (UsbSetupBuf->wIndexL == 0) {
              pDescr = (const uint8_t *)ReportDescriptor;
              len = ConfigurationDescriptor.HID_KeyboardHID.HIDReportLength;
            } else if (UsbSetupBuf->wIndexL == 1) {
              pDescr = (const uint8_t *)RawHIDReportDescriptor;
              len = ConfigurationDescriptor.HID_RawDescriptor.HIDReportLength;
            } else {
              len = 0xff;
            }
          } else {
            len = 0xff;
          }
          break;
        case 0x21: /* HID descriptor, per interface */
          if (UsbSetupBuf->wValueL != 0 || UsbSetupBuf->wIndexH != 0) {
            len = 0xFF;
          } else if (UsbSetupBuf->wIndexL == 0) {
            pDescr = (const uint8_t *)&ConfigurationDescriptor.HID_KeyboardHID;
            len = sizeof(USB_HID_Descriptor_HID_t);
          } else if (UsbSetupBuf->wIndexL == 1) {
            pDescr = (const uint8_t *)&ConfigurationDescriptor.HID_RawDescriptor;
            len = sizeof(USB_HID_Descriptor_HID_t);
          } else {
            len = 0xFF;
          }
          break;
        default:
          len = 0xff; // Unsupported descriptors or error
          break;
        }
        if (len != 0xff) {
          if (SetupLen > len) {
            SetupLen = len; // Limit length
          }
          len = SetupLen >= DEFAULT_ENDP0_SIZE
                    ? DEFAULT_ENDP0_SIZE
                    : SetupLen; // transmit length for this packet
          for (__data uint8_t i = 0; i < len; i++) {
            Ep0Buffer[i] = pDescr[i];
          }
          SetupLen -= len;
          pDescr += len;
        }
        break;
      case USB_SET_ADDRESS:
        if (UsbSetupBuf->bRequestType != 0 || UsbSetupBuf->wValueH != 0 ||
            UsbSetupBuf->wValueL > 127 || SetupLen != 0) {
          len = 0xFF;
          break;
        }
        SetupLen = UsbSetupBuf->wValueL; // Save the assigned address
        break;
      case USB_GET_CONFIGURATION:
        Ep0Buffer[0] = UsbConfig;
        if (SetupLen >= 1) {
          len = 1;
        }
        break;
      case USB_SET_CONFIGURATION:
        if (UsbSetupBuf->bRequestType != 0 || UsbSetupBuf->wValueH != 0 ||
            UsbSetupBuf->wValueL > 1 || SetupLen != 0) {
          len = 0xFF;
          break;
        }
        USBTransportReset();
        UEP1_CTRL = bUEP_AUTO_TOG | UEP_T_RES_NAK | UEP_R_RES_ACK;
        UEP2_CTRL = bUEP_AUTO_TOG | UEP_T_RES_NAK | UEP_R_RES_ACK;
        UsbConfig = UsbSetupBuf->wValueL;
        break;
      case USB_GET_INTERFACE:
        if (UsbSetupBuf->wIndexH != 0 || UsbSetupBuf->wIndexL > 1) {
          len = 0xFF;
          break;
        }
        Ep0Buffer[0] = 0;
        len = SetupLen ? 1 : 0;
        break;
      case USB_SET_INTERFACE:
        if (UsbSetupBuf->wIndexH != 0 || UsbSetupBuf->wIndexL > 1 ||
            UsbSetupBuf->wValueH != 0 || UsbSetupBuf->wValueL != 0 ||
            SetupLen != 0) len = 0xFF;
        break;
      case USB_CLEAR_FEATURE:
      case USB_SET_FEATURE:
        /* No remote wakeup is advertised. Validate the entire endpoint-halt
         * request before mutating either direction or its data toggle. */
        if (UsbSetupBuf->bRequestType != 0x02 || SetupLen != 0 ||
            UsbSetupBuf->wValueH != 0 || UsbSetupBuf->wValueL != 0 ||
            UsbSetupBuf->wIndexH != 0 ||
            !USBEndpointHalt(UsbSetupBuf->wIndexL, SetupReq == USB_SET_FEATURE))
          len = 0xFF;
        break;
      case USB_GET_STATUS: {
        __data uint8_t status = 0;
        if (SetupLen != 2 || UsbSetupBuf->wValueH != 0 ||
            UsbSetupBuf->wValueL != 0 || UsbSetupBuf->wIndexH != 0) {
          len = 0xFF;
          break;
        }
        switch (UsbSetupBuf->bRequestType) {
        case 0x80: /* Bus-powered device, no remote wakeup. */
          if (UsbSetupBuf->wIndexL != 0) status = 0xFF;
          break;
        case 0x81:
          if (!UsbConfig || UsbSetupBuf->wIndexL > 1) status = 0xFF;
          break;
        case 0x82:
          status = USBEndpointStatus(UsbSetupBuf->wIndexL);
          break;
        default:
          status = 0xFF;
          break;
        }
        if (status == 0xFF) {
          len = 0xFF;
          break;
        }
        Ep0Buffer[0] = status;
        Ep0Buffer[1] = 0;
        len = 2;
        break;
      }
      default:
        len = 0xff; // Failed
        break;
      }
    }
  } else {
    len = 0xff; // Wrong packet length
  }
  if (len == 0xff) {
    SetupReq = 0xFF;
    UEP0_CTRL =
        bUEP_R_TOG | bUEP_T_TOG | UEP_R_RES_STALL | UEP_T_RES_STALL; // STALL
  } else if (len <=
             DEFAULT_ENDP0_SIZE) // Tx data to host or send 0-length packet
  {
    UEP0_T_LEN = len;
    UEP0_CTRL = bUEP_R_TOG | bUEP_T_TOG | UEP_R_RES_ACK |
                (ControlLedPending ? UEP_T_RES_NAK : UEP_T_RES_ACK);
  } else {
    UEP0_T_LEN = 0; // Tx data to host or send 0-length packet
    UEP0_CTRL = bUEP_R_TOG | bUEP_T_TOG | UEP_R_RES_ACK |
                UEP_T_RES_ACK; // Expect DATA1, Answer ACK
  }
}

void USB_EP0_IN(void) {
  switch (SetupReq) {
  case USB_GET_DESCRIPTOR: {
    __data uint8_t len = SetupLen >= DEFAULT_ENDP0_SIZE
                             ? DEFAULT_ENDP0_SIZE
                             : SetupLen; // send length
    for (__data uint8_t i = 0; i < len; i++) {
      Ep0Buffer[i] = pDescr[i];
    }
    // memcpy( Ep0Buffer, pDescr, len );
    SetupLen -= len;
    pDescr += len;
    UEP0_T_LEN = len;
    UEP0_CTRL ^= bUEP_T_TOG; // Switch between DATA0 and DATA1
  } break;
  case USB_SET_ADDRESS:
    USB_DEV_AD = USB_DEV_AD & bUDA_GP_BIT | SetupLen;
    UEP0_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
    break;
  default:
    UEP0_T_LEN = 0; // End of transaction
    UEP0_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
    break;
  }
}

void USB_EP0_OUT(void) {
  if (ControlLedPending) {
    if (!U_TOG_OK) return;
    ControlLedPending = 0;
    if (USB_RX_LEN != 1) {
      UEP0_CTRL = UEP_R_RES_STALL | UEP_T_RES_STALL;
      return;
    }
    UsbLed = Ep0Buffer[0];
    UEP0_T_LEN = 0;
    UEP0_CTRL = bUEP_T_TOG | UEP_R_RES_NAK | UEP_T_RES_ACK;
    return; /* Control-write DATA1 IN status; completion returns IN to NAK. */
  }
  {
    UEP0_T_LEN = 0;
    UEP0_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK; /* Control-read status OUT. */
  }
}

#pragma save
#pragma nooverlay
void USBInterrupt(void) { // inline not really working in multiple files in SDCC
  if (UIF_TRANSFER) {
    // Dispatch to service functions
    __data uint8_t callIndex = USB_INT_ST & MASK_UIS_ENDP;
    switch (USB_INT_ST & MASK_UIS_TOKEN) {
    case UIS_TOKEN_OUT: { // SDCC will take IRAM if array of function pointer is
                          // used.
      switch (callIndex) {
      case 0:
        EP0_OUT_Callback();
        break;
      case 1:
        EP1_OUT_Callback();
        break;
      case 2:
        EP2_OUT_Callback();
        break;
      case 3:
        EP3_OUT_Callback();
        break;
      case 4:
        EP4_OUT_Callback();
        break;
      default:
        break;
      }
    } break;
    case UIS_TOKEN_SOF: { // SDCC will take IRAM if array of function pointer is
                          // used.
      switch (callIndex) {
      case 0:
        EP0_SOF_Callback();
        break;
      case 1:
        EP1_SOF_Callback();
        break;
      case 2:
        EP2_SOF_Callback();
        break;
      case 3:
        EP3_SOF_Callback();
        break;
      case 4:
        EP4_SOF_Callback();
        break;
      default:
        break;
      }
    } break;
    case UIS_TOKEN_IN: { // SDCC will take IRAM if array of function pointer is
                         // used.
      switch (callIndex) {
      case 0:
        EP0_IN_Callback();
        break;
      case 1:
        EP1_IN_Callback();
        break;
      case 2:
        EP2_IN_Callback();
        break;
      case 3:
        EP3_IN_Callback();
        break;
      case 4:
        EP4_IN_Callback();
        break;
      default:
        break;
      }
    } break;
    case UIS_TOKEN_SETUP: { // SDCC will take IRAM if array of function pointer
                            // is used.
      switch (callIndex) {
      case 0:
        EP0_SETUP_Callback();
        break;
      case 1:
        EP1_SETUP_Callback();
        break;
      case 2:
        EP2_SETUP_Callback();
        break;
      case 3:
        EP3_SETUP_Callback();
        break;
      case 4:
        EP4_SETUP_Callback();
        break;
      default:
        break;
      }
    } break;
    }

    UIF_TRANSFER = 0; // Clear interrupt flag
  }

  // Device mode USB bus reset
  if (UIF_BUS_RST) {
    UEP0_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
    UEP1_CTRL = bUEP_AUTO_TOG | UEP_T_RES_NAK | UEP_R_RES_ACK;
    UEP2_CTRL = bUEP_AUTO_TOG | UEP_T_RES_NAK | UEP_R_RES_ACK;

    USB_DEV_AD = 0x00;
    UIF_SUSPEND = 0;
    UIF_TRANSFER = 0;
    UIF_BUS_RST = 0;

    UsbConfig = 0;
    ControlLedPending = 0;
    USBTransportReset();

    // Clear interrupt flag
  }

  // USB bus suspend / wake up
  if (UIF_SUSPEND) {
    UIF_SUSPEND = 0;
    USBTransportSuspend((USB_MIS_ST & bUMS_SUSPEND) != 0);
    /* Only UIF_SUSPEND was handled. Other W1C flags may have arrived since
     * the transfer/reset branches above; leave them for their callbacks. */
  }
}
#pragma restore

void USBDeviceCfg(void) {
  USB_CTRL = 0x00;            // Clear USB control register
  USB_CTRL &= ~bUC_HOST_MODE; // This bit is the device selection mode
  USB_CTRL |= bUC_DEV_PU_EN | bUC_INT_BUSY |
              bUC_DMA_EN; // USB device and internal pull-up enable,
                          // automatically return to NAK before interrupt flag
                          // is cleared during interrupt
  USB_DEV_AD = 0x00;      // Device address initialization
  //     USB_CTRL |= bUC_LOW_SPEED;
  //     UDEV_CTRL |= bUD_LOW_SPEED; //Run for 1.5M
  USB_CTRL &= ~bUC_LOW_SPEED;
  UDEV_CTRL &= ~bUD_LOW_SPEED; // Select full speed 12M mode, default mode
#if defined(CH551) || defined(CH552) || defined(CH549)
  UDEV_CTRL = bUD_PD_DIS; // Disable DP/DM pull-down resistor
#endif
#if defined(CH559)
  UDEV_CTRL = bUD_DP_PD_DIS; // Disable DP/DM pull-down resistor
#endif
  UDEV_CTRL |= bUD_PORT_EN; // Enable physical port
}

void USBDeviceIntCfg(void) {
  USB_INT_EN |= bUIE_SUSPEND;  // Enable device hang interrupt
  USB_INT_EN |= bUIE_TRANSFER; // Enable USB transfer completion interrupt
  USB_INT_EN |= bUIE_BUS_RST;  // Enable device mode USB bus reset interrupt
  USB_INT_FG |= 0x1F;          // Clear interrupt flag
  IE_USB = 1;                  // Enable USB interrupt
  EA = 1;                      // Enable global interrupts
}

void USBDeviceEndPointCfg(void) {
#if defined(CH559)
  // CH559 use differend endianness for these registers
  UEP0_DMA_H = ((uint16_t)Ep0Buffer >> 8); // Endpoint 0 data transfer address
  UEP0_DMA_L = ((uint16_t)Ep0Buffer >> 0); // Endpoint 0 data transfer address
  UEP1_DMA_H = ((uint16_t)Ep1Buffer >> 8); // Endpoint 1 data transfer address
  UEP1_DMA_L = ((uint16_t)Ep1Buffer >> 0); // Endpoint 1 data transfer address
  UEP2_DMA_H = ((uint16_t)Ep2Buffer >> 8); // Endpoint 2 data transfer address
  UEP2_DMA_L = ((uint16_t)Ep2Buffer >> 0); // Endpoint 2 data transfer address
#else
  UEP0_DMA = (uint16_t)Ep0Buffer; // Endpoint 0 data transfer address
  UEP1_DMA = (uint16_t)Ep1Buffer; // Endpoint 1 data transfer address
  UEP2_DMA = (uint16_t)Ep2Buffer; // Endpoint 2 data transfer address
#endif
  UEP1_CTRL = bUEP_AUTO_TOG | UEP_T_RES_NAK |
              UEP_R_RES_ACK; // Endpoint 2 automatically flips the sync flag, IN
                             // transaction returns NAK, OUT returns ACK
  UEP2_CTRL = bUEP_AUTO_TOG | UEP_T_RES_NAK |
              UEP_R_RES_ACK; // Endpoint 2 automatically flips the sync flag, IN
                             // transaction returns NAK, OUT returns ACK
  UEP4_1_MOD = 0XC0;         // endpoint1 TX RX enable
  UEP2_3_MOD = 0X0C;         // endpoint2 TX RX enable
  UEP0_CTRL =
      UEP_R_RES_ACK | UEP_T_RES_NAK; // Manual flip, OUT transaction returns
                                     // ACK, IN transaction returns NAK
}
