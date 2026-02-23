# OS/II Architecture Diagram

```mermaid
flowchart TB
    A[Application Bytecode Modules<br/>OS/II mini BEAM subset] --> B[VM Process Model<br/>mailboxes + control policy]
    B --> C[VM Core Interpreter<br/>register VM + bounded state]
    C --> D[HAL/BIF Bridge<br/>typed command ABI]
    D --> E[Native Driver Plane<br/>Zephyr + nrfx]
    E --> F[nRF52840 Hardware<br/>GPIO PWM I2C ADC UART SPI RTC PDM]

    G[Sensor/Event Sources] --> E
    E --> H[Mailbox Events]
    H --> B

    I[Observability<br/>USB CDC logs + typed events] --- B
    I --- C
    I --- D
```

## Notes
- VM domain owns orchestration, retries, and mode transitions.
- Native domain owns timing-critical I/O and peripheral execution.
- Boundary is explicit through mailbox command/event contracts.
