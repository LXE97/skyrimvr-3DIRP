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

namespace vr3dirp
{
	using namespace RE;

	enum class BookType
	{
		kDisabled = 0,
		kJournal,
		kSpellbook,
		kNone
	};

	BookType ParseBookType(float a_value)
	{
		switch (static_cast<int>(a_value))
		{
		case static_cast<int>(BookType::kJournal):
			return BookType::kJournal;
		case static_cast<int>(BookType::kSpellbook):
			return BookType::kSpellbook;
		case static_cast<int>(BookType::kNone):
			return BookType::kNone;
		default:
			return BookType::kDisabled;
		}
	}

	struct HolsterSettings
	{
		bool     allow_empty_arrow_hand;
		float    shoulder_radius;
		NiPoint3 shoulder_offset;
		float    belly_radius;
		NiPoint3 belly_offset;
		BookType left_shoulder_primary;
		BookType left_shoulder_secondary;
		BookType left_shoulder_both;
		BookType right_shoulder_primary;
		BookType right_shoulder_secondary;
		BookType right_shoulder_both;
		BookType belly_primary;
		BookType belly_secondary;
		BookType belly_both;
	};

	namespace
	{
		/* SKSE Cosave definitions */
		constexpr RE::FormID    kDefaultHiddenQuest = 0x3372b;
		constexpr std::uint32_t kSerializationID = static_cast<std::uint32_t>('3') |
			(static_cast<std::uint32_t>('D') << 8) | (static_cast<std::uint32_t>('I') << 16) |
			(static_cast<std::uint32_t>('R') << 24);
		constexpr std::uint32_t kNewQuestsRecord = static_cast<std::uint32_t>('N') |
			(static_cast<std::uint32_t>('Q') << 8) | (static_cast<std::uint32_t>('S') << 16) |
			(static_cast<std::uint32_t>('T') << 24);
		constexpr std::uint32_t kHiddenQuestsRecord = static_cast<std::uint32_t>('H') |
			(static_cast<std::uint32_t>('Q') << 8) | (static_cast<std::uint32_t>('S') << 16) |
			(static_cast<std::uint32_t>('T') << 24);
		constexpr std::uint32_t kQuestRecordVersion = 1;

		std::shared_mutex g_quest_state_mutex;
		NewQuestSet       g_new_quests;
		HiddenQuestSet    g_hidden_quests{ kDefaultHiddenQuest };

		void ResetHiddenQuestsLocked() { g_hidden_quests = { kDefaultHiddenQuest }; }

		bool WriteQuestForms(SKSE::SerializationInterface* a_intfc, std::uint32_t a_type,
			const std::unordered_set<RE::FormID>& a_forms)
		{
			if (!a_intfc->OpenRecord(a_type, kQuestRecordVersion)) { return false; }

			const auto count = static_cast<std::uint32_t>(a_forms.size());
			if (!a_intfc->WriteRecordData(count)) { return false; }
			for (const auto form_id : a_forms)
			{
				if (!a_intfc->WriteRecordData(form_id)) { return false; }
			}
			return true;
		}

		bool ReadQuestForms(SKSE::SerializationInterface* a_intfc, std::uint32_t a_length,
			std::unordered_set<RE::FormID>& a_forms)
		{
			std::uint32_t count{};
			if (a_length < sizeof(count) || a_intfc->ReadRecordData(count) != sizeof(count))
			{
				return false;
			}

			const auto available_form_ids =
				(a_length - static_cast<std::uint32_t>(sizeof(count))) / sizeof(RE::FormID);
			if (count > available_form_ids) { return false; }

			std::unordered_set<RE::FormID> loaded_forms;
			for (std::uint32_t index = 0; index < count; ++index)
			{
				RE::FormID form_id{};
				if (a_intfc->ReadRecordData(form_id) != sizeof(form_id)) { return false; }
				if (a_intfc->ResolveFormID(form_id, form_id)) { loaded_forms.insert(form_id); }
			}

			a_forms = std::move(loaded_forms);
			return true;
		}

