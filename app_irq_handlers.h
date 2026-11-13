#ifndef APP_IRQ_HANDLERS_H
#define APP_IRQ_HANDLERS_H
/* Physical vector bytes: timer 0x2E, IICA0 0x28 (iodefine.h INTIICA0).
 * App ISR definitions are global RETI functions without vect=. */
#define APP_IRQ_HANDLERS(X)                                                                        \
    X(0x2E, IspTimerIsr)                                                                           \
    X(0x28, IspI2cIsr)
#endif
