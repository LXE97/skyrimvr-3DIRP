#include "main_plugin.h"

#include "art_addon.h"
#include "book.h"
#include "equipment_checker.h"
#include "helper_game.h"
#include "helper_math.h"
#include "holster.h"
#include "hooks.h"
#include "journal.h"
#include "menu_checker.h"
#include "mod_event_sink.hpp"
#include "settings.h"
#include "text_manager.h"
#include "vr_gui.h"
#include "vrinput.h"

constexpr const char* g_ini_path = "SKSE/Plugins/3DIRP.ini";

namespace vr3dirp
{
	using namespace RE;
	using namespace art_addon;
	using namespace vr_gui;

	enum class BookType
	{
		kJournal = 0,
		kSpellbook,
		kNone,
		kDisabled
	};

	BookType ParseBookType(const std::string& a_value)
	{
		if (a_value == "Book") { return BookType::kNone; }
		if (a_value == "Journal") { return BookType::kJournal; }
		if (a_value == "Spellbook") { return BookType::kSpellbook; }

		return BookType::kDisabled;
	}

	JournalSettings settings{};

	bool ReadConfig(const char* a_ini_path);

	static void RegisterVRInputCallback();
	static void PlayerUpdate();

	static bool OnDebugButton(const vrinput::ModInputEvent& e);
	static bool OnSecondaryDebugButton(const vrinput::ModInputEvent& e);
	void        OnMenuOpenClose(RE::MenuOpenCloseEvent const* evn);
	void        OnEquipped(const RE::TESEquipEvent* event);

	static void SummonBook(bool isLeft, BookType a_type);
	static void UpdateHandDebugModels();
	static void CreateHolsters();

	uint32_t      g_esp_index{};
	PapyrusVRAPI* g_papyrusvr{};

	bool g_debug_print = true;
	bool g_left_hand_mode = false;
	bool g_use_firstperson = false;
	bool g_show_debug_spheres = false;

	bool g_createHolstersPending = false;

	PlayerCharacter* pc{};

	ArtAddonPtr g_left_hand_center{};
	ArtAddonPtr g_left_hand_extents{};
	ArtAddonPtr g_right_hand_center{};
	ArtAddonPtr g_right_hand_extents{};
	static bool show_hands{};

	// settings
	bool     allow_empty_arrow_hand = false;
	float    shoulder_holster_radius = 10.f;
	float    shoulder_holster_x = -20.f;
	float    shoulder_holster_y = 0.f;
	float    shoulder_holster_z = 0.f;
	float    belly_holster_radius = 9.f;
	float    belly_holster_x = 0.f;
	float    belly_holster_y = 13.f;
	float    belly_holster_z = -3.f;
	BookType left_shoulder_primary = BookType::kJournal;
	BookType left_shoulder_secondary = BookType::kDisabled;
	BookType left_shoulder_both = BookType::kDisabled;
	BookType right_shoulder_primary = BookType::kJournal;
	BookType right_shoulder_secondary = BookType::kDisabled;
	BookType right_shoulder_both = BookType::kDisabled;
	BookType belly_primary = BookType::kDisabled;
	BookType belly_secondary = BookType::kDisabled;
	BookType belly_both = BookType::kJournal;

	void VrikActionSummonBookLeft(int) { SummonBook(true, BookType::kJournal); }

	void VrikActionSummonBookRight(int) { SummonBook(false, BookType::kJournal); }

	void SummonBook(bool isLeft, BookType a_type)
	{
		if (a_type == BookType::kDisabled) { return; }

		if (auto book = vr_gui::Controller::GetSingleton()->FindRoot<Book>()) { book->Close(); }
		else
		{
			auto                  hand_node = vrinput::GetHandNode(vrinput::Hand(isLeft), false);
			std::unique_ptr<Book> temp;

			switch (a_type)
			{
			case BookType::kJournal:
				temp = std::make_unique<Journal>(isLeft, pc->AsReference(), hand_node, settings);

				break;

			case BookType::kSpellbook:
				break;

			default:
				temp = std::make_unique<Book>(
					Book::kModelPath, isLeft, pc->AsReference(), hand_node, settings);
			}

			if (temp) { vr_gui::Controller::GetSingleton()->AddRoot(std::move(temp)); }
		}
	}