		void SaveQuestForms(SKSE::SerializationInterface* a_intfc)
		{
			JournalState state;
			{
				std::shared_lock lock(g_quest_state_mutex);
				state.hidden_quests = g_hidden_quests;
				state.new_quests = g_new_quests;
			}

			if (!WriteQuestForms(a_intfc, kNewQuestsRecord, state.new_quests))
			{
				SKSE::log::error("Unable to serialize new quests");
			}
			if (!WriteQuestForms(a_intfc, kHiddenQuestsRecord, state.hidden_quests))
			{
				SKSE::log::error("Unable to serialize hidden quests");
			}
		}

		void LoadQuestForms(SKSE::SerializationInterface* a_intfc)
		{
			NewQuestSet    new_quests;
			HiddenQuestSet hidden_quests{ kDefaultHiddenQuest };
			std::uint32_t  type{};
			std::uint32_t  version{};
			std::uint32_t  length{};

			while (a_intfc->GetNextRecordInfo(type, version, length))
			{
				if (type != kNewQuestsRecord && type != kHiddenQuestsRecord) { continue; }
				if (version != kQuestRecordVersion)
				{
					SKSE::log::warn("Unsupported quest serialization version {}", version);
					continue;
				}

				auto& destination = type == kNewQuestsRecord ?
					static_cast<std::unordered_set<RE::FormID>&>(new_quests) :
					static_cast<std::unordered_set<RE::FormID>&>(hidden_quests);
				if (!ReadQuestForms(a_intfc, length, destination))
				{
					SKSE::log::error("Unable to deserialize quest form record {:x}", type);
				}
			}

			std::unique_lock lock(g_quest_state_mutex);
			g_new_quests = std::move(new_quests);
			g_hidden_quests = std::move(hidden_quests);
		}

		void RevertQuestForms(SKSE::SerializationInterface*)
		{
			std::unique_lock lock(g_quest_state_mutex);
			g_new_quests.clear();
			ResetHiddenQuestsLocked();
		}


		/* Settings management */
		BookSettings ReadBookSettings()
		{
			auto*        manager = settings::Manager::GetSingleton();
			BookSettings result;

			result.light_fade = manager->Get("fLightIntensity");
			result.page_brightness = manager->Get("fPageBrightness");
			result.font_size = manager->Get("fFontSize");
			result.book_scale = manager->Get("fBookScale");
			result.rightpage_text_z_offset = manager->Get("fRightPageTextZOffset");
			result.leftpage_text_z_offset = manager->Get("fLeftPageTextZOffset");
			result.rightpage_text_z_offset_righthand = manager->Get("fRightPageTextZOffsetRight");
			result.leftpage_text_z_offset_righthand = manager->Get("fLeftPageTextZOffsetRight");
			result.horizontal_margin = manager->Get("fHorizontalMargin");
			result.top_margin = manager->Get("fTopMargin");
			result.close_button = static_cast<int>(manager->Get("iCloseButton"));
			result.close_action = static_cast<int>(manager->Get("iCloseAction"));
			result.close_timing = manager->Get("fCloseTiming");

			result.right_offset_x = manager->Get("fRightOffsetX");
			result.right_offset_y = manager->Get("fRightOffsetY");
			result.right_offset_z = manager->Get("fRightOffsetZ");
			result.right_rotate_w = manager->Get("fRightRotateW");
			result.right_rotate_x = manager->Get("fRightRotateX");
			result.right_rotate_y = manager->Get("fRightRotateY");
			result.right_rotate_z = manager->Get("fRightRotateZ");

			result.left_offset_x = manager->Get("fLeftOffsetX");
			result.left_offset_y = manager->Get("fLeftOffsetY");
			result.left_offset_z = manager->Get("fLeftOffsetZ");
			result.left_rotate_w = manager->Get("fLeftRotateW");
			result.left_rotate_x = manager->Get("fLeftRotateX");
			result.left_rotate_y = manager->Get("fLeftRotateY");
			result.left_rotate_z = manager->Get("fLeftRotateZ");

			return result;
		}

