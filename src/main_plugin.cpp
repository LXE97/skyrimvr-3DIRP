#include "main_plugin.h"

#include "book.h"
#include "helper_math.h"
#include "holster.h"
#include "hooks.h"
#include "journal.h"
#include "menu_checker.h"
#include "mod_event_sink.hpp"
#include "settings.h"
#include "vr_gui.h"
#include "vrinput.h"

namespace vr3dirp
{
	using namespace RE;
	using namespace art_addon;
	using namespace vr_gui;

	enum class BookType
	{
		kJournal = 0,
		kSpellbook,
		kNone
	};

	static void RegisterVRInputCallback();
	static void PlayerUpdate();
	static bool OnDebugButton(const vrinput::ModInputEvent& e);
	static bool OnSecondaryDebugButton(const vrinput::ModInputEvent& e);
	static bool OnDpad(const vrinput::ModInputEvent& e);
	static void SummonBook(bool isLeft, BookType a_type);

	uint32_t      g_esp_index{};
	PapyrusVRAPI* g_papyrusvr{};

	bool g_debug_print = true;
	bool g_left_hand_mode = false;
	bool g_use_firstperson = false;

	PlayerCharacter* pc{};

	// settings
	float shoulder_holster_radius = 20.f;
	float belly_holster_radius = 6.f;

	void VrikActionSummonBookLeft(int)
	{
		// if (auto book = vr_gui::Controller::GetSingleton()->FindRoot<Book>()) { book->Close(); }
		// else
		// {
			SummonBook(true, BookType::kNone);
		
	}

	void VrikActionSummonBookRight(int)
	{
		// if (auto book = vr_gui::Controller::GetSingleton()->FindRoot<Book>()) { book->Close(); }
		// else
		// {
			SummonBook(false, BookType::kNone);
		
	}

	void SummonBook(bool isLeft, BookType a_type)
	{
		if (auto book = vr_gui::Controller::GetSingleton()->FindRoot<Book>()) { book->Close(); }
		else
		{
			auto hand_node = vrinput::GetHandNode(vrinput::Hand(isLeft), false);
			// TODO: store in settings json or skse cosave
			NiTransform zero{};
			NiTransform default_transform;
			default_transform.scale = 1.0f;
			default_transform.translate = { 5.915527f, -10.583008f, 10.284607f };
			default_transform.rotate = { { 0.839558f, -0.198012f, -0.493753f },
				{ -0.528744f, -0.127261f, -0.825465f }, { 0.089592f, 0.956704f, -0.210660f } };

			std::unique_ptr<Book> temp;

			switch (a_type)
			{
			case BookType::kJournal:
				break;

			case BookType::kSpellbook:
				break;

			default:
				temp =
					std::make_unique<Book>(isLeft, pc->AsReference(), hand_node, default_transform);
			}
			auto* created = temp.get();
			vr_gui::Controller::GetSingleton()->AddRoot(std::move(temp));
		}
	}

	bool g_createHolstersPending = false;

	void CreateHolsters()
	{
		auto belly_node = pc->Get3D()->GetObjectByName("NPC Spine1 [Spn1]");
		if (belly_node)
		{
			NiTransform t{};
			t.translate = { 0, 13, 0 };

			auto  temp = std::make_unique<vr_gui::Holster>(belly_holster_radius, pc, belly_node, t,
				vrinput::Hand::kBoth, HolsterCallbacks{ .primary = [](Hand& h) {
					SummonBook(h.IsLeft(), BookType::kNone);
				} });
			auto* created = temp.get();
			vr_gui::Controller::GetSingleton()->AddRoot(std::move(temp));
		}
		else
		{
			SKSE::log::trace("belly node not initialized");
		}
	}

	void Init()
	{
		helper::InstallPlayerUpdateHook(PlayerUpdate);
		g_createHolstersPending = true;

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
	}

	static void PlayerUpdate()
	{
		art_addon::ArtAddonManager::GetSingleton()->Update();
		vr_gui::Controller::GetSingleton()->Update();

		if (g_createHolstersPending)
		{
			CreateHolsters();
			g_createHolstersPending = false;
		}
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

		if (e.button_state == vrinput::ButtonState::kButtonDown)
		{
			SKSE::log::trace("left hand empty : {}\nright hand empty: {}",
				helper::IsHandEmpty(true), helper::IsHandEmpty(false));

			toggle ^= 1;
		}

		return false;
	}

	static bool OnSecondaryDebugButton(const vrinput::ModInputEvent& e)
	{
		static bool toggle = true;

		vr_gui::Controller::GetSingleton()->ShowHitboxes(toggle);
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
