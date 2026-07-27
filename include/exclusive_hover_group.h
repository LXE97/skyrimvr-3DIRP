#pragma once

#include "vr_gui.h"

#include <array>

namespace vr_gui
{
	// Makes the parent's direct children behave as one hover target. When several children overlap,
	// the child nearest the hand is considered hovered and receives clicks.
	class ExclusiveHoverGroup : public Behavior
	{
	public:
		explicit ExclusiveHoverGroup(Widget* a_parent) : Behavior(a_parent) {}

		void Update(float a_delta) override;
		void OnClick(bool a_activate, Hand& a_hand, MenuAction a_action) override;

		bool IsExclusivelyHovered(const Widget* a_widget, bool a_isLeft) const
		{
			return active[a_isLeft] == a_widget;
		}

	private:
		std::array<Widget*, 2> active{};
	};
}
