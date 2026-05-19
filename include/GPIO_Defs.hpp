#pragma once
/*
* GPIO_Defs.hpp - Output Management class
*
* Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
* Copyright (c) 2021, 2026 Shelby Merrick
* http://www.forkineye.com
*
*  This program is provided free for you to use in any way that you wish,
*  subject to the laws and regulations where you are using it.  Due diligence
*  is strongly suggested before using this code.  Please give credit where due.
*
*  The Author makes no warranty of any kind, express or implied, with regard
*  to this program or the documentation contained in this document.  The
*  Author shall not be liable in any event for incidental or consequential
*  damages in connection with, or arising out of, the furnishing, performance
*  or use of these programs.
*
*/

#include "ESPixelStick.h"
#include "output/OutputMgrPortDefs.hpp"

#ifdef ARDUINO_ARCH_ESP32
    #include <driver/gpio.h>
    #include <hal/uart_types.h>
    #ifdef CONFIG_IDF_TARGET_ESP32
        #define DEFAULT_SPI_DEVICE      VSPI_HOST
    #elif  CONFIG_IDF_TARGET_ESP32S2
        #define DEFAULT_SPI_DEVICE      HSPI_HOST
    #else
        #define DEFAULT_SPI_DEVICE      spi_host_device_t::SPI3_HOST
    #endif
#endif

