# OS/II Architecture (ASCII)

```text
+------------------------------------------------------------------+
|                    Application Bytecode Modules                  |
|                  (OS/II restricted BEAM subset)                 |
+------------------------------------------------------------------+
                              |
                              v
+------------------------------------------------------------------+
| VM Process Model + VM Core                                       |
| - mailbox-driven control                                         |
| - register interpreter                                           |
| - bounded runtime state                                          |
+------------------------------------------------------------------+
                              |
                              v
+------------------------------------------------------------------+
| HAL/BIF Bridge                                                   |
| - typed command ABI                                              |
| - validation + dispatch                                          |
+------------------------------------------------------------------+
                              |
                              v
+------------------------------------------------------------------+
| Native Driver Plane (Zephyr + nrfx)                             |
| - ISR/DMA/peripheral control                                     |
| - timing-sensitive execution                                     |
+------------------------------------------------------------------+
                              |
                              v
+------------------------------------------------------------------+
| nRF52840 Hardware                                                |
| GPIO | PWM | I2C | ADC | UART | SPI | RTC | PDM                 |
+------------------------------------------------------------------+

Event path:
Native drivers -> mailbox events -> VM policy handlers -> HAL commands
```
