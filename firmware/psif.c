/* -*- c-file-style: "stroustrup"; indent-tabs-mode: nil; -*- */

#include <avr/io.h>
#include <util/delay.h>
#include "usbdrv/usbdrv.h"

#include "config.h"
#include "psif.h"


#define PS_OUT USB_OUTPORT(PS_CFG_IOPORTNAME) /* PORTx register for PS IF */
#define PS_IN  USB_INPORT(PS_CFG_IOPORTNAME)  /* PINx register for PS IF */
#define PS_DDR USB_DDRPORT(PS_CFG_IOPORTNAME) /* DDRx register for PS IF */
#define PS_PORT_MASK (_BV(PS_CFG_SEL_BIT) | _BV(PS_CFG_CLK_BIT) | _BV(PS_CFG_CMD_BIT) | _BV(PS_CFG_DAT_BIT))


/**
 * PS I/Fで使用するI/Oを初期化する。
 */
void ps_init(void)
{
    /* SEL,CLK,CMDを出力に、DATを入力に設定する */
    PS_DDR = (PS_DDR & ~(PS_PORT_MASK)) | (_BV(PS_CFG_SEL_BIT) | _BV(PS_CFG_CLK_BIT) | _BV(PS_CFG_CMD_BIT));
    /* DAT入力をプルアップし、SEL,CLK,CMDは初期状態で全てH出力とする */
    PS_OUT |= PS_PORT_MASK;
}

/**
 * デバイスを1バイトのコマンドを送信し、受信した応答を返す。
 *
 *  @param[in]  cmd  デバイスに送信するコマンドバイト
 *  @return          デバイスから受信したデータ
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

/**
 * デバイスのデータを読み取る。
 *
 * @param[in]  output  デバイスから受信したデータを保存するバッファへのポインタ
 * @note outputバッファは34バイト無いとバッファオーバーフローの可能性がある。
 *       受信データは(固定の2バイト + 可変の2〜32バイト)であるため。
 */
void ps_read(uchar *output)
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
