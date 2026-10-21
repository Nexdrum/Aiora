#pragma once

#include <string>

#include "AioraTypes.h"

namespace aiora {

bool saveDrumKitFile(const std::string& path, const Track& track, std::string* error = nullptr);
bool loadDrumKitFile(const std::string& path, std::vector<DrumPad>& pads, std::string* error = nullptr);

bool saveProjectFile(const std::string& path, const Project& project, std::string* error = nullptr);
bool loadProjectFile(const std::string& path, Project& project, std::string* error = nullptr);
bool savePatchFile(const std::string& path, const Patch& patch, std::string* error = nullptr);
bool loadPatchFile(const std::string& path, Patch& patch, std::string* error = nullptr);

} // namespace aiora
