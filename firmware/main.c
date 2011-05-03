/* -*- c-file-style: "stroustrup"; indent-tabs-mode: nil; -*- */
/* Name: main.c
 * Author: AGAWA Koji <atty303@gmail.com>
 * Copyright: AGAWA Koji
 * License: GPLv2
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

#define PS_OUT USB_OUTPORT(PS_CFG_IOPORTNAME) /* PORTx register for PS IF */
#define PS_IN  USB_INPORT(PS_CFG_IOPORTNAME)  /* PINx register for PS IF */
#define PS_DDR USB_DDRPORT(PS_CFG_IOPORTNAME) /* DDRx register for PS IF */
#define PS_PORT_MASK (_BV(PS_CFG_SEL_BIT) | _BV(PS_CFG_CLK_BIT) | _BV(PS_CFG_CMD_BIT) | _BV(PS_CFG_DAT_BIT))

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

/* USB report descriptor, size must match usbconfig.h */
PROGMEM char usbHidReportDescriptor[63] = {
    0x05, 0x01,                 // USAGE_PAGE (Generic Desktop)
    0x09, 0x04,                 // USAGE (Joystick)
    0xa1, 0x01,                 // COLLECTION (Application)
    0x09, 0x01,                 //   USAGE (Pointer)
    0xa1, 0x00,                 //   COLLECTION (Physical)
    0x85, 0x01,                 //     REPORT_ID (1)

    0x09, 0x30,                 //     USAGE (X)
    0x09, 0x31,                 //     USAGE (Y)
    0x15, 0x00,                 //     LOGICAL_MINIMUM (0)
    0x25, 0x0F,                 //     LOGICAL_MAXIMUM (15)
    0x75, 0x01,                 //     REPORT_SIZE (4)
    0x95, 0x02,                 //     REPORT_COUNT (2)
    0x81, 0x02,                 //     INPUT (Data,Var,Abs)

    0x09, 0x32,                 //     USAGE (Z)
    0x09, 0x33,                 //     USAGE (Rx)
    0x09, 0x34,                 //     USAGE (Ry)
    0x09, 0x35,                 //     USAGE (Rz)
    0x15, 0x00,                 //     LOGICAL_MINIMUM (0)
    0x26, 0xff, 0x00,           //     LOGICAL_MAXIMUM (255)
    0x75, 0x08,                 //     REPORT_SIZE (8)
    0x95, 0x04,                 //     REPORT_COUNT (4)
    0x81, 0x02,                 //     INPUT (Data,Var,Abs)

    0x05, 0x09,                 //     USAGE_PAGE (Button)
    0x19, 0x01,                 //     USAGE_MINIMUM (Button 1)
    0x29, 0x0B,                 //     USAGE_MAXIMUM (Button 11)
    0x15, 0x00,                 //     LOGICAL_MINIMUM (0)
    0x25, 0x01,                 //     LOGICAL_MAXIMUM (1)
    0x75, 0x01,                 //     REPORT_SIZE (1)
    0x95, 0x0B,                 //     REPORT_COUNT (11)
    0x81, 0x02,                 //     INPUT (Data,Var,Abs)
    0xc0,                       //   END_COLLECTION
    0xc0,                       // END_COLLECTION
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
    PS_DDR = (PS_DDR & ~(PS_PORT_MASK)) | (_BV(PS_CFG_SEL_BIT) | _BV(PS_CFG_CLK_BIT) | _BV(PS_CFG_CMD_BIT));
    /* DAT入力をプルアップし、SEL,CLK,CMDは初期状態で全てH出力とする */
    PS_OUT |= PS_PORT_MASK;
}

/* デバイスを1バイトのコマンドを送信し、受信した応答を返す。
   @param[in]  cmd  デバイスに送信するコマンドバイト
   @return          デバイスから受信したデータ
 */
static uchar ps_sendrecv(uchar cmd)
{
    uchar i = 8;
    uchar data = 0;

    while (i--) {
        /* CLKを立ち下げ */
        PS_OUT &= ~(_BV(PS_CFG_CLK_BIT));
        /* CMDビットを出力 */
        if (cmd & 1) {
            PS_OUT |= _BV(PS_CFG_CMD_BIT);
        } else {
            PS_OUT &= ~(_BV(PS_CFG_CMD_BIT));
        }
        cmd >>= 1;
        _delay_us(PS_CFG_CLK_DELAY_US);

        /* CLKを立ち上げ */
        PS_OUT |= _BV(PS_CFG_CLK_BIT);
        data >>= 1;
        /* DATを読み取り */
        data |= (PS_IN & _BV(PS_CFG_DAT_BIT)) ? 0x80 : 0x00;
        _delay_us(PS_CFG_CLK_DELAY_US);
    }

    /* ACKが来るであろう時間まで待つ。ACKそのものの確認はしない。 */
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
    PS_OUT &= ~(_BV(PS_CFG_SEL_BIT));
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
    PS_OUT |= _BV(PS_CFG_SEL_BIT);
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
