/* -*- c-file-style: "stroustrup"; -*- */
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
#include "oddebug.h"        /* This is also an example for using debug macros */
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

/* ATtiny45 = TCNT1 */
#define PS_TCNT TCNT2
/* ATtiny45 = TCCR1 */
#define PS_TCCR TCCR2B
/* ATtiny45 = (_BV(3)|_BV(2)|_BV(1)|_BV(0)) */
#define PS_TCCR_MASK (_BV(2)|_BV(1)|_BV(0))
#define PS_TCCR_CS_CK1 (0) /* Clock Select: 分周なし */
#define PS_TCCR_CS_CK8 (_BV(0)) /* Clock Select: 1/8分周 */
#define PS_TIMER_OVF_VECT TIMER2_OVF_vect

/* PSパッドのCLKの間隔(250KHz/4usec)をタイマーのカウント値へ算出する。
   実際のクロックは諸々のオーバーヘッドにより250KHzより遅くなるが、
   PSパッド動作に支障は無いはずである。
 */
#define PS_TIMER_CLK_HZ (250L * 1000)
#define PS_TIMER_CLK_PRESCALING 1
#define PS_TIMER_CLK_TICK ((F_CPU / PS_TIMER_CLK_PRESCALING) / PS_TIMER_CLK_HZ)
#if PS_TIMER_ACK_TICK >= 0x00 && PS_TIMER_ACK_TICK <= 0xff
#  define PS_TIMER_CLK_TCCR PS_TCCR_CS_CK1
#else
#  error "invalid timer configuration"
#endif
/* CLKの間隔を待つタイマーを開始する際に設定するカウント値を算出する。
   タイマーのオーバーフロー割り込みによりクロックを検出するので、
   オーバーフロー直前の値からクロック間隔を減算した値となる。
 */
#define PS_TIMER_CLK_TCNT (uchar)(0xff - PS_TIMER_CLK_TICK)

/* PSパッドへのデータ転送完了からACKが返るまでのタイマーカウント値を算出する。
   PS本体はパッドから10KHz/100usec以内に応答がなければ未接続と認識するので、
   逆説的に常に100usec待てば次のデータ転送が可能となるはずである。
   よって、ACKを確認せずにPSパッドとコミュニケーションする。
 */
#define PS_TIMER_ACK_HZ (10L * 1000)
#define PS_TIMER_ACK_PRESCALING 1
#define PS_TIMER_ACK_TICK ((F_CPU / PS_TIMER_ACK_PRESCALING) / PS_TIMER_ACK_HZ)
#if PS_TIMER_ACK_TICK >= 0x00 && PS_TIMER_ACK_TICK <= 0xff
#  define PS_TIMER_ACK_TCCR PS_TCCR_CS_CK1
#else
#  undef PS_TIMER_ACK_PRESCALING
#  undef PS_TIMER_ACK_TICK
#  define PS_TIMER_ACK_PRESCALING 8
#  define PS_TIMER_ACK_TICK ((F_CPU / PS_TIMER_ACK_PRESCALING) / PS_TIMER_ACK_HZ)
#  if PS_TIMER_ACK_TICK >= 0x00 && PS_TIMER_ACK_TICK <= 0xff
#    define PS_TIMER_ACK_TCCR PS_TCCR_CS_CK8
#  else
#    error "invalid ack timer configuration"
#  endif
#endif
#define PS_TIMER_ACK_TCNT (uchar)(0xff - PS_TIMER_ACK_TICK)


volatile static char wait_flag = 0;  /* wait_clkで使われる */

static void ps_timer_start(uchar tccr, uchar tcnt)
{
    wait_flag = 0;
    PS_TCCR = (PS_TCCR & ~(PS_TCCR_MASK)) | tccr;
    PS_TCNT = tcnt;
}

ISR(PS_TIMER_OVF_VECT)
{
    wait_flag = !0;
    PS_TCCR &= ~(PS_TCCR_MASK);
}

static void ps_timer_wait(void)
{
    for (;;) {
	if (wait_flag) break;
    }
}

static void ps_init(void)
{
    /* SEL,CLK,CMDを出力に、DATを入力に設定する */
    PSDDR = (PSDDR & ~(PS_PORT_MASK)) | (_BV(PS_CFG_SEL_BIT) | _BV(PS_CFG_CLK_BIT) | _BV(PS_CFG_CMD_BIT));
    /* DAT入力をプルアップする */
    PSOUT |= _BV(PS_CFG_DAT_BIT);

    /* FIXME: タイマー割り込みを有効にする */
    /* TIMSK2 |= TOIE2; */

    PS_SEL1();
    PS_CLK1();
    PS_CMD1();
}

/* コントローラへコマンドを送り、
   コントローラからの返答を返す */
