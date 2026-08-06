#include "backpack.h"

#include "vr_gui_utils.h"

namespace vr3dirp
{
	using namespace RE;
	using namespace vr_gui;
	using namespace art_addon;

	namespace
	{
		NiMatrix3 MakePlayerFacingRotation(const NiPoint3& a_origin)
		{
			auto* player = PlayerCharacter::GetSingleton();
			auto* player_root = player ? player->Get3D(false) : nullptr;
			if (!player_root) { return NiMatrix3{}; }

			NiPoint3 x_axis = player_root->world.translate - a_origin;
			x_axis.z = 0.0f;
			if (x_axis.SqrLength() <= 1e-6f) { return NiMatrix3{}; }
			x_axis /= x_axis.Length();

			const NiPoint3 z_axis{ 0.0f, 0.0f, 1.0f };
			const NiPoint3 y_axis = z_axis.Cross(x_axis);
			return { { x_axis.x, y_axis.x, z_axis.x }, { x_axis.y, y_axis.y, z_axis.y },
				{ x_axis.z, y_axis.z, z_axis.z } };
		}

		inline void AddObjectRefToInventory(
			RE::TESObjectREFR* a_held_object, RE::TESObjectREFR* a_new_owner)
		{
			if (a_held_object && a_new_owner && a_new_owner->As<RE::Actor>())
			{
				if (a_held_object->IsBook())
				{  // TODO: make this work :(
					a_new_owner->As<RE::Actor>()->PickUpObject(
						a_held_object, a_held_object->extraList.GetCount());
				}
				else
				{
					a_held_object->ActivateRef(
						a_new_owner, 0, 0, a_held_object->extraList.GetCount(), false);
				}
			}
		}
	}

	Backpack::Backpack(std::string_view a_model_path, bool a_isLeft,
		TESObjectREFR* a_objectReference, BackpackSettings a_settings,
		BackpackCallbacks a_callbacks) :
		BackpackSettingsOwner(std::move(a_settings), std::move(a_callbacks)),
		Widget(a_settings.window_radius, a_objectReference),
		initialHandIsLeft(a_isLeft)
	{
		auto* attachment_node = a_objectReference->Get3D(false);

		local.scale = settings.scale;

		if (attachment_node && attachment_node->world.scale != 0.0f)
		{
			local.scale /= attachment_node->world.scale;
		}

		inventory_listener_id = InventoryManager::GetSingleton()->AddInventoryChangedListener(
			[this](const std::vector<InventoryChange>& a_changes) {
				ApplyInventoryChanges(a_changes);
			});

		AddModel(settings.model_path, false, [this](ArtAddon* a) {
			// parse nif for widget attachment nodes
			auto grab_node = a->Get3D()->GetObjectByName("GrabNode");
			if (!grab_node)
			{
				SKSE::log::error("Invalid {} : Grab node not found", settings.model_path);
				return;
			}
			auto container_node = a->Get3D()->GetObjectByName("Container");
			auto grid_node = a->Get3D()->GetObjectByName("Grid");
			auto holster1_node = a->Get3D()->GetObjectByName("Holster1");
			auto holster2_node = a->Get3D()->GetObjectByName("Holster2");

			// widget layout construction
			auto interaction_volume =
				AddChild<Widget>(NiTransform{}, NiPoint3(10, 24, 33), a->Get3D());
			interaction_volume->AddBehavior<BlockInputOnHover>(
				InputBlock::kPrimary | InputBlock::kSecondary | InputBlock::kActivatePickLength,
				false);
			interaction_volume->SetPriority(90);

			auto grab_handle =
				AddChild<BackpackGrabNode>(NiTransform{}, NiPoint3(5, 5, 4), grab_node);
			grab_handle->SetPriority(5);
			grab_handle->StartGrab();

			item_container =
				AddChild<BackpackContainer>(NiTransform{}, NiPoint3(9, 19, 25), container_node);
			item_container->SetPriority(40);
			item_container->AddBehavior<ExclusiveHoverGroup>();

			//item_grid = AddChild<BackpackGrid>(NiTransform{}, NiPoint3(4, 4, 10), grid_node);
			RefreshInventory(InventoryManager::GetSingleton()->GetItems());

			//auto holster = AddChild<Widget>(NiTransform{}, NiPoint3(4, 4, 10), holster1_node);

			SKSE::log::trace("backpack created with radius {} and scale {}", radius, local.scale);
		});
	}

