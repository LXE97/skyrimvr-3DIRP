#include "vr_gui.h"

#include "helper_game.h"
#include "helper_math.h"
#include "text_manager.h"
#include "vr_gui_input_block.h"

namespace vr_gui
{
	static const char* kHelperModelPath = "3DIRP/vr_gui/HelperSphere.nif";
	static const char* kDebugModelPath = "3DIRP/vr_gui/DebugSphere.nif";
	static const char* kDebugBoxModelPath = "3DIRP/vr_gui/DrawExtents.nif";

	void Controller::Cleanup()
	{
		SKSE::log::trace("vr gui controller cleanup");
		roots.clear();
		pending_roots.clear();
		activator_overrides.clear();
		widgets_to_delete.clear();
		post_update_queue.clear();
		hovered_map.clear();
		update_in_progress = false;
	}

	void Controller::SetSettings(Settings a_settings)
	{
		if (initialized)
		{
			for (auto hand : { vrinput::Hand::kRight, vrinput::Hand::kLeft })
			{
				vrinput::RemoveCallback(
					InputEventHandlerStatic, settings.primary, hand, vrinput::ActionType::kPress);
				vrinput::RemoveCallback(
					InputEventHandlerStatic, settings.secondary, hand, vrinput::ActionType::kPress);
			}
		}

		settings = std::move(a_settings);

		if (initialized)
		{
			for (auto hand : { vrinput::Hand::kRight, vrinput::Hand::kLeft })
			{
				vrinput::AddCallback(
					InputEventHandlerStatic, settings.primary, hand, vrinput::ActionType::kPress);
				vrinput::AddCallback(
					InputEventHandlerStatic, settings.secondary, hand, vrinput::ActionType::kPress);
			}
		}
	}

	void Controller::Init()
	{
		SKSE::log::trace("Controller init");

		Cleanup();

		// create right (0) and left hands
		hands = { Hand(false), Hand(true) };

		if (auto form = TESForm::LookupByID(0x7))
		{
			player_form_ref = form;
			SKSE::log::trace("player_form_ref {}", fmt::ptr(player_form_ref));
		}
		else
		{
			SKSE::log::error("ERROR couldn't get player ref");
		}

		if (RE::PlayerCharacter::GetSingleton()->Is3DLoaded())
		{
			auto pcvr = RE::PlayerCharacter::GetSingleton()->GetVRNodeData();
			auto hand = pcvr->RightWandNode;
			if (auto rollover = pcvr->RoomNode->GetObjectByName(kRolloverNodeName))
			{
				rollover_default_hand = rollover->local;
				rollover_default_hand_pos = rollover->local.translate;
				rollover_default_hand_rot = rollover->local.rotate;

				SKSE::log::trace("default rollover transform:");
				helper::PrintVec(rollover_default_hand_pos);
			}
			else
			{
				SKSE::log::error("ERROR couldn't get rollover node");
			}
		}

		if (!initialized)
		{
			InputBlockManager::GetSingleton()->Init();

			// register for input events
			vrinput::AddCallback(InputEventHandlerStatic, settings.primary, vrinput::Hand::kRight,
				vrinput::ActionType::kPress);
			vrinput::AddCallback(InputEventHandlerStatic, settings.secondary, vrinput::Hand::kRight,
				vrinput::ActionType::kPress);
			vrinput::AddCallback(InputEventHandlerStatic, vr::EVRButtonId::k_EButton_DPad_Up,
				vrinput::Hand::kRight, vrinput::ActionType::kPress);
			vrinput::AddCallback(InputEventHandlerStatic, vr::EVRButtonId::k_EButton_DPad_Down,
				vrinput::Hand::kRight, vrinput::ActionType::kPress);
			vrinput::AddCallback(InputEventHandlerStatic, settings.primary, vrinput::Hand::kLeft,
				vrinput::ActionType::kPress);
			vrinput::AddCallback(InputEventHandlerStatic, settings.secondary, vrinput::Hand::kLeft,
				vrinput::ActionType::kPress);
			vrinput::AddCallback(InputEventHandlerStatic, vr::EVRButtonId::k_EButton_DPad_Up,
				vrinput::Hand::kLeft, vrinput::ActionType::kPress);
			vrinput::AddCallback(InputEventHandlerStatic, vr::EVRButtonId::k_EButton_DPad_Down,
				vrinput::Hand::kLeft, vrinput::ActionType::kPress);
			vrinput::AddCallback(InputEventHandlerStatic, vr::EVRButtonId::k_EButton_DPad_Left,
				vrinput::Hand::kLeft, vrinput::ActionType::kPress);
			vrinput::AddCallback(InputEventHandlerStatic, vr::EVRButtonId::k_EButton_DPad_Right,
				vrinput::Hand::kLeft, vrinput::ActionType::kPress);
			vrinput::AddCallback(InputEventHandlerStatic, vr::EVRButtonId::k_EButton_DPad_Left,
				vrinput::Hand::kRight, vrinput::ActionType::kPress);
			vrinput::AddCallback(InputEventHandlerStatic, vr::EVRButtonId::k_EButton_DPad_Right,
				vrinput::Hand::kRight, vrinput::ActionType::kPress);

			initialized = true;
		}
	}

