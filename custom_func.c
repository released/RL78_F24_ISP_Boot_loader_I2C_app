/* Project/API reading notes: Project-owned initialization and foreground work.
 * The selected main/startup adapter calls hardware_init() once and loop() repeatedly.
 * Keep application changes here so generated entry sources remain replaceable; lower-level APIs
 * live in this project's hal directories.
 */
#if defined(ISP_MCU_F13)
#include "r_cg_macrodriver.h"
#if defined(ISP_DEVICE_BOOT)
#include "fsl_flash_interface.h"
#endif
#else
#include "platform.h"
#include "user_def.h"
#define DI() BSP_DI()
#define EI() BSP_EI()
#endif
#include "isp_rl78_board.h"
#include "sw_crc32.h"
#include "isp_console.h"

static IspDevice g_device;
#include <string.h>
#if defined(ISP_XMODEM)
#include "isp_xmodem.h"
static union
{
    IspStream stream;
    IspXmodem x;
} g_input;
#define g_stream g_input.stream
#else
static IspStream g_stream;
#endif
static IspPacket g_request, g_reply;
static unsigned char g_wire[ISP_FRAME_MAX];
static unsigned short g_last_rx;
static unsigned short g_last_activity;
static const IspDeviceConfig g_config = {
    ISP_CONTRACT_ID,
    0U,
    ISP_UPDATE_TRANSPORT,
    64U,
    IMAGE_TARGET_ID,
    0U,
    0U,
    IMAGE_APPLICATION_START,
    IMAGE_APPLICATION_CAPACITY,
    IMAGE_CODE_FLASH_SIZE,
    IMAGE_RESERVED_SIZE,
    IMAGE_ERASE_BLOCK_SIZE,
    IMAGE_BOOT_VERSION,
    IMAGE_BOOT_BUILD,
    ISP_BUS_RATE,
    0UL,
    0UL,
    0UL,
    0U,
    0U,
    {0U, 0U},
#if defined(ISP_MCU_F13)
    {'R', '5', 'F', '1', '0', 'B', 'M', 'G', 0, 0, 0, 0, 0, 0, 0, 0},
    {'R', 'L', '7', '8', '/', 'F', '1', '3', 0, 0, 0, 0},
#else
    {'R', '7', 'F', '1', '2', '4', 'F', 'P', 'J', '5', 'x', 'F', 'B', 0, 0, 0},
    {'R', 'L', '7', '8', '/', 'F', '2', '4', 0, 0, 0, 0},
#endif
    IMAGE_IRQ_ABI};
#if !defined(ISP_DEVICE_TRANSFER)
unsigned char IspMetadataCheck(const unsigned long *words)
{
    return (unsigned char)(ImageMetadataValidate(words) == IMAGE_METADATA_OK);
}
/* Read only within the configured App extent.
 * Form each Flash address with 32-bit integer arithmetic before the RL78 far-pointer cast.
 */
unsigned char IspRomRead(unsigned long address, unsigned char *data, unsigned short size)
{
    unsigned short i;
    if (address < IMAGE_APPLICATION_START ||
        address > IMAGE_APPLICATION_START + IMAGE_APPLICATION_CAPACITY ||
        size > IMAGE_APPLICATION_START + IMAGE_APPLICATION_CAPACITY - address)
    {
        return 0U;
    }
    for (i = 0U; i < size; ++i)
    {
        data[i] = *(const unsigned char __far *)(address + (unsigned long)i);
    }
    return 1U;
}
/* Accept installed identity only after metadata, full-image CRC and every required vector
 * destination pass; invalid/blank Flash supplies no version floor.
 */
