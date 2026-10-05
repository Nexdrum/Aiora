#pragma once

#include <string>

#include "AioraTypes.h"

namespace aiora {

bool saveProjectFile(const std::string& path, const Project& project, std::string* error = nullptr);
bool loadProjectFile(const std::string& path, Project& project, std::string* error = nullptr);

} // namespace aiora
