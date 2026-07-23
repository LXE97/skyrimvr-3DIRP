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

	class QuestStringHolder : public Widget, ExclusiveHoverItem
	{
	public:
		QuestStringHolder(Widget* a_parent, std::string a_string, NiTransform a_local,
			TESQuest* a_target, const Book::Layout& a_layout, ExclusiveHoverGroup* a_group) :
			Widget(a_parent, a_local,
				NiPoint3((float)a_string.length() / 2, a_layout.body_character_spacing / 2, 1)),
			ExclusiveHoverItem(a_group),
			target(a_target)
		{
			// text =
			// 	std::make_unique<art_addon::AddonTextBox>(a_string, a_layout.body_character_spacing,
			// 		, a_local,
			// 		std::string{ a_layout.body_font_model_path });
			AddModel("HelperSphere.nif", false, [this](art_addon::ArtAddon* a) {
				a->Get3D()->local.translate.x -= this->extents.x + 0.5;
			});
		};

		void OnClick(bool a_activate, Hand& a_hand, MenuAction a_action) override
		{
			if (a_action == MenuAction::kPrimary && a_activate)
			{
				const bool tracked = !target->IsActive();

				if (helper::SetQuestTracked(target, tracked))
				{
					helper::SetGlowColor(model->Get3D(), tracked ? 0xFF0000 : 0xFFFFFF);
				}
			}
		}

		void OnHover(bool a_activate, Hand& a_hand) override
		{
			if (!group) { return; }

			if (a_activate) { group->Request(this, GetWorld().translate, a_hand); }
			else
			{
				group->Release(this, a_hand);
			}
		}

	private:
		TESQuest*                                target{};
		std::unique_ptr<art_addon::AddonTextBox> text;
	};

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

			right_page.AddChild<QuestStringHolder>(quest.owner->GetFullName(), cursor, quest.owner,
				a_context.layout, right_page.GetGroup());
		}
	}

	Journal::Journal(Widget* a_parent, bool a_isLeft, NiTransform a_transform) :
		Book(a_parent, kJournalModelPath, a_isLeft, a_transform)
	{
		auto seen_types = GetPlayerQuestTypes();

		std::ranges::sort(
			seen_types, {}, [](QuestType a_type) { return GetQuestTypeInfo(a_type).priority; });

		AddChapter(std::make_unique<QuestChapter>(std::nullopt));
		for (auto type : seen_types) { AddChapter(std::make_unique<QuestChapter>(type)); }
	}

}