	void Init()
	{
		helper::InstallPlayerUpdateHook(PlayerUpdate);
		g_createHolstersPending = true;

		hooks::ModelReferenceEffect_SaveGameHook::Install();

		ReadConfig(g_ini_path);

		menuchecker::begin();

		RegisterVRInputCallback();

		g_vrikInterface->addGestureAction(VrikActionSummonBookLeft, "Journal Left");
		g_vrikInterface->addGestureAction(VrikActionSummonBookRight, "Journal Right");

		auto menu_sink = EventSink<RE::MenuOpenCloseEvent>::GetSingleton();
		menu_sink->AddCallback(OnMenuOpenClose);
		RE::UI::GetSingleton()->AddEventSink(menu_sink);

		auto equip_sink = EventSink<RE::TESEquipEvent>::GetSingleton();
		equip_sink->AddCallback(OnEquipped);
		RE::ScriptEventSourceHolder::GetSingleton()->AddEventSink(equip_sink);
	}

	void OnMenuOpenClose(RE::MenuOpenCloseEvent const* evn)
	{
		if (!evn->opening && std::strcmp(evn->menuName.data(), "Journal Menu") == 0)
		{
			ReadConfig(g_ini_path);

			vr_gui::Controller::GetSingleton()->ShowHitboxes(g_show_debug_spheres);
		}
	}

	void OnEquipped(const RE::TESEquipEvent* event) { equipment_checker::OnEquipEvent(event); }

	static void PlayerUpdate()
	{
		art_addon::ArtAddonManager::GetSingleton()->Update();
		vr_gui::Controller::GetSingleton()->Update();

		if (g_createHolstersPending && pc->Get3D())
		{
			CreateHolsters();
			g_createHolstersPending = false;
		}
	}

	void PreLoadGame()
	{
		ShowHands(false);
		vr_gui::Controller::GetSingleton()->Cleanup();
		g_createHolstersPending = true;
	}

	void OnSaveGame()
	{
		const auto config_path = helper::GetGamePath() / g_ini_path;

		helper::WriteFloatToIni(config_path, "fRightOffsetX", settings.right_offset_x);
		helper::WriteFloatToIni(config_path, "fRightOffsetY", settings.right_offset_y);
		helper::WriteFloatToIni(config_path, "fRightOffsetZ", settings.right_offset_z);
		helper::WriteFloatToIni(config_path, "fRightRotateW", settings.right_rotate_w);
		helper::WriteFloatToIni(config_path, "fRightRotateX", settings.right_rotate_x);
		helper::WriteFloatToIni(config_path, "fRightRotateY", settings.right_rotate_y);
		helper::WriteFloatToIni(config_path, "fRightRotateZ", settings.right_rotate_z);

		helper::WriteFloatToIni(config_path, "fLeftOffsetX", settings.left_offset_x);
		helper::WriteFloatToIni(config_path, "fLeftOffsetY", settings.left_offset_y);
		helper::WriteFloatToIni(config_path, "fLeftOffsetZ", settings.left_offset_z);
		helper::WriteFloatToIni(config_path, "fLeftRotateW", settings.left_rotate_w);
		helper::WriteFloatToIni(config_path, "fLeftRotateX", settings.left_rotate_x);
		helper::WriteFloatToIni(config_path, "fLeftRotateY", settings.left_rotate_y);
		helper::WriteFloatToIni(config_path, "fLeftRotateZ", settings.left_rotate_z);

		helper::WriteStringToIni(config_path, "sHiddenQuests", settings.hidden_quests);
	}

