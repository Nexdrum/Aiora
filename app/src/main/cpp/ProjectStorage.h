#pragma once

#include <string>

#include "AioraTypes.h"

namespace aiora {

bool saveProjectFile(const std::string& path, const Project& project, std::string* error = nullptr);
bool loadProjectFile(const std::string& path, Project& project, std::string* error = nullptr);
bool savePatchFile(const std::string& path, const Patch& patch, std::string* error = nullptr);
bool loadPatchFile(const std::string& path, Patch& patch, std::string* error = nullptr);

} // namespace aiora