unsigned char IspInstalledValid(void)
{
    const unsigned long __far *words;
    const unsigned char __far *entry;
    unsigned long size, at, target;
    words = (const unsigned long __far *)(IMAGE_APPLICATION_START + 256UL);
    if (ImageMetadataValidate(words) != IMAGE_METADATA_OK)
    {
        return 0U;
    }
    size = words[6];
    if (caculate_crc32_checksum(IMAGE_APPLICATION_START, size - 4UL) !=
        *(const unsigned long __far *)(IMAGE_APPLICATION_START + size - 4UL))
    {
        return 0U;
    }
    for (at = 0UL; at < 256UL; at += 4UL)
    {
#if defined(ISP_MCU_F13)
        if (at == 4UL)
        {
            continue;
        }
#endif
        entry = (const unsigned char __far *)(IMAGE_APPLICATION_START + at);
        if (entry[0] != 0xECU)
        {
            return 0U;
        }
        target = (unsigned long)entry[1] | ((unsigned long)entry[2] << 8) |
                 ((unsigned long)entry[3] << 16);
        if (target < IMAGE_APPLICATION_START + 512UL ||
            target >= IMAGE_APPLICATION_START + size - 4UL)
        {
            return 0U;
        }
    }
    return 1U;
}
#if defined(ISP_DEVICE_BOOT)
static unsigned char g_flash_ready;
static unsigned char prepare_flash(void)
{
    if (g_flash_ready)
    {
        return 1U;
    }
#if defined(ISP_MCU_F13)
    g_flash_ready = (unsigned char)(init_fcl() == 0U);
#else
    g_flash_ready = (unsigned char)(Device_FlashInit() == 0U);
#endif
    return g_flash_ready;
}
unsigned char IspFlashErase(unsigned long address)
{
    unsigned char result;
    if (!prepare_flash())
    {
        return 0U;
    }
    DI();
#if defined(ISP_MCU_F13)
    result = (unsigned char)(flash_erase_app_area(address) == 0U);
#else
    result = (unsigned char)(Device_FlashErase(address) == 0U);
#endif
    EI();
    return result;
}
unsigned char IspFlashWrite(unsigned long address, const unsigned char *data)
{
    unsigned char result;
    /* The request buffer is data RAM and remains owned by the main loop until
     * the synchronous Flash call returns. The library never writes this input.
     */
    DI();
#if defined(ISP_MCU_F13)
    result =
        (unsigned char)(flash_write_packet_data(address, (unsigned char __near *)data, 4U) == 0U);
#else
    result = (unsigned char)(Device_FlashWrite(address, (unsigned char __near *)data) == 0U);
#endif
    EI();
    return result;
}
#endif
unsigned char IspImageVerify(unsigned long size, unsigned long crc)
{
    if (*(const unsigned long __far *)(IMAGE_APPLICATION_START + 256UL + 24UL) != size ||
        *(const unsigned long __far *)(IMAGE_APPLICATION_START + size - 4UL) != crc)
    {
        return 0U;
    }
    return IspInstalledValid();
}
#endif
void IspRuntimeInit(unsigned char mode)
{
#if !defined(ISP_DEVICE_TRANSFER)
    const unsigned long __far *words;
#endif
    IspConsoleInit();
    IspStreamInit(&g_stream);
    IspDeviceInit(&g_device, &g_config, 0, mode);
#if !defined(ISP_DEVICE_BOOT) && !defined(ISP_I2C)
    IspDeviceSetDiagnostics(&g_device, IspUartDiagnostics);
#endif
#if !defined(ISP_DEVICE_TRANSFER)
    if (IspInstalledValid())
    {
        words = (const unsigned long __far *)(IMAGE_APPLICATION_START + 256UL);
        IspDeviceInstalled(&g_device, words[4], words[5], words[6], (unsigned char)words[29]);
    }
#endif
    g_last_rx = IspTick();
    g_last_activity = g_last_rx;
    IspI2cInit();
}
unsigned char IspRuntimeActive(void)
{
    return g_device.active;
}
unsigned short IspRuntimeIdle(void)
{
    return (unsigned short)(IspTick() - g_last_activity);
}
void IspRuntimeHandoff(void)
{
    DI();
    IspI2cStop();
    IspUartStop();
    IspTimerStop();
#if defined(ISP_MCU_F13) && defined(ISP_DEVICE_BOOT)
    if (g_flash_ready)
    {
        FSL_Close();
    }
#endif
    *(volatile unsigned long __far *)(BOOT_APP_SHARED_RAM_ADDRESS + 4UL) =
        BOOT_APP_HANDOFF_SIGNATURE;
    *(volatile unsigned char __far *)BOOT_APP_SHARED_RAM_ADDRESS = BOOT_APP_RAM_OWNER_APPLICATION;
}
/* Included after the board-specific Flash/metadata adapter. This is main-loop
 * code, not an ISR. Each project selects exactly one update protocol.
 */
