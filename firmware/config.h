/*
  usbdrv PB1 = D+, PB2 = D-
  PS PB0 = DAT

  Green - 1 DAT
  Orange - 2 CMD
  Black - 3
  Blue - 4 GND
  Yellow - 5 3.6V
  Red - 6 SEL
  Brown - 7 CLK
  Gray - 9
 */

#define PS_CFG_IOPORTNAME B
#define PS_CFG_DAT_BIT 0	/* IN */
#define PS_CFG_CMD_BIT 5	/* OUT */
#define PS_CFG_SEL_BIT 4	/* OUT */
#define PS_CFG_CLK_BIT 3	/* OUT */

