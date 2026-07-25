#include "main_plugin.h"

#include "book.h"
#include "helper_math.h"
#include "hooks.h"
#include "journal.h"
#include "menu_checker.h"
#include "mod_event_sink.hpp"
#include "settings.h"
#include "vr_gui.h"
#include "vrinput.h"

namespace vr3dui
{
	using namespace RE;
	using namespace art_addon;
	using namespace vr_gui;

	static void RegisterVRInputCallback();
	static void PlayerUpdate();
	static bool OnDebugButton(const vrinput::ModInputEvent& e);
	static bool OnSecondaryDebugButton(const vrinput::ModInputEvent& e);
	static bool OnDpad(const vrinput::ModInputEvent& e);
	static void SummonBook(bool isLeft, Book** store);

	uint32_t      g_esp_index{};
	PapyrusVRAPI* g_papyrusvr{};

	bool g_debug_print = true;
	bool g_left_hand_mode = false;
	bool g_use_firstperson = false;

	PlayerCharacter* pc{};

	Book* book;

	void VrikActionSummonBookLeft(int presscount) { SummonBook(true, &book); }

	void VrikActionSummonBookRight(int presscount) { SummonBook(false, &book); }

	void SummonBook(bool isLeft, Book** store)
	{
		if (!*store)
		{
			auto        hand_node = vrinput::GetHandNode(vrinput::Hand(isLeft), false);
			NiTransform t{};
			t.translate = { 10, 0, 0 };

			// TODO: store in settings json or skse cosave
			NiTransform zero{};
			NiTransform default_transform;
			default_transform.scale = 1.0f;
			default_transform.translate = { 5.915527f, -10.583008f, 10.284607f };
			default_transform.rotate = { { 0.839558f, -0.198012f, -0.493753f },
				{ -0.528744f, -0.127261f, -0.825465f }, { 0.089592f, 0.956704f, -0.210660f } };

			auto temp =
				std::make_unique<Book>(isLeft, pc->AsReference(), hand_node, default_transform);
			book = temp.get();
			vr_gui::Controller::GetSingleton()->AddRoot(std::move(temp));
		}
		else
		{
			(*store)->Close();
			*store = nullptr;
		}
	}

	void Init()
	{
		helper::InstallPlayerUpdateHook(PlayerUpdate);

		hooks::ModelReferenceEffect_SaveGameHook::Install();

		menuchecker::begin();

		RegisterVRInputCallback();

		g_vrikInterface->addGestureAction(VrikActionSummonBookLeft, "Book Left");
		g_vrikInterface->addGestureAction(VrikActionSummonBookRight, "Book Right");

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
	}

	ArtAddonPtr handebug;

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

	static bool OnDebugButton(const vrinput::ModInputEvent& e)
	{
		static bool toggle = true;

		if (e.button_state == vrinput::ButtonState::kButtonDown) { toggle ^= 1; }

		return false;
	}

	static bool OnSecondaryDebugButton(const vrinput::ModInputEvent& e)
	{
		static bool toggle = true;

		if (book) book->ShowHitboxes(toggle);
		if (e.button_state == vrinput::ButtonState::kButtonDown) {}

		toggle ^= 1;
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
