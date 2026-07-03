#include "spellbook.h"

namespace spellbook
{
	namespace
	{
		void SetControllerFlags(NiAVObject* a_obj)
		{
			if (!a_obj) { return; }

			for (auto* controller = a_obj->GetControllers(); controller;
				controller = controller->next.get())
			{
				controller->flags.set(false, RE::NiTimeController::Flag::kCycleType_Clamp);
				controller->flags.set(true, RE::NiTimeController::Flag::kCycleType_Loop);
				controller->flags.set(true, RE::NiTimeController::Flag::kForceUpdate);
				controller->flags.set(true, RE::NiTimeController::Flag::kActive);
				controller->flags.set(false, RE::NiTimeController::Flag::kAnimType_AppTime);
				controller->flags.set(false, RE::NiTimeController::Flag::kComputeScaledTime);
			}

			if (auto* node = a_obj->AsNode())
			{
				for (auto& child : node->children) { SetControllerFlags(child.get()); }
			}
		}
	}

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
		if (a_spbk) { a_spbk->AddBehavior<BookAnimator>(Spellbook::close, 2.f); }
	}

	Spellbook::Spellbook(bool a_isLeft, NiTransform a_transform) :
		AttachedWindow(kDefaultWindowRadius, RE::PlayerCharacter::GetSingleton(),
			vrinput::GetHandNode(vrinput::Hand::kLeft, false)->AsNode(), a_transform)
	{
		const std::string kModelPath = "SpellBookVR/custom.nif";

		AddModel(kModelPath, false, [windowscale = local.scale](ArtAddon* m) {
			if (auto node = m->Get3D())
			{
				SetControllerFlags(node);
				node->local.scale = windowscale / node->parent->world.scale;
			}
		});

		NiTransform t;
		t.translate.z = -20;
		grab_node = AddChild<GrabNode>(isLeft, NiPoint3(0, 0, -10));
		grab_node->MoveTo({ 0, 0, -10 });

		AddBehavior<HandPointing>(!isLeft);
		AddBehavior<BookAnimator>(open);

		BuildPages();
		AttachButtons();
		SetupGrid();
	}

	bool Spellbook::HandStateFilter(Hand& a_hand) const
	{
		if (a_hand.isLeft != isLeft) { return true; }
		return false;
	}

	void Spellbook::OnClick(bool a_activate, Hand& a_hand, MenuAction a_action) {}

	void Spellbook::BuildPages() {}
	void Spellbook::AttachButtons() {}
	void Spellbook::SetupGrid() {}

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
				auto other_hand = vrinput::GetHandNode((vrinput::Hand) false, false)->world;
				parent_store = other_hand.Invert() * parent->GetWorld();
			}

			if (!a_activate) { helper::PrintTransform(parent->GetLocal()); }
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
		if (!a_activate) { isGrabbed = false; }
	}

	void BookAnimator::Update(float delta)
	{
		if (auto node = parent->Get3D())
		{
			NiUpdateData ctx{};
			accumulate += delta * speed;
			if (accumulate > keyframes.end)
			{
				accumulate = keyframes.steady;
				Remove();
			}
			helper::SetControllerTime(node, accumulate);
			node->Update(ctx);
		}
	}

	void BookAnimator::OnDetach()
	{
		if (&keyframes == &Spellbook::close) { Controller::GetSingleton()->MarkForDelete(parent); }
	}
}