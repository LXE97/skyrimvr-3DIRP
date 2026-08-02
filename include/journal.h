#pragma once

#include "book.h"

namespace vr3dirp
{
	using namespace RE;
	using namespace vr_gui;
	using QuestType = RE::QUEST_DATA::Type;
	using HiddenQuestSet = std::unordered_set<RE::FormID>;

	struct JournalSettings : BookSettings
	{
		float       journal_scale = 0.9f;
		float       quest_line_spacing = 0.6f;
		std::string hidden_quests = "3372b";
		int        show_misc_all = 0;
	};

	class Journal;

	struct QuestTypeInfo
	{
		std::uint8_t     priority;
		std::string_view chapter_model_path;
	};

	inline constexpr std::string_view kAllQuestsChapterModelPath = "3DIRP/Journal/Chapters/All.nif";
	inline constexpr std::string_view kFavoritesChapterModelPath = "3DIRP/Journal/Chapters/Favorites.nif";

	inline constexpr std::array<QuestTypeInfo, 12> kQuestTypeInfo{ {
		{ 12, "3DIRP/Journal/Chapters/None.nif" },            // kNone
		{ 3, "3DIRP/Journal/Chapters/MainQuest.nif" },        // kMainQuest
		{ 6, "3DIRP/Journal/Chapters/College.nif" },          // kMagesGuild
		{ 7, "3DIRP/Journal/Chapters/ThievesGuild.nif" },     // kThievesGuild
		{ 8, "3DIRP/Journal/Chapters/DarkBrotherhood.nif" },  // kDarkBrotherhood
		{ 9, "3DIRP/Journal/Chapters/Companions.nif" },       // kCompanionsQuest
		{ 1, "3DIRP/Journal/Chapters/Miscellaneous.nif" },    // kMiscellaneous
		{ 4, "3DIRP/Journal/Chapters/Daedric.nif" },          // kDaedric
		{ 2, "3DIRP/Journal/Chapters/SideQuest.nif" },        // kSideQuest
		{ 5, "3DIRP/Journal/Chapters/CivilWar.nif" },         // kCivilWar
		{ 10, "3DIRP/Journal/Chapters/Dawnguard.nif" },       // kDLC01_Vampire
		{ 11, "3DIRP/Journal/Chapters/Dragonborn.nif" },      // kDLC02_Dragonborn
	} };

	struct JournalQuestData
	{
		RE::TESQuest*                               owner;
		std::string                                 name;
		std::vector<RE::BGSInstancedQuestObjective> current_objectives;
		std::vector<RE::BGSInstancedQuestObjective> completed_objectives;
		std::vector<RE::BGSInstancedQuestObjective> failed_objectives;

		bool tracked{ false };
		bool completed{ false };
	};

	constexpr QuestTypeInfo GetQuestTypeInfo(QuestType a_type) noexcept
	{
		const auto index = static_cast<std::size_t>(a_type);

		if (index < kQuestTypeInfo.size()) { return kQuestTypeInfo[index]; }

		return { std::numeric_limits<std::uint8_t>::max(), "3DIRP/Journal/Chapters/None.nif" };
	}

	class QuestChapter : public Chapter
	{
	public:
		QuestChapter(std::optional<QuestType> a_type, Journal& a_journal, bool a_tracked_only = false) :
			Chapter(std::string{
				a_type ? GetQuestTypeInfo(*a_type).chapter_model_path :
						 a_tracked_only ? kFavoritesChapterModelPath : kAllQuestsChapterModelPath }),
			type(a_type),
			journal(a_journal),
			favorites(a_tracked_only)
		{}

		void MakePages(const Book::Layout& a_layout) override;
		void OnSelected(const Book::Layout& a_layout) override;

		std::optional<QuestType> type;

	private:
		Journal& journal;
		bool favorites {false};
	};

	class QuestPage : public Page
	{
	public:
		QuestPage(std::vector<JournalQuestData> a_quests, Journal& a_journal) :
			quests(std::move(a_quests)),
			journal(a_journal)
		{}

		void OnSelected() override { selected_quest_index = 0; }
		void Draw(Book::PageContext a_context, bool a_left_page) override;
		void HideQuest(const JournalQuestData& a_quest, Book::PageContext a_context);

	private:
		bool IsHidden(const JournalQuestData& a_quest) const;

		std::vector<JournalQuestData> quests;
		Journal&                      journal;
		std::size_t                   selected_quest_index{};
	};

	class HoldToActivate : public Behavior
	{
	public:
		using OnActivate = std::function<void()>;

		HoldToActivate(Widget* a_parent, OnActivate a_onActivate,
			MenuAction a_action = MenuAction::kSecondary) :
			Behavior(a_parent),
			onActivate(std::move(a_onActivate)),
			action(a_action)
		{}

		void OnClick(bool a_activate, Hand& a_hand, MenuAction a_action) override;
		void OnHover(bool a_activate, Hand& a_hand) override;
		void Update(float a_delta) override;

	private:
		static constexpr float kHoldTime = 1.0f;

		OnActivate onActivate;
		MenuAction action;
		float      elapsed{};
		bool       button_held{};
		bool       armed{};
	};

	class Journal : public Book
	{
	public:
		static constexpr std::string_view kModelPath = "3DIRP/Journal/journal.nif";

		Journal(bool a_isLeft, TESObjectREFR* a_objectReference, NiAVObject* a_root,
			JournalSettings& a_settings);

		bool IsQuestHidden(const RE::TESQuest* a_quest) const;
		bool HideQuest(const RE::TESQuest* a_quest);

		const HiddenQuestSet&  GetHiddenQuests() const { return hidden_quests; }
		const JournalSettings& GetJournalSettings() const { return journal_settings; }

	private:
		JournalSettings& journal_settings;
		HiddenQuestSet   hidden_quests;
	};

}