// Platform specific GPIO definitions
#if   defined (BOARD_ESP32_CAM)
#   include "platforms/GPIO_Defs_ESP32_CAM.hpp"
#elif defined (BOARD_ESP32_D1_MINI_ETH)
#   include "platforms/GPIO_Defs_ESP32_D1_MINI_ETH.hpp"
#elif defined (BOARD_ESP32_D1_MINI)
#   include "platforms/GPIO_Defs_ESP32_D1_MINI.hpp"
#elif defined (BOARD_ESP32_KA)
#   include "platforms/GPIO_Defs_ESP32_KA.hpp"
#elif defined (BOARD_ESP32_KA_4)
#   include "platforms/GPIO_Defs_ESP32_KA_4.hpp"
#elif defined (BOARD_ESP32_LOLIN_D32_PRO_ETH)
#   include "platforms/GPIO_Defs_ESP32_LoLin_D32_PRO_ETH.hpp"
#elif defined (BOARD_ESP32_LOLIN_D32_PRO)
#   include "platforms/GPIO_Defs_ESP32_LoLin_D32_PRO.hpp"
#elif defined (BOARD_ESP32_M5STACK_ATOM)
#   include "platforms/GPIO_Defs_ESP32_M5Stack_Atom.hpp"
#elif defined (BOARD_ESP32_MH_ET_LIVE_MiniKit)
#   include "platforms/GPIO_Defs_ESP32_MH_ET_LIVE_MiniKit.hpp"
#elif defined (BOARD_ESP32_QUINLED_DIG_OCTA)
#   include "platforms/GPIO_Defs_ESP32_QUINLED_Dig-Octa.hpp"
#elif defined (BOARD_ESP32_OLIMEX_GATEWAY)
#   include "platforms/GPIO_Defs_ESP32_Olimex_Gateway.hpp"
#elif defined (BOARD_ESP32_QUINLED_QUAD_ETH)
#   include "platforms/GPIO_Defs_ESP32_QUINLED_QUAD_ETH.hpp"
#elif defined (BOARD_ESP32_QUINLED_QUAD_ETH_P5)
#   include "platforms/GPIO_Defs_ESP32_QUINLED_QUAD_ETH_P5.hpp"
#elif defined (BOARD_ESP32_QUINLED_QUAD_AE_PLUS)
#   include "platforms/GPIO_Defs_ESP32_QUINLED_QUAD_AE_Plus.hpp"
#elif defined (BOARD_ESP32_QUINLED_QUAD_AE_PLUS_8)
#   include "platforms/GPIO_Defs_ESP32_QUINLED_QUAD_AE_Plus_8.hpp"
#elif defined (BOARD_ESP32_QUINLED_QUAD)
#   include "platforms/GPIO_Defs_ESP32_QUINLED_QUAD.hpp"
#elif defined (BOARD_ESP32_QUINLED_QUAD_P5)
#   include "platforms/GPIO_Defs_ESP32_QUINLED_QUAD_P5.hpp"
#elif defined (BOARD_ESP32_QUINLED_UNO_ETH)
#   include "platforms/GPIO_Defs_ESP32_QUINLED_UNO_ETH.hpp"
#elif defined (BOARD_ESP32_QUINLED_UNO)
#   include "platforms/GPIO_Defs_ESP32_QUINLED_UNO.hpp"
#elif defined (BOARD_ESP32_QUINLED_UNO_AE_PLUS)
#   include "platforms/GPIO_Defs_ESP32_QUINLED_UNO_AE_Plus.hpp"
#elif defined (BOARD_ESP32_QUINLED_UNO_ESPSV3)
#   include "platforms/GPIO_Defs_ESP32_QUINLED_UNO_ESPSV3.hpp"
#elif defined (BOARD_ESP32_QUINLED_UNO_ETH_ESPSV3)
#   include "platforms/GPIO_Defs_ESP32_QUINLED_UNO_ETH_ESPSV3.hpp"
#elif defined (BOARD_ESP32_TTGO_T8)
#   include "platforms/GPIO_Defs_ESP32_TTGO_T8.hpp"
#elif defined (BOARD_ESP32_BONG69)
#   include "platforms/GPIO_Defs_ESP32_Bong69.hpp"
#elif defined (BOARD_ESP32_WT32ETH01)
#   include "platforms/GPIO_Defs_ESP32_WT32ETH01.hpp"
#elif defined (BOARD_ESP32_WT32ETH01_WASATCH)
#   include "platforms/GPIO_Defs_ESP32_WT32ETH01_Wasatch.hpp"
#elif defined (BOARD_ESP32_FOO)
#   include "platforms/GPIO_Defs_ESP32_FOO.hpp"
#elif defined (BOARD_ESP32_TWILIGHTLORD)
#   include "platforms/GPIO_Defs_ESP32_TWILIGHTLORD.hpp"
#elif defined (BOARD_ESP32_TWILIGHTLORD_ETH)
#   include "platforms/GPIO_Defs_ESP32_TWILIGHTLORD_ETH.hpp"
#elif defined (BOARD_ESP32_DEVKITC)
#   include "platforms/GPIO_Defs_ESP32_DevkitC.hpp"
#elif defined (BOARD_ESP01S)
#   include "platforms/GPIO_Defs_ESP8266_ESP01S.hpp"
#elif defined (BOARD_ESP32S3_DEVKITC)
#   include "platforms/GPIO_Defs_ESP32S3_DevkitC.hpp"
#elif defined (BOARD_SEEED_XIAO_ESP32S3)
#   include "platforms/GPIO_Defs_ESP32S3_seed_XIAO.hpp"
#elif defined (BOARD_ESPS_V3)
#   include "platforms/GPIO_Defs_ESP8266_ESPS_V3.hpp"
#elif defined (BOARD_ESPS_ESP3DEUXQUATRO_DMX)
#   include "platforms/GPIO_Defs_ESP32_ESP3DEUXQuattro_DMX.hpp"
#elif defined (BOARD_ESP32_OCTA2GO)
#   include "platforms/GPIO_Defs_ESP32_Octa2go.hpp"
#elif defined (BOARD_ESP32_TETRA2GO)
#   include "platforms/GPIO_Defs_ESP32_Tetra2go.hpp"
#elif defined (BOARD_ESP32_SOLO2GO)
#   include "platforms/GPIO_Defs_ESP32_Solo2go.hpp"
#elif defined (BOARD_ESP32_KR_LIGHTS_MSM)
#   include "platforms/GPIO_Defs_ESP32_kr_lights_msm.hpp"
#elif defined (BOARD_ESP32_BREAKDANCEV2)
#   include "platforms/GPIO_Defs_ESP32_BreakDanceV2.hpp"
#elif defined(BOARD_ESP32_DEVKITV_ETH)
#   include "platforms/GPIO_Defs_ESP32_DevkitV_ETH.hpp"
#elif defined(BOARD_ESP32S3_FH4R2)
#   include "platforms/GPIO_Defs_ESP32S3_FH4R2.hpp"
#elif defined (ARDUINO_ARCH_ESP32)
#   include "platforms/GPIO_Defs_ESP32_generic.hpp"
#elif defined (ARDUINO_ARCH_ESP8266)
#   include "platforms/GPIO_Defs_ESP8266_Generic.hpp"
#else
#   error "No valid platform definition"
#endif // ndef platform specific GPIO definitions

#if defined(SUPPORT_SD) || defined(SUPPORT_SD_MMC)
#   define SUPPORT_FPP
#endif // defined(SUPPORT_SD) || defined(SUPPORT_SD_MMC)