static uchar ps_putgetc(uchar cmd)
{
    /* uchar i = 16; */
    uchar i = 8;
    uchar data = 0;

//    PS_CLK1();
//    PS_CMD1();
//    _delay_us(1);

    while (i--) {
#if 0
//	ps_timer_start(PS_TIMER_CLK_TCCR, PS_TIMER_CLK_TCNT);
	if (i & 1) {
	    /* PSOUT = (PSOUT & ~(_BV(PS_CFG_CMD_BIT))) | (cmd & 1); */
	    if (cmd & 1) {
		PS_CMD1();
	    } else {
		PS_CMD0();
	    }
	    cmd >>= 1;
	    PS_CLK0();
	} else {
	    PS_CLK1();
	    data >>= 1;
	    data |= (PSIN & _BV(PS_CFG_DAT_BIT)) ? 0x80 : 0x00;
	}
//	ps_timer_wait();
	_delay_us(8);
#else
/*
	    if (cmd & 1) {
		PS_CMD1();
	    } else {
		PS_CMD0();
	    }
	    cmd >>= 1;
	    _delay_us(2);
	    PS_CLK0();
	    _delay_us(2);

	    PS_CLK1();
	    _delay_us(2);
	    data >>= 1;
	    data |= (PSIN & _BV(PS_CFG_DAT_BIT)) ? 0x80 : 0x00;
	    _delay_us(2);
*/
	PS_CLK0();
	if (cmd & 1) { PS_CMD1(); } else { PS_CMD0(); }
	cmd >>= 1;
	_delay_us(10);
	PS_CLK1();
	data >>= 1;
	data |= (PSIN & _BV(PS_CFG_DAT_BIT)) ? 0x80 : 0x00;
	_delay_us(10);
#endif
    }

//    PS_CMD1();

    /* ACK待ち */
//    ps_timer_start(PS_TIMER_ACK_TCCR, PS_TIMER_ACK_TCNT);
//    ps_timer_wait();
    _delay_us(20);

    return data;
}

/* 
   outputは33バイト以上必要
 */
static void ps_read(uchar *output)
{
    uchar data_len;

      PS_CMD1();
      PS_CLK1();
    PS_SEL0();
    _delay_us(17);

    /* 1バイト目: CMD=0x01, DAT=不定 */
    ps_putgetc(0x01);

    /* 2バイト目: CMD=0x42, DAT=上位4ビット:デバイスタイプ 下位4ビット:data_len */
    *output = ps_putgetc(0x42);
    /* 4バイト目以降のデータ転送バイト数 / 2 */
    data_len = (*output++ & 0x0f);
#if 0				/* この条件に入るデバイスは無いと思うので省略 */
    if (!data_len) {
	data_len = 0x10;
    }
#endif

    /* 3バイト目: CMD=0x00, DAT=0x5A */
    *output++ = ps_putgetc(0x00);

    while (data_len--) {
	*output++ = ps_putgetc(0x00);
	*output++ = ps_putgetc(0x00);
    }

      PS_CMD1();
      _delay_ms(1);
    PS_SEL1();
}

static uchar psdata[33];

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
static int      sinus = 7 << 6, cosinus = 0;
static uchar    idleRate;   /* repeat rate for keyboards, never used for mice */

char lastTimer0Value;

/* The following function advances sin/cos by a fixed angle
 * and stores the difference to the previous coordinates in the report
 * descriptor.
 * The algorithm is the simulation of a second order differential equation.
 */
static void advanceCircleByFixedAngle(void)
{
char    d;

#define DIVIDE_BY_64(val)  (val + (val > 0 ? 32 : -32)) >> 6    /* rounding divide */
    reportBuffer.dx = d = DIVIDE_BY_64(cosinus);
    sinus += d;
    reportBuffer.dy = d = DIVIDE_BY_64(sinus);
    cosinus -= d;
}

/* ------------------------------------------------------------------------- */

usbMsgLen_t usbFunctionSetup(uchar data[8])
{
usbRequest_t    *rq = (void *)data;

    /* The following requests are never used. But since they are required by
     * the specification, we implement them in this example.
     */
    if((rq->bmRequestType & USBRQ_TYPE_MASK) == USBRQ_TYPE_CLASS){    /* class request type */
        DBG1(0x50, &rq->bRequest, 1);   /* debug output: print our request */
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
	    usbMsgLen_t len = 16;
	    if (len > rq->wLength.word)
		len = rq->wLength.word;
	    usbMsgPtr = psdata;
	    psdata[12] = 0xDE;
	    psdata[13] = 0xAD;
	    psdata[14] = 0xBE;
	    psdata[15] = 0xEF;
	    return len;
	}
        /* no vendor specific requests implemented */
    }
    return 0;   /* default for not implemented requests: return no data back to host */
}

/* ------------------------------------------------------------------------- */
static void usb_reenumerate(void)
{
    uchar   i;
    usbDeviceDisconnect();  /* enforce re-enumeration, do this while interrupts are disabled! */
    i = 0;
    while(--i){             /* fake USB disconnect for > 250 ms */
        wdt_reset();
        _delay_ms(1);
    }
    usbDeviceConnect();
}

int __attribute__((noreturn)) main(void)
{
    TCCR0B = 3;
    /* OSCCAL = 220; */

    wdt_enable(WDTO_1S);
    /* Even if you don't use the watchdog, turn it off here. On newer devices,
     * the status of the watchdog (on/off, period) is PRESERVED OVER RESET!
     */
    /* RESET status: all port bits are inputs without pull-up.
     * That's the way we need D+ and D-. Therefore we don't need any
     * additional hardware initialization.
     */
    odDebugInit();
    DBG1(0x00, 0, 0);       /* debug output: main starts */
    usbInit();
    ps_init();
    usb_reenumerate();
    sei();
    DBG1(0x01, 0, 0);       /* debug output: main loop starts */
    for(;;){                /* main event loop */
        DBG1(0x02, 0, 0);   /* debug output: main loop iterates */
        wdt_reset();
        usbPoll();
        if(usbInterruptIsReady()){
            /* called after every poll of the interrupt endpoint */
	    reportBuffer.dx = ps_main();
	    reportBuffer.dy = 0;
            /* advanceCircleByFixedAngle(); */
            DBG1(0x03, 0, 0);   /* debug output: interrupt report prepared */
            usbSetInterrupt((void *)&reportBuffer, sizeof(reportBuffer));
        }
    }
}
