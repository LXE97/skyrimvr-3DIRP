#include "journal.h"

#include "text_manager.h"

namespace vr3dirp
{
	namespace
	{
		void ParseQuestString(
			std::string& a_text, RE::TESQuest* a_quest, std::uint32_t a_instanceID)
		{
			constexpr std::string_view kAlias = "Alias=";
			constexpr std::string_view kAliasShortName = "Alias.ShortName=";
			constexpr std::string_view kGlobal = "Global=";

			std::size_t search_pos = 0;
			while (true)
			{
				const auto tag_begin = a_text.find('<', search_pos);
				if (tag_begin == std::string::npos) { break; }

				const auto tag_end = a_text.find('>', tag_begin + 1);
				if (tag_end == std::string::npos) { break; }

				const std::string_view tag{
					a_text.data() + tag_begin + 1, tag_end - tag_begin - 1 };
				std::string replacement;

				if (tag.starts_with(kAliasShortName) && tag.size() > kAliasShortName.size())
				{
					replacement = helper::ResolveAliasName(a_quest, a_instanceID,
						tag.substr(kAliasShortName.size()), true);
				}
				else if (tag.starts_with(kAlias) && tag.size() > kAlias.size())
				{
					replacement = helper::ResolveAliasName(
						a_quest, a_instanceID, tag.substr(kAlias.size()), false);
				}
				else if (tag.starts_with(kGlobal) && tag.size() > kGlobal.size())
				{
					replacement = helper::ResolveGlobalValue(
						a_quest, a_instanceID, tag.substr(kGlobal.size()));
				}

				if (replacement.empty())
				{
					search_pos = tag_end + 1;
					continue;
				}

				a_text.replace(tag_begin, tag_end - tag_begin + 1, replacement);
				search_pos = tag_begin + replacement.size();
			}
		}

		char GetStatusSymbol(const JournalQuestData& a_quest)
		{
			//if (a_quest.failed) return 'x';
			if (a_quest.completed) return '/';
			if (a_quest.tracked) return '>';
			return 0x7F;
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
						it->current_objectives.push_back(instance);
					}
					else if (state == QUEST_OBJECTIVE_STATE::kFailedDisplayed)
					{
						it->failed_objectives.push_back(instance);
					}
					else
					{
						it->completed_objectives.push_back(instance);
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
			std::string quest_name = std::string(1, GetStatusSymbol(quest)) + (char)0x7F;
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
				pages.emplace_back(std::make_unique<QuestPage>(std::move(page_quests), journal));

				page_quests.clear();
				used_height = 0.0f;
			}

			if (!page_quests.empty()) { used_height += a_layout.quest_line_spacing; }

			used_height += text_height;
			page_quests.emplace_back(std::move(quest));
		}

