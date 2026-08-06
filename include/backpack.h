#pragma once

#include "exclusive_hover_group.h"
#include "inventory_manager.h"
#include "inventory_view_base.h"
#include "ni_animator.h"
#include "vr_gui.h"
#include "vr_gui_input_block.h"

#include <concepts>
#include <utility>

namespace vr3dirp
{
	using namespace RE;
	using namespace vr_gui;

	struct BackpackSettings
	{
		float       light_fade = 1.f;
		float       window_radius = 50.f;
		float       scale = 1.0f;
		std::string model_path = "3DIRP/Backpack/backpack_player.nif";
	};

	struct BackpackCallbacks
	{};

	class BackpackSettingsOwner
	{
	protected:
		BackpackSettingsOwner(BackpackSettings a_settings, BackpackCallbacks a_callbacks);

		BackpackSettings  settings;
		BackpackCallbacks callbacks;
	};

	class BackpackContainer;
	class BackpackGrid;

	class Backpack : public BackpackSettingsOwner, public Widget
	{
		friend class BackpackGrabNode;

	public:
		Backpack(std::string_view a_model_path, bool a_isLeft, TESObjectREFR* a_objectReference,
			BackpackSettings a_settings, BackpackCallbacks a_callbacks = {});
		~Backpack() override;
		bool OnHiggsDropped(bool a_is_left, TESObjectREFR* a_dropped_reference);

		void DrawExtents(bool a_show) override {}

		static constexpr float kDefaultWindowRadius = 80.f;

	protected:
		bool initialHandIsLeft;

	private:
		BackpackContainer* item_container{};
		BackpackGrid*      item_grid{};
		std::uint64_t      inventory_listener_id{};

		void RefreshInventory(const std::vector<ShadowItem>& a_items);
		void ApplyInventoryChanges(const std::vector<InventoryChange>& a_changes);
	};

	class InventoryItemWidget : public Widget
	{
	public:
		InventoryItemWidget(
			Widget* a_parent, NiTransform a_local, NiPoint3 a_halfextents, ShadowItem a_item);

		void OnClick(bool a_activate, Hand& a_hand, MenuAction a_action) override;
		[[nodiscard]] const ShadowItem& GetItem() const { return item; }
		[[nodiscard]] bool              Matches(const ShadowItem& a_item) const;
		void                            UpdateItem(ShadowItem a_item);
		void                            DrawExtents(bool show) override;

	private:
		ShadowItem item;
	};

	class BackpackContainer : public Container, private InventoryViewBase
	{
	public:
		using Container::Container;

		bool ContainsWorldPoint(const NiPoint3& a_point) const;
		bool StoreDroppedObject(bool a_is_left, TESObjectREFR* a_reference);
		void Refresh(const std::vector<ShadowItem>& a_items);
		void ApplyChanges(const std::vector<InventoryChange>& a_changes)
		{ InventoryViewBase::ApplyChanges(a_changes); }

	private:
		const std::vector<std::unique_ptr<Widget>>& GetItemChildren() const override;
		bool                 ShouldDisplay(const ShadowItem& a_item) const override;
		InventoryItemWidget* CreateWidget(const ShadowItem& a_item) override;
		void UpdateWidget(InventoryItemWidget& a_widget, const ShadowItem& a_item) override;
		void DeleteWidget(InventoryItemWidget& a_widget) override;
	};

	class BackpackGrid : public GridContainer, private InventoryViewBase
	{
	public:
		BackpackGrid(Widget* a_parent, NiTransform a_local, NiPoint3 a_halfextents) :
			GridContainer(a_parent, std::move(a_local), a_halfextents, 4, 0.5f)
		{}

		void Refresh(const std::vector<ShadowItem>& a_items);
		void ApplyChanges(const std::vector<InventoryChange>& a_changes)
		{ InventoryViewBase::ApplyChanges(a_changes); }

	private:
		const std::vector<std::unique_ptr<Widget>>& GetItemChildren() const override;
		bool                 ShouldDisplay(const ShadowItem& a_item) const override;
		InventoryItemWidget* CreateWidget(const ShadowItem& a_item) override;
		void UpdateWidget(InventoryItemWidget& a_widget, const ShadowItem& a_item) override;
		void DeleteWidget(InventoryItemWidget& a_widget) override;
		void OnChangesApplied() override;
	};

	class BackpackGrabNode : public Widget
	{
	public:
		using Widget::Widget;

		void OnClick(bool a_activate, Hand& a_hand, MenuAction a_action) override;

		void OnHover(bool a_activate, Hand& a_hand) override;

		void Update(float delta) override;

		void StartGrab();

	private:
		static constexpr float kGrabHoldTime = 0.3f;

		void Release();
		void UpdateGrabTransform();

		bool  isGrabbed = false;
		bool  isGrabButtonHeld = false;
		float grabHoldTime = 0.0f;
		Hand* grabHand{};

		ModeHandle hand_mode;
	};
}