		JournalSettings ReadJournalSettings()
		{
			auto* manager = settings::Manager::GetSingleton();
			return {
				.journal_scale = manager->Get("fJournalScale"),
				.quest_line_spacing = manager->Get("fQuestLineSpacing"),
				.show_misc_all = static_cast<int>(manager->Get("iShowMiscInAll")),
				.highlight_new_quests = manager->Get("bHighlightNewQuests") != 0.0f,
				.hide_button = static_cast<int>(manager->Get("iHideButton")),
				.font = static_cast<int>(manager->Get("iFont")),
			};
		}

		BookCallbacks MakeBookCallbacks()
		{
			return { .transform_changed = [](bool a_is_left, const NiTransform& a_transform) {
				auto*      manager = settings::Manager::GetSingleton();
				const auto rotation = helper::Mat2Quat(a_transform.rotate);
				const auto side = a_is_left ? "Left" : "Right";

				manager->Set(std::format("f{}OffsetX", side), a_transform.translate.x);
				manager->Set(std::format("f{}OffsetY", side), a_transform.translate.y);
				manager->Set(std::format("f{}OffsetZ", side), a_transform.translate.z);
				manager->Set(std::format("f{}RotateW", side), rotation.w);
				manager->Set(std::format("f{}RotateX", side), rotation.x);
				manager->Set(std::format("f{}RotateY", side), rotation.y);
				manager->Set(std::format("f{}RotateZ", side), rotation.z);
			} };
		}

		JournalState GetJournalState()
		{
			std::shared_lock lock(g_quest_state_mutex);
			return { .hidden_quests = g_hidden_quests, .new_quests = g_new_quests };
		}

		JournalCallbacks MakeJournalCallbacks()
		{
			return {
				.quest_hidden =
					[](RE::FormID a_form_id) {
						std::unique_lock lock(g_quest_state_mutex);
						g_hidden_quests.insert(a_form_id);
					},
				.new_quests_removed =
					[](const NewQuestSet& a_form_ids) {
						std::unique_lock lock(g_quest_state_mutex);
						for (const auto form_id : a_form_ids) { g_new_quests.erase(form_id); }
					},
			};
		}

		HolsterSettings ReadHolsterSettings()
		{
			auto* manager = settings::Manager::GetSingleton();

			return {
				.allow_empty_arrow_hand = static_cast<bool>(manager->Get("bAllowEmptyArrowHand")),
				.shoulder_radius = manager->Get("fShoulderRadius"),
				.shoulder_offset = { manager->Get("fShoulderX"), manager->Get("fShoulderY"),
					manager->Get("fShoulderZ") },
				.belly_radius = manager->Get("fBellyRadius"),
				.belly_offset = { manager->Get("fBellyX"), manager->Get("fBellyY"),
					manager->Get("fBellyZ") },
				.left_shoulder_primary = ParseBookType(manager->Get("iLeftShoulderPrimary")),
				.left_shoulder_secondary = ParseBookType(manager->Get("iLeftShoulderSecondary")),
				.left_shoulder_both = ParseBookType(manager->Get("iLeftShoulderBoth")),
				.right_shoulder_primary = ParseBookType(manager->Get("iRightShoulderPrimary")),
				.right_shoulder_secondary = ParseBookType(manager->Get("iRightShoulderSecondary")),
				.right_shoulder_both = ParseBookType(manager->Get("iRightShoulderBoth")),
				.belly_primary = ParseBookType(manager->Get("iBellyPrimary")),
				.belly_secondary = ParseBookType(manager->Get("iBellySecondary")),
				.belly_both = ParseBookType(manager->Get("iBellyBoth")),
			};
		}