	Backpack::~Backpack()
	{ InventoryManager::GetSingleton()->RemoveInventoryChangedListener(inventory_listener_id); }

	void Backpack::RefreshInventory(const std::vector<ShadowItem>& a_items)
	{
		if (item_container) { item_container->Refresh(a_items); }
		if (item_grid) { item_grid->Refresh(a_items); }
	}

	void Backpack::ApplyInventoryChanges(const std::vector<InventoryChange>& a_changes)
	{
		if (item_container) { item_container->ApplyChanges(a_changes); }
		if (item_grid) { item_grid->ApplyChanges(a_changes); }
	}

	bool Backpack::OnHiggsDropped(bool a_is_left, TESObjectREFR* a_dropped_reference)
	{ return item_container && item_container->StoreDroppedObject(a_is_left, a_dropped_reference); }

	InventoryItemWidget::InventoryItemWidget(
		Widget* a_parent, NiTransform a_local, NiPoint3 a_halfextents, ShadowItem a_item) :
		Widget(a_parent, std::move(a_local), a_halfextents),
		item(std::move(a_item))
	{
		// This widget remains the direct hover/click target. Its overlap test delegates
		// to the centered child hitbox created once the model bounds are available.
		SetHitTestEnabled(false);
		SetPriority(50);
		bool loading_model = false;

		if (a_item.id)
		{
			if (auto* object = TESForm::LookupByID<TESBoundObject>(item.id))
			{
				if (auto path = helper::GetObjectModelPath(object))
				{
					loading_model = true;
					AddModel(path, false, [this](ArtAddon* a) {
						if (!a || !a->Get3D())
						{
							SetHitTestEnabled(true);
							SKSE::log::warn(
								"InventoryItemWidget: model unavailable for {:x}", item.id);
							return;
						}
						float        newradius{};
						RE::NiPoint3 newcenter{};
						RE::NiPoint3 newextents{};
						helper::CalculateBoundsDirect(a->Get3D(), newradius, newcenter, newextents);
						if (newradius > 0.f)
						{
							NiTransform hitbox_transform{};
							hitbox_transform.translate = newcenter;
							hitbox = AddChild<InventoryItemHitbox>(
								std::move(hitbox_transform), newextents);
							// Collision traversal should report InventoryItemWidget as the target,
							// not the implementation-only child that supplies its geometry.
							hitbox->SetHitTestEnabled(false);
							SetHitTestEnabled(true);
						}
						else
						{
							SetHitTestEnabled(true);
							SKSE::log::warn(
								"InventoryItemWidget: bounds calculation failed on {:x}", item.id);
						}
					});
				}
			}
		}
		if (!loading_model) { SetHitTestEnabled(true); }
	}

	bool InventoryItemWidget::Matches(const ShadowItem& a_item) const
	{
		if (item.id != a_item.id) { return false; }
		if (item.unique_id || a_item.unique_id)
		{
			return item.unique_id && item.unique_id == a_item.unique_id;
		}
		if (item.extradata && a_item.extradata) { return item.extradata == a_item.extradata; }
		return true;
	}

	void InventoryItemWidget::UpdateItem(ShadowItem a_item) { item = std::move(a_item); }

	void InventoryItemWidget::OnClick(bool a_activate, Hand& a_hand, MenuAction a_action)
	{
		if (a_action != MenuAction::kSecondary) { return; }

		// 3d container
		if (auto grabber = GetBehavior<GrabAndDrag>())
		{
			Widget::OnClick(a_activate, a_hand, a_action);
			if (!a_activate)
			{
				if (!parent->IsHovered(a_hand.IsLeft())) { DropItem(a_hand.IsLeft(), false); }
				else
				{
					InventoryManager::GetSingleton()->SetPlacement(
						item.id, GetTransform(), item.unique_id, item.extradata);
				}
			}
		}
		// grid
		else if (!a_activate) { DropItem(a_hand.IsLeft(), true); }
	}

