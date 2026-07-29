#include "journal.h"

#include "text_manager.h"

namespace vr3dirp
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

	void QuestChapter::MakePages(const Book::Layout& a_layout)
	{
		auto quests = GetQuestsByType(type);

		std::ranges::sort(quests, [](const JournalQuestData& a_lhs, const JournalQuestData& a_rhs) {
			if (a_lhs.completed != a_rhs.completed) { return !a_lhs.completed; }

			return std::string_view{ a_lhs.owner->GetFullName() } <
				std::string_view{ a_rhs.owner->GetFullName() };
		});

		const auto line_height = a_layout.quest_line_spacing;
		const int  lines_per_page =
			std::max(1, static_cast<int>(a_layout.kRightPageHeight / line_height));

		int              total_quest_lines = 0;
		std::vector<int> quest_line_counts;
		quest_line_counts.reserve(quests.size());

		for (const auto& questinfo : quests)
		{
			std::string title = questinfo.owner->GetFullName();
			const int   lines = std::max(1,
				FormatParagraph(title, a_layout.body_text_scale, a_layout.body_character_spacing,
					a_layout.kRightPageWidth - a_layout.horizontal_margin -
						a_layout.body_text_scale - a_layout.body_character_spacing));

			total_quest_lines += lines;
			quest_line_counts.push_back(lines);
		}

		const auto page_count =
			std::max(1, (total_quest_lines + lines_per_page - 1) / lines_per_page);

		pages.clear();
		pages.reserve(static_cast<std::size_t>(page_count));

		std::vector<JournalQuestData> page_quests;
		int                           page_lines = 0;

		for (std::size_t i = 0; i < quests.size(); ++i)
		{
			const int quest_lines = quest_line_counts[i];

			if (!page_quests.empty() && page_lines + quest_lines > lines_per_page)
			{
				pages.emplace_back(std::make_unique<QuestPage>(std::move(page_quests)));
				page_quests.clear();
				page_lines = 0;
			}

			page_quests.emplace_back(std::move(quests[i]));
			page_lines += quest_lines;
		}

		if (!page_quests.empty() || pages.empty())
		{
			pages.emplace_back(std::make_unique<QuestPage>(std::move(page_quests)));
		}
	}

	void QuestPage::Draw(Book::PageContext a_context, bool a_left_page)
	{
		if (quests.empty()) { return; }

		auto& L = a_context.layout;

		if (a_left_page)
		{
			auto& page_anchor = a_context.left_page;
			if (selected_quest_index <= quests.size())
			{
				// Get selected quest from hover group
				const auto& quest = quests[selected_quest_index];

				NiTransform zero{};
				auto textmanager = page_anchor.AddChild<TextManager>(zero, "3DIRP/charx256.nif");

				RE::BSString log_text{};
				quest.owner->GetJournalTextForInstance(log_text, quest.owner->currentInstanceID);
				std::string formatted_str = log_text.c_str();

				int log_lines = FormatParagraph(
					formatted_str, L.body_text_scale, L.body_character_spacing, L.kLeftPageWidth);

				NiTransform page_cursor{};
				page_cursor.translate.y = L.kLeftPageHeight * 0.5f - L.top_margin;
				const float page_half_width = L.kLeftPageWidth * 0.5f;

				page_cursor.translate.x = -page_half_width + L.horizontal_margin;
				page_cursor.scale = L.body_text_scale;

				auto* log = page_anchor.AddChild<Widget>(page_cursor, NiPoint3{});
				textmanager->AddText(log, formatted_str, L.body_character_spacing);

				page_cursor.translate.y -=
					log_lines * art_addon::AddonTextBox::kLineSpacing * L.body_text_scale + 1;

				for (auto& obj : quest.current_objectives)
				{
					if (page_cursor.translate.y < L.kRightPageHeight * -0.5f) { return; }
					std::string temp = "0 ";
					temp.append(obj->displayText.c_str());
					int   lines = FormatParagraph(temp, L.body_text_scale, L.body_character_spacing,
						L.kLeftPageWidth - L.horizontal_margin);
					auto* objective_text = page_anchor.AddChild<Widget>(page_cursor, NiPoint3{});
					textmanager->AddText(objective_text, temp, L.body_character_spacing);
					page_cursor.translate.y -= lines * L.objective_spacing;
				}

				for (auto& obj : quest.completed_objectives)
				{
					if (page_cursor.translate.y < L.kRightPageHeight * -0.5f) { return; }
					std::string temp = "- ";
					temp.append(obj->displayText.c_str());
					int   lines = FormatParagraph(temp, L.body_text_scale, L.body_character_spacing,
						L.kLeftPageWidth - L.horizontal_margin);
					auto* objective_text = page_anchor.AddChild<Widget>(page_cursor, NiPoint3{});
					textmanager->AddText(objective_text, temp, L.body_character_spacing);
					page_cursor.translate.y -= lines * L.objective_spacing;
				}
			}
		}
		else
		{
			auto&       page_anchor = a_context.right_page;
			NiTransform zero{};
			auto        textmanager = page_anchor.AddChild<TextManager>(zero, "3DIRP/charx256.nif");
			NiTransform page_cursor{};
			NiTransform line_cursor{};
			const float page_half_width = L.kRightPageWidth * 0.5f;

			page_cursor.translate.y = L.kRightPageHeight * 0.5f - L.top_margin;
			auto* indicator = page_anchor.AddChild<Widget>(page_cursor, NiPoint3());

			for (std::size_t i = 0; i < quests.size(); ++i)
			{
				const auto& quest = quests[i];
				std::string quest_name = quest.owner->GetFullName();
				const int   quest_lines = std::max(1,
					FormatParagraph(quest_name, L.body_text_scale, L.body_character_spacing,
						L.kRightPageWidth));

				line_cursor.translate.x = { -page_half_width + L.horizontal_margin };

				auto* line = page_anchor.AddChild<Widget>(
					page_cursor, NiPoint3(page_half_width, L.body_text_scale * 0.5f, 0.5f));
				line->SetPriority(page_anchor.GetPriority() + 1);

				line_cursor.scale = L.body_text_scale;
				auto* tracker = line->AddChild<Widget>(line_cursor, NiPoint3{});
				textmanager->AddText(tracker, quest.tracked ? ">" : "0", 0);

				line_cursor.translate.x += L.body_text_scale + L.body_character_spacing;

				auto* name = line->AddChild<Widget>(line_cursor, NiPoint3{});
				textmanager->AddText(name, quest_name, L.body_character_spacing);
				page_cursor.translate.y -= quest_lines * L.quest_line_spacing;

				// on hovered/highlighted: show visual and set selected quest index
				// on click/activate: draw quest info on other page
				// IsSelected: check selected quest index
				line->AddBehavior<SelectionHighlight>(
					page_anchor.GetBehavior<ExclusiveHoverGroup>(),
					[this, line, indicator, i, a_context](bool highlight) {
						if (highlight)
						{
							selected_quest_index = i;
							a_context.left_page.ClearChildren();
							Draw(a_context, true);
							auto&    L = a_context.layout;
							NiPoint3 offset = { L.kRightPageWidth * -0.5f + L.horizontal_margin +
									(L.body_text_scale + L.body_character_spacing) * 3 + 2.5f,
								line->GetTransform().translate.y - L.body_text_scale * 0.5f + 0.1f,
								0 };
							indicator->MoveTo(offset);
						}
					},
					[this, a_context, tracker, textmanager] {
						auto& quest = quests[selected_quest_index];
						quest.tracked ^= 1;
						helper::SetQuestTracked(quest.owner, quest.tracked);
						textmanager->SetCharacter(tracker, 0, quest.tracked ? '>' : '0');
										},
					[this, i] { return selected_quest_index == i; });

				if (selected_quest_index == i)
				{
					NiPoint3 offset = { L.kRightPageWidth * -0.5f + L.horizontal_margin +
							(L.body_text_scale + L.body_character_spacing) * 3 + 2.5f,
						line->GetTransform().translate.y - L.body_text_scale * 0.5f + 0.1f, 0 };
					indicator->MoveTo(offset);
				}
				indicator->AddModel("3DIRP/QuestIndicator.nif");
			}
		}
	}

	Journal::Journal(
		bool a_isLeft, TESObjectREFR* a_objectReference, NiAVObject* a_root, NiTransform a_local) :
		Book(Journal::kModelPath, a_isLeft, a_objectReference, a_root, a_local)
	{
		auto seen_types = GetPlayerQuestTypes();

		std::ranges::sort(
			seen_types, {}, [](QuestType a_type) { return GetQuestTypeInfo(a_type).priority; });

		AddChapter(std::make_unique<QuestChapter>(std::nullopt));
		for (auto type : seen_types) { AddChapter(std::make_unique<QuestChapter>(type)); }
	}

}
