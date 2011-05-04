/* -*- c-file-style: "stroustrup"; indent-tabs-mode: nil; -*- */

#include <avr/pgmspace.h>
#include "usbdrv.h"

/* data for Input/Output/Feature */
#define IOF_CONSTANT (1<<0)	/* 0: Data */
#define IOF_VARIABLE (1<<1)	/* 0: Array */
#define IOF_RELATIVE (1<<2)	/* 0: Absolute */
#define IOF_WRAP     (1<<3)	/* 0: No Wrap */
#define IOF_NONLINEAR (1<<4)	/* 0: Linear */
#define IOF_NOPREFERRED (1<<5)	/* 0: Preferred */
#define IOF_NULLSTATE (1<<6)	/* 0: No Null position */
/* data for Output/Feature */
#define OF_VOLATIVE (1<<7)	/* 0: Non Volatile */
#define OF_BUFFERED_BYTES (1<<8) /* 0: Bit Field */
/* data for Collection */
#define C_PHYSICAL 0x00
#define C_APPLICATION 0x01
#define C_LOGICAL 0x02
#define C_REPORT 0x03
#define C_NAMED_ARRAY 0x04
#define C_USAGE_SWITCH 0x05
#define C_USAGE_MODIFIER 0x06

#define _SIZE(size) (size == 4 ? 3 : size)
#define _ITEM(bTag, bType, bSize) (((bTag&15)<<4) | (bType<<2) | _SIZE(bSize))
#define _ITEM_MAIN(bTag, bSize)   _ITEM(bTag, 0, bSize)
#define _ITEM_GLOBAL(bTag, bSize) _ITEM(bTag, 1, bSize)
#define _ITEM_LOCAL(bTag, bSize)  _ITEM(bTag, 2, bSize)

/* Main Items */
#define M_INPUT(s)		_ITEM_MAIN(8, s)
#define M_OUTPUT(s)		_ITEM_MAIN(9, s)
#define M_FEATURE(s)		_ITEM_MAIN(11, s)
#define M_COLLECTION(s)		_ITEM_MAIN(10, s)
#define M_END_COLLECTION(s)	_ITEM_MAIN(12, s)

/* Global Items */
#define G_USAGE_PAGE(s)		_ITEM_GLOBAL(0, s)
#define G_LOGICAL_MINIMUM(s)	_ITEM_GLOBAL(1, s)
#define G_LOGICAL_MAXIMUM(s)	_ITEM_GLOBAL(2, s)
#define G_PHYSICAL_MINIMUM(s)	_ITEM_GLOBAL(3, s)
#define G_PHYSICAL_MAXIMUM(s)	_ITEM_GLOBAL(4, s)
#define G_UNIT_EXPONENT(s)	_ITEM_GLOBAL(5, s)
#define G_UNIT(s)		_ITEM_GLOBAL(6, s)
#define G_REPORT_SIZE(s)	_ITEM_GLOBAL(7, s)
#define G_REPORT_ID(s)		_ITEM_GLOBAL(8, s)
#define G_REPORT_COUNT(s)	_ITEM_GLOBAL(9, s)
#define G_PUSH(s)		_ITEM_GLOBAL(10, s)
#define G_POP(s)		_ITEM_GLOBAL(11, s)

/* Local Items */
#define L_USAGE(s)		_ITEM_LOCAL(0, s)
#define L_USAGE_MINIMUM(s)	_ITEM_LOCAL(1, s)
#define L_USAGE_MAXIMUM(s)	_ITEM_LOCAL(2, s)
#define L_DESIGNATOR_INDEX(s)	_ITEM_LOCAL(3, s)
#define L_DESIGNATOR_MINIMUM(s)	_ITEM_LOCAL(4, s)
#define L_DESIGNATOR_MAXIMUM(s)	_ITEM_LOCAL(5, s)
#define L_STRING_INDEX(s)	_ITEM_LOCAL(7, s)
#define L_STRING_MINIMUM(s)	_ITEM_LOCAL(8, s)
#define L_STRING_MAXIMUM(s)	_ITEM_LOCAL(9, s)
#define L_DELIMITER(s)		_ITEM_LOCAL(10, s)


PROGMEM char usbHidReportDescriptor[40] = {
  G_USAGE_PAGE(1), 0x01,      /* Generic Desktop */
  L_USAGE(1), 0x04,           /* Joystick */
  M_COLLECTION(1), C_APPLICATION,
    L_USAGE(1), 0x01,        /* Pointer */
    M_COLLECTION(1), C_PHYSICAL,
      L_USAGE(1), 0x30,     /* X */
      L_USAGE(1), 0x31,     /* Y */
      G_LOGICAL_MINIMUM(1), 0x00,
      G_LOGICAL_MAXIMUM(1), 0x0F,
      G_REPORT_SIZE(1), 0x04,
      G_REPORT_COUNT(1), 0x02,
      M_INPUT(1), IOF_VARIABLE,
    M_END_COLLECTION(0),        /* Physical */

    G_LOGICAL_MINIMUM(1), 0x00,
    G_LOGICAL_MAXIMUM(2), 0xFF, 0x00,
    G_REPORT_SIZE(1), 0x08,
    G_REPORT_COUNT(1), 0x80,
    L_USAGE(2), 0x00, 0x00,    /* Undefined */
    M_FEATURE(1), IOF_VARIABLE,

  M_END_COLLECTION(0),        /* Application */
};