	void Controller::Update()
	{
		update_in_progress = true;

		// Only process input if there is at least one enabled widget tree.
		bool do_update = false;

		const auto                         now = std::chrono::steady_clock::now();
		const std::chrono::duration<float> elapsed = now - last_update_time;
		const float                        delta = elapsed.count();
		last_update_time = now;

		for (const auto& root : roots)
		{
			if (root->IsEnabled())
			{
				root->UpdateImpl(delta);
				do_update = true;
			}
		}

		HandlePostUpdateQueue();

		std::vector<ButtonEvent> pending;
		{
			std::scoped_lock lock(button_queue_mutex);
			pending.swap(button_queue);
		}

		if (do_update)
		{
			HandleInput();
			HandleEvents(pending);
			HandlePostUpdateQueue();
		}
		else
		{
			activator_overrides.clear();
		}

		//HandleHUDOverrides happens in the PostWandUpdate
		HandleDeletionQueue();

		update_in_progress = false;
		for (auto& root : pending_roots) { roots.emplace_back(std::move(root)); }
		pending_roots.clear();
	}

	bool Controller::InputEventHandlerStatic(const vrinput::ModInputEvent& e)
	{ return GetSingleton()->InputEventHandler(e); }

	bool Controller::InputEventHandler(const vrinput::ModInputEvent& e)
	{
		ButtonEvent temp;
		temp.down = (bool)e.button_state;
		temp.isLeft = (bool)e.device;
		bool block_input = false;

		if (e.button_ID == settings.primary)
		{
			temp.button = MenuAction::kPrimary;
			block_input =
				InputBlockManager::GetSingleton()->IsBlocked(temp.isLeft, InputBlock::kPrimary);
		}
		else if (e.button_ID == settings.secondary)
		{
			temp.button = MenuAction::kSecondary;
			block_input =
				InputBlockManager::GetSingleton()->IsBlocked(temp.isLeft, InputBlock::kSecondary);
		}
		else if (e.button_ID == vr::EVRButtonId::k_EButton_DPad_Up)
		{
			temp.button = MenuAction::kScrollUp;
			block_input =
				InputBlockManager::GetSingleton()->IsBlocked(temp.isLeft, InputBlock::kJoystick);
		}
		else if (e.button_ID == vr::EVRButtonId::k_EButton_DPad_Down)
		{
			temp.button = MenuAction::kScrollDown;
			block_input =
				InputBlockManager::GetSingleton()->IsBlocked(temp.isLeft, InputBlock::kJoystick);
		}
		else if (e.button_ID == vr::EVRButtonId::k_EButton_DPad_Left)
		{
			temp.button = MenuAction::kScrollLeft;
			block_input =
				InputBlockManager::GetSingleton()->IsBlocked(temp.isLeft, InputBlock::kJoystick);
		}
		else if (e.button_ID == vr::EVRButtonId::k_EButton_DPad_Right)
		{
			temp.button = MenuAction::kScrollRight;
			block_input =
				InputBlockManager::GetSingleton()->IsBlocked(temp.isLeft, InputBlock::kJoystick);
		}

		std::scoped_lock lock(button_queue_mutex);
		button_queue.push_back(temp);

		return temp.down && block_input;
	}

