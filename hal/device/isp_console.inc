/* Foreground-only bounded UART0 text. Data is the complete validated payload. */
#include "isp_console.h"
#if defined(__CCRL__)
#define ISP_CONSOLE_ROM __far
#else
#define ISP_CONSOLE_ROM
#endif
#if defined(ISP_MCU_G16)
#define ISP_CONSOLE_SLOTS 2U
#else
#define ISP_CONSOLE_SLOTS 4U
#endif
typedef struct
{
#if defined(ISP_MCU_G16)
    const IspPacket ISP_RAM *packet;
#else
    IspPacket packet;
#endif
    unsigned char ready;
} IspConsoleRecord;
static IspConsoleRecord g_console_records[ISP_CONSOLE_SLOTS];
static unsigned char g_console_head, g_console_tail, g_console_count;
static unsigned short g_console_dropped, g_console_notice_drops;
/* kind: 0 idle, 1 trace, 2 drop notice, 3 explicit reset notice. */
static unsigned char g_console_kind, g_console_char;
static unsigned char g_console_byte, g_console_digit, g_console_end;
static unsigned char g_console_reset, g_console_reply_pending;
static unsigned short g_console_reset_at;
#if defined(ISP_MCU_G16)
static unsigned short g_console_queued_at;
#endif
#if defined(ISP_CONSOLE_PRINTF)
static unsigned char g_console_capture_active, g_console_capture_size;
static char g_console_capture_bytes[2];
#endif
int IspConsoleCapture(unsigned char value)
{
#if defined(ISP_CONSOLE_PRINTF)
    if (g_console_capture_active)
    {
        if (g_console_capture_size >= 2U)
        {
            return -1;
        }
        g_console_capture_bytes[g_console_capture_size++] = (char)value;
        return (int)value;
    }
#else
    (void)value;
#endif
    return -2;
}
static unsigned char ConsoleDigit(unsigned char value, unsigned char low)
{
#if defined(ISP_CONSOLE_PRINTF)
    int result;
    /* Reuse the full RH850 formatter; capture one two-character atom. */
    g_console_capture_size = 0U;
    g_console_capture_active = 1U;
    result = tiny_printf("%02X", (unsigned int)value);
    g_console_capture_active = 0U;
    if (result != 2 || g_console_capture_size != 2U)
    {
        return (unsigned char)'?';
    }
    return (unsigned char)g_console_capture_bytes[low];
#else
    static const char ISP_CONSOLE_ROM digits[] = "0123456789ABCDEF";
    return (unsigned char)digits[low ? (value & 15U) : (value >> 4)];
#endif
}
static const IspPacket ISP_RAM *ConsolePacket(void)
{
#if defined(ISP_MCU_G16)
    return g_console_records[g_console_tail].packet;
#else
    return &g_console_records[g_console_tail].packet;
#endif
}
static void ConsoleDrop(void)
{
    if (g_console_dropped != 65535U)
    {
        ++g_console_dropped;
    }
}
void IspConsoleInit(void)
{
    g_console_head = g_console_tail = g_console_count = 0U;
    g_console_dropped = g_console_notice_drops = 0U;
    g_console_kind = g_console_char = 0U;
    g_console_reset = g_console_reply_pending = 0U;
#if defined(ISP_CONSOLE_PRINTF)
    g_console_capture_active = g_console_capture_size = 0U;
#endif
}
void IspConsoleTrace(const IspPacket ISP_RAM *packet)
{
    IspConsoleRecord *record;
#if defined(ISP_I2C)
    if (packet->response)
    {
        g_console_reply_pending = 1U;
    }
#endif
    if (packet->length > ISP_PAYLOAD_MAX || g_console_count == ISP_CONSOLE_SLOTS)
    {
        ConsoleDrop();
        return;
    }
#if defined(ISP_MCU_G16)
    if (!g_console_count)
    {
        g_console_queued_at = IspConsoleTick();
    }
#endif
    record = &g_console_records[g_console_head];
#if defined(ISP_MCU_G16)
    record->packet = packet;
#else
    record->packet = *packet;
#endif
#if defined(ISP_I2C)
    record->ready = (unsigned char)!packet->response;
#else
    record->ready = 1U;
#endif
    g_console_head = (unsigned char)((g_console_head + 1U) % ISP_CONSOLE_SLOTS);
    ++g_console_count;
}
unsigned char IspConsoleIdle(void)
{
    return (unsigned char)(!g_console_count && !g_console_kind &&
                           !g_console_dropped && !g_console_reset && IspConsoleTxIdle());
}
unsigned char IspConsoleYield(void)
{
#if defined(ISP_MCU_G16)
    if (g_console_count)
    {
        if ((unsigned short)(IspConsoleTick() - g_console_queued_at) < 200U)
        {
            return 1U;
        }
        /* Keep the fixed stack. A stopped Log must not pin protocol RAM. */
        while (g_console_count)
        {
            --g_console_count;
            ConsoleDrop();
        }
        g_console_head = g_console_tail = 0U;
        g_console_kind = g_console_char = 0U;
    }
#endif
    return 0U;
}
static unsigned char ConsoleHeader(void)
{
#if defined(ISP_I2C)
    static const char ISP_CONSOLE_ROM text[] = "TRACE ISP I2C  RX seq=0x0000 cmd=0x00 len=0x0000 data=[";
#else
    static const char ISP_CONSOLE_ROM text[] = "TRACE ISP UART RX seq=0x0000 cmd=0x00 len=0x0000 data=[";
#endif
    const IspPacket ISP_RAM *packet;
    unsigned char value;
    packet = ConsolePacket();
    if (g_console_char == 15U && packet->response)
    {
        return (unsigned char)'T';
    }
    if (g_console_char >= 24U && g_console_char <= 27U)
    {
        value = g_console_char < 26U ? (unsigned char)(packet->sequence >> 8) : (unsigned char)packet->sequence;
        return ConsoleDigit(value, (unsigned char)(g_console_char & 1U));
    }
    if (g_console_char == 35U || g_console_char == 36U)
    {
        return ConsoleDigit(packet->command, (unsigned char)(g_console_char == 36U));
    }
    if (g_console_char >= 44U && g_console_char <= 47U)
    {
        value = g_console_char < 46U ? (unsigned char)(packet->length >> 8) : (unsigned char)packet->length;
        return ConsoleDigit(value, (unsigned char)(g_console_char & 1U));
    }
    return (unsigned char)text[g_console_char];
}
void IspConsolePoll(unsigned char reset_allowed)
{
    unsigned char value, at;
    const IspPacket ISP_RAM *packet;
    static const char ISP_CONSOLE_ROM ending[] = "]\r\n";
    static const char ISP_CONSOLE_ROM reset[] = "\r\nUART0 z: MCU reset\r\n";
    static const char ISP_CONSOLE_ROM dropped[] = "\r\nTRACE ISP LOG_DROP=0x0000\r\n";
    for (at = 0U; at < 4U && IspConsoleReceive(&value); ++at)
    {
        if (value == (unsigned char)'z' || value == (unsigned char)'Z')
        {
            g_console_reset = 1U;
        }
    }
    if (IspConsoleReplyComplete())
    {
        g_console_reply_pending = 0U;
        for (at = 0U; at < ISP_CONSOLE_SLOTS; ++at)
        {
            g_console_records[at].ready = 1U;
        }
    }
    else if (g_console_reply_pending && IspConsoleReplyAbandoned())
    {
        /* The backend discarded this reply. Never print it as delivered or
         * turn it into a successful protocol reset. Retire borrowed packets. */
        g_console_reply_pending = 0U;
        while (g_console_count)
        {
            --g_console_count;
            ConsoleDrop();
        }
        g_console_head = g_console_tail = 0U;
        g_console_kind = g_console_char = 0U;
    }
    if (g_console_reset && (g_console_kind == 3U ||
        (reset_allowed && !g_console_reply_pending && IspConsoleBusIdle())))
    {
        if (g_console_kind != 3U)
        {
            g_console_count = 0U;
            g_console_head = g_console_tail = 0U;
            g_console_kind = 3U;
            g_console_char = 0U;
            g_console_reset_at = IspConsoleTick();
        }
        if (g_console_char < sizeof(reset) - 1U &&
            IspConsoleTransmit((unsigned char)reset[g_console_char]))
        {
            ++g_console_char;
        }
        if (reset_allowed && !g_console_reply_pending && IspConsoleBusIdle() &&
            ((g_console_char == sizeof(reset) - 1U && IspConsoleTxIdle()) ||
             (unsigned short)(IspConsoleTick() - g_console_reset_at) >= 200U))
        {
            g_console_reset = g_console_kind = 0U;
            IspConsoleReset();
        }
        return;
    }
    if (!g_console_kind)
    {
        if (g_console_dropped)
        {
            g_console_kind = 2U;
            g_console_notice_drops = g_console_dropped;
            g_console_dropped = 0U;
        }
        else if (g_console_count && g_console_records[g_console_tail].ready)
        {
            g_console_kind = 1U;
            g_console_byte = g_console_digit = g_console_end = 0U;
        }
        else
        {
            return;
        }
        g_console_char = 0U;
    }
    if (g_console_kind == 2U)
    {
        value = (unsigned char)dropped[g_console_char];
        if (g_console_char >= 23U && g_console_char <= 26U)
        {
            value = g_console_char < 25U ? (unsigned char)(g_console_notice_drops >> 8) : (unsigned char)g_console_notice_drops;
            value = ConsoleDigit(value, (unsigned char)((g_console_char - 23U) & 1U));
        }
        if (IspConsoleTransmit(value) && ++g_console_char == sizeof(dropped) - 1U)
        {
            g_console_kind = 0U;
        }
        return;
    }
    if (g_console_char < 55U)
    {
        if (IspConsoleTransmit(ConsoleHeader()))
        {
            ++g_console_char;
        }
        return;
    }
    packet = ConsolePacket();
    if (packet->length && g_console_char < 57U)
    {
        value = g_console_char == 55U ? (unsigned char)'\r' : (unsigned char)'\n';
        if (IspConsoleTransmit(value))
        {
            ++g_console_char;
        }
        return;
    }
    if (g_console_byte < packet->length)
    {
        if (g_console_digit < 2U)
        {
            value = ConsoleDigit(packet->data[g_console_byte], g_console_digit);
        }
        else if (((g_console_byte + 1U) % 8U) == 0U ||
                 g_console_byte + 1U == packet->length)
        {
            value = g_console_digit == 2U ? (unsigned char)'\r' : (unsigned char)'\n';
        }
        else
        {
            value = (unsigned char)' ';
        }
        if (IspConsoleTransmit(value))
        {
            if (g_console_digit < 2U || value == (unsigned char)'\r')
            {
                ++g_console_digit;
            }
            else
            {
                g_console_digit = 0U;
                ++g_console_byte;
            }
        }
        return;
    }
    if (IspConsoleTransmit((unsigned char)ending[g_console_end]) && ++g_console_end == 3U)
    {
        g_console_tail = (unsigned char)((g_console_tail + 1U) % ISP_CONSOLE_SLOTS);
        --g_console_count;
        g_console_kind = 0U;
    }
}
