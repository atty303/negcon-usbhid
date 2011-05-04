/* -*- c-file-style: "stroustrup"; indent-tabs-mode: nil; -*- */

#include <avr/eeprom.h>
#include <avr/pgmspace.h>

#include "setting.h"

static EEMEM setting_t setting_eeprom;
static setting_t setting_sram;

void setting_init(void)
{
    eeprom_read_block(&setting_sram, &setting_eeprom, sizeof(setting_eeprom));
}

void setting_update(void)
{
    eeprom_update_block(&setting_sram, &setting_eeprom, sizeof(setting_eeprom));
}

setting_t *setting_get(void)
{
    return &setting_sram;
}
