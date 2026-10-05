#pragma once

#include <string_view>

#include "AioraTypes.h"

namespace aiora {

// Native offline equivalent of the browser AI designer fallback.
Patch heuristicPatch(std::string_view description);

} // namespace aiora
