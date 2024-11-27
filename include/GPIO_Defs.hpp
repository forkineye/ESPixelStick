#pragma once
/*
* GPIO_Defs.hpp - Output Management class
*
* Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
* Copyright (c) 2021, 2022 Shelby Merrick
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

#ifdef ARDUINO_ARCH_ESP32
#   include <driver/gpio.h>
#   include <hal/uart_types.h>
#endif

#ifdef ARDUINO_ARCH_ESP8266
typedef enum
{
    GPIO_NUM_0 = 0,
    GPIO_NUM_1,
    GPIO_NUM_2,
    GPIO_NUM_3,
    GPIO_NUM_4,
    GPIO_NUM_5,
    GPIO_NUM_6,
    GPIO_NUM_7,
    GPIO_NUM_8,
    GPIO_NUM_9,
    GPIO_NUM_10,
    GPIO_NUM_11,
    GPIO_NUM_12,
    GPIO_NUM_13,
    GPIO_NUM_14,
    GPIO_NUM_15,
    GPIO_NUM_16,
    GPIO_NUM_17,
    GPIO_NUM_18,
    GPIO_NUM_19,
    GPIO_NUM_20,
    GPIO_NUM_21,
    GPIO_NUM_22,
    GPIO_NUM_23,
    GPIO_NUM_24,
    GPIO_NUM_25,
    GPIO_NUM_26,
    GPIO_NUM_27,
    GPIO_NUM_28,
    GPIO_NUM_29,
    GPIO_NUM_30,
    GPIO_NUM_31,
    GPIO_NUM_32,
    GPIO_NUM_33,
    GPIO_NUM_34,
} gpio_num_t;

typedef enum
{
    UART_NUM_0 = 0,
    UART_NUM_1,
    UART_NUM_2
} uart_port_t;
#endif // def ARDUINO_ARCH_ESP8266

// Platform specific GPIO definitions
#if   defined (BOARD_ESP32_CAM)
#   include "platforms/GPIO_Defs_ESP32_CAM.hpp"
#elif defined (BOARD_ESP32_D1_MINI_ETH)
#   include "platforms/GPIO_Defs_ESP32_D1_MINI_ETH.hpp"
#elif defined (BOARD_ESP32_D1_MINI)
#   include "platforms/GPIO_Defs_ESP32_D1_MINI.hpp"
#elif defined (BOARD_ESP32_KA)
#   include "platforms/GPIO_Defs_ESP32_KA.hpp"
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
#elif defined (BOARD_ESP32_QUINLED_QUAD_AE_PLUS)
#   include "platforms/GPIO_Defs_ESP32_QUINLED_QUAD_AE_Plus.hpp"
#elif defined (BOARD_ESP32_QUINLED_QUAD_AE_PLUS_8)
#   include "platforms/GPIO_Defs_ESP32_QUINLED_QUAD_AE_Plus_8.hpp"
#elif defined (BOARD_ESP32_QUINLED_QUAD)
#   include "platforms/GPIO_Defs_ESP32_QUINLED_QUAD.hpp"
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
#elif defined (BOARD_ESP32_TWILIGHTLORD)
#   include "platforms/GPIO_Defs_ESP32_TWILIGHTLORD.hpp"
#elif defined (BOARD_ESP32_TWILIGHTLORD_ETH)
#   include "platforms/GPIO_Defs_ESP32_TWILIGHTLORD_ETH.hpp"
#elif defined (BOARD_ESP32_DEVKITC)
#   include "platforms/GPIO_Defs_ESP32_DevkitC.hpp"
#elif defined (BOARD_ESP01S)
#   include "platforms/GPIO_Defs_ESP8266_ESP01S.hpp"
#elif defined (BOARD_ESPS_V3)
#   include "platforms/GPIO_Defs_ESP8266_ESPS_V3.hpp"
#elif defined (BOARD_ESPS_ESP3DEUXQUATRO_DMX)
#   include "platforms/GPIO_Defs_ESP32_ESP3DEUXQuattro_DMX.hpp"
#elif defined (BOARD_ESP32_OCTA2GO)
#   include "platforms/GPIO_Defs_ESP32_Octa2go.hpp"
#elif defined (BOARD_ESP32_TETRA2GO)
#   include "platforms/GPIO_Defs_ESP32_Tetra2go.hpp"
#elif defined (BOARD_ESP32_KR_LIGHTS_MSM)
#   include "platforms/GPIO_Defs_ESP32_kr_lights_msm.hpp"
#elif defined (BOARD_ESP32_BREAKDANCEV2)
#   include "platforms/GPIO_Defs_ESP32_BreakDanceV2.hpp"
#elif defined (BOARD_ESP8266_D1MINI_LOLIN_SD)
#   include "platforms/GPIO_Defs_ESP8266_D1_mini_lolinsd.hpp"
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
