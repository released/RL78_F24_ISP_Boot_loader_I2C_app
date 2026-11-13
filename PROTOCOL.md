# I2C ISP device protocol - RL78/F24

This is the contract for the independent RL78_F24_ISP_Boot_loader_I2C, RL78_F24_ISP_Boot_loader_I2C_app and RL78_F24_I2C_Slave_Transfer projects. [Tool operation](https://released.github.io/slide_isp_tool_for_RL78/#uart-device-operation) owns wiring and UI steps. The device UART0 Log is a separate text connection.

## Transport and identity

| Item | Value |
| --- | --- |
| Update link | IICA0 SCL / SDA P62 / P63, 7-bit address 0x20, 100 kHz |
| Contract / protocol ID | 2 / 0x49 |
| Image magic | ISP plus NUL, bytes 49 53 50 00 |
| Metadata | App+0x100, 256 bytes; core128 and reserved128 FF |
| Metadata protocol word | 0x01010049 |
| Policy word at metadata0x64 | 0x02012003 (policy3,32 bytes,DEVICE_APP,I2C) |
| Policy baud at0x70 | 0 |
| Version policy at0x74 | 0=any version;1=same/newer from valid installed App |
| Link words0x30..0x3F | 100000,0x20,0,0 |

## Framed control bytes

| Offset | Bytes | Meaning |
| --- | --- | --- |
| 0 | 2 | 49 53 frame marker |
| 2 | 1 | Framing version1 |
| 3 | 1 | 0=request,1=response |
| 4 | 2 | Sequence LE16,1..65535 |
| 6 | 1 | Command |
| 7 | 1 | Status;request0 |
| 8 | 2 | Payload length LE16,0..96 |
| 10 | 0..96 | Payload |
| 10+length | 4 | CRC32 of header/payload,LE32 |

CRC32/ISO-HDLC uses reflected polynomial EDB88320, initial/final XOR FFFFFFFF; 123456789 gives CBF43926. Maximum frame110 bytes; partial framed input expires after200ms. Equal sequence replay requires identical payload. Conflicting duplicates are rejected.

## Commands and update boundaries

| Command | Payload / result |
| --- | --- |
| 01 INFO | 88-byte live MCU/profile/geometry/installed identity/capabilities |
| 02 ENTER | App replies completely before resetting into Boot;Boot ACK |
| 03 METADATA | LE16 offset plus4..64 bytes;ordered stage256 before START |
| 04 START /0E FORCE_START | LE32 full image size plusLE32 stored final CRC;validate before erase |
| 05 WRITE /06 END | WRITE=LE32 offset plus4..64 bytes;END validates whole image before reset |
| 07 ABORT | Clear session/staging;does not restore erased contents |
| 08 READ_METADATA | Valid installed App;LE16 offset plusU8 count1..64 |
| 09 VERIFY | Boot read-only installed-image validation |
| 20 PING /21 ECHO | App/Transfer only;0..96-byte echo |
| 22 STATS /23 CLEAR_STATS | App/Transfer only;fourLE32 counters:requests,replays,errors,written |


Framed ISP requests stage metadata and check START before ordered 64-byte writes. END checks complete length, stored trailer, full CRC32, metadata and App vectors before reset.

I2C sends one STOP-delimited request (maximum 110 bytes), then polls a fixed 111-byte read window: status 00=busy or A5=ready, followed by the framed reply and FF padding. Main-loop processing performs Flash work; the IICA ISR only moves mailbox bytes. ENTER/END reset waits for the complete ready read, final NACK and STOP.

Boot checks metadata, MCU/partition/IRQ/minimum-Boot contract, valid-installed-App policy, range, stored trailer and full CRC before launch. FORCE skips ordering only for its transaction;no persistent unlock/floor. Boot idle30s returns to an already valid App only before an active update. App ISR authoring uses app_irq_handlers.h,AA/55 ownership and generated full tables.

Build/package/model results are offline evidence;board electrical/IRQ/Flash/reset/power acceptance is separate.
