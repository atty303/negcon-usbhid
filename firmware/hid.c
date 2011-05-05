/* -*- c-file-style: "stroustrup"; indent-tabs-mode: nil; -*- */

#include <avr/pgmspace.h>
#include "usbdrv/usbdrv.h"

#include "hid_intern.h"

/* USB report descriptor, size must match usbconfig.h */
PROGMEM char usbHidReportDescriptor[68] = {
  G_USAGE_PAGE(1), 0x01,        /* Generic Desktop */
  L_USAGE(1), 0x04,             /* Joystick */
  M_COLLECTION(1), C_APPLICATION,
    L_USAGE(1), 0x01,           /* Pointer */
    M_COLLECTION(1), C_PHYSICAL,

      G_USAGE_PAGE(1), 0x09,    /* Button */
      L_USAGE_MINIMUM(1), 0x01, /* Button 1 */
      L_USAGE_MAXIMUM(1), 0x10, /* Button 16 */
      G_LOGICAL_MINIMUM(1), 0x00,
      G_LOGICAL_MAXIMUM(1), 0x01,
      G_REPORT_COUNT(1), 0x10,
      G_REPORT_SIZE(1), 0x01,
      M_INPUT(1), IOF_VARIABLE,

      G_USAGE_PAGE(1), 0x01,    /* Generic Desktop */
      L_USAGE(1), 0x30,         /* X */
      L_USAGE(1), 0x31,         /* Y */
      L_USAGE(1), 0x32,         /* Z */
      L_USAGE(1), 0x33,         /* Rx */
      L_USAGE(1), 0x34,         /* Ry */
      L_USAGE(1), 0x35,         /* Rz */
      G_LOGICAL_MINIMUM(1), (uchar)-128,
      G_LOGICAL_MAXIMUM(1), (uchar)127,
      G_REPORT_SIZE(1), 0x08,
      G_REPORT_COUNT(1), 0x06,
      M_INPUT(1), IOF_VARIABLE,
    M_END_COLLECTION(0),

    /* Feature Report: 128 bytes for generic use */
    G_USAGE_PAGE(2), 0x00, 0xFF,  /* Vendor Defined */
    L_USAGE(1), 0x00,
    G_LOGICAL_MINIMUM(1), 0x00,
    G_LOGICAL_MAXIMUM(2), 0xFF, 0x00,
    G_REPORT_SIZE(1), 0x08,
    G_REPORT_COUNT(1), 0x80,
    M_FEATURE(1), IOF_VARIABLE,

  M_END_COLLECTION(0),
};