	void InventoryItemWidget::DropItem(bool a_isLeft, bool grab)
	{
		auto* player = PlayerCharacter::GetSingleton();
		auto* object = TESForm::LookupByID<TESBoundObject>(item.id);
		if (!model || !player || !object || item.count <= 0 ||
			(grab && (!g_higgsInterface || !g_higgsInterface->CanGrabObject(a_isLeft))))
		{
			return;
		}

		InventoryManager::GetSingleton()->ExpectRemoval(
			item.id, item.count, InventoryActionSource::kUnknown);

		RE::NiPoint3 droploc = model->Get3D()->world.translate;
		RE::NiPoint3 droprot;
		model->Get3D()->world.rotate.ToEulerAnglesXYZ(droprot);

		if (auto dropped_handle =
				player->DropObject(object, item.extradata, item.count, &droploc, &droprot);
			grab && dropped_handle)
		{
			auto reference = dropped_handle.get();
			if (g_higgsInterface && reference && reference->Get3D() &&
				g_higgsInterface->CanGrabObject(a_isLeft))
			{
				g_higgsInterface->GrabObject(reference.get(), a_isLeft);
			}
			else
			{
				InventoryManager::GetSingleton()->QueueHiggsGrab(
					std::move(dropped_handle), a_isLeft);
			}
		}
	}

	bool InventoryItemWidget::TestOverlap(Hand& a_hand) const
	{ return hitbox ? hitbox->TestOverlap(a_hand) : Widget::TestOverlap(a_hand); }

	// The child hitbox draws the centered debug bounds during recursive ShowHitboxes().
	void InventoryItemWidget::DrawExtents(bool) {}

	bool BackpackContainer::ContainsWorldPoint(const NiPoint3& a_point) const
	{
		const auto local_point = GetWorld().Invert() * a_point;
		return std::abs(local_point.x) <= extents.x && std::abs(local_point.y) <= extents.y &&
			std::abs(local_point.z) <= extents.z;
	}

	/* add the grabbed object to the inventory - it will be added to the backpack View on the inventory event*/
	bool BackpackContainer::StoreDroppedObject(bool a_is_left, TESObjectREFR* a_reference)
	{
		SKSE::log::trace("backpackdropped");
		if (IsHovered(a_is_left))
		{
			SKSE::log::trace("hovered true");
			auto* player = PlayerCharacter::GetSingleton();
			auto* object = a_reference ? a_reference->GetBaseObject() : nullptr;
			auto* object_3d = a_reference ? a_reference->Get3D(false) : nullptr;
			if (!player || !object || !object_3d) { return false; }

			const auto placement = GetWorld().Invert() * object_3d->world;
			const auto count = std::max(1, a_reference->extraList.GetCount());
			InventoryManager::GetSingleton()->ExpectAddition(object->GetFormID(), count,
				InventoryActionSource::kBackpackDrop, placement, RE::ObjectRefHandle(a_reference));

			AddObjectRefToInventory(a_reference, player);
			return true;
		}
		return false;
	}

	void BackpackContainer::Refresh(const std::vector<ShadowItem>& a_items)
	{
		ClearChildren();
		for (const auto& item : a_items)
		{
			if (!item.placement3d) { continue; }
			auto* widget = AddChild<InventoryItemWidget>(
				*item.placement3d, NiPoint3{ 1.5f, 1.5f, 1.5f }, item);
			widget->AddBehavior<GrabAndDrag>();
		}
	}

	const std::vector<std::unique_ptr<Widget>>& BackpackContainer::GetItemChildren() const
	{ return Widget::GetChildren(); }

	bool BackpackContainer::ShouldDisplay(const ShadowItem& a_item) const
	{ return a_item.placement3d.has_value(); }

	InventoryItemWidget* BackpackContainer::CreateWidget(const ShadowItem& a_item)
	{
		auto* widget = AddChild<InventoryItemWidget>(
			*a_item.placement3d, NiPoint3{ 1.5f, 1.5f, 1.5f }, a_item);
		widget->AddBehavior<GrabAndDrag>();
		return widget;
	}

