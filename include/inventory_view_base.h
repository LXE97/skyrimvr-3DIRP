#pragma once

#include "inventory_manager.h"
#include "vr_gui.h"

namespace vr3dirp
{
	class InventoryItemWidget;

	class InventoryViewBase
	{
	public:
		virtual ~InventoryViewBase() = default;

		void ApplyChanges(const std::vector<InventoryChange>& a_changes);

	protected:
		virtual const std::vector<std::unique_ptr<vr_gui::Widget>>& GetItemChildren() const = 0;
		virtual bool ShouldDisplay(const ShadowItem& a_item) const = 0;
		virtual InventoryItemWidget* CreateWidget(const ShadowItem& a_item) = 0;
		virtual void UpdateWidget(InventoryItemWidget& a_widget, const ShadowItem& a_item) = 0;
		virtual void DeleteWidget(InventoryItemWidget& a_widget) = 0;
		virtual void OnChangesApplied() {}

	private:
		InventoryItemWidget* FindItem(const ShadowItem& a_item) const;
	};
}