		vr::EVRButtonId ReadControllerButton(std::string_view a_key)
		{
			switch (static_cast<int>(settings::Manager::GetSingleton()->Get(a_key)))
			{
			case 1:
				return vr::EVRButtonId::k_EButton_Grip;
			case 2:
				return vr::EVRButtonId::k_EButton_A;
			case 3:
				return vr::EVRButtonId::k_EButton_ApplicationMenu;
			default:
				return vr::EVRButtonId::k_EButton_SteamVR_Trigger;
			}
		}
	}

	void ResetHiddenQuests()
	{
		std::unique_lock lock(g_quest_state_mutex);
		ResetHiddenQuestsLocked();
	}

	static void RegisterVRInputCallback();
	static void PlayerUpdate();

	static bool OnDebugButton(const vrinput::ModInputEvent& e);
	static bool OnSecondaryDebugButton(const vrinput::ModInputEvent& e);

	static void OnEquipped(const RE::TESEquipEvent* event);
	static void OnQuestStartStop(const RE::TESQuestStartStopEvent* a_event);

	void        SummonBook(bool isLeft, BookType a_type);
	static void CreateHolsters();

	PapyrusVRAPI* g_papyrusvr{};

	bool                          g_left_hand_mode = false;
	bool                          g_use_firstperson = false;
	bool                          g_show_debug_spheres = false;
	bool                          g_createHolstersPending = false;
	std::vector<vr_gui::Holster*> g_holsters;

	PlayerCharacter* pc{};

	void VrikActionSummonBookLeft(int) { SummonBook(true, BookType::kJournal); }

	void VrikActionSummonBookRight(int) { SummonBook(false, BookType::kJournal); }

	void Init()
	{
		settings::Manager::GetSingleton()->Init(
			helper::GetGamePath() / "SKSE/Plugins/3DIRP_VR_UI.json");

		helper::InstallPlayerUpdateHook(PlayerUpdate);

		g_createHolstersPending = true;

		hooks::ModelReferenceEffect_SaveGameHook::Install();

		menuchecker::begin();

		RegisterVRInputCallback();

		g_vrikInterface->addGestureAction(VrikActionSummonBookLeft, "Journal Left");
		g_vrikInterface->addGestureAction(VrikActionSummonBookRight, "Journal Right");

		auto equip_sink = EventSink<RE::TESEquipEvent>::GetSingleton();
		equip_sink->AddCallback(OnEquipped);
		RE::ScriptEventSourceHolder::GetSingleton()->AddEventSink(equip_sink);

		auto* quest_sink = EventSink<RE::TESQuestStartStopEvent>::GetSingleton();
		quest_sink->AddCallback(OnQuestStartStop);
		RE::ScriptEventSourceHolder::GetSingleton()->AddEventSink(quest_sink);
	}

	void InitSerialization()
	{
		auto* serialization = SKSE::GetSerializationInterface();
		serialization->SetUniqueID(kSerializationID);
		serialization->SetSaveCallback(SaveQuestForms);
		serialization->SetLoadCallback(LoadQuestForms);
		serialization->SetRevertCallback(RevertQuestForms);
	}

	void OnQuestStartStop(const RE::TESQuestStartStopEvent* a_event)
	{
		if (!a_event || !a_event->started) { return; }
		SKSE::log::trace("quest started {}", a_event->formID);

		auto* quest = RE::TESForm::LookupByID<RE::TESQuest>(a_event->formID);
		if (!quest) { return; }

		std::unique_lock lock(g_quest_state_mutex);
		g_new_quests.insert(quest->GetFormID());
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
		vr_gui::Controller::GetSingleton()->Cleanup();
		g_holsters.clear();
		g_createHolstersPending = true;
	}

	void OnSaveGame() { settings::Manager::GetSingleton()->Save(); }

	void OnGameLoad()
	{
		settings::Manager::GetSingleton()->Reload();
		ApplySettings();
		vr_gui::Controller::GetSingleton()->Init();
		pc = PlayerCharacter::GetSingleton();
	}

