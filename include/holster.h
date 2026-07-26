#pragma once
#include "vr_gui.h"

namespace vr_gui
{
	using namespace RE;

	using HolsterCallback = std::function<void(Hand& h)>;
	struct HolsterCallbacks
	{
		HolsterCallback primary{};
		HolsterCallback secondary{};
		HolsterCallback both{};
	};

	/* Holster executes its callback if the player is holding the assigned button when their hand exits the holster.
    * If the button is already held before entering, a timer is started and checked on exit.
    */
	class Holster : public Widget
	{
	public:
		Holster(float a_radius, TESObjectREFR* a_objectReference, NiAVObject* a_transformParentNode,
			NiTransform a_local, vrinput::Hand a_hand_type, HolsterCallbacks a_callbacks = {}) :
			Widget(a_radius, a_objectReference, a_transformParentNode, a_local),
			hand_filter(a_hand_type),
			callbacks(std::move(a_callbacks))
		{}

		void OnHover(bool a_activate, Hand& a_hand) override
		{
			if (a_activate)
			{
				bool primary =
					Controller::GetSingleton()->IsMenuActionPressed(a_hand, MenuAction::kPrimary);
				bool secondary =
					Controller::GetSingleton()->IsMenuActionPressed(a_hand, MenuAction::kSecondary);
				if (primary || secondary) { entered_while_held = true; }
			}
			else
			{
				if (!entered_while_held || entered_while_held && hold_time > 0.5f)
				{
					bool primary = Controller::GetSingleton()->IsMenuActionPressed(
						a_hand, MenuAction::kPrimary);
					bool secondary = Controller::GetSingleton()->IsMenuActionPressed(
						a_hand, MenuAction::kSecondary);

					if (primary && secondary &&  callbacks.both) { callbacks.both(a_hand); }
					else if (primary && callbacks.primary) { callbacks.primary(a_hand); }
					else if (secondary && callbacks.secondary) { callbacks.secondary(a_hand); }
				}
				entered_while_held = false;
				hold_time = 0.f;
			}
		}

		void Update(float a_delta) override
		{
			if (entered_while_held) { hold_time += a_delta; }
		}

		bool HandStateFilter(Hand& a_hand) const override
		{
			if (a_hand.GetState() == Hand::State::kReady)
			{
				if (hand_filter == vrinput::Hand::kLeft && a_hand.IsLeft() ||
					hand_filter == vrinput::Hand::kRight && !a_hand.IsLeft() ||
					hand_filter == vrinput::Hand::kBoth)
				{
					return true;
				}
			}
			return false;
		}

	private:
		bool  entered_while_held{ false };
		float hold_time{ 0 };

		vrinput::Hand   hand_filter;
		HolsterCallbacks callbacks;
	};
}