		if (!page_quests.empty() || pages.empty())
		{
			pages.emplace_back(std::make_unique<QuestPage>(std::move(page_quests), journal));
		}
	}

	void QuestPage::Draw(Book::PageContext a_context, bool a_left_page)
	{
		if (quests.empty()) { return; }

		auto& L = a_context.layout;

		if (a_left_page)
		{
			auto& page_anchor = a_context.left_page;
			if (selected_quest_index < quests.size() && !IsHidden(quests[selected_quest_index]))
			{
				// Get selected quest from hover group
				const auto& quest = quests[selected_quest_index];

				NiTransform zero{};
				auto textmanager = page_anchor.AddChild<TextManager>(zero, "3DIRP/charx256_0.nif");

				auto* hide_button = page_anchor.AddChild<Widget>(
					zero, NiPoint3(L.kLeftPageWidth * 0.5f, L.kLeftPageHeight * 0.5f, 0.5f));
				hide_button->AddBehavior<HoldToActivate>(
					[this, quest, a_context] { HideQuest(quest, a_context); });

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

				for (const auto& instance : quest.current_objectives)
				{
					if (page_cursor.translate.y < L.kRightPageHeight * -0.5f) { return; }
					auto* objective = instance.Objective;
					if (!objective) { continue; }
					std::string temp = "0 ";
					temp.append(objective->displayText.c_str());
					ParseQuestString(temp, objective->ownerQuest, instance.instanceID);
					int   lines = FormatParagraph(temp, L.body_text_scale, L.body_character_spacing,
						L.kLeftPageWidth - L.horizontal_margin);
					auto* objective_text = page_anchor.AddChild<Widget>(page_cursor, NiPoint3{});
					textmanager->AddText(objective_text, temp, L.body_character_spacing);
					page_cursor.translate.y -= lines * L.objective_spacing;
				}

				for (const auto& instance : quest.failed_objectives)
				{
					if (page_cursor.translate.y < L.kRightPageHeight * -0.5f) { return; }
					auto* objective = instance.Objective;
					if (!objective) { continue; }
					std::string temp = "x ";
					temp.append(objective->displayText.c_str());
					ParseQuestString(temp, objective->ownerQuest, instance.instanceID);
					int   lines = FormatParagraph(temp, L.body_text_scale, L.body_character_spacing,
						L.kLeftPageWidth - L.horizontal_margin);
					auto* objective_text = page_anchor.AddChild<Widget>(page_cursor, NiPoint3{});
					textmanager->AddText(objective_text, temp, L.body_character_spacing);
					page_cursor.translate.y -= lines * L.objective_spacing;
				}

				for (const auto& instance : quest.completed_objectives)
				{
					if (page_cursor.translate.y < L.kRightPageHeight * -0.5f) { return; }
					auto* objective = instance.Objective;
					if (!objective) { continue; }
					std::string temp = "- ";
					temp.append(objective->displayText.c_str());
					ParseQuestString(temp, objective->ownerQuest, instance.instanceID);
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
			auto& page_anchor = a_context.right_page;

			NiTransform zero{};
			NiTransform page_cursor{};
			NiTransform line_cursor{};

			auto textmanager = page_anchor.AddChild<TextManager>(zero, "3DIRP/charx256_0.nif");

			const float page_half_width = L.kRightPageWidth * 0.5f;
			const float line_height = art_addon::AddonTextBox::kLineSpacing * L.body_text_scale;

			page_cursor.translate.y = L.kRightPageHeight * 0.5f - L.top_margin;
			line_cursor.scale = L.body_text_scale;
			line_cursor.translate.x = { -page_half_width + L.horizontal_margin };

			auto* indicator = page_anchor.AddChild<Widget>(page_cursor, NiPoint3());

			for (std::size_t i = 0; i < quests.size(); ++i)
			{
				const auto& quest = quests[i];
				if (IsHidden(quest)) { continue; }

				std::string quest_name = std::string(1, GetStatusSymbol(quest)) + ' ';
				quest_name.append(quest.owner->GetFullName());

				const int quest_lines = FormatParagraph(quest_name, L.body_text_scale,
					L.body_character_spacing, L.kRightPageWidth - L.horizontal_margin);

				auto* line = page_anchor.AddChild<Widget>(
					page_cursor, NiPoint3(page_half_width, L.body_text_scale * 0.5f, 0.5f));
				line->SetPriority(page_anchor.GetPriority() + 1);
				auto* line_text = line->AddChild<Widget>(line_cursor, NiPoint3());

				textmanager->AddText(line_text, quest_name, L.body_character_spacing);

				line->AddBehavior<SelectionHighlight>(
					page_anchor.GetBehavior<ExclusiveHoverGroup>(),
					// on hovered/highlighted: show visual and set selected quest index
					[this, line, indicator, i, a_context](bool highlight) {
						if (highlight)
						{
							selected_quest_index = i;
							a_context.left_page.ClearChildren();

							auto&    L = a_context.layout;
							NiPoint3 offset = { L.kRightPageWidth * -0.5f + L.horizontal_margin +
									(L.body_text_scale + L.body_character_spacing) * 3 + 2.5f,
								line->GetTransform().translate.y - L.body_text_scale * 0.3f, 0 };
							indicator->MoveTo(offset);

							if (!a_context.left_page.GetChildren().empty()) Draw(a_context, true);
						}
					},
					// on click/activate: toggle quest tracked status
					[this, a_context, line_text, textmanager] {
						auto& quest = quests[selected_quest_index];
						if (!quest.completed)
						{
							quest.tracked ^= 1;
							helper::SetQuestTracked(quest.owner, quest.tracked);
							textmanager->SetCharacter(line_text, 0, quest.tracked ? '>' : 0x7F);
						}
					},
					// IsSelected: check selected quest index
					[this, i] { return selected_quest_index == i; });

				page_cursor.translate.y -=
					line_height * static_cast<float>(quest_lines) + L.quest_line_spacing;

				if (selected_quest_index == i)
				{
					NiPoint3 offset = { 0,
						line->GetTransform().translate.y - L.body_text_scale * 0.3f, 0 };
					indicator->MoveTo(offset);
				}
			}
			indicator->AddModel("3DIRP/QuestIndicator.nif");

			// Display page numbers on right side only
			if (!a_left_page)
			{
				std::string page_str = std::to_string(a_context.page_index + 1);
				page_str.append("/").append(std::to_string(a_context.num_pages));
				NiTransform t;
				t.translate = { L.kRightPageWidth * 0.5f - 1.5f, 0.5f - L.kRightPageHeight * 0.5f,
					0.0f };
				auto* page_number = page_anchor.AddChild<Widget>(t, NiPoint3{});
				textmanager->AddText(page_number, page_str, L.body_character_spacing);
			}
		}
	}

	bool QuestPage::IsHidden(const JournalQuestData& a_quest) const
	{
		return journal.IsQuestHidden(a_quest.owner);
	}

	void QuestPage::HideQuest(const JournalQuestData& a_quest, Book::PageContext a_context)
	{
		if (!a_quest.owner) { return; }
		auto hidden = std::ranges::find(quests, a_quest.owner, &JournalQuestData::owner);
		if (hidden == quests.end()) { return; }
		if (!journal.HideQuest(a_quest.owner)) { return; }

		auto next_visible = std::ranges::find_if(
			std::next(hidden), quests.end(), [this](const auto& a_entry) { return !IsHidden(a_entry); });

		if (next_visible == quests.end())
		{
			next_visible = std::ranges::find_if(
				quests.begin(), hidden, [this](const auto& a_entry) { return !IsHidden(a_entry); });
		}

		const bool has_visible_quest = next_visible != quests.end();
		if (has_visible_quest)
		{
			selected_quest_index =
				static_cast<std::size_t>(std::distance(quests.begin(), next_visible));
		}

		Controller::GetSingleton()->QueuePostUpdate([this, a_context, has_visible_quest] {
			a_context.left_page.ClearChildren();
			a_context.right_page.ClearChildren();

			Draw(a_context, false);
			if (has_visible_quest) { Draw(a_context, true); }
		});
	}

	void HoldToActivate::OnClick(bool a_activate, Hand&, MenuAction a_action)
	{
		if (a_action != action) { return; }

		buttonHeld = a_activate;
		elapsed = 0.0f;
		armed = a_activate;
	}

	void HoldToActivate::OnHover(bool a_activate, Hand&)
	{
		if (a_activate) { return; }

		buttonHeld = false;
		elapsed = 0.0f;
		armed = false;
	}

	void HoldToActivate::Update(float a_delta)
	{
		if (!buttonHeld || !armed) { return; }

		elapsed += a_delta;
		if (elapsed < kHoldTime) { return; }

		armed = false;
		if (onActivate) { onActivate(); }
	}

	Journal::Journal(
		bool a_isLeft, TESObjectREFR* a_objectReference, NiAVObject* a_root, NiTransform a_local) :
		Book(Journal::kModelPath, a_isLeft, a_objectReference, a_root, a_local)
	{
		auto seen_types = GetPlayerQuestTypes();

		std::ranges::sort(
			seen_types, {}, [](QuestType a_type) { return GetQuestTypeInfo(a_type).priority; });

		AddChapter(std::make_unique<QuestChapter>(std::nullopt, *this));
		for (auto type : seen_types)
		{
			AddChapter(std::make_unique<QuestChapter>(type, *this));
		}
	}

	bool Journal::IsQuestHidden(const RE::TESQuest* a_quest) const
	{
		return a_quest && hiddenQuests.contains(a_quest->GetFormID());
	}

	bool Journal::HideQuest(const RE::TESQuest* a_quest)
	{
		return a_quest && hiddenQuests.insert(a_quest->GetFormID()).second;
	}

}
