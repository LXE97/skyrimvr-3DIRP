#include "main_plugin.h"

#include "art_addon.h"
#include "book.h"
#include "helper_game.h"
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

	static void SummonBook(bool isLeft, BookType a_type);
	static void UpdateHandDebugModels();

	uint32_t      g_esp_index{};
	PapyrusVRAPI* g_papyrusvr{};

	bool g_debug_print = true;
	bool g_left_hand_mode = false;
	bool g_use_firstperson = false;

	PlayerCharacter* pc{};

	ArtAddonPtr g_left_hand_center{};
	ArtAddonPtr g_left_hand_extents{};
	ArtAddonPtr g_right_hand_center{};
	ArtAddonPtr g_right_hand_extents{};
	static bool show_hands{};

	// settings
	float shoulder_holster_radius = 10.f;
	float belly_holster_radius = 9.f;

	void VrikActionSummonBookLeft(int) { SummonBook(true, BookType::kNone); }

	void VrikActionSummonBookRight(int) { SummonBook(false, BookType::kNone); }

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
			default_transform.translate = { 10.912109, -11.599609, 6.068359 };
			default_transform.rotate = {
				{ 0.887867, 0.113981, -0.444655 },
				{ -0.433474, -0.118227, -0.892423 },
				{ -0.154712, 0.984591, -0.055837 },
			};

			NiTransform default_transform_right;
			default_transform_right.scale = 1.0f;
			default_transform_right.translate = { 8.862305, 4.031250, 12.304688 };
			default_transform_right.rotate = {
				{ 0.259819, -0.026158, 0.962818 },
				{ 0.964624, 0.008581, -0.261627 },
				{ -0.003148, 0.996287, 0.027915 },
			};

			auto& t = isLeft ? default_transform : default_transform_right;

			std::unique_ptr<Book> temp;

			switch (a_type)
			{
			case BookType::kJournal:
				t.scale *= 0.9f;
				temp = std::make_unique<Journal>(isLeft, pc->AsReference(), hand_node, t);

				break;

			case BookType::kSpellbook:
				break;

			default:
				temp = std::make_unique<Book>(
					Book::kModelPath, isLeft, pc->AsReference(), hand_node, t);
			}

			if (temp) { vr_gui::Controller::GetSingleton()->AddRoot(std::move(temp)); }
		}
	}

	bool g_createHolstersPending = false;

	void CreateHolsters()
	{
		auto belly_node = pc->Get3D()->GetObjectByName("NPC Spine1 [Spn1]");
		if (belly_node)
		{
			NiTransform t{};
			t.translate = { 0, 13, -3 };

			auto temp = std::make_unique<vr_gui::Holster>(belly_holster_radius, pc, belly_node, t,
				vrinput::Hand::kBoth, HolsterCallbacks{ .both = [](Hand& h) {
					SummonBook(h.IsLeft(), BookType::kNone);
				} });
			vr_gui::Controller::GetSingleton()->AddRoot(std::move(temp));
		}
		else
		{
			SKSE::log::trace("belly node not initialized");
		}

		auto head_node = pc->Get3D()->GetObjectByName("NPC Head [Head]");
		if (head_node)
		{
			NiTransform t{};
			t.translate = { -20, 0, 0 };

			auto temp = std::make_unique<vr_gui::Holster>(shoulder_holster_radius, pc, head_node, t,
				vrinput::Hand::kLeft, HolsterCallbacks{ .primary = [](Hand& h) {
					SummonBook(h.IsLeft(), BookType::kJournal);
				} });

			vr_gui::Controller::GetSingleton()->AddRoot(std::move(temp));

			t.translate = { 20, 0, 0 };
			auto temp_right = std::make_unique<vr_gui::Holster>(shoulder_holster_radius, pc,
				head_node, t, vrinput::Hand::kRight, HolsterCallbacks{ .primary = [](Hand& h) {
					SummonBook(h.IsLeft(), BookType::kJournal);
				} });
			vr_gui::Controller::GetSingleton()->AddRoot(std::move(temp_right));
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
	}

	static void PlayerUpdate()
	{
		art_addon::ArtAddonManager::GetSingleton()->Update();
		vr_gui::Controller::GetSingleton()->Update();

		UpdateHandDebugModels();

		if (g_createHolstersPending && pc->Get3D())
		{
			CreateHolsters();
			g_createHolstersPending = false;
		}
	}

	void PreLoadGame()
	{
		//ShowHands(false);
		vr_gui::Controller::GetSingleton()->Cleanup();
		g_createHolstersPending = true;
	}

	void OnGameLoad()
	{
		vr_gui::Controller::GetSingleton()->Init();
		pc = PlayerCharacter::GetSingleton();
	}

	void ShowHands(bool a_show)
	{
		show_hands = a_show;

		if (!a_show)
		{
			g_left_hand_center.reset();
			g_left_hand_extents.reset();
			g_right_hand_center.reset();
			g_right_hand_extents.reset();
			return;
		}

		auto* player = PlayerCharacter::GetSingleton();
		auto* player_root = player ? player->Get3D() : nullptr;
		if (!player || !player_root) { return; }

		NiTransform local{};
		if (!g_left_hand_center)
		{
			g_left_hand_center = ArtAddon::Make("DebugSphere.nif", player, player_root, local);
		}
		if (!g_left_hand_extents)
		{
			g_left_hand_extents = ArtAddon::Make("DrawExtents.nif", player, player_root, local);
		}
		if (!g_right_hand_center)
		{
			g_right_hand_center = ArtAddon::Make("DebugSphere.nif", player, player_root, local);
		}
		if (!g_right_hand_extents)
		{
			g_right_hand_extents = ArtAddon::Make("DrawExtents.nif", player, player_root, local);
		}

		UpdateHandDebugModels();
	}

	static void UpdateHandDebugModels()
	{
		auto* controller = Controller::GetSingleton();

		auto update_hand = [controller](
							   bool a_isLeft, ArtAddonPtr& a_center, ArtAddonPtr& a_extents) {
			auto* hand = controller->GetHand(a_isLeft);

			if (a_center && a_center->Get3D())
			{
				auto sphere_world = hand->GetTransform();
				sphere_world.scale *= hand->GetRadius();
				a_center->SetWorldTransform(sphere_world);
			}

			if (a_extents && a_extents->Get3D())
			{
				a_extents->SetWorldTransform(hand->GetBoxTransform());
				helper::DrawBox(a_extents.get(), *hand->GetExtents());
				NiUpdateData context{};
				a_extents->Get3D()->Update(context);
			}
		};

		update_hand(true, g_left_hand_center, g_left_hand_extents);
		update_hand(false, g_right_hand_center, g_right_hand_extents);
	}

	static bool OnDebugButton(const vrinput::ModInputEvent& e)
	{
		if (e.button_state == vrinput::ButtonState::kButtonDown) {}
		return false;
	}

	static bool OnSecondaryDebugButton(const vrinput::ModInputEvent& e)
	{
		static bool toggle = true;

		vr_gui::Controller::GetSingleton()->ShowHitboxes(toggle);
		ShowHands(toggle);

		if (e.button_state == vrinput::ButtonState::kButtonDown) {}

		toggle ^= 1;
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