	void Controller::MarkForDelete(Widget* w)
	{
		if (w)
		{
			w->SetEnabled(false);
			widgets_to_delete.push_back(w);
		}
	}

	void Controller::QueuePostUpdate(std::function<void()> a_action)
	{
		if (a_action) { post_update_queue.emplace_back(std::move(a_action)); }
	}

	void Controller::HandlePostUpdateQueue()
	{
		auto pending = std::move(post_update_queue);
		post_update_queue.clear();

		for (auto& action : pending) { action(); }
	}

	void Controller::HandleDeletionQueue()
	{
		for (auto* widget : widgets_to_delete)
		{
			if (IsValid(widget))
			{
				if (widget->GetParent())
				{
					// Remove references to the widget and all of its descendants before
					// destroying the subtree.
					for (auto& [hand, windowMap] : hovered_map)
					{
						for (auto& [window, widgets] : windowMap)
						{
							std::erase_if(widgets, [this, widget](Widget* hovered) {
								return IsValid(widget, hovered);
							});
						}
					}
					widget->GetParent()->RemoveChild(widget);
				}
				else
				{
					RemoveRoot(widget);
				}
			}
			else
			{
				SKSE::log::trace("Invalid widget in deletion queue");
			}
		}
		widgets_to_delete.clear();
	}

	bool Controller::IsValid(Widget* a_target) const
	{
		for (auto& root : roots)
		{
			if (IsValid(root.get(), a_target)) { return true; }
		}
		return false;
	}

	bool Controller::IsValid(Widget* a_root, Widget* a_target) const
	{
		if (a_root == a_target) { return true; }

		for (auto& w : a_root->children)
		{
			if (IsValid(w.get(), a_target)) { return true; }
		}

		return false;
	}

	void Widget::RemoveChild(Widget* a_child)
	{
		auto it = std::find_if(children.begin(), children.end(),
			[a_child](const std::unique_ptr<Widget>& ptr) { return ptr.get() == a_child; });

		if (it != children.end()) { children.erase(it); }
	}

	void Controller::RemoveRoot(Widget* a_root)
	{
		auto it = std::find_if(roots.begin(), roots.end(),
			[a_root](const std::unique_ptr<Widget>& ptr) { return ptr.get() == a_root; });

		if (it != roots.end())
		{
			RemoveActivatorOverridesForRoot(a_root);
			roots.erase(it);
			for (auto& hand : hands) { hovered_map[&hand].erase(a_root); }
		}
#ifdef HUD_OVERRIDES
		if (roots.empty())
		{
			SKSE::GetTaskInterface()->AddTask([this]() {
				if (activator_obj) { activator_obj->MoveTo(modspacemarker_obj); }
			});
		}
#endif
	}

	void Controller::HandleHUDOverrides()
	{
		static Widget* prev_override = nullptr;

		if (activator_obj && modspacemarker_obj)
		{
			// Reset when no more overrides to do
			if (activator_overrides.empty())
			{
				if (prev_override != nullptr)
				{
					prev_override = nullptr;
					//SKSE::GetTaskInterface()->AddTask([this]() { ClearHUDOverride(); });
					ClearHUDOverride();
				}
			}
			// New override to set
			else if (activator_overrides.back().owner != prev_override)
			{
				prev_override = activator_overrides.back().owner;
				SetHUDOverride(activator_overrides.back());
			}
			// No changes in override data but override is still active, just do position update
			else
			{
				// move to follow main hand (Activation hand)
				//SKSE::GetTaskInterface()->AddTask([this]() { ToggleActivator(true); });
				ToggleActivator(true);
				if (activator_overrides.back().isLeft)
				{
					// override the rollover UI position
					auto pc = PlayerCharacter::GetSingleton();

					if (auto rollover_node = pc->RoomNode->GetObjectByName(kRolloverNodeName))
					{
						NiUpdateData ctx;
						auto         hand = pc->LeftWandNode;

						auto desired_world = hand->world * rollover_default_hand;

						rollover_node->local =
							rollover_node->parent->world.Invert() * desired_world;

						rollover_node->Update(ctx);
					}
				}
			}
		}
	}

