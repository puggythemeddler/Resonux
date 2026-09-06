#pragma once
#include "effects/Effect.h"

namespace fx {

Effect* create(int id);
const char* nameOf(int id);

}  // namespace fx