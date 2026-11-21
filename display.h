#pragma once
#include <Arduino.h>

namespace Display {
  void begin();
  // Two lines, 16 characters each (extra characters are cut, short lines are padded).
  void show(const char *line1, const char *line2);
}
