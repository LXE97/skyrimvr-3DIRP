#include "journal.h"

#include "text_manager.h"

namespace vr3dirp
{
	namespace
	{

		char GetStatusSymbol(const JournalQuestData& a_quest)
		{
			//if (a_quest.failed) return 'x';
			if (a_quest.completed) return '/';
			if (a_quest.tracked) return '>';
			return ' ';
		}

		std::vector<QuestType> GetPlayerQuestTypes()
		{
			std::vector<QuestType> seen;

			for (auto& instance : PlayerCharacter::GetSingleton()->objectives)
			{
				if (auto obj = instance.Objective)
				{
					auto* quest = obj ? obj->ownerQuest : nullptr;

					if (!quest) { continue; }

					if (obj->state == QUEST_OBJECTIVE_STATE::kFailedDisplayed ||
						obj->state == QUEST_OBJECTIVE_STATE::kCompletedDisplayed ||
						obj->state == QUEST_OBJECTIVE_STATE::kDisplayed)
					{
						const auto type = quest->GetType();
						if (std::ranges::find(seen, type) == seen.end()) { seen.push_back(type); }
					}
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

				if (objective->state == QUEST_OBJECTIVE_STATE::kFailedDisplayed ||
					objective->state == QUEST_OBJECTIVE_STATE::kCompletedDisplayed ||
					objective->state == QUEST_OBJECTIVE_STATE::kDisplayed)
				{
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
					else if (state == QUEST_OBJECTIVE_STATE::kFailedDisplayed)
					{
						it->failed_objectives.push_back(objective);
					}
					else
					{
						it->completed_objectives.push_back(objective);
					}
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

		const float line_height = art_addon::AddonTextBox::kLineSpacing * a_layout.body_text_scale;

		const float available_height =
			a_layout.kRightPageHeight - a_layout.top_margin - a_layout.bottom_margin;

		pages.clear();

		std::vector<JournalQuestData> page_quests;
		float                         used_height = 0.0f;

		for (auto& quest : quests)
		{
			// Use exactly the same string and width as Draw().
			std::string quest_name = std::string(1, GetStatusSymbol(quest)) + ' ';
			quest_name.append(quest.owner->GetFullName());

			const int quest_lines = std::max(1,
				FormatParagraph(quest_name, a_layout.body_text_scale,
					a_layout.body_character_spacing,
					a_layout.kRightPageWidth - a_layout.horizontal_margin));

			const float text_height = line_height * static_cast<float>(quest_lines);

			// A gap is needed before this item only when another item
			// already occupies the page.
			const float required_height =
				text_height + (page_quests.empty() ? 0.0f : a_layout.quest_line_spacing);

			if (!page_quests.empty() && used_height + required_height > available_height)
			{
				pages.emplace_back(std::make_unique<QuestPage>(std::move(page_quests)));

				page_quests.clear();
				used_height = 0.0f;
			}

			if (!page_quests.empty()) { used_height += a_layout.quest_line_spacing; }

			used_height += text_height;
			page_quests.emplace_back(std::move(quest));
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
				auto textmanager = page_anchor.AddChild<TextManager>(zero, "3DIRP/charx256_0.nif");

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

				for (auto& obj : quest.failed_objectives)
				{
					if (page_cursor.translate.y < L.kRightPageHeight * -0.5f) { return; }
					std::string temp = "x ";
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
		else if (!a_left_page)
		{
			auto&       page_anchor = a_context.right_page;
			NiTransform zero{};
			auto textmanager = page_anchor.AddChild<TextManager>(zero, "3DIRP/charx256_0.nif");
			NiTransform page_cursor{};

			const float page_half_width = L.kRightPageWidth * 0.5f;
			const float line_height = art_addon::AddonTextBox::kLineSpacing * L.body_text_scale;

			page_cursor.translate.y = L.kRightPageHeight * 0.5f - L.top_margin;
			page_cursor.translate.x = { -page_half_width + L.horizontal_margin };
			page_cursor.scale = L.body_text_scale;

			auto* indicator = page_anchor.AddChild<Widget>(page_cursor, NiPoint3());

			for (std::size_t i = 0; i < quests.size(); ++i)
			{
				const auto& quest = quests[i];
				std::string quest_name = std::string(1, GetStatusSymbol(quest)) + ' ';
				quest_name.append(quest.owner->GetFullName());

				const int quest_lines = FormatParagraph(quest_name, L.body_text_scale,
					L.body_character_spacing, L.kRightPageWidth - L.horizontal_margin);

				auto* line = page_anchor.AddChild<Widget>(
					page_cursor, NiPoint3(page_half_width, L.body_text_scale * 0.5f, 0.5f));
				line->SetPriority(page_anchor.GetPriority() + 1);

				textmanager->AddText(line, quest_name, L.body_character_spacing);

				line->AddBehavior<SelectionHighlight>(
					page_anchor.GetBehavior<ExclusiveHoverGroup>(),
					// on hovered/highlighted: show visual and set selected quest index
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
					// on click/activate: toggle quest tracked status
					[this, a_context, line, textmanager] {
						auto& quest = quests[selected_quest_index];
						quest.tracked ^= 1;
						helper::SetQuestTracked(quest.owner, quest.tracked);
						textmanager->SetCharacter(line, 0, '7');
					},
					// IsSelected: check selected quest index
					[this, i] { return selected_quest_index == i; });

				page_cursor.translate.y -=
					line_height * static_cast<float>(quest_lines) + L.quest_line_spacing;

				if (selected_quest_index == i)
				{
					NiPoint3 offset = { L.kRightPageWidth * -0.5f + L.horizontal_margin +
							(L.body_text_scale + L.body_character_spacing) * 3 + 2.5f,
						line->GetTransform().translate.y - L.body_text_scale * 0.5f + 0.1f, 0 };
					indicator->MoveTo(offset);
				}
			}
			indicator->AddModel("3DIRP/QuestIndicator.nif");
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
