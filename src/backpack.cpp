#include "backpack.h"

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

		AddModel(settings.model_path, false, [this](ArtAddon* a) {
			// parse nif for widget attachment nodes
			auto grab_node = a->Get3D()->GetObjectByName("GrabNode");
			if (!grab_node)
			{
				SKSE::log::error("Invalid {} : Grab node not found", settings.model_path);
				return;
			}
			auto container_node = a->Get3D()->GetObjectByName("Container");

			// widget layout construction
			auto grab_handle =
				AddChild<BackpackGrabNode>(NiTransform{}, NiPoint3(5, 5, 5), grab_node);
			grab_handle->SetPriority(5);
			grab_handle->StartGrab();

			auto container = AddChild<Widget>(NiTransform{}, NiPoint3(6, 12, 22), container_node);

			SKSE::log::trace("backpack created with radius {} and scale {}", radius, local.scale);
		});
	}

	Backpack::~Backpack() {}

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
		SKSE::log::trace("grabnode hoevered");
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

}
