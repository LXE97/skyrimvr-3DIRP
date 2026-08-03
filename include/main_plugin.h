#pragma once

#include "SKSE/Impl/Stubs.h"
#include "VR/OpenVRUtils.h"
#include "VR/PapyrusVRAPI.h"
#include "VR/VRManagerAPI.h"
#include "higgsinterface001.h"
#include "vrikinterface001.h"

namespace vr3dirp
{
	extern PapyrusVRAPI* g_papyrusvr;

	void Init();
	void InitSerialization();

	void OnGameLoad();

	void PreLoadGame();

	void OnSaveGame();

	void ApplySettings();
	void ResetHiddenQuests();
}
