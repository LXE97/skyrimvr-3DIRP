#pragma once

#include "vr_gui.h"

#include <algorithm>
#include <array>
#include <limits>
#include <memory>
#include <vector>

namespace vr_gui
{
	class ExclusiveHoverGroup;

	class ExclusiveHoverItem
	{
	public:
		explicit ExclusiveHoverItem(ExclusiveHoverGroup* a_group);
		virtual ~ExclusiveHoverItem();

		ExclusiveHoverItem(const ExclusiveHoverItem&) = delete;
		ExclusiveHoverItem(ExclusiveHoverItem&&) = delete;
		ExclusiveHoverItem& operator=(const ExclusiveHoverItem&) = delete;
		ExclusiveHoverItem& operator=(ExclusiveHoverItem&&) = delete;

		virtual void OnHoverExclusive(bool a_activate, Hand& a_hand) = 0;

		bool IsExclusivelyHovered(bool a_isLeft) const { return exclusive_hover_state[a_isLeft]; }

	protected:
		ExclusiveHoverGroup* group{};

	private:
		bool exclusive_hover_state[2] = { false, false };
		friend class ExclusiveHoverGroup;
	};

	class ExclusiveHoverGroup
	{
	public:
		ExclusiveHoverGroup() = default;

		~ExclusiveHoverGroup();

		ExclusiveHoverGroup(const ExclusiveHoverGroup&) = delete;
		ExclusiveHoverGroup(ExclusiveHoverGroup&&) = delete;
		ExclusiveHoverGroup& operator=(const ExclusiveHoverGroup&) = delete;
		ExclusiveHoverGroup& operator=(ExclusiveHoverGroup&&) = delete;

		void Request(ExclusiveHoverItem* a_item, const NiPoint3& a_origin, Hand& a_hand);

		void Release(ExclusiveHoverItem* a_item, Hand& a_hand);

		void Update()
		{
			for (auto& state : hand_states) { Update(state); }
		}

	private:
		struct HoverRequest
		{
			ExclusiveHoverItem* item{};
			NiPoint3            origin{};
		};

		struct HandState
		{
			std::vector<HoverRequest> requests;
			ExclusiveHoverItem*       active{};
			Hand*                     hand{};
		};

		void Register(ExclusiveHoverItem* a_item);

		void Unregister(ExclusiveHoverItem* a_item);

		void Update(HandState& a_state);

		std::vector<ExclusiveHoverItem*> registered_items;
		std::array<HandState, 2>         hand_states;

		friend class ExclusiveHoverItem;
	};

}
