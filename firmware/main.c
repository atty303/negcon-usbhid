/* -*- c-file-style: "stroustrup"; -*- */
/* Name: main.c
 * Author: <insert your name here>
 * Copyright: <insert your copyright message here>
 * License: <insert your license reference here>
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include "usbdrv.h"

usbMsgLen_t usbFunctionSetup(uchar setupData[8])
{
    return 0;
}

int main(void)
{
    usbInit();
    usbDeviceDisconnect();
    {
	uchar i = 0;
	while (--i)  _delay_ms(1);
    }
    usbDeviceConnect();

    sei();

    for(;;){
	usbPoll();
    }
    return 0;   /* never reached */
}
