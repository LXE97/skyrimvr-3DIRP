#include "exclusive_hover_group.h"

#include <limits>

namespace vr_gui
{
	void ExclusiveHoverGroup::Update(float)
	{
		if (!parent) { return; }

		for (const bool is_left : { false, true })
		{
			auto* hand = Controller::GetSingleton()->GetHand(is_left);
			auto* winner = static_cast<Widget*>(nullptr);
			float best_score = std::numeric_limits<float>::max();

			for (const auto& child : parent->GetChildren())
			{
				if (!child->IsEnabled() || !child->IsHovered(is_left)) { continue; }

				const auto offset = child->GetHoverPosition() - hand->GetBoxTransform().translate;
				float      score = offset.Dot(offset);
				if (child.get() == active[is_left]) { score *= positional_hysteresis_factor; }
				if (score < best_score || (score == best_score && child.get() == active[is_left]))
				{
					best_score = score;
					winner = child.get();
				}
			}

			active[is_left] = winner;
		}
	}

	void ExclusiveHoverGroup::OnClick(bool a_activate, Hand& a_hand, MenuAction a_action)
	{
		// Collision traversal happens after behavior updates, so refresh before routing input.
		Update(0.0f);
		if (auto* target = active[a_hand.IsLeft()])
		{
			target->OnClick(a_activate, a_hand, a_action);
		}
	}
}
