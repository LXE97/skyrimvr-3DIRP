#include "main_plugin.h"

#include "helper_math.h"
#include "menu_checker.h"
#include "mod_event_sink.hpp"
#include "settings.h"
#include "vr_gui.h"
#include "vrinput.h"

namespace spellbookvr
{
	static void RegisterVRInputCallback();
	static void PlayerUpdate();
	static bool OnDebugButton(const vrinput::ModInputEvent& e);
	static bool OnSecondaryDebugButton(const vrinput::ModInputEvent& e);
	static bool OnDpad(const vrinput::ModInputEvent& e);

	uint32_t g_esp_index{};
	PapyrusVRAPI* g_papyrusvr{};

	bool g_debug_print = true;
	bool g_left_hand_mode = false;
	bool g_use_firstperson = false;

	void Init()
	{
		helper::InstallPlayerUpdateHook(PlayerUpdate);

		menuchecker::begin();

		RegisterVRInputCallback();

		vrinput::AddCallback(OnDebugButton, vr::EVRButtonId::k_EButton_ApplicationMenu,
			vrinput::Hand::kRight, vrinput::ActionType::kPress);
		vrinput::AddCallback(OnSecondaryDebugButton, vr::EVRButtonId::k_EButton_A,
			vrinput::Hand::kRight, vrinput::ActionType::kPress);
		vrinput::AddCallback(OnDpad, vr::EVRButtonId::k_EButton_DPad_Up, vrinput::Hand::kRight,
			vrinput::ActionType::kPress);
		vrinput::AddCallback(OnDpad, vr::EVRButtonId::k_EButton_DPad_Down, vrinput::Hand::kRight,
			vrinput::ActionType::kPress);

		vrinput::StartBlockingAll();
	}

	static void PlayerUpdate()
	{
		art_addon::ArtAddonManager::GetSingleton()->Update();
		vr_gui::Controller::GetSingleton()->Update();
	}

	void PreLoadGame() { vr_gui::Controller::GetSingleton()->Cleanup(); }

	void OnGameLoad() { vr_gui::Controller::GetSingleton()->Init(); }

	std::vector<RE::NiTimeController*> ctrs;

	void GetAllControllers(RE::NiAVObject* a_obj)
	{
		if (!a_obj) { return; }

		for (auto* controller = a_obj->GetControllers(); controller;
			controller = controller->next.get())
		{
			ctrs.push_back(controller);
		}

		if (auto* node = a_obj->AsNode())
		{
			for (auto& child : node->children) { GetAllControllers(child.get()); }
		}
	}

	void SetCtrFlags(RE::NiAVObject* a_obj)
	{
		if (!a_obj) { return; }

		for (auto* controller = a_obj->GetControllers(); controller;
			controller = controller->next.get())
		{
			controller->flags.set(false, RE::NiTimeController::Flag::kCycleType_Clamp);
			controller->flags.set(true, RE::NiTimeController::Flag::kCycleType_Loop);
			controller->flags.set(true, RE::NiTimeController::Flag::kForceUpdate);
			controller->flags.set(true, RE::NiTimeController::Flag::kActive);
			controller->flags.set(false, RE::NiTimeController::Flag::kAnimType_AppTime);
			controller->flags.set(false, RE::NiTimeController::Flag::kComputeScaledTime);
		}

		if (auto* node = a_obj->AsNode())
		{
			for (auto& child : node->children) { SetCtrFlags(child.get()); }
		}
	}

	static bool OnDebugButton(const vrinput::ModInputEvent& e)
	{
		static bool toggle = true;

		if (e.button_state == vrinput::ButtonState::kButtonDown) { toggle ^= 1; }

		return false;
	}

	static bool OnSecondaryDebugButton(const vrinput::ModInputEvent& e)
	{
		static bool toggle = true;

		if (e.button_state == vrinput::ButtonState::kButtonDown) { toggle ^= 1; }

		return false;
	}

	static bool OnDpad(const vrinput::ModInputEvent& e)
	{
		static bool toggle = true;

		if (e.button_state == vrinput::ButtonState::kButtonDown) { toggle ^= 1; }
		return false;
	}

	static void RegisterVRInputCallback()
	{
		auto OVRHookManager = g_papyrusvr->GetOpenVRHook();
		if (OVRHookManager && OVRHookManager->IsInitialized())
		{
			OVRHookManager = RequestOpenVRHookManagerObject();
			if (OVRHookManager)
			{
				SKSE::log::info("Successfully requested OpenVRHookManagerAPI.");

				vrinput::InitControllerHooks();

				vrinput::g_leftcontroller =
					OVRHookManager->GetVRSystem()->GetTrackedDeviceIndexForControllerRole(
						vr::TrackedControllerRole_LeftHand);
				vrinput::g_rightcontroller =
					OVRHookManager->GetVRSystem()->GetTrackedDeviceIndexForControllerRole(
						vr::TrackedControllerRole_RightHand);

				vrinput::g_IVRSystem = OVRHookManager->GetVRSystem();

				OVRHookManager->RegisterControllerStateCB(vrinput::ControllerInputCallback);
				OVRHookManager->RegisterGetPosesCB(vrinput::ControllerPoseCallback);
			}
		}
		else
		{
			SKSE::log::trace("Failed to initialize OVRHookManager");
		}
	}
}
