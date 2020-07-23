#pragma once
#include <Arduino.h>

#define MYFILE String (__FILE__).substring(String (__FILE__).lastIndexOf ("\\") + 1)

#define DEBUG_V(v)  LOG_PORT.println(String("------ ") + String(FPSTR(__func__) ) + ":" + MYFILE + ":" + String(__LINE__ ) + ": " + String(v) + String(" ------"));
#define DEBUG_START	DEBUG_V(F("Start"))
#define DEBUG_END	DEBUG_V(F("End"))
