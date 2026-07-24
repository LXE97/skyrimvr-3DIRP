#include "journal.h"

namespace vr3dui
{
	namespace
	{
		std::vector<QuestType> GetPlayerQuestTypes()
		{
			std::vector<QuestType> seen;

			for (auto& instance : PlayerCharacter::GetSingleton()->objectives)
			{
				if (auto obj = instance.Objective)
				{
					auto* quest = obj ? obj->ownerQuest : nullptr;

					if (!quest) { continue; }

					const auto type = quest->GetType();
					if (std::ranges::find(seen, type) == seen.end()) { seen.push_back(type); }
				}
			}

			return seen;
		}

		std::vector<JournalQuestData> GetQuestsByType(std::optional<QuestType> a_type)
		{
			std::vector<JournalQuestData> result;

			auto* player = PlayerCharacter::GetSingleton();
			if (!player) { return result; }

			for (const auto& instance : player->objectives)
			{
				auto* objective = instance.Objective;
				auto* quest = objective ? objective->ownerQuest : nullptr;

				if (!quest) { continue; }

				// nullopt means accept every quest type
				if (a_type && quest->GetType() != *a_type) { continue; }

				const auto state = instance.InstanceState;

				if (state != QUEST_OBJECTIVE_STATE::kDisplayed &&
					state != QUEST_OBJECTIVE_STATE::kCompletedDisplayed)
				{
					continue;
				}

				auto it = std::ranges::find(result, quest, &JournalQuestData::owner);

				if (it == result.end())
				{
					result.emplace_back();
					it = std::prev(result.end());

					it->owner = quest;
					it->tracked = quest->IsActive();
					it->completed = quest->IsCompleted();
				}

				if (state == QUEST_OBJECTIVE_STATE::kDisplayed)
				{
					it->current_objectives.push_back(objective);
				}
				else
				{
					it->completed_objectives.push_back(objective);
				}
			}

			return result;
		}
	}

	void QuestChapter::MakePages(const Book::Layout&)
	{
		auto quests = GetQuestsByType(type);

		pages.emplace_back(std::make_unique<QuestPage>(quests));

		// calculate number of lines per page based on font size, line spacing, and page height
		// sort quests vector into completed and active, then sort each sub list alphabetically
		// determine number of pages to create
		// split quests vector and pass to each page
	}

	void QuestPage::Draw(Book::PageContext a_context)
	{
		if (quests.empty()) { return; }

		auto& right_page = a_context.right_page;

		NiTransform cursor{};
		cursor.scale = a_context.layout.body_text_scale;
		cursor.translate.y = a_context.layout.kPageHeight * 0.5f - a_context.layout.top_margin;

		for (const auto& quest : quests)
		{
			cursor.translate.y -= a_context.layout.quest_line_spacing;

			const std::string_view quest_name = quest.owner->GetFullName();
			const float            text_half_width = static_cast<float>(quest_name.size()) * 0.5f;

			auto* line = right_page.AddChild<Widget>(cursor,
				NiPoint3(text_half_width, a_context.layout.quest_line_spacing * 0.5f, 1.0f));

			auto* name = line->AddChild<Widget>(NiTransform{}, NiPoint3{});
			name->AddText(quest_name, a_context.layout.body_character_spacing,
				a_context.layout.body_font_model_path);

			NiTransform tracker_transform{};
			tracker_transform.translate.x = -text_half_width - 0.5f;
			auto* tracker = line->AddChild<Widget>(tracker_transform, NiPoint3(0.5f, 0.5f, 0.5f));
			tracker->AddModel("HelperSphere.nif");
		}
	}

	Journal::Journal(
		bool a_isLeft, TESObjectREFR* a_objectReference, NiAVObject* a_root, NiTransform a_local) :
		Book(a_isLeft, a_objectReference, a_root, a_local)
	{
		auto seen_types = GetPlayerQuestTypes();

		std::ranges::sort(
			seen_types, {}, [](QuestType a_type) { return GetQuestTypeInfo(a_type).priority; });

		AddChapter(std::make_unique<QuestChapter>(std::nullopt));
		for (auto type : seen_types) { AddChapter(std::make_unique<QuestChapter>(type)); }
	}

}