	void BackpackContainer::UpdateWidget(InventoryItemWidget& a_widget, const ShadowItem& a_item)
	{
		a_widget.UpdateItem(a_item);
		a_widget.SetTransform(*a_item.placement3d);
	}

	void BackpackContainer::DeleteWidget(InventoryItemWidget& a_widget)
	{ RemoveChild(std::addressof(a_widget)); }

	void BackpackGrid::Refresh(const std::vector<ShadowItem>& a_items)
	{
		auto sorted_items = a_items;
		auto category = [](const ShadowItem& a_item) {
			if (a_item.source != InventoryActionSource::kUnknown) { return 0; }
			if (a_item.extradata && a_item.extradata->HasType<RE::ExtraHotkey>()) { return 1; }
			auto* form = TESForm::LookupByID(a_item.id);
			if (form && form->As<TESObjectARMO>()) { return 2; }
			if (a_item.extradata && a_item.extradata->HasQuestObjectAlias()) { return 3; }
			return 4;
		};
		std::ranges::stable_sort(
			sorted_items, [&category](const ShadowItem& a_left, const ShadowItem& a_right) {
				return category(a_left) < category(a_right);
			});

		ClearChildren();
		for (const auto& item : sorted_items)
		{
			AddItem<InventoryItemWidget>(NiTransform{}, NiPoint3{ 1.0f, 1.0f, 1.0f }, item);
		}
	}

	const std::vector<std::unique_ptr<Widget>>& BackpackGrid::GetItemChildren() const
	{ return Widget::GetChildren(); }

	bool BackpackGrid::ShouldDisplay(const ShadowItem&) const { return true; }

	InventoryItemWidget* BackpackGrid::CreateWidget(const ShadowItem& a_item)
	{ return AddItem<InventoryItemWidget>(NiTransform{}, NiPoint3{ 1.0f, 1.0f, 1.0f }, a_item); }

	void BackpackGrid::UpdateWidget(InventoryItemWidget& a_widget, const ShadowItem& a_item)
	{ a_widget.UpdateItem(a_item); }

	void BackpackGrid::DeleteWidget(InventoryItemWidget& a_widget)
	{ RemoveGridChild(std::addressof(a_widget)); }

	void BackpackGrid::OnChangesApplied() { Redraw(); }

	BackpackSettingsOwner::BackpackSettingsOwner(
		BackpackSettings a_settings, BackpackCallbacks a_callbacks) :
		settings(std::move(a_settings)),
		callbacks(std::move(a_callbacks))
	{}

	void BackpackGrabNode::OnClick(bool a_activate, Hand& a_hand, MenuAction a_action)
	{
		if (a_action == MenuAction::kSecondary)
		{
			if (a_activate)
			{
				SKSE::log::trace("grabnode clicked");
				isGrabButtonHeld = true;
				grabHoldTime = 0.0f;
				grabHand = &a_hand;
			}
			else
			{
				if (isGrabbed) { Release(); }
			}
		}
	}

	void BackpackGrabNode::Update(float delta)
	{
		if (isGrabButtonHeld && !isGrabbed && grabHand)
		{
			grabHoldTime += delta;
			if (grabHoldTime >= kGrabHoldTime)
			{
				isGrabbed = true;
				hand_mode = grabHand->RequestMode(Hand::Mode::kFist, Hand::ModePriority::kGrab);
			}
		}

		if (isGrabbed && grabHand)
		{
			if (vrinput::GetButtonState(Controller::GetSingleton()->GetSettings().secondary,
					vrinput::Hand(grabHand->IsLeft()),
					vrinput::ActionType::kPress) == vrinput::ButtonState::kButtonUp)
			{
				Release();
				return;
			}
			UpdateGrabTransform();
		}
	}

	void BackpackGrabNode::UpdateGrabTransform()
	{
		auto* backpack = dynamic_cast<Backpack*>(GetRoot());
		if (!backpack || !grabHand) { return; }

		const auto hand_transform = grabHand->GetTransform();

		// Stage 1: restore the upright, player-facing default orientation.
		auto backpack_world = backpack->GetWorld();
		backpack_world.rotate = MakePlayerFacingRotation(hand_transform.translate);
		backpack->SetTransform(backpack_world);

		// Stage 2: move the reoriented root until the model's GrabNode is centered
		// on the vr_gui hand transform.
		backpack_world.translate += hand_transform.translate - GetWorld().translate;
		backpack->SetTransform(backpack_world);
	}

