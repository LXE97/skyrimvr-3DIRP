#include "inventory_view_base.h"

#include "backpack.h"

namespace vr3dirp
{
	void InventoryViewBase::ApplyChanges(const std::vector<InventoryChange>& a_changes)
	{
		for (const auto& change : a_changes)
		{
			auto* widget = FindItem(change.item);

			switch (change.type)
			{
			case InventoryChangeType::kAdded:
			case InventoryChangeType::kUpdated:
				if (ShouldDisplay(change.item))
				{
					if (widget) { UpdateWidget(*widget, change.item); }
					else { CreateWidget(change.item); }
				}
				else if (widget)
				{
					DeleteWidget(*widget);
				}
				break;

			case InventoryChangeType::kRemoved:
				if (widget) { DeleteWidget(*widget); }
				break;
			}
		}

		OnChangesApplied();
	}

	InventoryItemWidget* InventoryViewBase::FindItem(const ShadowItem& a_item) const
	{
		for (const auto& child : GetItemChildren())
		{
			auto* widget = dynamic_cast<InventoryItemWidget*>(child.get());
			if (widget && widget->Matches(a_item)) { return widget; }
		}
		return nullptr;
	}
}
