#ifndef F24_DEVICE_PREINCLUDE_H
#define F24_DEVICE_PREINCLUDE_H
/* Load this project's own generated SC settings before the board overrides. */
#include "r_bsp_config.h"

#if (BSP_CFG_FMX_HZ != 4000000) || (BSP_CFG_HISYSCLK_SOURCE != 1)
#error F24_device_requires_the_target_board_4MHz_X1_configuration
#endif
/* Do not enter generated, unbounded crystal/PLL waits before main. */
#undef BSP_CFG_MAINCLK_SOURCE
#define BSP_CFG_MAINCLK_SOURCE (0)
#undef BSP_CFG_HISYSCLK_OPERATION
#define BSP_CFG_HISYSCLK_OPERATION (1)
#undef BSP_CFG_PLL_OPERATION
#define BSP_CFG_PLL_OPERATION (0)
#undef BSP_CFG_PLL_MODE
#define BSP_CFG_PLL_MODE (0)
#undef BSP_CFG_FMP_DIVIDE
#define BSP_CFG_FMP_DIVIDE (0)
#undef BSP_CFG_PIOR46
#define BSP_CFG_PIOR46 (0)
#ifdef F24_DEVICE_APP
/* Boot hands over a locked 80MHz PLL / CPU40. CMC is write-once. */
#undef BSP_CFG_STARTUP_DISABLE
#define BSP_CFG_STARTUP_DISABLE (1)
#endif

#endif
