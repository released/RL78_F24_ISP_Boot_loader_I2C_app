#ifndef ISP_CONSOLE_H
#define ISP_CONSOLE_H

#include "isp_protocol.h"

/* UART0 text is independent of the selected binary ISP transport. */
void IspConsoleInit(void);
void IspConsoleTrace(const IspPacket ISP_RAM *packet);
void IspConsolePoll(unsigned char reset_allowed);
unsigned char IspConsoleIdle(void);
/* G16 pins existing packet buffers for at most200ms; other MCUs return0. */
unsigned char IspConsoleYield(void);
int IspConsoleCapture(unsigned char value);

/* Board-owned, nonblocking foreground hooks. Never call from an ISR. */
unsigned char IspConsoleReceive(unsigned char *value);
unsigned char IspConsoleTransmit(unsigned char value);
unsigned char IspConsoleTxIdle(void);
unsigned char IspConsoleBusIdle(void);
unsigned char IspConsoleReplyComplete(void);
unsigned char IspConsoleReplyAbandoned(void);
unsigned short IspConsoleTick(void);
void IspConsoleReset(void);

#endif
