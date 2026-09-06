#pragma once
#include "config/Config.h"
#include "led/LEDDriver.h"

LEDDriver* createLEDDriver(const StripConfig& cfg);