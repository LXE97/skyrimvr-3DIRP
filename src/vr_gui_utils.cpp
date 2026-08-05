#include "vr_gui_utils.h"

namespace vr_gui
{
	bool BasicHitbox::TestOverlap(Hand& a_hand) const
	{
		// only use hand sphere
		auto t = a_hand.GetTransform();
		auto w = GetWorld();
		// broad phase
		if (helper::IntersectSphereSphere(
				t.translate, a_hand.GetRadius() * t.scale, w.translate, w.scale * radius))
		{  // narrow phase
			return helper::IntersectSphereOBB(
				t.translate, a_hand.GetRadius() * t.scale, w, extents * w.scale);
		}
		return false;
	}

    	void SelectionHighlight::Update(float)
	{
		if (!parent || (can_interact && !can_interact())) { return; }

		const bool selected = is_selected && is_selected();
		const bool should_highlight = selected ||
			(hover_group &&
				(hover_group->IsExclusivelyHovered(parent, false) ||
					hover_group->IsExclusivelyHovered(parent, true)));

		if (should_highlight != highlighted)
		{
			if (on_highlight) { on_highlight(should_highlight); }
			highlighted = should_highlight;
		}
	}

	void SelectionHighlight::OnClick(bool a_activate, Hand& a_hand, MenuAction a_action)
	{
		if (can_interact && !can_interact()) { return; }
		if (!a_activate || a_action != MenuAction::kPrimary) { return; }
		if (hover_group) { hover_group->Update(0.0f); }
		if (hover_group && !hover_group->IsExclusivelyHovered(parent, a_hand.IsLeft())) { return; }

		if (on_activate) on_activate();
	}
}
