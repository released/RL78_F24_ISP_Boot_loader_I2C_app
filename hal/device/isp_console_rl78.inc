/* Foreground UART0 console hooks. The project retains its existing board API.
 * SAU TSF/BFF are inspected without waiting; UART0 receive IRQ remains masked.
 */
unsigned char IspConsoleReceive(unsigned char *value)
{
    unsigned char error;
    if (!SRIF0)
    {
        return 0U;
    }
    error = (unsigned char)(SSR01 & 7U);
#if defined(ISP_MCU_G16) || defined(ISP_MCU_G22)
    *value = RXD0;
#else
    *value = SDR01L;
#endif
    SRIF0 = 0U;
    SIR01 = error;
    return (unsigned char)!error;
}
unsigned char IspConsoleTxIdle(void)
{
    return (unsigned char)((SSR00 & 0x60U) == 0U);
}
unsigned char IspConsoleTransmit(unsigned char value)
{
    if (!IspConsoleTxIdle())
    {
        return 0U;
    }
    STIF0 = 0U;
#if defined(ISP_MCU_G16) || defined(ISP_MCU_G22)
    TXD0 = value;
#else
    SDR00L = value;
#endif
    return 1U;
}
unsigned char IspConsoleBusIdle(void)
{
#if defined(ISP_I2C)
    return (unsigned char)(!IICBSY0 && !IICAIF0);
#else
    unsigned char snapshot[24];
    IspUartDiagnostics(snapshot);
#if defined(ISP_MCU_F13) || defined(ISP_MCU_F24)
    return (unsigned char)(!IspRead32(snapshot + 20U) && !SRIF1 &&
                           !(SSR10 & 0x60U) && !(SSR11 & 0x40U));
#else
    return (unsigned char)(!IspRead32(snapshot + 20U) && !SRIF1 &&
                           !(SSR02 & 0x60U) && !(SSR03 & 0x40U));
#endif
#endif
}
unsigned char IspConsoleReplyComplete(void)
{
#if defined(ISP_I2C)
    return IspI2cReplyComplete();
#else
    /* IspUartSend returns only after the final byte completed transmission. */
    return 1U;
#endif
}
unsigned short IspConsoleTick(void)
{
    return IspTick();
}
unsigned char IspConsoleReplyAbandoned(void)
{
#if defined(ISP_I2C)
    return IspI2cReplyIdle();
#else
    return 0U;
#endif
}
void IspConsoleReset(void)
{
    IspBoardReset();
}
