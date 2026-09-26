#ifndef TUSB_CONFIG_H_
#define TUSB_CONFIG_H_

#ifndef CFG_TUSB_MCU
#error CFG_TUSB_MCU must be defined by the Pico SDK
#endif

#define CFG_TUSB_OS                 OPT_OS_NONE
#define CFG_TUSB_RHPORT0_MODE       OPT_MODE_HOST

#ifndef CFG_TUSB_MEM_SECTION
#define CFG_TUSB_MEM_SECTION
#endif

#ifndef CFG_TUSB_MEM_ALIGN
#define CFG_TUSB_MEM_ALIGN          __attribute__((aligned(4)))
#endif

#define CFG_TUH_ENUMERATION_BUFSIZE 256
#define CFG_TUH_HUB                 0
#define CFG_TUH_CDC                 0
#define CFG_TUH_HID                 0
#define CFG_TUH_MSC                 0
#define CFG_TUH_VENDOR              0
#define CFG_TUH_DEVICE_MAX          1
#define CFG_TUH_MIDI                1

#endif
