#pragma once

#include "book.h"

namespace vr3dui
{
	using namespace RE;
	using namespace vr_gui;
	using QuestType = RE::QUEST_DATA::Type;

	struct QuestTypeInfo
	{
		std::uint8_t     priority;
		std::string_view chapter_model_path;
	};

	inline constexpr std::string_view kAllQuestsChapterModelPath = "3DIRP/Chapters/All.nif";

	inline constexpr std::array<QuestTypeInfo, 12> kQuestTypeInfo{ {
		{ 12, "3DIRP/Chapters/None.nif" },            // kNone
		{ 1, "3DIRP/Chapters/MainQuest.nif" },        // kMainQuest
		{ 6, "3DIRP/Chapters/MagesGuild.nif" },       // kMagesGuild
		{ 7, "3DIRP/Chapters/ThievesGuild.nif" },     // kThievesGuild
		{ 8, "3DIRP/Chapters/DarkBrotherhood.nif" },  // kDarkBrotherhood
		{ 9, "3DIRP/Chapters/Companions.nif" },       // kCompanionsQuest
		{ 3, "3DIRP/Chapters/Miscellaneous.nif" },    // kMiscellaneous
		{ 4, "3DIRP/Chapters/Daedric.nif" },          // kDaedric
		{ 2, "3DIRP/Chapters/SideQuest.nif" },        // kSideQuest
		{ 5, "3DIRP/Chapters/CivilWar.nif" },         // kCivilWar
		{ 10, "3DIRP/Chapters/Dawnguard.nif" },       // kDLC01_Vampire
		{ 11, "3DIRP/Chapters/Dragonborn.nif" },      // kDLC02_Dragonborn
	} };

	struct JournalQuestData
	{
		RE::TESQuest*                       owner;
		std::vector<RE::BGSQuestObjective*> current_objectives;
		std::vector<RE::BGSQuestObjective*> completed_objectives;

		bool tracked{ false };
		bool completed{ false };
	};

	constexpr QuestTypeInfo GetQuestTypeInfo(QuestType a_type) noexcept
	{
		const auto index = static_cast<std::size_t>(a_type);

		if (index < kQuestTypeInfo.size()) { return kQuestTypeInfo[index]; }

		return { std::numeric_limits<std::uint8_t>::max(), "3DIRP/Chapters/None.nif" };
	}

	class QuestChapter : public Chapter
	{
	public:
		QuestChapter(std::optional<QuestType> a_type) :
			Chapter(std::string{
				a_type ? GetQuestTypeInfo(*a_type).chapter_model_path : "3DIRP/Chapters/All.nif" }),
			type(a_type)
		{}

		void MakePages(const Book::Layout& a_layout) override;

		std::optional<QuestType> type;
	};

	class QuestPage : public Page
	{
	public:
		explicit QuestPage(std::vector<JournalQuestData> a_quests) : quests(std::move(a_quests)) {}

		void Draw(Book::PageContext a_context, bool a_left_page) override;

	private:
		std::vector<JournalQuestData> quests;
	};

	class Journal : public Book
	{
	public:
		static constexpr std::string_view kModelPath = "3DIRP/journal.nif";

		Journal(bool a_isLeft, TESObjectREFR* a_objectReference, NiAVObject* a_root,
			NiTransform a_local);

	private:
	};

}
