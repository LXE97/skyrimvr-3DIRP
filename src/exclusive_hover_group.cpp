#include "exclusive_hover_group.h"

namespace vr_gui
{
	ExclusiveHoverItem::ExclusiveHoverItem(ExclusiveHoverGroup* a_group) : group(a_group)
	{
		if (group) { group->Register(this); }
	}

	ExclusiveHoverItem::~ExclusiveHoverItem()
	{
		if (group) { group->Unregister(this); }
	}

	ExclusiveHoverGroup::~ExclusiveHoverGroup()
	{
		for (auto* item : registered_items) { item->group = nullptr; }
	}

	void ExclusiveHoverGroup::Request(
		ExclusiveHoverItem* a_item, const NiPoint3& a_origin, Hand& a_hand)
	{
		if (!a_item || a_item->group != this) { return; }

		auto& state = hand_states[a_hand.IsLeft()];
		state.hand = std::addressof(a_hand);

		auto request = std::ranges::find(state.requests, a_item, &HoverRequest::item);
		if (request == state.requests.end()) { state.requests.push_back({ a_item, a_origin }); }
		else
		{
			request->origin = a_origin;
		}

		Update(state);
	}

	void ExclusiveHoverGroup::Release(ExclusiveHoverItem* a_item, Hand& a_hand)
	{
		if (!a_item) { return; }

		auto& state = hand_states[a_hand.IsLeft()];
		state.hand = std::addressof(a_hand);
		std::erase_if(state.requests,
			[a_item](const HoverRequest& request) { return request.item == a_item; });

		Update(state);
	}

	void ExclusiveHoverGroup::Register(ExclusiveHoverItem* a_item)
	{
		if (a_item && std::ranges::find(registered_items, a_item) == registered_items.end())
		{
			registered_items.push_back(a_item);
		}
	}

	void ExclusiveHoverGroup::Unregister(ExclusiveHoverItem* a_item)
	{
		if (!a_item) { return; }

		std::erase(registered_items, a_item);
		for (auto& state : hand_states)
		{
			std::erase_if(state.requests,
				[a_item](const HoverRequest& request) { return request.item == a_item; });
			if (state.active == a_item) { state.active = nullptr; }
		}

		a_item->group = nullptr;
	}

	void ExclusiveHoverGroup::Update(HandState& a_state)
	{
		if (!a_state.hand) { return; }

		ExclusiveHoverItem* winner = nullptr;
		float               best_score = std::numeric_limits<float>::max();
		const auto          hand_origin = a_state.hand->GetTransform().translate;

		for (const auto& request : a_state.requests)
		{
			const auto  offset = request.origin - hand_origin;
			const float score = offset.x * offset.x + offset.y * offset.y + offset.z * offset.z;

			if (score < best_score || (score == best_score && request.item == a_state.active))
			{
				best_score = score;
				winner = request.item;
			}
		}

		if (winner == a_state.active) { return; }

		if (a_state.active)
		{
			a_state.active->exclusive_hover_state[a_state.hand->IsLeft()] = false;
			a_state.active->OnHoverExclusive(false, *a_state.hand);
		}

		a_state.active = winner;

		if (a_state.active)
		{
			a_state.active->exclusive_hover_state[a_state.hand->IsLeft()] = true;
			a_state.active->OnHoverExclusive(true, *a_state.hand);
		}
	}
}