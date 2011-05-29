#ifndef __SETTING_H_INCLUDED__
#define __SETTING_H_INCLUDED__

typedef struct calibrate_t {
    uint8_t lower_threshold;
    uint8_t higher_threshold;
} calibrate_t;

typedef uint8_t map_t;

typedef struct setting_t {
    /*
       0 0 0 0 B B B B
               \     /
                ------ map to button

       A A A A x x D D -- direction 0: 0x00 -> 0xFF, 1: 0x80 -> 0xFF, 2: 0xFF -> 0x00 3: .0x80 -> 0x00
       \     /
        ------ map to axis
               0: (reserved), 1: (x), 2: (y), 3: z, 4: rx, 5: ry, 6: rz, 7: N/A, 15: (reserved)

       1 1 1 1 1 1 1 1
     */
    struct {
        /* 物理的にデジタルボタン */
        map_t start;
        map_t a;
        map_t b;
        map_t r;
        /* 十字キー */
        map_t left;
        map_t down;
        map_t right;
        map_t up;
        /* アナログボタン 0x00..0xFF */
        map_t i;
        map_t ii;
        map_t l;
    } mapping;                  /* 11 bytes */

    struct {
        calibrate_t i;
        calibrate_t ii;
        calibrate_t l;
    } calibration;              /* 6 bytes */
} setting_t;			/* 17 bytes */


void setting_init(void);
void setting_update(void);
setting_t *setting_get(void);

#endif
