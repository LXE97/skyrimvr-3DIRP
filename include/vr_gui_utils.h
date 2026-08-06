#pragma once
#include "vr_gui.h"
#include "vr_gui_input_block.h"
#include "exclusive_hover_group.h"

namespace vr_gui
{

	using namespace RE;

	// widget for interaction volumes that only uses the Hand's radius for hit detection
	class BasicHitbox : public Widget
	{
	public:
		using Widget::Widget;

		bool TestOverlap(Hand& a_hand) const override;
	};

	class HandInteractionMode : public Behavior
	{
	public:
		HandInteractionMode(Widget* a_parent) : Behavior(a_parent) {}

		void OnHover(bool a_activate, Hand& a_hand) override
		{
			if (a_activate && hover_hand && hover_hand != std::addressof(a_hand) && pointing_active)
			{
				Deactivate();
			}

			hover_hand = std::addressof(a_hand);
			pending_active = a_activate;
			transition_elapsed = 0.0f;
		}

		void Update(float a_delta) override
		{
			if (!hover_hand || pending_active == pointing_active) { return; }

			transition_elapsed += a_delta;
			if (transition_elapsed < kTransitionDelay) { return; }

			auto* pc = RE::PlayerCharacter::GetSingleton();
			if (pending_active)
			{
				hand_mode =
					hover_hand->RequestMode(Hand::Mode::kPointing, Hand::ModePriority::kPassive);

				if (auto* equipped = pc->GetEquippedObject(hover_hand->IsLeft());
					pc->IsWeaponDrawn() && equipped && equipped->As<RE::SpellItem>())
				{
					pc->DrawWeaponMagicHands(false);
					force_sheathed = true;
				}

				smoothing_handle_l = Controller::GetSingleton()->GetHand(true)->RequestSmoothing();
				smoothing_handle_r = Controller::GetSingleton()->GetHand(false)->RequestSmoothing();
			}
			else
			{
				Deactivate();
			}

			pointing_active = pending_active;
			transition_elapsed = 0.0f;
		}

	private:
		static constexpr float kTransitionDelay = 0.2f;

		void Deactivate()
		{
			hand_mode.Release();

			auto* pc = RE::PlayerCharacter::GetSingleton();
			if (hover_hand)
			{
				if (auto* equipped = pc->GetEquippedObject(hover_hand->IsLeft());
					equipped && equipped->As<RE::SpellItem>() && force_sheathed)
				{
					pc->DrawWeaponMagicHands(true);
				}
			}
			force_sheathed = false;

			smoothing_handle_l.Release();
			smoothing_handle_r.Release();
			pointing_active = false;
		}

		ModeHandle      hand_mode;
		SmoothingHandle smoothing_handle_l;
		SmoothingHandle smoothing_handle_r;
		Hand*           hover_hand{};
		bool            force_sheathed{ false };
		bool            pending_active{ false };
		bool            pointing_active{ false };
		float           transition_elapsed{};
	};

	class BlockInputOnHover : public Behavior
	{
	public:
		BlockInputOnHover(Widget* a_parent, InputBlock a_blocks,  bool a_left_hand_mode) :
			Behavior(a_parent),
			blocks(a_blocks),
			left_hand_mode(a_left_hand_mode)
		{}

		void OnHover(bool a_activate, Hand& a_hand) override
		{
			auto& handle = handles[a_hand.IsLeft()];

			if (a_activate)
			{
				auto       hand_blocks = blocks;
				const bool is_main_hand = a_hand.IsLeft() == left_hand_mode;
				if (!is_main_hand)
				{
					hand_blocks = static_cast<InputBlock>(std::to_underlying(hand_blocks) &
						~std::to_underlying(InputBlock::kActivatePickLength));
				}

				handle = InputBlockManager::GetSingleton()->Acquire(a_hand.IsLeft(), hand_blocks);
			}
			else
			{
				handle.Release();
			}
		}

	private:
		InputBlock                      blocks;
		 bool                     left_hand_mode;
		std::array<InputBlockHandle, 2> handles;
	};

	class SelectionHighlight : public Behavior
	{
	public:
		using OnHighlight = std::function<void(bool)>;
		using OnActivate = std::function<void()>;
		using IsSelected = std::function<bool()>;
		using CanInteract = std::function<bool()>;

		SelectionHighlight(Widget* a_parent, ExclusiveHoverGroup* a_hover_group,
			OnHighlight a_on_highlight, OnActivate a_on_activate = {},
			IsSelected a_is_selected = {}, CanInteract a_can_interact = {}) :
			Behavior(a_parent),
			hover_group(a_hover_group),
			is_selected(std::move(a_is_selected)),
			on_activate(std::move(a_on_activate)),
			on_highlight(std::move(a_on_highlight)),
			can_interact(std::move(a_can_interact))
		{}

		void Update(float a_delta) override;
		void OnClick(bool a_activate, Hand& a_hand, MenuAction a_action) override;

	private:
		ExclusiveHoverGroup* hover_group{};
		bool                 highlighted{};
		IsSelected           is_selected{};
		OnActivate           on_activate{};
		OnHighlight          on_highlight{};
		CanInteract          can_interact{};
	};

}
