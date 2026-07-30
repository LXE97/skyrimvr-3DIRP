#pragma once

#include "vr_gui.h"

#include <algorithm>
#include <array>

namespace vr_gui
{
	// Makes the parent's direct children behave as one hover target. When several children overlap,
	// the child nearest the hand is considered hovered and receives clicks.
	class ExclusiveHoverGroup : public Behavior
	{
	public:
		static constexpr float kDefaultPositionalHysteresisFactor = 0.97f;

		explicit ExclusiveHoverGroup(Widget* a_parent,
			float a_positional_hysteresis_factor = kDefaultPositionalHysteresisFactor) :
			Behavior(a_parent),
			positional_hysteresis_factor(std::clamp(a_positional_hysteresis_factor, 0.0f, 1.0f))
		{}

		void Update(float a_delta) override;
		void OnClick(bool a_activate, Hand& a_hand, MenuAction a_action) override;

		bool IsExclusivelyHovered(const Widget* a_widget, bool a_isLeft) const
		{
			return active[a_isLeft] == a_widget;
		}

	private:
		std::array<Widget*, 2> active{};
		// Biases the current selection's squared distance. Lower values require a challenger
		// to be proportionally closer before it can replace the current selection.
		float positional_hysteresis_factor;
	};
}
