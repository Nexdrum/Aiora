#pragma once

#include <string>
#include <string_view>

#include "AioraTypes.h"

namespace aiora {

std::string serializeProjectJson(const Project& project);
bool deserializeProjectJson(std::string_view json, Project& project, std::string* error = nullptr);

std::string serializePatchJson(const Patch& patch);
bool deserializePatchJson(std::string_view json, Patch& patch, std::string* error = nullptr);

} // namespace aiora
