#include "spellbook.h"

namespace spellbook
{
	using namespace art_addon;

	Spellbook* Summon(bool isLeft)
	{
		RE::NiTransform default_t;
		default_t.scale = 1;
		default_t.translate = { 5.915527, -10.583008, 10.284607 };
		default_t.rotate = { { 0.839558, -0.198012, -0.493753 },
			{ -0.528744, -0.127261, -0.825465 }, { 0.089592, 0.956704, -0.210660 } };

		auto spellbook = std::make_unique<Spellbook>(isLeft, default_t);
		auto result = spellbook.get();

		Controller::GetSingleton()->AddWindow(std::move(spellbook));

		return result;
	}

	void Dismiss(Spellbook* a_spbk)
	{
		if (a_spbk) { a_spbk->Close(); }
	}

	void Spellbook::Close()
	{
		if (animator.HasQueued()) { animator.ClearQueue(); }
		animator.Queue(close, animation_speed * 1.5,
			[this]() { Controller::GetSingleton()->MarkForDelete(this); });
	}

	Spellbook::Spellbook(bool a_isLeft, NiTransform a_transform) :
		AttachedWindow(kDefaultWindowRadius, RE::PlayerCharacter::GetSingleton(),
			vrinput::GetHandNode(vrinput::Hand(a_isLeft), false)->AsNode(), a_transform),
		isLeft(a_isLeft)
	{
		const std::string kModelPath = "SpellBookVR/custom.nif";

		AddModel(kModelPath, false, [this, windowscale = local.scale](ArtAddon* m) {
			if (auto node = m->Get3D())
			{
				ni_animator::SetControllerFlags(node, true, false, false, false, true);
				node->local.scale = windowscale / node->parent->world.scale;
				this->DisplaySpellInfo("SpellbookVR/char_2048.nif");
			}
		});

		AttachButtons();
		SetupGrid(Category::kAll);

		NiTransform t;
		t.translate = { 0, -10, -20 };
		grab_node = AddChild<GrabNode>(isLeft, NiPoint3(0, 0, -10));
		grab_node->MoveTo({ 0, 0, -10 });

		t.translate = { 0, 0, 5 };
		interaction_volume = AddChild<BasicHitbox>(t, NiPoint3(20, 10, 10), 20);

		interaction_volume->AddBehavior<HandPointing>(!isLeft);

		animator.PlayImmediately(open, animation_speed);
	}

	void Spellbook::Update(float a_delta)
	{
		AttachedWindow::Update(a_delta);
		animator.Update(Get3D(), a_delta);
	}

	void Spellbook::AttachButtons() {}

	void Spellbook::SetupGrid(Category a_category) {}

	void Spellbook::TurnPage(bool a_direction_left)
	{
		if (a_direction_left) { animator.Queue(flip_left, animation_speed); }
		else
		{
			animator.Queue(flip_right, animation_speed);
		}
	}

	void Spellbook::VirtualParent(NiAVObject* a_new, NiTransform& a_offset)
	{
		auto  desired_world = a_new->world * a_offset;
		float scale_store = 1;

		auto parent_to_target = parent_node->world.Invert() * desired_world;

		local = parent_to_target;

		if (auto node = model->Get3D())
		{
			scale_store = node->local.scale;
			NiUpdateData ctx{};
			auto         model_parent_to_target = node->parent->world.Invert() * desired_world;
			node->local = model_parent_to_target;
			node->local.scale = scale_store;

			node->Update(ctx);
		}
		local.scale = scale_store;
	}

	void Spellbook::DisplaySpellInfo(std::string font, float z_offset)
	{
		NiTransform t{};
		t.translate = { 2, 10, z_offset };
		t.rotate.SetEulerAnglesXYZ(0, 3.141593, 3.141593);
		if (auto node = model->Get3D()->GetObjectByName(kLeftPageNodeName))
		{
			SKSE::log::trace("drawing text");
			description = std::make_unique<AddonTextBox>(
				"Accusamus magnam neque est libero. Ipsam quia aut in \n"
				"recusandae assumenda consequatur illo. Muae nostrum\n"
				"praesentium illo id magni.\n Dolorem et similique saepe ut\n"
				"voluptatum exercitationem sit.aut et culpa quidem."
				"Inventore alias at quidem dolorem\n aut et culpa quidem.\n"
				"Ut non est dicta. A qui molestiae sit reprehenderit voluptatem\n"
				"at mollitia. Werum possimus\n consequuntur architecto. Officia\n"
				"ipsum soluta cum suscipit.\n\n"
				"Hic ex sed cupiditate voluptatum.\n Earum officia eaque quis in\n"
				"perferendis et dolorem sint.\n Et vero temporibus sed. Wpsum\n"
				"ea ex blanditiis totam \ndoloremque similique.\n"
				"at mollitia. Werum possimus consequuntur architecto. Officia\n"
				"Hic ex sed cupiditate voluptatum. Earum officia eaque quis",

				-0.2f, node, t, font);
		}
		else
		{
			SKSE::log::trace("node not found");
		}
	}

	GrabNode::GrabNode(Widget* a_parent, bool isLeft, NiPoint3 a_offset) :
		isLeft(isLeft),
		offset(a_offset),
		Widget(a_parent, NiTransform{}, NiPoint3(5, 5, 5))
	{}

	void GrabNode::OnClick(bool a_activate, Hand& a_hand, MenuAction a_action)
	{
		if (a_action == MenuAction::kSecondary)
		{
			isGrabbed = a_activate;
			if (a_activate)
			{
                hand_mode = a_hand.RequestMode(Hand::Mode::kFist, Hand::ModePriority::kGrab);
				auto other_hand = vrinput::GetHandNode((vrinput::Hand) false, false)->world;
				parent_store = other_hand.Invert() * parent->GetWorld();
			}

			if (!a_activate)
			{  
                hand_mode.Release();
                //helper::PrintTransform(parent->GetLocal());
			}
		}
	}

	void GrabNode::Update(float delta)
	{
		if (isGrabbed)
		{
			dynamic_cast<Spellbook*>(parent)->VirtualParent(
				vrinput::GetHandNode((vrinput::Hand) false, false), parent_store);
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
			isGrabbed = false;
			hand_mode.Release();
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
			dynamic_cast<Spellbook*>(parent)->TurnPage(true);
		}
		else if (a_activate && a_action == MenuAction::kScrollRight)
		{
			dynamic_cast<Spellbook*>(parent)->TurnPage(false);
		}
	}
}