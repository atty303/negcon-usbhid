/* -*- c-file-style: "stroustrup"; indent-tabs-mode: nil; -*- */
/* Name: main.c
 * Author: <insert your name here>
 * Copyright: <insert your copyright message here>
 * License: <insert your license reference here>
 *
 * http://blog.livedoor.jp/ikehiro/archives/456433.html
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/wdt.h>
#include <util/delay.h>

#include "usbdrv.h"
#include "config.h"

/* Declarations
   ================================================================ */

#define PSOUT USB_OUTPORT(PS_CFG_IOPORTNAME)
#define PSIN USB_INPORT(PS_CFG_IOPORTNAME)
#define PSDDR USB_DDRPORT(PS_CFG_IOPORTNAME)

#define PS_PORT_MASK (_BV(PS_CFG_SEL_BIT) | _BV(PS_CFG_CLK_BIT) | _BV(PS_CFG_CMD_BIT) | _BV(PS_CFG_DAT_BIT))

#define PS_SEL1() { PSOUT |= _BV(PS_CFG_SEL_BIT); }
#define PS_SEL0() { PSOUT &= ~(_BV(PS_CFG_SEL_BIT)); }
#define PS_CLK1() { PSOUT |= _BV(PS_CFG_CLK_BIT); }
#define PS_CLK0() { PSOUT &= ~(_BV(PS_CFG_CLK_BIT)); }
#define PS_CMD1() { PSOUT |= _BV(PS_CFG_CMD_BIT); }
#define PS_CMD0() { PSOUT &= ~(_BV(PS_CFG_CMD_BIT)); }

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

PROGMEM char usbHidReportDescriptor[63] = { /* USB report descriptor, size must match usbconfig.h */
    0x05, 0x01,                    // USAGE_PAGE (Generic Desktop)
    0x09, 0x04,                    // USAGE (Joystick)
    0xa1, 0x01,                    // COLLECTION (Application)
    0x09, 0x01,                    //   USAGE (Pointer)
    0xa1, 0x00,                    //   COLLECTION (Physical)
    0x85, 0x01,                    //     REPORT_ID (1)

    0x09, 0x30,                    //     USAGE (X)
    0x09, 0x31,                    //     USAGE (Y)
    0x15, 0x00,                    //     LOGICAL_MINIMUM (0)
    0x25, 0x0F,                    //     LOGICAL_MAXIMUM (15)
    0x75, 0x01,                    //     REPORT_SIZE (4)
    0x95, 0x02,                    //     REPORT_COUNT (2)
    0x81, 0x02,                    //     INPUT (Data,Var,Abs)

    0x09, 0x32,                    //     USAGE (Z)
    0x09, 0x33,                    //     USAGE (Rx)
    0x09, 0x34,                    //     USAGE (Ry)
    0x09, 0x35,                    //     USAGE (Rz)
    0x15, 0x00,                    //     LOGICAL_MINIMUM (0)
    0x26, 0xff, 0x00,              //     LOGICAL_MAXIMUM (255)
    0x75, 0x08,                    //     REPORT_SIZE (8)
    0x95, 0x04,                    //     REPORT_COUNT (4)
    0x81, 0x02,                    //     INPUT (Data,Var,Abs)

    0x05, 0x09,                    //     USAGE_PAGE (Button)
    0x19, 0x01,                    //     USAGE_MINIMUM (Button 1)
    0x29, 0x0B,                    //     USAGE_MAXIMUM (Button 11)
    0x15, 0x00,                    //     LOGICAL_MINIMUM (0)
    0x25, 0x01,                    //     LOGICAL_MAXIMUM (1)
    0x75, 0x01,                    //     REPORT_SIZE (1)
    0x95, 0x0B,                    //     REPORT_COUNT (11)
    0x81, 0x02,                    //     INPUT (Data,Var,Abs)
    0xc0,                          //     END_COLLECTION
    0xc0,                          // END_COLLECTION
};

char lastTimer0Value;           /* required by osctune.h */

static uchar psdata[34];
static report_t reportBuffer;
static uchar idleRate; /* repeat rate for keyboards, never used for mice */

/* 
   ================================================================ */

static void ps_init(void)
{
    /* SEL,CLK,CMDを出力に、DATを入力に設定する */
    PSDDR = (PSDDR & ~(PS_PORT_MASK)) | (_BV(PS_CFG_SEL_BIT) | _BV(PS_CFG_CLK_BIT) | _BV(PS_CFG_CMD_BIT));
    /* DAT入力をプルアップする */
    PSOUT |= _BV(PS_CFG_DAT_BIT);

    PS_SEL1();
    PS_CLK1();
    PS_CMD1();
}

