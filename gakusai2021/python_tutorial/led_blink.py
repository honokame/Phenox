#!/usr/bin/env python
# -*- coding: utf-8 -*-

import time
import phenox as px
import phenox_params as pxprm

if __name__ == "__main__":
    pxprm.set_parameter()
    led_target = px.PX_LED_RED
    #led_target = px.PX_LED_GREEN
    led_is_on = False

    while True:
        time.sleep(1.0)
        px.set_led(led_target, led_is_on)
        led_is_on = not led_is_on

