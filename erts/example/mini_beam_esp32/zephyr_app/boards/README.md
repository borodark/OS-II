# Board Profiles

These overlays define OS/II board profiles by ensuring consistent aliases:

- `led0` for GPIO command pin/channel 0
- enabled `pwm0` node label for PWM channel routing
- `i2c0` for I2C bus 0

Profiles:
- `nrf52840dk_nrf52840.overlay`
- `arduino_nano_33_ble.overlay`

The runtime is compiled with `MB_STRICT_DTS`; build fails if required aliases/nodes are missing.