	void Controller::HandleInput()
	{
		for (auto& hand : hands)
		{
			// Get updated device state, skip if failed
			if (!hand.Update()) { continue; }

			for (auto& root : roots)
			{
				if (root->IsEnabled() && root->HandStateFilter(hand))
				{
					// Copy currently hovered children for this hand and widget tree.
					auto&                cur_list = hovered_map[&hand][root.get()];
					std::vector<Widget*> prev_list = cur_list;

					// Rebuild list of currently hovered widgets, and dispatch any newly hovered events
					cur_list.clear();
					if (root->TestRootOverlap(hand))
					{
						if (!root->IsHovered(hand.IsLeft())) root->OnHoverImpl(true, hand);
						TraverseCollision(*root, hand, cur_list);
					}
					else
					{
						if (root->IsHovered(hand.IsLeft())) root->OnHoverImpl(false, hand);
					}

					// Any widgets in prev but not cur must have transitioned from hovered -> inactive state
					for (Widget* w : prev_list)
					{
						auto it = std::find(cur_list.begin(), cur_list.end(), w);
						if (it == cur_list.end()) { w->OnHoverImpl(false, hand); }
					}

					// Clean inactive widget trees out of the global hover map.
					if (cur_list.empty()) { hovered_map[&hand].erase(root.get()); }
				}
			}
		}
	}

	void Controller::HandleEvents(const std::vector<ButtonEvent>& a_events)
	{
		for (auto& event : a_events)
		{
			// Dispatch the button to the highest priority widget that is hovered
			// only look in the first window with hovered widgets for this hand- unlikely to have overlapping Windows
			Hand* hand = &hands[event.isLeft];
			auto& hovered_this_hand = hovered_map[hand];

			for (auto& [window, list] : hovered_this_hand)
			{
				if (!list.empty())
				{
					std::sort(list.begin(), list.end(),
						[](Widget* a, Widget* b) { return a->GetPriority() < b->GetPriority(); });
					list[0]->OnClick(event.down, *hand, event.button);
					break;
				}
			}
		}
	}

	void Controller::TraverseCollision(
		Widget& a_parent, Hand& a_hand, std::vector<Widget*>& a_hover_list)
	{
		for (auto& child : a_parent.GetChildren())
		{
			if (!child->IsEnabled()) { continue; }

			if (child->IsHitTestEnabled() && child->HandStateFilter(a_hand) &&
				child->TestOverlap(a_hand))
			{
				if (!child->IsHovered(a_hand.IsLeft())) { child->OnHoverImpl(true, a_hand); }

				a_hover_list.push_back(child.get());
			}

			TraverseCollision(*child, a_hand, a_hover_list);
		}
	}

	void Controller::PushActivatorOverride(ActivatorOverride&& a_data)
	{ activator_overrides.emplace_back(std::move(a_data)); }

	void Controller::RemoveActivatorOverride(Widget* a_owner)
	{
		auto it = std::find_if(activator_overrides.begin(), activator_overrides.end(),
			[&](const ActivatorOverride& a) { return a.owner == a_owner; });
		if (it != activator_overrides.end()) { activator_overrides.erase(it); }
	}

	void Controller::RemoveActivatorOverride(Widget* a_owner, bool a_isLeft)
	{
		auto it = std::find_if(activator_overrides.begin(), activator_overrides.end(),
			[&](const ActivatorOverride& a) { return a.owner == a_owner && a.isLeft == a_isLeft; });
		if (it != activator_overrides.end()) { activator_overrides.erase(it); }
	}

