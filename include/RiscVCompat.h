#pragma once
/*
* RiscVCompat.h - Portability shim for ESP32 RISC-V targets
*
* Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
* Copyright (c) 2026 Shelby Merrick
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
* ----------------------------------------------------------------------------
* Some third party libraries (notably ESPAsyncE131's RingBuf.h) implement their
* "atomic block" using the xtensa-only inline assembly instructions xt_rsil /
* xt_wsr_ps whenever ARDUINO_ARCH_ESP32 is defined. Those instructions do not
* exist on the RISC-V ESP32 parts (C3 / C5 / C6 / H2 / P4). The arduino-esp32
* v2.x core used to provide compatible xt_rsil / xt_wsr_ps macros for RISC-V,
* but the v3.x core (required for the ESP32-P4) no longer does, so the libraries
* fall back to the broken xtensa path and fail to compile.
*
* This header restores portable equivalents with the same save/restore
* semantics as rsil/wsr (raise the interrupt mask, returning the previous
* state, then restore it) using the FreeRTOS primitives that exist on every
* ESP32 port. It is force-included (-include) for RISC-V build environments so
* that the unmodified upstream libraries continue to build.
*/

#if defined(ARDUINO_ARCH_ESP32) && !defined(__XTENSA__)
#   ifndef xt_rsil
#       include <freertos/FreeRTOS.h>
#       define xt_rsil(level)    ((uint32_t)portSET_INTERRUPT_MASK_FROM_ISR())
#       define xt_wsr_ps(state)  portCLEAR_INTERRUPT_MASK_FROM_ISR((UBaseType_t)(state))
#   endif // ndef xt_rsil
#endif // ESP32 RISC-V
