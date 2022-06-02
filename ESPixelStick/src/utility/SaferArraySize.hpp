#pragma once
/*
* ESPixelStick.h
*
* Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
* Copyright (c) 2016, 2022 Shelby Merrick
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

#include "../ESPixelStick.h"

// Safer methods to get size of an array.
// The methods explicitly differentiate between size in bytes vs. element count.
// Results are constexpr, allowing use of the function(s) in static_assert.
// Compilation results in a compile-time constant.
//     (e.g., zero-cost at execution)
//
// Below are **over-simplified** examples of why this header exists.
// In short, it helps drive clarity as to whether the intended result
// is a size in bytes, or a count of elements, and helps avoids errors
// from future refactoring.
//
// //////////////////////////////////////////////////////////////////////
// Example #1 - Reduce Cost of Root-Causing Refactoring Errors
// //////////////////////////////////////////////////////////////////////
//
// Consider the valid function:
//
// #define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))
// void foo(void) {
//     STRUCTURE_TYPE alpha[10];
//     for (int i = 0; i < ARRAY_SIZE(alpha); i++) {
//         alpha[i].FieldZeta = i; 
//     }
//     // pass the ten structures to various functions
// }
//
// Now, it is later re-factored, so the caller provides the array of structures:
//
// void foo(STRUCTURE_TYPE* alpha) {
//     for (int i = 0; i < ARRAY_SIZE(alpha); i++) {
//         alpha[i].FieldZeta = i; 
//     }
//     // pass the ten structures to various functions
// }
//
// The above code compiles without error.  However, only one element
// of the array gets initialized.  Havok occurs only at runtime, and
// typically some time after the above code has finished executing,
// making this un-fun to root-cause.
//
// //////////////////////////////////////////////////////////////////////
// Example #2 - Element count vs. Byte count confusion:
// //////////////////////////////////////////////////////////////////////
//
// Consider the valid function:
//
// struct FIZZ { char data; }; // typ. in some other header file
// void bar(void) {
//     FIZZ array[10];
//     memset(array, 0, 10); // typ. other function expected count of bytes
//     // pass the ten structures to various functions
// }
//
// Later, the structure is changed.
//
// struct FIZZ { char data; char isValid; };
//
// As a result, the function bar() function only initialized 5 of
// the structures to zero ... the rest are uninitialized memory.
//
// //////////////////////////////////////////////////////////////////////
// The above examples are admittedly simple, to where one might think,
// "But that's obvious... I'd never do that / miss that in code review."
// In real-world situations, the same underlying issues are often
// obfuscated by macros, multiple levels of functions, etc.
//
// Thus, explicitly stating intent (bytes vs. elements) is preferable.
// //////////////////////////////////////////////////////////////////////

template <typename T, size_t N>
constexpr inline  __attribute__((always_inline)) size_t SaferArrayByteSize(T const (&)[N]) noexcept
{
    static_assert(N >= 1);
    static_assert(sizeof(T) >= 1);
    return N * sizeof(T);
}
template <typename T, size_t N>
constexpr inline  __attribute__((always_inline)) size_t SaferArrayElementCount(T const (&)[N]) noexcept
{
    static_assert(N >= 1);
    static_assert(sizeof(T) >= 1);
    return N;
}

// Technically, the compiler is not required to evaluate constexpr at compilation time.
// This can be forced (where absolutely required, e.g., ISRs), by wrapping the evaluation
// of the constexpr in the following template:
//
// Example:
//     static_eval<size_t, constexpr_strlen("Hello, World!")>::value
//
// At time of initial implementation, no instances were noticed as requiring this.
//
template<typename RESULT_TYPE, RESULT_TYPE EXPRESSION>
struct static_eval
{
    static constexpr RESULT_TYPE value = EXPRESSION;
};