static unsigned char g_pending_action;
#if defined(ISP_XMODEM)
static unsigned char g_x_active;
#if !defined(ISP_DEVICE_BOOT)
static unsigned long g_x_crc;
#endif
unsigned char IspXmodemWrite(unsigned long offset, const unsigned char ISP_RAM *data,
                             unsigned short size)
{
#if !defined(ISP_DEVICE_BOOT)
    unsigned short at;
    unsigned char bit;
    (void)offset;
    for (at = 0U; at < size; ++at)
    {
        g_x_crc ^= data[at];
        for (bit = 0U; bit < 8U; ++bit)
        {
            g_x_crc = (g_x_crc & 1UL) ? (g_x_crc >> 1) ^ 0xEDB88320UL : g_x_crc >> 1;
        }
    }
    g_device.written += size;
    return 1U;
#else
    return (unsigned char)(IspDeviceWrite(&g_device, offset, data, size) == ISP_OK);
#endif
}
unsigned char IspXmodemFinish(void)
{
#if defined(ISP_DEVICE_BOOT)
    if (IspDeviceFinish(&g_device) != ISP_OK)
    {
        return 0U;
    }
    g_pending_action = g_device.action;
    return 1U;
#else
    g_device.active = 0U;
    return (unsigned char)((g_x_crc ^ 0xFFFFFFFFUL) == g_device.image_crc);
#endif
}
void IspXmodemAbort(void)
{
    g_device.active = 0U;
    g_device.staged = 0U;
    g_pending_action = 0U;
}
void IspXmodemSend(unsigned char byte)
{
    /* A local TX failure is not a lost ACK at the peer. Do not retain a
     * successful EOT's reset action when its final ACK could not leave TX.
     */
    if (!IspUartSend(&byte, 1U))
    {
        IspXmodemAbort();
        g_input.x.active = 0U;
    }
}
#endif
/* Consume a deferred action only after its complete successful reply.
 * A failed transmit must not trigger reset or handoff.
 */
