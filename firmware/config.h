/* -*- c-file-style: "stroustrup"; indent-tabs-mode: nil; -*- */

#ifndef __CONFIG_H_INCLUDED__
#define __CONFIG_H_INCLUDED__

/* PSパッドのハードウェア設定 */
/* ------------------------------------------------------------------------- */

/* PSパッドの信号線を接続するポート(x of PORTx, PINx, DDRx) */
#define PS_CFG_IOPORTNAME B
/* DATに接続されているポートのビット位置 */
#define PS_CFG_DAT_BIT 0
/* CMDに接続されているポートのビット位置 */
#define PS_CFG_CMD_BIT 5
/* SELに接続されているポートのビット位置 */
#define PS_CFG_SEL_BIT 4
/* CLKに接続されているポートのビット位置 */
#define PS_CFG_CLK_BIT 3

/* PSパッドのタイミング設定 */
/* ------------------------------------------------------------------------- */

/* SELの立ち下げから最初のCLK立ち下げまでのウェイト時間(usec) */
#define PS_CFG_SEL_DELAY_US 17
/* CLKのトグル間のウェイト時間(usec) */
#define PS_CFG_CLK_DELAY_US 15
/* ACK待ちのウェイト時間(usec) */
#define PS_CFG_ACK_DELAY_US 20

#endif
