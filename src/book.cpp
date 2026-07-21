#include "book.h"

namespace vr3dui
{
	using namespace art_addon;

	Window* SummonAttachedBook(bool isLeft)
	{
		const std::string kTestModel = "SpellBookVR/custom.nif";

		RE::NiTransform default_t;
		default_t.scale = 1.0;
		default_t.translate = { 5.915527, -10.583008, 10.284607 };
		default_t.rotate = { { 0.839558, -0.198012, -0.493753 },
			{ -0.528744, -0.127261, -0.825465 }, { 0.089592, 0.956704, -0.210660 } };

		auto window = Controller::GetSingleton()->AddWindow(std::make_unique<AttachedWindow>(
			Book::kDefaultWindowRadius, RE::PlayerCharacter::GetSingleton(),
			vrinput::GetHandNode(vrinput::Hand(isLeft), false)->AsNode(), default_t));
		RE::NiTransform temp;
		temp.scale = 0.9;
		window->AddChild<Book>(kTestModel, isLeft, temp);

		return window;
	}

	void DismissBook(Window* a_book_window)
	{
		if (a_book_window)
		{
			auto book = a_book_window->FindChild<Book>();
			if (book) { book->Close(); }
		}
	}

	void Book::Close()
	{
		if (animator.HasQueued()) { animator.ClearQueue(); }
		animator.Queue(close, animation_speed * 1.5,
			[this]() { Controller::GetSingleton()->MarkForDelete(this->parent); });
	}

	Book::Book(Widget* a_parent, std::string a_model_path, bool a_isLeft, NiTransform a_transform) :
		Widget(a_parent, a_transform, NiPoint3(40, 40, 40)),
		isLeft(a_isLeft)
	{
		AddModel(a_model_path, false, [this, windowscale = local.scale](ArtAddon* m) {
			if (auto node = m->Get3D())
			{
				ni_animator::SetControllerFlags(node, true, false, false, false, true);
				node->local.scale = windowscale / node->parent->world.scale;

				//test categories
				if (auto tabparent = node->GetObjectByName(kTabParentNodeName))
				{
					SKSE::log::trace("adding catageory tab");
					NiTransform t;
					t.translate = kTabOffset;
					auto tab = this->AddChild<ModelDrivenWidget>(tabparent, t, NiPoint3(2, 1.5, 1));
					tab->AddModel(kChapterTabModel, false, [](ArtAddon* m){
						SKSE::log::trace("tab created");
					});
				}
			}
		});

		NiTransform t;

		t.translate = { 10, -18, -4 };
		grab_node = AddChild<GrabNode>(isLeft, t);

		t.translate = { -10, 0, 5 };
		auto interaction_volume = AddChild<BasicHitbox>(isLeft, t, NiPoint3(26, 17, 8));
		interaction_volume->AddBehavior<HandPointing>(!isLeft);

		animator.PlayImmediately(open, animation_speed);
	}

	void Book::Update(float a_delta)
	{
		Widget::Update(a_delta);
		animator.Update(Get3D(), a_delta);
	}

	void Book::TurnPage(bool a_direction_left)
	{
		if (animator.QueuedCount() >= 2) { return; }

		if (a_direction_left) { animator.Queue(flip_left, animation_speed); }
		else
		{
			animator.Queue(flip_right, animation_speed);
		}
	}

	void Book::VirtualParent(NiAVObject* a_new, NiTransform& a_offset)
	{
		auto* window = GetWindow();
		auto* root = window ? window->GetRootNode() : nullptr;
		if (!a_new || !root) { return; }

		const auto desired_world = a_new->world * a_offset;
		const auto desired_window_world = desired_world * local.Invert();

		window->GetTransform() = root->world.Invert() * desired_window_world;

		if (auto* node = Get3D())
		{
			const float inverse_parent_scale = node->local.scale;
			node->local = GetLocalToRoot();
			node->local.scale = inverse_parent_scale;

			NiUpdateData ctx{};
			node->Update(ctx);
		}
	}

	GrabNode::GrabNode(Widget* a_parent, bool isLeft, NiTransform a_local) :
		isLeft(isLeft),
		Widget(a_parent, a_local, NiPoint3(4, 4, 4))
	{ priority = 1; }

	void GrabNode::OnClick(bool a_activate, Hand& a_hand, MenuAction a_action)
	{
		if (a_action == MenuAction::kSecondary)
		{
			if (a_activate)
			{
				isGrabButtonHeld = true;
				grabHoldTime = 0.0f;
				grabHand = &a_hand;
			}
			else
			{
				isGrabButtonHeld = false;
				grabHoldTime = 0.0f;
				grabHand = nullptr;
				isGrabbed = false;
				follow_target = nullptr;

				if (!IsHovered(a_hand.IsLeft())) { hand_mode.Release(); }
				else
				{
					hand_mode = a_hand.RequestMode(Hand::Mode::kOpen, Hand::ModePriority::kGrab);
				}
			}
		}
	}

	void GrabNode::Update(float delta)
	{
		if (isGrabButtonHeld && !isGrabbed && grabHand)
		{
			grabHoldTime += delta;
			if (grabHoldTime >= kGrabHoldTime)
			{
				follow_target = vrinput::GetHandNode(
					grabHand->IsLeft() ? vrinput::Hand::kLeft : vrinput::Hand::kRight, false);
				if (follow_target)
				{
					isGrabbed = true;
					hand_mode = grabHand->RequestMode(Hand::Mode::kFist, Hand::ModePriority::kGrab);
					parent_store = follow_target->world.Invert() * parent->GetWorld();
				}
			}
		}

		if (isGrabbed && follow_target)
		{
			dynamic_cast<Book*>(parent)->VirtualParent(follow_target, parent_store);
		}
	}

	void GrabNode::OnHover(bool a_activate, Hand& a_hand)
	{
		if (a_activate)
		{
			hand_mode = a_hand.RequestMode(Hand::Mode::kOpen, Hand::ModePriority::kGrab);
		}
		if (!a_activate)
		{
			isGrabButtonHeld = false;
			grabHoldTime = 0.0f;
			grabHand = nullptr;
			isGrabbed = false;
			hand_mode.Release();
			follow_target = nullptr;
		}
	}

	bool BasicHitbox::TestOverlap(Hand& a_hand) const
	{
		// only use hand sphere
		auto t = a_hand.GetTransform();
		auto w = GetWorld();
		// broad phase
		if (helper::IntersectSphereSphere(
				t.translate, a_hand.GetRadius() * t.scale, w.translate, w.scale * radius))
		{  // narrow phase
			return helper::IntersectSphereOBB(
				t.translate, a_hand.GetRadius() * t.scale, w, extents * w.scale);
		}
		return false;
	}

	void BasicHitbox::OnClick(bool a_activate, Hand& a_hand, MenuAction a_action)
	{
		if (a_activate && a_action == MenuAction::kScrollLeft)
		{
			dynamic_cast<Book*>(parent)->TurnPage(true);
		}
		else if (a_activate && a_action == MenuAction::kScrollRight)
		{
			dynamic_cast<Book*>(parent)->TurnPage(false);
		}
	}

	void Book::DrawExtents(bool show) {}
}
