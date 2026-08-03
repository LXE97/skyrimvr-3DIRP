#include "journal.h"

#include "settings.h"

#include "text_manager.h"

namespace vr3dirp
{
	namespace
	{
		constexpr int kShowMiscOnFavorites = 0b01;
		constexpr int kShowMiscOnAll = 0b10;

		MenuAction GetBookMenuAction(int a_button)
		{
			switch (a_button)
			{
			case 0:
				return MenuAction::kPrimary;
			case 1:
				return MenuAction::kSecondary;
			default:
				return MenuAction::kNone;
			}
		}

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

				const std::string_view tag{ a_text.data() + tag_begin + 1,
					tag_end - tag_begin - 1 };
				std::string            replacement;

				if (tag.starts_with(kAliasShortName) && tag.size() > kAliasShortName.size())
				{
					replacement = helper::ResolveAliasName(
						a_quest, a_instanceID, tag.substr(kAliasShortName.size()), true);
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

		constexpr char ToGlyphCharacter(unsigned int a_glyph)
		{ return static_cast<char>(static_cast<unsigned char>(a_glyph)); }

		unsigned int GetStatusSymbol(const JournalQuestData& a_quest)
		{
			if (a_quest.unseen) return 0x83U;
			if (a_quest.completed) return 0x82U;
			if (a_quest.tracked) return 0x80U;
			return 0x7FU;
		}

		std::string GetQuestName(const JournalQuestData& a_quest, const Book::Layout& a_layout)
		{
			std::string name = a_quest.owner ? a_quest.owner->GetFullName() : "";
			if (!name.empty()) { return name; }

			const RE::BGSInstancedQuestObjective* first_objective = nullptr;
			for (const auto* objectives : { &a_quest.current_objectives,
					 &a_quest.completed_objectives, &a_quest.failed_objectives })
			{
				if (!objectives->empty())
				{
					first_objective = std::addressof(objectives->front());
					break;
				}
			}

			auto* objective = first_objective ? first_objective->Objective : nullptr;
			if (!objective) { return name; }

			name = objective->displayText.c_str();
			ParseQuestString(name, objective->ownerQuest, first_objective->instanceID);

			const std::string prefix =
				std::string(1, ToGlyphCharacter(GetStatusSymbol(a_quest))) + ' ';
			const float line_width = a_layout.kRightPageWidth - a_layout.horizontal_margin;
			const float prefix_width =
				GetTextWidth(prefix, a_layout.body_text_scale, a_layout.body_character_spacing);
			const float separating_spacing =
				a_layout.body_character_spacing * a_layout.body_text_scale;
			TrimToLine(name, a_layout.body_text_scale, a_layout.body_character_spacing,
				line_width - prefix_width - separating_spacing);
			return name;
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
						const auto raw_type = quest->GetType();
						const auto type =
							raw_type == QuestType::kNone ? QuestType::kSideQuest : raw_type;
						if (std::ranges::find(seen, type) == seen.end()) { seen.push_back(type); }
					}
				}
			}