	void BackpackGrabNode::StartGrab()
	{
		auto* backpack = dynamic_cast<Backpack*>(GetRoot());
		if (!backpack) { return; }

		grabHand = Controller::GetSingleton()->GetHand(backpack->initialHandIsLeft);
		if (!grabHand) { return; }

		isGrabButtonHeld = true;
		grabHoldTime = kGrabHoldTime;
		isGrabbed = true;
		hand_mode = grabHand->RequestMode(Hand::Mode::kFist, Hand::ModePriority::kGrab);
		UpdateGrabTransform();
	}

	void BackpackGrabNode::Release()
	{
		isGrabButtonHeld = false;
		grabHoldTime = 0.0f;
		grabHand = nullptr;
		isGrabbed = false;
		hand_mode.Release();
	}

	void BackpackGrabNode::OnHover(bool a_activate, Hand& a_hand)
	{
		if (a_activate && !isGrabbed)
		{
			hand_mode = a_hand.RequestMode(Hand::Mode::kOpen, Hand::ModePriority::kGrab);
		}
		if (!a_activate && !isGrabbed)
		{
			isGrabButtonHeld = false;
			grabHoldTime = 0.0f;
			grabHand = nullptr;
			isGrabbed = false;
			hand_mode.Release();
		}
	}

	void GrabAndDrag::StartGrab(bool a_is_left)
	{
		if (!parent || is_grabbed) { return; }

		grab_hand = Controller::GetSingleton()->GetHand(a_is_left);
		if (!grab_hand) { return; }

		hand_to_widget = grab_hand->GetTransform().Invert() * parent->GetWorld();
		is_grabbed = true;
		hand_mode.Release();
		hand_mode = grab_hand->RequestMode(Hand::Mode::kFist, Hand::ModePriority::kGrab);
		UpdateGrabTransform();
	}

	void GrabAndDrag::StopGrab()
	{
		is_grabbed = false;
		grab_hand = nullptr;
		hand_mode.Release();
	}

	void GrabAndDrag::OnClick(bool a_activate, Hand& a_hand, MenuAction a_action)
	{
		if (a_action != MenuAction::kSecondary) { return; }
		if (a_activate) { StartGrab(a_hand.IsLeft()); }
		else { StopGrab(); }
	}

	void GrabAndDrag::OnHover(bool a_activate, Hand& a_hand)
	{
		if (is_grabbed) { return; }
		if (a_activate)
		{
			hand_mode = a_hand.RequestMode(Hand::Mode::kOpen, Hand::ModePriority::kGrab);
		}
		else
		{
			hand_mode.Release();
		}
	}

	void GrabAndDrag::Update(float)
	{
		if (!is_grabbed || !grab_hand) { return; }

		if (vrinput::GetButtonState(Controller::GetSingleton()->GetSettings().secondary,
				vrinput::Hand(grab_hand->IsLeft()),
				vrinput::ActionType::kPress) == vrinput::ButtonState::kButtonUp)
		{
			auto* released_hand = grab_hand;
			if (parent) { parent->OnClick(false, *released_hand, MenuAction::kSecondary); }
			if (is_grabbed) { StopGrab(); }
			return;
		}

		UpdateGrabTransform();
	}

	void GrabAndDrag::UpdateGrabTransform()
	{
		if (!parent || !grab_hand) { return; }

		const auto desired_world = grab_hand->GetTransform() * hand_to_widget;
		if (auto* widget_parent = parent->GetParent())
		{
			parent->SetTransform(widget_parent->GetWorld().Invert() * desired_world);
		}
		else if (auto* transform_parent = parent->GetTransformParentNode())
		{
			parent->SetTransform(transform_parent->world.Invert() * desired_world);
		}
		else
		{
			parent->SetTransform(desired_world);
		}
	}

}