	bool Controller::IsMenuActionPressed(Hand& a_hand, MenuAction a_action)
	{
		if (a_action == MenuAction::kPrimary)
		{
			return vrinput::GetButtonState(settings.primary, vrinput::Hand(a_hand.IsLeft()),
					   vrinput::ActionType::kPress) == vrinput::ButtonState::kButtonDown;
		}
		else if (a_action == MenuAction::kSecondary)
		{
			return vrinput::GetButtonState(settings.secondary, vrinput::Hand(a_hand.IsLeft()),
					   vrinput::ActionType::kPress) == vrinput::ButtonState::kButtonDown;
		}
		return false;
	}

	void Controller::RemoveActivatorOverridesForRoot(Widget* a_root)
	{
		auto it = std::remove_if(activator_overrides.begin(), activator_overrides.end(),
			[&](const ActivatorOverride& a) { return a.root == a_root; });
		if (it != activator_overrides.end())
		{
			activator_overrides.erase(it, activator_overrides.end());
		}
	}

	void Controller::ClearExtraData(RE::TESObjectREFR* a_obj)
	{
		if (a_obj->extraList.HasType(RE::ExtraDataType::kEnchantment))
		{
			a_obj->extraList.RemoveByType(RE::ExtraDataType::kEnchantment);
		}
		if (a_obj->extraList.HasType(RE::ExtraDataType::kOwnership))
		{
			auto own = a_obj->extraList.GetByType<RE::ExtraOwnership>();
			own->owner = player_form_ref;
		}
		else
		{
			auto own = RE::ExtraOwnership::Create<RE::ExtraOwnership>();
			own->owner = player_form_ref;
			a_obj->extraList.Add(own);
		}
	}

	void Controller::ClearHUDOverride()
	{
		activator_obj->SetObjectReference(activator_default_base);
		//  reset extradata

		ClearExtraData(activator_obj);
		ToggleActivator(false);
	}

	void Controller::SetHUDOverride(ActivatorOverride& a_data)
	{
		if (!a_data.base) return;

		// The rollover text doesn't refresh until you move off this object, so disable it
		// it will be enabled on the next update
		ToggleActivator(false);

		ClearExtraData(activator_obj);

		if (a_data.text.empty())
		{
			// change the displayed item in the rollover UI
			activator_obj->SetObjectReference(a_data.base);
			activator_obj->SetDisplayName(a_data.base->GetName(), false);

			if (auto e = a_data.extradata)
			{
				if (e->HasType(RE::ExtraDataType::kEnchantment))
				{
					auto ench = a_data.extradata->GetByType<RE::ExtraEnchantment>();
					auto ench_copy = RE::ExtraEnchantment::Create<RE::ExtraEnchantment>();
					ench_copy->charge = ench->charge;
					ench_copy->enchantment = ench->enchantment;
					activator_obj->extraList.Add(ench_copy);
				}
				if (e->HasType(RE::ExtraDataType::kOwnership))
				{
					auto own = a_data.extradata->GetByType<RE::ExtraOwnership>();
					if (activator_obj->extraList.HasType(RE::ExtraDataType::kOwnership))
					{
						auto act_own = activator_obj->extraList.GetByType<RE::ExtraOwnership>();
						act_own->owner = own->owner;
					}
					else
					{
						auto own_copy = RE::ExtraOwnership::Create<RE::ExtraOwnership>();
						own_copy->owner = own->owner;
						activator_obj->extraList.Add(own_copy);
					}
				}
				if (e->HasType(RE::ExtraDataType::kPoison))
				{
					//TODO: poisoned indicator
				}
			}
		}
		else
		{
			activator_obj->SetObjectReference(activator_default_base);
			activator_obj->SetDisplayName(a_data.text, true);
		}
	}