			return seen;
		}

		std::vector<JournalQuestData> GetQuestsByType(
			std::optional<QuestType> a_type, Journal& a_journal, bool tracked_only = false)
		{
			std::vector<JournalQuestData> result;
			const auto&                   settings = a_journal.GetJournalSettings();
			NewQuestSet                    quests_with_displayed_objectives;

			auto* player = PlayerCharacter::GetSingleton();
			if (!player) { return result; }

			for (const auto& instance : player->objectives)
			{
				auto* objective = instance.Objective;
				auto* quest = objective ? objective->ownerQuest : nullptr;

				if (!quest) { continue; }

				const bool is_displayed =
					objective->state == QUEST_OBJECTIVE_STATE::kFailedDisplayed ||
					objective->state == QUEST_OBJECTIVE_STATE::kCompletedDisplayed ||
					objective->state == QUEST_OBJECTIVE_STATE::kDisplayed;

				if (!a_type)
				{
					if (is_displayed)
					{
						quests_with_displayed_objectives.insert(quest->GetFormID());
					}
				}

				const auto raw_quest_type = quest->GetType();
				const auto quest_type =
					raw_quest_type == QuestType::kNone ? QuestType::kSideQuest : raw_quest_type;
				if (a_type)
				{
					if (quest_type != *a_type) { continue; }
				}
				else if (quest_type == QuestType::kMiscellaneous)
				{
					const int show_misc_flag = tracked_only ? kShowMiscOnFavorites : kShowMiscOnAll;
					if ((settings.show_misc_all & show_misc_flag) == 0) { continue; }
				}

				const auto state = instance.InstanceState;

				if (is_displayed)
				{
					auto it = std::ranges::find(result, quest, &JournalQuestData::owner);

					if (it == result.end())
					{
						result.emplace_back();
						it = std::prev(result.end());

						it->owner = quest;
						it->tracked = quest->IsActive();
						it->completed = quest->IsCompleted();
						it->unseen = a_journal.IsQuestUnseen(quest);
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

			if (!a_type)
			{
				a_journal.PruneNewQuests(quests_with_displayed_objectives);
			}

			return result;
		}
	}

	void QuestChapter::MakePages(const Book::Layout& a_layout)
	{
		auto quests = GetQuestsByType(type, journal, favorites);
		std::erase_if(quests, [this](const JournalQuestData& a_quest) {
			return journal.IsQuestHidden(a_quest.owner);
		});
		if (favorites)
		{
			std::erase_if(quests, [this](const JournalQuestData& a_quest) {
				return !a_quest.tracked || a_quest.completed;
			});
		}

		std::ranges::sort(
			quests, [&a_layout](const JournalQuestData& a_lhs, const JournalQuestData& a_rhs) {
				if (a_lhs.completed != a_rhs.completed) { return !a_lhs.completed; }
				if (a_lhs.unseen != a_rhs.unseen) { return a_lhs.unseen; }

				return GetQuestName(a_lhs, a_layout) < GetQuestName(a_rhs, a_layout);
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
			std::string quest_name = std::string(1, ToGlyphCharacter(GetStatusSymbol(quest))) + ' ';
			quest_name.append(GetQuestName(quest, a_layout));

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

	void QuestChapter::OnSelected(const Book::Layout& a_layout) { MakePages(a_layout); }

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
				auto        textmanager =
					page_anchor.AddChild<TextManager>(zero, "3DIRP/vr_gui/charx256_0.nif");

				auto* hide_button = page_anchor.AddChild<Widget>(
					zero, NiPoint3(L.kLeftPageWidth * 0.5f, L.kLeftPageHeight * 0.5f, 0.5f));
				const auto hide_action =
					GetBookMenuAction(journal.GetJournalSettings().hide_button);
				if (hide_action != MenuAction::kNone)
				{
					hide_button->AddBehavior<HoldToActivate>(
						[this, quest, a_context] { HideQuest(quest, a_context); }, hide_action);
				}

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

				auto drawObjective = [L, &page_cursor, &page_anchor, &textmanager](
										 const RE::BGSInstancedQuestObjective obj,
										 unsigned int                         symbol) {
					if (page_cursor.translate.y < L.kRightPageHeight * -0.5f) { return; }
					auto*       objective = obj.Objective;
					std::string temp = std::string(1, ToGlyphCharacter(symbol)) + ' ';
					temp.append(objective->displayText.c_str());
					ParseQuestString(temp, objective->ownerQuest, obj.instanceID);
					int   lines = FormatParagraph(temp, L.body_text_scale, L.body_character_spacing,
						L.kLeftPageWidth - L.horizontal_margin);
					auto* objective_text = page_anchor.AddChild<Widget>(page_cursor, NiPoint3{});
					textmanager->AddText(objective_text, temp, L.body_character_spacing);
					page_cursor.translate.y -= lines * L.objective_spacing;
				};

				for (const auto& instance : quest.current_objectives)
				{
					if (!instance.Objective) { continue; }
					drawObjective(instance, 0x83U);
				}

				for (const auto& instance : quest.failed_objectives)
				{
					if (!instance.Objective) { continue; }
					drawObjective(instance, 0x81U);
				}

				for (const auto& instance : quest.completed_objectives)
				{
					if (!instance.Objective) { continue; }
					drawObjective(instance, 0x82U);
				}
			}
		}
		else if (!a_left_page)
		{
			auto& page_anchor = a_context.right_page;
			//if (!page_anchor.GetChildren().empty()) { return; }

			NiTransform zero{};
			NiTransform page_cursor{};
			NiTransform line_cursor{};

			auto textmanager =
				page_anchor.AddChild<TextManager>(zero, "3DIRP/vr_gui/charx256_0.nif");

			const float page_half_width = L.kRightPageWidth * 0.5f;
			const float line_height = art_addon::AddonTextBox::kLineSpacing * L.body_text_scale;

			page_cursor.translate.y = L.kRightPageHeight * 0.5f - L.top_margin;
			line_cursor.scale = L.body_text_scale;
			line_cursor.translate.x = { -page_half_width + L.horizontal_margin };

			auto* indicator = page_anchor.AddChild<Widget>(page_cursor, NiPoint3());

			for (std::size_t i = 0; i < quests.size(); ++i)
			{
				auto& quest = quests[i];
				if (IsHidden(quest))
				{
					continue;
				}

				std::string quest_name =
					std::string(1, ToGlyphCharacter(GetStatusSymbol(quest))) + ' ';
				quest_name.append(GetQuestName(quest, L));

				const int quest_lines = FormatParagraph(quest_name, L.body_text_scale,
					L.body_character_spacing, L.kRightPageWidth - L.horizontal_margin);

				auto* line = page_anchor.AddChild<Widget>(
					page_cursor, NiPoint3(page_half_width, L.body_text_scale * 0.5f, 0.5f));
				line->SetPriority(page_anchor.GetPriority() + 1);
				auto* line_text = line->AddChild<Widget>(line_cursor, NiPoint3());

				textmanager->AddText(line_text, quest_name, L.body_character_spacing);
				if (quest.unseen)
				{
					journal.MarkQuestSeen(quest.owner);
					quest.unseen = false;
				}

				line->AddBehavior<SelectionHighlight>(
					page_anchor.GetBehavior<ExclusiveHoverGroup>(),
					// on hovered/highlighted: show visual and set selected quest index
					[this, line, indicator, i, a_context](bool highlight) {
						if (highlight && selected_quest_index != i)
						{
							selected_quest_index = i;

							auto&    L = a_context.layout;
							NiPoint3 offset = { 0,
								line->GetTransform().translate.y - L.body_text_scale * 0.3f, 0 };
							indicator->MoveTo(offset);

							a_context.left_page.ClearChildren();
							Draw(a_context, true);
						}
					},
					// on click/activate: toggle quest tracked status
					[this, a_context, line_text, textmanager] {
						auto& quest = quests[selected_quest_index];
						if (!quest.completed)
						{
							quest.tracked ^= 1;
							helper::SetQuestTracked(quest.owner, quest.tracked);
							textmanager->SetCharacter(
								line_text, 0, ToGlyphCharacter(quest.tracked ? 0x80U : 0x7FU));
						}
					},
					[](){return false;},
					[book = &journal] { return !book->IsAnimating(); });

				page_cursor.translate.y -=
					line_height * static_cast<float>(quest_lines) + L.quest_line_spacing;

				if (selected_quest_index == i)
				{
					NiPoint3 offset = { 0,
						line->GetTransform().translate.y - L.body_text_scale * 0.3f, 0 };
					indicator->MoveTo(offset);
				}
			}
			indicator->AddModel("3DIRP/Journal/QuestIndicator.nif");

			// Display page numbers on right side only
			if (!a_left_page)
			{
				std::string page_str = std::to_string(a_context.page_index + 1);
				page_str.append("/").append(std::to_string(a_context.num_pages));
				NiTransform t;
				t.scale = L.body_text_scale;
				t.translate = { L.kRightPageWidth * 0.5f - 1.5f * L.body_text_scale,
					L.body_text_scale * 0.5f - L.kRightPageHeight * 0.5f, 0.0f };
				auto* page_number = page_anchor.AddChild<Widget>(t, NiPoint3{});
				textmanager->AddText(page_number, page_str, L.body_character_spacing);
			}
		}
	}

	bool QuestPage::IsHidden(const JournalQuestData& a_quest) const
	{ return journal.IsQuestHidden(a_quest.owner); }

	void QuestPage::HideQuest(const JournalQuestData& a_quest, Book::PageContext a_context)
	{
		if (!a_quest.owner) { return; }
		auto hidden = std::ranges::find(quests, a_quest.owner, &JournalQuestData::owner);
		if (hidden == quests.end()) { return; }
		if (!journal.HideQuest(a_quest.owner)) { return; }

		auto next_visible = std::ranges::find_if(std::next(hidden), quests.end(),
			[this](const auto& a_entry) { return !IsHidden(a_entry); });

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

		button_held = a_activate;
		elapsed = 0.0f;
		armed = a_activate;
	}

	void HoldToActivate::OnHover(bool a_activate, Hand&)
	{
		if (a_activate) { return; }

		button_held = false;
		elapsed = 0.0f;
		armed = false;
	}

	void HoldToActivate::Update(float a_delta)
	{
		if (!button_held || !armed) { return; }

		elapsed += a_delta;
		if (elapsed < kHoldTime) { return; }

		armed = false;
		if (onActivate) { onActivate(); }
	}

	Journal::Journal(bool a_isLeft, TESObjectREFR* a_objectReference, NiAVObject* a_root) :
		Book(Journal::kModelPath, a_isLeft, a_objectReference, a_root,
			settings::Manager::GetSingleton()->Get("fJournalScale"))
	{
		auto* manager = settings::Manager::GetSingleton();
		journal_settings.journal_scale = manager->Get("fJournalScale");
		journal_settings.quest_line_spacing = manager->Get("fQuestLineSpacing");
		journal_settings.show_misc_all = static_cast<int>(manager->Get("iShowMiscInAll"));
		journal_settings.highlight_new_quests = manager->Get("bHighlightNewQuests") != 0.0f;
		journal_settings.hide_button = static_cast<int>(manager->Get("iHideButton"));
		journal_settings.font = static_cast<int>(manager->Get("iFont"));

		layout.quest_line_spacing = journal_settings.quest_line_spacing;
		layout.tab_spacing = 1.8f;
		layout.tab_scale = 1.1f;
		layout.tab_origin.y = 10.7f;

		hidden_quests = journal_settings.hidden_quests;
		new_quests = settings::GetNewQuestList();
		new_quests_at_open = new_quests;

		auto seen_types = GetPlayerQuestTypes();

		std::ranges::sort(
			seen_types, {}, [](QuestType a_type) { return GetQuestTypeInfo(a_type).priority; });

		AddChapter(std::make_unique<QuestChapter>(std::nullopt, *this, true));
		AddChapter(std::make_unique<QuestChapter>(std::nullopt, *this));
		for (auto type : seen_types) { AddChapter(std::make_unique<QuestChapter>(type, *this)); }

		chapters[0]->OnSelected(layout);
	}

	Journal::~Journal()
	{
		settings::CommitNewQuestList(new_quests_at_open, new_quests);
	}

	bool Journal::IsQuestHidden(const RE::TESQuest* a_quest) const
	{ return a_quest && hidden_quests.contains(a_quest->GetFormID()); }

	bool Journal::HideQuest(const RE::TESQuest* a_quest)
	{
		if (!a_quest) { return false; }

		const auto form_id = a_quest->GetFormID();
		if (!hidden_quests.insert(form_id).second) { return false; }

		journal_settings.hidden_quests.insert(form_id);

		SKSE::log::trace("hiding quest {:x}", form_id);
		return true;
	}

	bool Journal::IsQuestUnseen(const RE::TESQuest* a_quest) const
	{
		return journal_settings.highlight_new_quests && a_quest &&
			new_quests.contains(a_quest->GetFormID());
	}

	void Journal::MarkQuestSeen(const RE::TESQuest* a_quest)
	{
		if (a_quest) { new_quests.erase(a_quest->GetFormID()); }
	}

	void Journal::PruneNewQuests(const NewQuestSet& a_visible_quests)
	{
		std::erase_if(new_quests,
			[&a_visible_quests](const FormID a_form_id) {
				return !a_visible_quests.contains(a_form_id);
			});
	}

}