/* コントローラへコマンドを送り、
   コントローラからの返答を返す */
static uchar ps_sendrecv(uchar cmd)
{
    uchar i = 8;
    uchar data = 0;

    while (i--) {
        PS_CLK0();
        if (cmd & 1) { PS_CMD1(); } else { PS_CMD0(); }
        cmd >>= 1;
        _delay_us(PS_CFG_CLK_DELAY_US);
        PS_CLK1();
        data >>= 1;
        data |= (PSIN & _BV(PS_CFG_DAT_BIT)) ? 0x80 : 0x00;
        _delay_us(PS_CFG_CLK_DELAY_US);
    }

    /* ACK待ち */
    _delay_us(PS_CFG_ACK_DELAY_US);

    return data;
}

/*
  @param[in]  output  デバイスから受信したデータを保存するバッファへのポインタ
  @note outputバッファは34バイト無いとバッファオーバーフローの可能性がある。
        受信データは(固定の2バイト + 可変の2〜32バイト)であるため。
 */
static void ps_read(uchar *output)
{
    /* 4バイト目以降のデータ転送ワード数(1ワード=2バイト)。デバイスからのレスポンスにより決まる。 */
    uchar data_len;

    /* SELを立ち下げてデバイスとの通信を開始する。 */
    /* その後の最初のCLK立ち下げまでに長めのウェイトを入れないと通信開始に失敗する。 */
    PS_SEL0();
    _delay_us(PS_CFG_SEL_DELAY_US);

    /* 1バイト目: CMD=0x01, DAT=不定 */
    ps_sendrecv(0x01);

    /* 2バイト目: CMD=0x42, DAT=上位4ビット:デバイスタイプ 下位4ビット:data_len */
    *output = ps_sendrecv(0x42);

    data_len = (*output++ & 0x0f);
    /* data_len == 0 は 0x10 として扱う */
    if (data_len == 0) {
        data_len = 0x10;
    }

    /* 3バイト目: CMD=0x00, DAT=0x5A */
    *output++ = ps_sendrecv(0x00);

    /* 4バイト目以降 */
    while (data_len--) {
        *output++ = ps_sendrecv(0x00);
        *output++ = ps_sendrecv(0x00);
    }

    /* SELを立ち上げてデバイスとの通信を終了する。 */
    PS_SEL1();
}

static void ps_main(void)
{
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

    reportBuffer.buttons.b.b1 = (psdata[2] & 0x80) ? 0 : 1; /* left */
    reportBuffer.buttons.b.b2 = (psdata[2] & 0x40) ? 0 : 1; /* down */
    reportBuffer.buttons.b.b3 = (psdata[2] & 0x20) ? 0 : 1; /* right */
    reportBuffer.buttons.b.b4 = (psdata[2] & 0x10) ? 0 : 1; /* up */
    reportBuffer.buttons.b.b5 = (psdata[2] & 0x08) ? 0 : 1; /* start */
    reportBuffer.buttons.b.b6 = (psdata[3] & 0x20) ? 0 : 1; /* a */
    reportBuffer.buttons.b.b7 = (psdata[3] & 0x10) ? 0 : 1; /* b */
    reportBuffer.buttons.b.b8 = (psdata[3] & 0x08) ? 0 : 1; /* r */
}

usbMsgLen_t usbFunctionSetup(uchar data[8])
{
    usbRequest_t *rq = (void *)data;

    /* The following requests are never used. But since they are required by
     * the specification, we implement them in this example.
     */
    if ((rq->bmRequestType & USBRQ_TYPE_MASK) == USBRQ_TYPE_CLASS) {    /* class request type */
        if (rq->bRequest == USBRQ_HID_GET_REPORT) {  /* wValue: ReportType (highbyte), ReportID (lowbyte) */
            /* we only have one report type, so don't look at wValue */
            usbMsgPtr = (void *)&reportBuffer;
            return sizeof(reportBuffer);
        } else if (rq->bRequest == USBRQ_HID_GET_IDLE) {
            usbMsgPtr = &idleRate;
            return 1;
        } else if (rq->bRequest == USBRQ_HID_SET_IDLE) {
            idleRate = rq->wValue.bytes[1];
        }
    } else {
        if (rq->bRequest == 0x01) {
            usbMsgLen_t len = 2 + ((psdata[0] & 0x0f) * 2);
            if (len > rq->wLength.word)
                len = rq->wLength.word;
            usbMsgPtr = psdata;
            return len;
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