	void Controller::ToggleActivator(bool a_enabled)
	{
		constexpr const char* collision_node_name = "CollisionNode";

		if (auto root = activator_obj->GetCurrent3D())
		{
			if (auto collider = root->GetObjectByName(collision_node_name))
			{
				RE::NiUpdateData ctx;
				if (a_enabled)
				{
					auto handloc = PlayerCharacter::GetSingleton()->RightWandNode->world.translate;

					collider->local.translate = collider->parent->world.rotate.Transpose() *
						(handloc - collider->parent->world.translate);

					collider->Update(ctx);
				}
				else
				{
					collider->local.translate.z += 500;
					collider->Update(ctx);

					// fix the rotation, no better place to put this
					root->world.rotate = NiMatrix3();
				}
			}
		}
	}

	void Controller::ShowHitboxes(bool show)
	{
		for (auto& root : roots) { root->ShowHitboxes(show); }
	}

	Widget::Widget(Widget* a_parent, NiTransform a_local, NiPoint3 a_halfextents, float a_radius,
		NiAVObject* a_transformParentNode) :
		local(std::move(a_local)),
		extents(a_halfextents),
		radius(a_radius),
		base_extents(a_halfextents),
		base_radius(a_radius),
		parent(a_parent),
		transform_parent_node(a_transformParentNode)
	{}

	Widget::Widget(float a_radius, TESObjectREFR* a_objectReference,
		NiAVObject* a_transformParentNode, std::optional<NiTransform> a_local) :
		Widget(
			nullptr, a_local.value_or(NiTransform{}), NiPoint3{}, a_radius, a_transformParentNode)
	{ object_reference = a_objectReference; }

	Widget::~Widget() { children.clear(); }

	void Widget::ShowHitboxes(bool show)
	{
		DrawExtents(show);

		for (auto& child : children) { child->ShowHitboxes(show); }
	}

	void Widget::DrawExtents(bool show)
	{
		using namespace art_addon;
		if (show)
		{
			if (!parent)
			{
				AddModel(kDebugModelPath, true, [radius = this->radius](ArtAddon* sphere) {
					if (sphere && sphere->Get3D())
					{
						// The addon's root transform is overwritten whenever the widget moves.
						// Scale the model's internal sphere node so the radius persists.
						if (auto* geometry =
								sphere->Get3D()->GetObjectByName("Z4K_OVERLAPSPHERE"))
						{
							geometry->local.scale *= radius;
						}
					}
				});
			}
			else
			{
				AddModel(kDebugBoxModelPath, true,
					[extents = this->extents](ArtAddon* box) { helper::DrawBox(box, extents); });
			}
		}
		else
		{
			visual_effects.clear();
		}
	}

	bool Widget::TestOverlap(Hand& a_hand) const
	{
		// TODO: rewrite to use a downward pass instead of GetWorld every time
		auto t = a_hand.GetTransform();
		auto w = GetWorld();
		// broad phase
		if (helper::IntersectSphereSphere(
				t.translate, a_hand.GetRadius() * t.scale, w.translate, w.scale * radius))
		{  // narrow phase
			auto tbox = a_hand.GetBoxTransform();
			return helper::IntersectOBBOBB(
				tbox, *a_hand.GetExtents() * t.scale, w, extents * w.scale);
		}

		return false;
	}

	void Widget::OnClick(bool a_activate, Hand& a_hand, MenuAction a_action)
	{
		for (auto& behavior : behaviors)
		{
			if (!behavior->IsMarkedForRemoval())
			{
				behavior->OnClick(a_activate, a_hand, a_action);
			}
		}
	}

	void Widget::OnHoverImpl(bool activate, Hand& hand)
	{
		hover_state[hand.IsLeft()] = activate;
		OnHover(activate, hand);

		for (auto& b : behaviors)
		{
			if (!b->IsMarkedForRemoval()) { b->OnHover(activate, hand); }
		}
	}

	void Widget::Hide()
	{
		if (auto* node = Get3D()) { node->SetAppCulled(true); }
		for (auto& text : text_boxes)
		{
			if (auto* node = text->Get3D()) { node->SetAppCulled(true); }
		}
		hide = true;
		for (auto& w : children) { w->Hide(); }
	}

