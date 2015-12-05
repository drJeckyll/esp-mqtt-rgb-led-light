# esp-mqtt-rgb-led-light
ESP8266 MQTT control of RGB Led strip

Code was written for this product: http://www.aliexpress.com/item/rgb-strip-WiFi-controller-1-port-control-15-rgb-lights-communicate-with-Android-phone-to-dim/32301423622.html

You can have different location settings - see include/user_config_desk.h, include/user_config_home.h, include/user_config_office.h. You can add more locations in include/user_config.h.

Settings for MQTT and WIFI are in location settings .h files.

Each color is controled via separate MQTT topic. You can change all colors in same time, all white colors in same time and all in same time. You can also change PWM period between (1000 and 10000).

Settings are saved after each change. When device is rebooted settings are restored. After each change you will recive new settings via settings/reply topic in JSON format.

All topics can be adjusted via location .h files.

When all colors are set to 0 or max, PWM will be turned OFF. In every other case PWM will be ON. This is ESP8266 PWM limitation. Max can be calculated like this: period * 1000 / 45. When you change period it will chnage max.

This code was tested with ESP SDK 1.4.0.