	void ApplySettings()
	{
		vr_gui::Controller::GetSingleton()->SetSettings({
			.primary = ReadControllerButton("iPrimaryButton"),
			.secondary = ReadControllerButton("iSecondaryButton"),
		});

		vr_gui::Controller::GetSingleton()->ShowHitboxes(
			settings::Manager::GetSingleton()->Get("bShowDebugSpheres") != 0.0f);

		auto* controller = vr_gui::Controller::GetSingleton();
		for (auto* holster : g_holsters) { controller->MarkForDelete(holster); }
		g_holsters.clear();
		g_createHolstersPending = true;

		if (settings::Manager::GetSingleton()->Get("bDebugLog") != 0.0f)
		{
			settings::Manager::GetSingleton()->PrintSettings();
			spdlog::set_level(spdlog::level::trace);
		}
		else
		{
			spdlog::set_level(spdlog::level::info);
		}
	}

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
				temp = std::make_unique<Journal>(isLeft, pc->AsReference(), hand_node,
					ReadBookSettings(), MakeBookCallbacks(), ReadJournalSettings(),
					GetJournalState(), MakeJournalCallbacks());

				break;

			case BookType::kSpellbook:
				break;

			default:
				temp = std::make_unique<Book>(Book::kModelPath, isLeft, pc->AsReference(),
					hand_node, ReadBookSettings(), MakeBookCallbacks());
			}

			if (temp) { vr_gui::Controller::GetSingleton()->AddRoot(std::move(temp)); }
		}
	}

	static void CreateHolsters()
	{
		const auto holster_settings = ReadHolsterSettings();
		auto*      controller = vr_gui::Controller::GetSingleton();

		auto add_holster = [controller](std::unique_ptr<vr_gui::Holster> a_holster) {
			auto* holster =
				static_cast<vr_gui::Holster*>(controller->AddRoot(std::move(a_holster)));
			g_holsters.push_back(holster);
		};

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

		if (has_action(holster_settings.belly_primary, holster_settings.belly_secondary,
				holster_settings.belly_both))
		{
			auto belly_node = pc->Get3D()->GetObjectByName("NPC Spine1 [Spn1]");
			if (belly_node)
			{
				NiTransform t{};
				t.translate = holster_settings.belly_offset;

				auto temp = std::make_unique<vr_gui::Holster>(holster_settings.belly_radius, pc,
					belly_node, t, vrinput::Hand::kBoth,
					make_callbacks(holster_settings.belly_primary, holster_settings.belly_secondary,
						holster_settings.belly_both));
				add_holster(std::move(temp));
			}
			else
			{
				SKSE::log::trace("belly node not initialized");
			}
		}

		const bool create_left = has_action(holster_settings.left_shoulder_primary,
			holster_settings.left_shoulder_secondary, holster_settings.left_shoulder_both);
		const bool create_right = has_action(holster_settings.right_shoulder_primary,
			holster_settings.right_shoulder_secondary, holster_settings.right_shoulder_both);
		if (!create_left && !create_right) { return; }

		auto head_node = pc->Get3D()->GetObjectByName("NPC Head [Head]");
		if (head_node)
		{
			NiTransform t{};
			if (create_left)
			{
				t.translate = holster_settings.shoulder_offset;
				auto temp = std::make_unique<vr_gui::Holster>(holster_settings.shoulder_radius, pc,
					head_node, t, vrinput::Hand::kLeft,
					make_callbacks(holster_settings.left_shoulder_primary,
						holster_settings.left_shoulder_secondary,
						holster_settings.left_shoulder_both),
					holster_settings.allow_empty_arrow_hand);
				add_holster(std::move(temp));
			}

			if (create_right)
			{
				t.translate = holster_settings.shoulder_offset;
				t.translate.x = -t.translate.x;
				auto temp = std::make_unique<vr_gui::Holster>(holster_settings.shoulder_radius, pc,
					head_node, t, vrinput::Hand::kRight,
					make_callbacks(holster_settings.right_shoulder_primary,
						holster_settings.right_shoulder_secondary,
						holster_settings.right_shoulder_both),
					holster_settings.allow_empty_arrow_hand);
				add_holster(std::move(temp));
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

}
