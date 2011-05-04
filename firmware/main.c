/* -*- c-file-style: "stroustrup"; indent-tabs-mode: nil; -*- */
/* Name: main.c
 * Author: AGAWA Koji <atty303@gmail.com>
 * Copyright: AGAWA Koji
 * License: GPLv2
 */

#include <string.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/wdt.h>
#include <util/delay.h>
#include "usbdrv/usbdrv.h"

#include "config.h"
#include "psif.h"
#include "setting.h"

/* Declarations
   ================================================================ */

typedef struct {
    uchar x : 4;
    uchar y : 4;
    uchar z;
    uchar rx;
    uchar ry;
    uchar rz;
    union {
        uint16_t value;
        struct {
            uchar b1 : 1;
            uchar b2 : 1;
            uchar b3 : 1;
            uchar b4 : 1;
            uchar b5 : 1;
            uchar b6 : 1;
            uchar b7 : 1;
            uchar b8 : 1;
            uchar b9 : 1;
            uchar b10 : 1;
            uchar b11 : 1;
            uchar b12 : 1;
            uchar b13 : 1;
            uchar b14 : 1;
            uchar b15 : 1;
            uchar b16 : 1;
        } b;
    } buttons;
} report_t;

char lastTimer0Value;           /* required by osctune.h */

static uchar psdata[34];
static report_t reportBuffer;
static uchar idleRate; /* repeat rate for keyboards, never used for mice */


static inline uchar is_map_to_button(uchar m)
{
    return !(m & 0xF0);
}
static inline uchar map_button(uchar m)
{
    return m & 0x0F;
}
static inline uchar is_map_to_none(uchar m)
{
    return (m & 0xFF) == 0xFF;
}

static void handle_digital_button(uchar statebit, uchar *mapping)
{
    if (is_map_to_button(*mapping)) {
        if (!statebit) {
            /* デジタルボタンが押下されている */
            reportBuffer.buttons.value |= _BV(map_button(*mapping));
        } else {
            reportBuffer.buttons.value &= ~_BV(map_button(*mapping));
        }
    }
}
static void handle_linear_analog(uchar state, uchar *mapping, calibrate_t *calib)
{
    
}

/* PS device
   ================================================================ */

static void ps_main(void)
{
    setting_t *setting = setting_get();
    ps_read(psdata);

    reportBuffer.x = 8;
    reportBuffer.y = 8;

    /* ねじり 0x00 - 0x80 - 0xFF */
    reportBuffer.z = psdata[4];

    /* I Button */
    reportBuffer.rx = psdata[5];

    /* II Button */
    reportBuffer.ry = psdata[6];

    /* L Button */
    reportBuffer.ry = psdata[7];

    handle_digital_button(psdata[2] & 0x80, &setting->mapping.left);
    handle_digital_button(psdata[2] & 0x40, &setting->mapping.down);
    handle_digital_button(psdata[2] & 0x20, &setting->mapping.right);
    handle_digital_button(psdata[2] & 0x10, &setting->mapping.up);
    handle_digital_button(psdata[2] & 0x08, &setting->mapping.start);
    handle_digital_button(psdata[3] & 0x20, &setting->mapping.a);
    handle_digital_button(psdata[3] & 0x10, &setting->mapping.b);
    handle_digital_button(psdata[3] & 0x08, &setting->mapping.r);
}

static uchar *_current_pointer;
static uchar _bytes_remaining;
static void *_callback;

static usbMsgLen_t start_function_read_transfer(void *data, uchar len)
{
    _current_pointer = data;
    _bytes_remaining = len;
    return USB_NO_MSG;
}
static usbMsgLen_t start_function_write_transfer(void *data, uchar maxlen, void *callback)
{
    _current_pointer = data;
    _bytes_remaining = maxlen;
    _callback = callback;
    return USB_NO_MSG;
}

/* usbFunctionRead() is called when the host requests a chunk of data from
 * the device. For more information see the documentation in usbdrv/usbdrv.h.
 */
uchar usbFunctionRead(uchar *data, uchar len)
{
    if (len > _bytes_remaining)
        len = _bytes_remaining;
    memcpy(data, _current_pointer, len);
    _current_pointer += len;
    _bytes_remaining -= len;
    return len;
}

/* usbFunctionWrite() is called when the host sends a chunk of data to the
 * device. For more information see the documentation in usbdrv/usbdrv.h.
 */
uchar usbFunctionWrite(uchar *data, uchar len)
{
    if (_bytes_remaining == 0)
        return 1;               /* end of transfer */
    if (len > _bytes_remaining)
        len = _bytes_remaining;
    memcpy(_current_pointer, data, len);
    _current_pointer += len;
    _bytes_remaining -= len;
    return _bytes_remaining == 0; /* return 1 if this was the last chunk */
}

static usbMsgLen_t handle_get_report_input(usbRequest_t *rq)
{
    usbMsgPtr = (void *)&reportBuffer;
    return sizeof(reportBuffer);
}

static usbMsgLen_t handle_get_report_feature(usbRequest_t *rq)
{
    usbMsgLen_t len = 2 + ((psdata[0] & 0x0f) * 2);
    if (len > rq->wLength.word)
        len = rq->wLength.word;
    usbMsgPtr = psdata;
    return len;
}

static usbMsgLen_t handle_set_report_output(usbRequest_t *rq)
{
    return 0;
}

static usbMsgLen_t handle_set_report_feature(usbRequest_t *rq)
{
    return 0;
}

usbMsgLen_t usbFunctionSetup(uchar data[8])
{
    usbRequest_t *rq = (void *)data;

    if ((rq->bmRequestType & USBRQ_TYPE_MASK) == USBRQ_TYPE_CLASS) {
        if (rq->bRequest == USBRQ_HID_GET_REPORT) {
            /* wValue: ReportType (highbyte), ReportID (lowbyte) */
            if (rq->wValue.bytes[1] == 1) {
                return handle_get_report_input(rq);
            } else if (rq->wValue.bytes[1] == 3) {
                return handle_get_report_feature(rq);
            }
        } else if (rq->bRequest == USBRQ_HID_SET_REPORT) {
            if (rq->wValue.bytes[1] == 2) {
                return handle_set_report_output(rq);
            } else if (rq->wValue.bytes[1] == 3) {
                return handle_set_report_feature(rq);
            }
        } else if (rq->bRequest == USBRQ_HID_GET_IDLE) {
            usbMsgPtr = &idleRate;
            return 1;
        } else if (rq->bRequest == USBRQ_HID_SET_IDLE) {
            idleRate = rq->wValue.bytes[1];
        }
    }
    return 0;   /* default for not implemented requests: return no data back to host */
}

static void usb_reenumerate(void)
{
    uchar i = 0;
    usbDeviceDisconnect();  /* enforce re-enumeration, do this while interrupts are disabled! */
    while (--i) {           /* fake USB disconnect for > 250 ms */
        wdt_reset();
        _delay_ms(1);
    }
    usbDeviceConnect();
}

int __attribute__((noreturn)) main(void)
{
    wdt_enable(WDTO_1S);

    TCCR0B = 3;                 /* required by osctune.h */

    setting_init();

    usbInit();
    ps_init();
    usb_reenumerate();

    sei();

    for (;;) {                /* main event loop */
        wdt_reset();
        usbPoll();
        if (usbInterruptIsReady()) {
            /* called after every poll of the interrupt endpoint */
            ps_main();
            usbSetInterrupt((void *)&reportBuffer, sizeof(reportBuffer));
        }
    }
}