	void Widget::Show()
	{
		if (auto* node = Get3D()) { node->SetAppCulled(false); }
		for (auto& text : text_boxes)
		{
			if (auto* node = text->Get3D()) { node->SetAppCulled(false); }
		}
		hide = false;
		for (auto& w : children) { w->Show(); }
	}

	void Widget::Resize(float a_scale)
	{
		local.scale = a_scale;
		UpdateModelTransform();
	}

	void Widget::ClearChildren()
	{
		auto* controller = Controller::GetSingleton();

		for (const auto& child : children) { controller->MarkForDelete(child.get()); }
	}

	void Widget::SetTransform(NiTransform a_transform)
	{
		local = std::move(a_transform);
		UpdateModelTransform();
	}

	void Widget::SetTransformParentNode(NiAVObject* a_node)
	{
		transform_parent_node = a_node;
		UpdateModelTransform();
	}

	/* Set new local translation */
	void Widget::MoveTo(NiPoint3 a_translate_local)
	{
		local.translate = a_translate_local;
		UpdateModelTransform();
	}

	void Widget::OnHover(bool a_activate, Hand& a_hand) {}

	void Widget::Update(float delta) {}

	void Widget::UpdateImpl(float delta)
	{
		if (!IsEnabled()) { return; }

		Update(delta);

		for (auto& b : behaviors)
		{
			if (!b->IsMarkedForRemoval()) { b->Update(delta); }
		}

		if (IsTreeWorldAnchored()) { UpdateOwnModelTransform(); }

		for (auto& child : children) { child->UpdateImpl(delta); }

		std::erase_if(behaviors, [](const auto& b) { return b->IsMarkedForRemoval(); });
	}

	const Widget* Widget::GetRoot() const { return parent ? parent->GetRoot() : this; }

	Widget* Widget::GetRoot() { return parent ? parent->GetRoot() : this; }

	TESObjectREFR* Widget::GetObjectReference() const
	{
		return object_reference ? object_reference :
			parent              ? parent->GetObjectReference() :
								  nullptr;
	}

	bool Widget::IsWorldAnchored() const { return !parent && !transform_parent_node; }

	bool Widget::IsTreeWorldAnchored() const { return GetRoot()->IsWorldAnchored(); }

	NiTransform Widget::GetWorld(int depth) const
	{
		if (depth > MAX_DEPTH)
		{
			SKSE::log::error("Widget tree recursion exceeded");
			return NiTransform();
		}

		if (transform_parent_node) { return transform_parent_node->world * local; }
		if (parent) { return parent->GetWorld(depth + 1) * local; }

		return local;
	}

	NiAVObject* Widget::GetModelAttachmentNode() const
	{
		if (transform_parent_node) { return transform_parent_node; }
		if (parent) { return parent->GetModelAttachmentNode(); }

		return object_reference ? object_reference->Get3D(false) : nullptr;
	}

	void Widget::UpdateOwnModelTransform()
	{
		const auto world = GetWorld();
		if (model) { model->SetWorldTransform(world); }
		for (auto& effect : visual_effects) { effect->SetWorldTransform(world); }
		for (auto& text : text_boxes) { text->SetWorldTransform(world); }
	}

	void Widget::UpdateModelTransform()
	{
		UpdateOwnModelTransform();
		for (auto& child : children) { child->UpdateModelTransform(); }
	}

	void Widget::AddModel(const std::string_view a_path, bool a_temporaryEffect,
		art_addon::ArtAddon::OnInitialized a_callback)
	{
		auto* object = GetObjectReference();
		auto* target = GetModelAttachmentNode();
		if (!object || !target) { return; }

		const auto transform = target->world.Invert() * GetWorld();
		auto       addon = art_addon::ArtAddon::Make(a_path, object, target, transform,
			[this, callback = std::move(a_callback)](art_addon::ArtAddon* a_addon) {
				if (IsHidden() && a_addon && a_addon->Get3D())
				{
					a_addon->Get3D()->SetAppCulled(true);
				}

				if (callback) { callback(a_addon); }
			});

		if (a_temporaryEffect) { visual_effects.emplace_back(std::move(addon)); }
		else
		{
			model = std::move(addon);
		}
	}

