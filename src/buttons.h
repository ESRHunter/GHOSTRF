#pragma once
#include <Arduino.h>
#include "types.h"

void buttons_init();
ButtonEvent buttons_poll();
void buttons_inject_event(ButtonEvent evt);