static void perform_action(void)
{
#if defined(ISP_DEVICE_APP)
    if (g_pending_action == ISP_ACTION_ENTER)
    {
        *(volatile unsigned long __far *)(BOOT_APP_SHARED_RAM_ADDRESS + 12UL) =
            BOOT_APP_RESET_TO_BOOT_SIGNATURE;
    }
#else
    if (g_pending_action == ISP_ACTION_RESET)
    {
        *(volatile unsigned long __far *)(BOOT_APP_SHARED_RAM_ADDRESS + 8UL) =
            BOOT_APP_UPDATE_COMPLETE_SIGNATURE;
    }
#endif
    if (g_pending_action)
    {
        g_pending_action = 0U;
        IspBoardReset();
    }
}
void IspRuntimePoll(void)
{
    unsigned char byte;
    unsigned short length;
    IspConsolePoll(0U);
#if defined(ISP_I2C)
    IspI2cPoll(IspTick());
    if (g_pending_action && IspI2cReplyIdle())
    {
        /* An abandoned reply cannot authorize a later protocol reset. */
        g_pending_action = 0U;
    }
    if (g_pending_action && IspI2cReplyComplete())
    {
        perform_action();
    }
#elif defined(ISP_XMODEM)
    if (g_x_active)
    {
        IspXmodemPoll(&g_input.x, IspTick(), 0);
        while (IspUartGet(&byte))
        {
            IspXmodemFeed(&g_input.x, byte, IspTick(), 0);
        }
        if (!g_input.x.active)
        {
            g_x_active = 0U;
            perform_action();
            IspStreamInit(&g_stream);
        }
        g_last_activity = IspTick();
        IspHeartbeat();
        return;
    }
#endif
    if (g_stream.count && (unsigned short)(IspTick() - g_last_rx) >= ISP_INTERBYTE_TIMEOUT_MS)
    {
        g_stream.count = 0U;
    }
#if defined(ISP_I2C)
    while (IspI2cGet(&byte))
    {
        if (g_stream.count < ISP_FRAME_MAX)
        {
            g_stream.bytes[g_stream.count++] = byte;
        }
    }
    if (g_stream.count)
    {
#else
    while (IspUartGet(&byte))
    {
#endif
        g_last_rx = IspTick();
#if defined(ISP_I2C)
        if (IspDecode(g_stream.bytes, g_stream.count, &g_request))
        {
#else
        if (IspStreamFeed(&g_stream, byte, &g_request))
        {
#endif
            g_last_activity = g_last_rx;
            g_pending_action = 0U;
#if defined(ISP_DEVICE_APP)
            if (IspTick() != 0U)
            {
                g_device.ready = 1U;
            }
#endif
            IspConsoleTrace(&g_request);
            IspDeviceProcess(&g_device, &g_request, &g_reply);
            length = IspEncode(&g_reply, g_wire);
#if defined(ISP_I2C)
            if (!IspI2cSend(g_wire, length))
            {
                return;
            }
            IspConsoleTrace(&g_reply);
            if (!g_reply.status)
            {
                g_pending_action = g_device.action;
            }
#else
            if (!IspUartSend(g_wire, length))
            {
                return;
            }
            IspConsoleTrace(&g_reply);
            if (!g_reply.status)
            {
#if defined(ISP_XMODEM)
                if (((g_request.command == ISP_CMD_START ||
                      g_request.command == ISP_CMD_FORCE_START) &&
                     g_device.active) ||
                    g_device.action == ISP_ACTION_STREAM_TEST)
                {
                    IspXmodemInit(&g_input.x, g_device.image_size, IspTick());
                    g_x_active = 1U;
#if !defined(ISP_DEVICE_BOOT)
                    g_x_crc = 0xFFFFFFFFUL;
#endif
                    return;
                }
#endif
                g_pending_action = g_device.action;
                perform_action();
            }
#endif
        }
    }
#if defined(ISP_I2C)
    IspI2cRelease();
    /* I2C requests have a STOP boundary; a partial frame cannot consume bytes
     * from a later transaction or block a valid subsequent request. */
    g_stream.count = 0U;
#endif
    IspConsolePoll((unsigned char)(!g_device.active && !g_pending_action && !g_stream.count));
    IspHeartbeat();
}


/* Project initialization and recurring work. */
#include "isp_rl78_board.h"
#include "app_identity.h"

/* A visible label for A/B update checks; independent of semantic version.
 * Edit App release identity in app_image_config.h and IRQ mapping in
 * app_irq_handlers.h. Application work belongs in this project, not Boot.
 */
#define APP_LOG_TAG "V1"

static unsigned long g_custom_counter;
static unsigned short g_custom_last;

/* Initialize the role selected by this project before foreground processing.
 * Preserve clock, image/handoff and peripheral/IRQ enable order when adding application
 * initialization.
 */
void hardware_init(void)
{
    if (*(volatile unsigned long __far *)(BOOT_APP_SHARED_RAM_ADDRESS + 4UL) !=
        BOOT_APP_HANDOFF_SIGNATURE)
    {
        *(volatile unsigned long __far *)(BOOT_APP_SHARED_RAM_ADDRESS + 12UL) =
            BOOT_APP_RESET_TO_BOOT_SIGNATURE;
        IspBoardReset();
    }
    *(volatile unsigned long __far *)(BOOT_APP_SHARED_RAM_ADDRESS + 4UL) = 0UL;
    if (!IspBoardInit())
    {
        for (;;)
        {
        }
    }
    IspRuntimeInit(ISP_MODE_APP);
    IspLog("app image tag: " APP_LOG_TAG "\r\nISP I2C App: boot-validated\r\n");
    if (*(volatile unsigned long __far *)(BOOT_APP_SHARED_RAM_ADDRESS + 8UL) ==
        BOOT_APP_UPDATE_COMPLETE_SIGNATURE)
    {
        *(volatile unsigned long __far *)(BOOT_APP_SHARED_RAM_ADDRESS + 8UL) = 0UL;
        IspLog("ISP update: completed\r\n");
    }
    g_custom_counter = 0UL;
    g_custom_last = IspTick();
    IspLog("UART0 console: z/Z reset; ISP RX/TX trace enabled\r\n");
}

/* Foreground service point called repeatedly by the selected main adapter.
 * Keep protocol/application work and text logging bounded so pending traffic and deferred actions
 * can continue.
 */
void loop(void)
{
    unsigned long value;
    unsigned char index;
    char number[11];

    IspRuntimePoll();
    if (IspConsoleIdle() && (unsigned short)(IspTick() - g_custom_last) >= 1000U)
    {
        g_custom_last = IspTick();
        value = g_custom_counter++;
        index = 10U;
        number[index] = '\0';
        do
        {
            number[--index] = (char)('0' + value % 10UL);
            value /= 10UL;
        } while (value);
        IspLog("log(timer)-" APP_LOG_TAG ": ");
        IspLog((const char __far *)(number + index));
        IspLog("\r\n");
    }
    /* Add nonblocking App work here. Keep UART0 text off the UART1 link. */
}

#include "isp_console_rl78.inc"
#include "isp_console.inc"
