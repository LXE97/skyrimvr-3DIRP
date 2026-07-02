#pragma once

#include "SKSE/Impl/Stubs.h"
#include "VR/OpenVRUtils.h"
#include "VR/PapyrusVRAPI.h"
#include "VR/VRManagerAPI.h"
#include "higgsinterface001.h"
#include "vrikinterface001.h"

namespace spellbookvr
{
	constexpr const char* kPluginName = "Real_Spellbook_VR";

	extern uint32_t g_esp_index;

	extern PapyrusVRAPI* g_papyrusvr;

	void Init();

	void OnGameLoad();

	void PreLoadGame();
}