	void OnGameLoad()
	{
		vr_gui::Controller::GetSingleton()->Init();
		pc = PlayerCharacter::GetSingleton();
		//art_addon::ArtAddonManager::GetSingleton()->OnGameLoad();
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

	// static bool OnDebugButton(const vrinput::ModInputEvent& e)
	// {
	// 	static bool toggle = true;

	// 	if (e.button_state == vrinput::ButtonState::kButtonDown) { toggle ^= 1; }
	// 	return false;
	// }

	// static bool OnSecondaryDebugButton(const vrinput::ModInputEvent& e)
	// {
	// 	static bool toggle = true;

	// 	if (e.button_state == vrinput::ButtonState::kButtonDown) { toggle ^= 1; }

	// 	return false;
	// }

	void CreateHolsters()
	{
		auto has_action = [](BookType a_primary, BookType a_secondary, BookType a_both) {
			return a_primary != BookType::kDisabled || a_secondary != BookType::kDisabled ||
				a_both != BookType::kDisabled;
		};
		auto make_callback = [](BookType a_type) -> HolsterCallback {
			if (a_type == BookType::kDisabled) { return {}; }

			return [a_type](Hand& a_hand) { SummonBook(a_hand.IsLeft(), a_type); };
		};
		auto make_callbacks = [&make_callback](
								  BookType a_primary, BookType a_secondary, BookType a_both) {
			return HolsterCallbacks{ .primary = make_callback(a_primary),
				.secondary = make_callback(a_secondary),
				.both = make_callback(a_both) };
		};

		if (has_action(belly_primary, belly_secondary, belly_both))
		{
			auto belly_node = pc->Get3D()->GetObjectByName("NPC Spine1 [Spn1]");
			if (belly_node)
			{
				NiTransform t{};
				t.translate = { belly_holster_x, belly_holster_y, belly_holster_z };

				auto temp = std::make_unique<vr_gui::Holster>(belly_holster_radius, pc, belly_node,
					t, vrinput::Hand::kBoth,
					make_callbacks(belly_primary, belly_secondary, belly_both));
				vr_gui::Controller::GetSingleton()->AddRoot(std::move(temp));
			}
			else
			{
				SKSE::log::trace("belly node not initialized");
			}
		}

		const bool create_left =
			has_action(left_shoulder_primary, left_shoulder_secondary, left_shoulder_both);
		const bool create_right =
			has_action(right_shoulder_primary, right_shoulder_secondary, right_shoulder_both);
		if (!create_left && !create_right) { return; }

		auto head_node = pc->Get3D()->GetObjectByName("NPC Head [Head]");
		if (head_node)
		{
			NiTransform t{};
			if (create_left)
			{
				t.translate = { shoulder_holster_x, shoulder_holster_y, shoulder_holster_z };
				auto temp = std::make_unique<vr_gui::Holster>(shoulder_holster_radius, pc,
					head_node, t, vrinput::Hand::kLeft,
					make_callbacks(
						left_shoulder_primary, left_shoulder_secondary, left_shoulder_both),
					allow_empty_arrow_hand);
				vr_gui::Controller::GetSingleton()->AddRoot(std::move(temp));
			}

			if (create_right)
			{
				t.translate = { -shoulder_holster_x, shoulder_holster_y, shoulder_holster_z };
				auto temp = std::make_unique<vr_gui::Holster>(shoulder_holster_radius, pc,
					head_node, t, vrinput::Hand::kRight,
					make_callbacks(
						right_shoulder_primary, right_shoulder_secondary, right_shoulder_both),
					allow_empty_arrow_hand);
				vr_gui::Controller::GetSingleton()->AddRoot(std::move(temp));
			}
		}
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

	bool ReadConfig(const char* a_ini_path)
	{
		using namespace std::filesystem;
		static std::filesystem::file_time_type last_read = {};

		auto config_path = helper::GetGamePath() / a_ini_path;

		if (auto setting = RE::GetINISetting("bLeftHandedMode:VRInput"))
		{
			g_left_hand_mode = setting->GetBool();
		}

		try
		{
			auto last_write = last_write_time(config_path);

			if (last_write > last_read)
			{
				std::ifstream config(config_path);
				if (config.is_open())
				{
					allow_empty_arrow_hand =
						(bool)helper::ReadIntFromIni(config, "bAllowEmptyArrowHand");
					shoulder_holster_radius = helper::ReadFloatFromIni(config, "fShoulderRadius");
					belly_holster_radius = helper::ReadFloatFromIni(config, "fBellyRadius");
					shoulder_holster_x = helper::ReadFloatFromIni(config, "fShoulderX");
					shoulder_holster_y = helper::ReadFloatFromIni(config, "fShoulderY");
					shoulder_holster_z = helper::ReadFloatFromIni(config, "fShoulderZ");
					belly_holster_x = helper::ReadFloatFromIni(config, "fBellyX");
					belly_holster_y = helper::ReadFloatFromIni(config, "fBellyY");
					belly_holster_z = helper::ReadFloatFromIni(config, "fBellyZ");
					left_shoulder_primary =
						ParseBookType(helper::ReadStringFromIni(config, "sLeftShoulderPrimary"));
					left_shoulder_secondary =
						ParseBookType(helper::ReadStringFromIni(config, "sLeftShoulderSecondary"));
					left_shoulder_both =
						ParseBookType(helper::ReadStringFromIni(config, "sLeftShoulderBoth"));
					right_shoulder_primary =
						ParseBookType(helper::ReadStringFromIni(config, "sRightShoulderPrimary"));
					right_shoulder_secondary =
						ParseBookType(helper::ReadStringFromIni(config, "sRightShoulderSecondary"));
					right_shoulder_both =
						ParseBookType(helper::ReadStringFromIni(config, "sRightShoulderBoth"));
					belly_primary =
						ParseBookType(helper::ReadStringFromIni(config, "sBellyPrimary"));
					belly_secondary =
						ParseBookType(helper::ReadStringFromIni(config, "sBellySecondary"));
					belly_both = ParseBookType(helper::ReadStringFromIni(config, "sBellyBoth"));

					settings.light_fade = helper::ReadFloatFromIni(config, "fLightIntensity");
					settings.font_size = helper::ReadFloatFromIni(config, "fFontSize");
					settings.book_scale = helper::ReadFloatFromIni(config, "fBookScale");
					settings.journal_scale = helper::ReadFloatFromIni(config, "fJournalScale");
					settings.quest_line_spacing =
						helper::ReadFloatFromIni(config, "fQuestLineSpacing");
					settings.rightpage_text_z_offset =
						helper::ReadFloatFromIni(config, "fRightPageTextZOffset");
					settings.rightpage_text_z_offset_righthand =
						helper::ReadFloatFromIni(config, "fRightPageTextZOffsetRight");
					settings.leftpage_text_z_offset =
						helper::ReadFloatFromIni(config, "fLeftPageTextZOffset");
					settings.leftpage_text_z_offset_righthand =
						helper::ReadFloatFromIni(config, "fLeftPageTextZOffsetRight");
					settings.horizontal_margin =
						helper::ReadFloatFromIni(config, "fHorizontalMargin");
					settings.top_margin = helper::ReadFloatFromIni(config, "fTopMargin");
					settings.show_misc_all = helper::ReadIntFromIni(config, "iShowMiscInAll");
					settings.hidden_quests = helper::ReadStringFromIni(config, "sHiddenQuests");

					settings.right_offset_x = helper::ReadFloatFromIni(config, "fRightOffsetX");
					settings.right_offset_y = helper::ReadFloatFromIni(config, "fRightOffsetY");
					settings.right_offset_z = helper::ReadFloatFromIni(config, "fRightOffsetZ");
					settings.right_rotate_w = helper::ReadFloatFromIni(config, "fRightRotateW");
					settings.right_rotate_x = helper::ReadFloatFromIni(config, "fRightRotateX");
					settings.right_rotate_y = helper::ReadFloatFromIni(config, "fRightRotateY");
					settings.right_rotate_z = helper::ReadFloatFromIni(config, "fRightRotateZ");

					settings.left_offset_x = helper::ReadFloatFromIni(config, "fLeftOffsetX");
					settings.left_offset_y = helper::ReadFloatFromIni(config, "fLeftOffsetY");
					settings.left_offset_z = helper::ReadFloatFromIni(config, "fLeftOffsetZ");
					settings.left_rotate_w = helper::ReadFloatFromIni(config, "fLeftRotateW");
					settings.left_rotate_x = helper::ReadFloatFromIni(config, "fLeftRotateX");
					settings.left_rotate_y = helper::ReadFloatFromIni(config, "fLeftRotateY");
					settings.left_rotate_z = helper::ReadFloatFromIni(config, "fLeftRotateZ");

					g_show_debug_spheres = (bool)helper::ReadIntFromIni(config, "bShowHitboxes");

					config.close();
					last_read = last_write_time(config_path);
					return true;
				}
				else
				{
					SKSE::log::error("error opening ini");
					last_read = file_time_type{};
				}
			}
			else
			{
				SKSE::log::trace("ini not read (no changes)");
			}
		} catch (const filesystem_error&)
		{
			SKSE::log::error("ini not found, using defaults");
			last_read = file_time_type{};
		}
		return false;
	}
}
