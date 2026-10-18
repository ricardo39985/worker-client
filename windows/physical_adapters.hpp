#pragma once
#include <windows.h>
#include <string>
#include <vector>
namespace ow::win {
struct DisplayInstance {std::string id,name;};
std::vector<DisplayInstance> present_display_instances();
// Resolve the adapter interface selected by Windows for this exact DXGI LUID
// to its present Display-class PnP device. No model/name/ordinal matching.
std::string display_instance_id(LUID luid);
}