	bool Widget::TestRootOverlap(Hand& a_hand) const
	{
		const auto hand = a_hand.GetTransform();
		const auto world = GetWorld();
		return helper::IntersectSphereSphere(
			hand.translate, a_hand.GetRadius(), world.translate, world.scale * radius);
	}

	art_addon::AddonTextBox* Widget::AddText(
		std::string_view a_text, float a_spacing, std::string_view a_fontPath)
	{
		auto* object = GetObjectReference();
		auto* target = GetModelAttachmentNode();
		if (!object || !target) { return nullptr; }

		const auto transform = target->world.Invert() * GetWorld();
		text_boxes.emplace_back(std::make_unique<art_addon::AddonTextBox>(
			a_text, a_spacing, object, target, transform, std::string{ a_fontPath }));
		return text_boxes.back().get();
	}

	void Container::Filter(FilterFunc f)
	{
		for (auto& child : children)
		{
			if (auto b = f(*child))
			{
				child->SetEnabled(true);
				child->Show();
			}
			else
			{
				child->SetEnabled(false);
				child->Hide();
			}
		}
	}

	void Container::Sort(SortFunc f)
	{
		std::sort(children.begin(), children.end(),
			[f](const auto& a, const auto& b) { return f(*a, *b); });
	}

	void GridContainer::Filter(FilterFunc f)
	{
		current_filter = f;
		Redraw();
	}

	void GridContainer::Sort(SortFunc f)
	{
		current_sort = f;
		Redraw();
	}

	void GridContainer::RemoveGridChild(Widget* a_child)
	{
		Controller::GetSingleton()->MarkForDelete(a_child);
		redraw_counter = 2;
	}

	void GridContainer::Scroll(int a_adjustment)
	{ scroll_position = std::clamp(scroll_position + a_adjustment, 0, GetVisibleRows()); }

	void GridContainer::Update(float delta)
	{
		if (redraw_counter > 0)
		{
			if (--redraw_counter == 0) { Redraw(); }
		}
		if (last_scroll_position != scroll_position)
		{
			Redraw();
			last_scroll_position = scroll_position;
		}
	}

	void GridContainer::Redraw()
	{
		if (current_sort) { Container::Sort(current_sort); }

		visible_index = 0;

		const float usable_width =
			extents.x * 2.0f - static_cast<float>(items_per_row - 1) * padding;

		item_radius = usable_width / static_cast<float>(items_per_row) * 0.5f;

		const float spacing = item_radius * 2.0f + padding;

		const float total_width = static_cast<float>(items_per_row) * item_radius * 2.0f +
			static_cast<float>(items_per_row - 1) * padding;

		const float start_x = -total_width * 0.5f + item_radius;
		const float start_z = extents.z - item_radius;

		const int first_visible_row = static_cast<int>(std::floor(scroll_position));
		const int visible_rows =
			static_cast<int>(std::floor((extents.z * 2.0f + padding) / spacing));

		for (auto& child : children)
		{
			if (current_filter && !current_filter(*child))
			{
				child->Hide();
				child->SetEnabled(false);
				continue;
			}

			const int row = visible_index / items_per_row;
			const int col = visible_index % items_per_row;

			const bool inside = row >= first_visible_row && row < first_visible_row + visible_rows;

			if (inside)
			{
				const int local_row = row - first_visible_row;

				const NiPoint3 position{ start_x + static_cast<float>(col) * spacing, 0.0f,
					start_z - static_cast<float>(local_row) * spacing };

				child->Resize(item_radius);
				child->MoveTo(position);
				child->Show();
				child->SetEnabled(true);
			}
			else
			{
				child->Hide();
				child->SetEnabled(false);
			}

			++visible_index;
		}
	}

}
