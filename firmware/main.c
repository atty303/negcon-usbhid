/* -*- c-file-style: "stroustrup"; -*- */
/* Name: main.c
 * Author: <insert your name here>
 * Copyright: <insert your copyright message here>
 * License: <insert your license reference here>
 */

#include <avr/io.h>
#include "usbdrv.h"

usbMsgLen_t usbFunctionSetup(uchar setupData[8])
{
    return 0;
}

int main(void)
{
    /* insert your hardware initialization here */
    for(;;){
        /* insert your main loop code here */
    }
    return 0;   /* never reached */
}
