//
// Created by pxanych on 18.12.2023.
//

#ifndef LED_UPDATER_H
#define LED_UPDATER_H

#include "stdint.h"

typedef struct
{
    uint16_t queue_size;
    uint16_t led_count;
} led_updater_params_t;


typedef struct
{
    union
    {
        struct
        {
            uint8_t red;
            uint8_t green;
            uint8_t blue;
        };
        struct
        {
            uint32_t led_spec;
        };
    };

} led_color;

void led_updater_task(void* pvArgs);

/**
 * \brief
 * \param timestamp when should be this update applied (milliseconds since previous zero timestamp).
 * \param len size of leds array
 * \param leds
 */
void submit_update(uint32_t timestamp, uint16_t len, led_color* leds);


#endif //LED_UPDATER_H
