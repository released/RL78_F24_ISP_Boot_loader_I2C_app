/* Project/API reading notes: One selected device update interface.
 * Baud/address/protocol values must match the board backend, paired Boot/App image contract and
 * Tool selection.
 * The independent diagnostic Log UART is not the binary ISP data channel.
 */
#ifndef ISP_TRANSPORT_CONFIG_H
#define ISP_TRANSPORT_CONFIG_H
/* Selected update interface: IICA0 slave P62 / P63, address0x20/100kHz.
 * UART0 remains diagnostic text. UART1 constants below are unused by I2C.
 * IICA0 physical vector 0x28 is defined by iodefine.h INTIICA0.
 * Boot/App use this same bus contract and compile-time address/rate.
 */
#define ISP_UART_BAUD (115200UL)
#define ISP_UART_RX_VECTOR (0x40U)
#define ISP_INTERBYTE_TIMEOUT_MS (200U)
#define ISP_BOOT_IDLE_TIMEOUT_MS (30000U)
#define ISP_BOOT_PROTOCOL_ID (0x49U)
#define ISP_BOOT_PROTOCOL_VERSION (1U)
/* Reserved link words retain the common 128-byte metadata core positions. */
#define ISP_LINK_MODE (0U)
#define ISP_LINK_ADDRESS_KIND (0U)
#define ISP_LINK_PARAMETER_0 (100000UL)
#define ISP_LINK_PARAMETER_1 (0x20UL)
#define ISP_LINK_PARAMETER_2 (0UL)
#define ISP_LINK_PARAMETER_3 (0UL)
#define ISP_BOOT_DEBUG_UART (0U)

/* One compiled update interface; UART0 remains diagnostic text. */
#define ISP_I2C 1
#define ISP_CONTRACT_ID (2U)
#define ISP_UPDATE_TRANSPORT (2U)
#define ISP_BUS_RATE (100000UL)
#define ISP_POLICY_UART_BAUD (0UL)
#define ISP_I2C_ADDRESS (0x20U)
#endif
