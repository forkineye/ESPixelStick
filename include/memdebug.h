#pragma once
#include <Arduino.h>
/*
// Use GPIO16 / Wemos D0 for hardware debug triggering
#define DEBUG_HW_PIN    16
#define DEBUG_HW_SET    digitalWrite(DEBUG_HW_PIN, HIGH)
#define DEBUG_HW_CLEAR  digitalWrite(DEBUG_HW_PIN, LOW)
#define DEBUG_HW_START  do {pinMode(DEBUG_HW_PIN, OUTPUT); DEBUG_HW_CLEAR;} while(0)
#define DEBUG_HW_END    do {DEBUG_HW_CLEAR; pinMode(DEBUG_HW_PIN, INPUT);} while(0)
*/

#ifndef LOG_PORT
#define LOG_PORT Serial
#endif // ndef LOG_PORT

#define MYFILE String (__FILE__).substring(String (__FILE__).lastIndexOf ("\\") + 1)

#define DEBUG_V(v)  {LOG_PORT.println(String("------ ") + String(FPSTR(__func__) ) + ":" + MYFILE + ":" + String(__LINE__ ) + ": " + String(v) + String(" ------")); LOG_PORT.flush();}
#define DEBUG_START	DEBUG_V(F ("Start"))
#define DEBUG_END	DEBUG_V(F ("End"))
