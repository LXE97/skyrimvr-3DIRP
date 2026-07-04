#include "main_plugin.h"

#include "helper_math.h"
#include "hooks.h"
#include "menu_checker.h"
#include "mod_event_sink.hpp"
#include "settings.h"
#include "spellbook.h"
#include "vr_gui.h"
#include "vrinput.h"

namespace spellbookvr
{
	using namespace RE;
	using namespace art_addon;
	using namespace vr_gui;

	static void RegisterVRInputCallback();
	static void PlayerUpdate();
	static bool OnDebugButton(const vrinput::ModInputEvent& e);
	static bool OnSecondaryDebugButton(const vrinput::ModInputEvent& e);
	static bool OnDpad(const vrinput::ModInputEvent& e);

	uint32_t      g_esp_index{};
	PapyrusVRAPI* g_papyrusvr{};

	bool g_debug_print = true;
	bool g_left_hand_mode = false;
	bool g_use_firstperson = false;

	PlayerCharacter* pc{};

	std::vector<std::string> books;
	int                      selector{};
	ArtAddonPtr              book;

	void Init()
	{
		helper::InstallPlayerUpdateHook(PlayerUpdate);

		hooks::ModelReferenceEffect_SaveGameHook::Install();

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
		vrinput::AddCallback(OnDpad, vr::EVRButtonId::k_EButton_DPad_Left, vrinput::Hand::kRight,
			vrinput::ActionType::kPress);
		vrinput::AddCallback(OnDpad, vr::EVRButtonId::k_EButton_DPad_Right, vrinput::Hand::kRight,
			vrinput::ActionType::kPress);
		vrinput::StartBlockingAll();
	}

	static void PlayerUpdate()
	{
		art_addon::ArtAddonManager::GetSingleton()->Update();
		vr_gui::Controller::GetSingleton()->Update();
	}

	void PreLoadGame() { vr_gui::Controller::GetSingleton()->Cleanup(); }

	void OnGameLoad()
	{
		vr_gui::Controller::GetSingleton()->Init();
		pc = PlayerCharacter::GetSingleton();
	}

	spellbook::Spellbook* spbk;

	static bool OnDebugButton(const vrinput::ModInputEvent& e)
	{
		static bool toggle = true;

		if (e.button_state == vrinput::ButtonState::kButtonDown)
		{
			if (!spbk) { spbk = spellbook::Summon(true); }
			else
			{
				spellbook::Dismiss(spbk);
				spbk = nullptr;
			}

			toggle ^= 1;
		}

		return false;
	}

	static bool OnSecondaryDebugButton(const vrinput::ModInputEvent& e)
	{
		static bool                  toggle = true;
		static constexpr const char* fonts[4] = { "SpellbookVR/char_bold.nif",
			"SpellbookVR/char_feather.nif", "SpellbookVR/char_2048.nif",
			"SpellbookVR/char_2048_feather.nif" };
		static int                   i = 0;

		if (e.button_state == vrinput::ButtonState::kButtonDown)
		{
			static float offset = 0.005;

			if (spbk)
			{
				spbk->DisplaySpellInfo("SpellbookVR/char_2048.nif");
				offset += 0.001;
				SKSE::log::trace("offset {}", offset);
			}
			toggle ^= 1;
		}

		return false;
	}

	static bool OnDpad(const vrinput::ModInputEvent& e)
	{
		static bool toggle = true;
		if (spbk)
		{
			if (e.button_state == vrinput::ButtonState::kButtonDown &&
				e.button_ID == vr::EVRButtonId::k_EButton_DPad_Left)
			{
				spbk->TurnPage(true);
			}
			else if (e.button_state == vrinput::ButtonState::kButtonDown &&
				e.button_ID == vr::EVRButtonId::k_EButton_DPad_Right)
			{
				spbk->TurnPage(false);
			}
		}

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
