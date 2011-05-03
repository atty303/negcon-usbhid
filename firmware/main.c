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

/* ------------------------------------------------------------------------- */
/* ------------------------------ PS interface ----------------------------- */
/* ------------------------------------------------------------------------- */

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


static uchar psdata[34];


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

static int ps_main(void)
{
    ps_read(psdata);
    if (!(psdata[2] & 0x80)) {
        return -1;
    }
    if (!(psdata[2] & 0x20)) {
        return 1;
    }
    return 0;
}

/* ------------------------------------------------------------------------- */
/* ----------------------------- USB interface ----------------------------- */
/* ------------------------------------------------------------------------- */

PROGMEM char usbHidReportDescriptor[52] = { /* USB report descriptor, size must match usbconfig.h */
    0x05, 0x01,                    // USAGE_PAGE (Generic Desktop)
    0x09, 0x02,                    // USAGE (Mouse)
    0xa1, 0x01,                    // COLLECTION (Application)
    0x09, 0x01,                    //   USAGE (Pointer)
    0xA1, 0x00,                    //   COLLECTION (Physical)
    0x05, 0x09,                    //     USAGE_PAGE (Button)
    0x19, 0x01,                    //     USAGE_MINIMUM
    0x29, 0x03,                    //     USAGE_MAXIMUM
    0x15, 0x00,                    //     LOGICAL_MINIMUM (0)
    0x25, 0x01,                    //     LOGICAL_MAXIMUM (1)
    0x95, 0x03,                    //     REPORT_COUNT (3)
    0x75, 0x01,                    //     REPORT_SIZE (1)
    0x81, 0x02,                    //     INPUT (Data,Var,Abs)
    0x95, 0x01,                    //     REPORT_COUNT (1)
    0x75, 0x05,                    //     REPORT_SIZE (5)
    0x81, 0x03,                    //     INPUT (Const,Var,Abs)
    0x05, 0x01,                    //     USAGE_PAGE (Generic Desktop)
    0x09, 0x30,                    //     USAGE (X)
    0x09, 0x31,                    //     USAGE (Y)
    0x09, 0x38,                    //     USAGE (Wheel)
    0x15, 0x81,                    //     LOGICAL_MINIMUM (-127)
    0x25, 0x7F,                    //     LOGICAL_MAXIMUM (127)
    0x75, 0x08,                    //     REPORT_SIZE (8)
    0x95, 0x03,                    //     REPORT_COUNT (3)
    0x81, 0x06,                    //     INPUT (Data,Var,Rel)
    0xC0,                          //   END_COLLECTION
    0xC0,                          // END COLLECTION
};
/* This is the same report descriptor as seen in a Logitech mouse. The data
 * described by this descriptor consists of 4 bytes:
 *      .  .  .  .  . B2 B1 B0 .... one byte with mouse button states
 *     X7 X6 X5 X4 X3 X2 X1 X0 .... 8 bit signed relative coordinate x
 *     Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0 .... 8 bit signed relative coordinate y
 *     W7 W6 W5 W4 W3 W2 W1 W0 .... 8 bit signed relative coordinate wheel
 */
typedef struct{
    uchar   buttonMask;
    char    dx;
    char    dy;
    char    dWheel;
}report_t;

static report_t reportBuffer;
static uchar    idleRate;   /* repeat rate for keyboards, never used for mice */

/* required by osctune.h */
char lastTimer0Value;

/* ------------------------------------------------------------------------- */

usbMsgLen_t usbFunctionSetup(uchar data[8])
{
usbRequest_t    *rq = (void *)data;

    /* The following requests are never used. But since they are required by
     * the specification, we implement them in this example.
     */
    if((rq->bmRequestType & USBRQ_TYPE_MASK) == USBRQ_TYPE_CLASS){    /* class request type */
        if(rq->bRequest == USBRQ_HID_GET_REPORT){  /* wValue: ReportType (highbyte), ReportID (lowbyte) */
            /* we only have one report type, so don't look at wValue */
            usbMsgPtr = (void *)&reportBuffer;
            return sizeof(reportBuffer);
        }else if(rq->bRequest == USBRQ_HID_GET_IDLE){
            usbMsgPtr = &idleRate;
            return 1;
        }else if(rq->bRequest == USBRQ_HID_SET_IDLE){
            idleRate = rq->wValue.bytes[1];
        }
    }else{
        if (rq->bRequest == 0x01) {
            usbMsgLen_t len = 2 + ((psdata[0] & 0x0f) * 2);
            if (len > rq->wLength.word)
                len = rq->wLength.word;
            usbMsgPtr = psdata;
            return len;
        }
        /* no vendor specific requests implemented */
    }
    return 0;   /* default for not implemented requests: return no data back to host */
}

/* ------------------------------------------------------------------------- */
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

    /* required by osctune.h */
    TCCR0B = 3;
    usbInit();
    ps_init();
    usb_reenumerate();
    sei();

    for (;;) {                /* main event loop */
        wdt_reset();
        usbPoll();
        if (usbInterruptIsReady()) {
            /* called after every poll of the interrupt endpoint */
            reportBuffer.dx = ps_main();
            reportBuffer.dy = 0;
            usbSetInterrupt((void *)&reportBuffer, sizeof(reportBuffer));
        }
    }
}
