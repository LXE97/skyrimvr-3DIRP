#include "book.h"

#include "helper_game.h"

namespace vr3dirp
{
	using namespace art_addon;
	static constexpr float kHiddenSpellScale = 0.001f;

	Book::~Book()
	{
		if (stored_spell)
		{
			auto* actor = GetObjectReference()->As<RE::Actor>();
			auto* equip_manager = RE::ActorEquipManager::GetSingleton();

			if (actor && equip_manager)
			{
				equip_manager->EquipSpell(actor, stored_spell, helper::GetHandEquipSlot(isLeft));
			}
		}
	}

	void Book::Close()
	{
		if (animator.HasQueued()) { animator.ClearQueue(); }
		animator.PlayImmediately(close, animation_speed * 1.2,
			[this]() { Controller::GetSingleton()->MarkForDelete(this); });
	}

	Book::Book(
		bool a_isLeft, TESObjectREFR* a_objectReference, NiAVObject* a_root, NiTransform a_local) :
		Widget(kDefaultWindowRadius, a_objectReference, a_root, a_local),
		isLeft(a_isLeft)
	{
		// store state of dismissal button when summoned to avoid instant closing
		const auto settings = Controller::GetSingleton()->GetSettings();
		secondary_pressed_during_creation =
			vrinput::GetButtonState(settings.secondary, vrinput::Hand(a_isLeft),
				vrinput::ActionType::kPress) == vrinput::ButtonState::kButtonDown;

		auto* pc = RE::PlayerCharacter::GetSingleton();

		if (auto equipped = pc->GetEquippedObject(a_isLeft); equipped && pc->IsWeaponDrawn())
		{
			if ((stored_spell = equipped->As<RE::SpellItem>()))
			{
				helper::UnequipSpell(pc, stored_spell, a_isLeft);
			}
		}

		hand_mode = Controller::GetSingleton()->GetHand(a_isLeft)->RequestMode(
			Hand::Mode::kFist, Hand::ModePriority::kPassive);

		local.scale /= a_root->world.scale;

		// Block inputs on the book hand for as long as it exists
		block_handle = InputBlockManager::GetSingleton()->Acquire(a_isLeft,
			InputBlock::kPrimary | InputBlock::kSecondary | InputBlock::kHiggs |
				InputBlock::kVrikGestures |
				(!a_isLeft ? InputBlock::kActivatePickLength : InputBlock::kNone));

		AddModel(kModelPath, false, [this](ArtAddon* a) {
			ni_animator::SetControllerFlags(Get3D(), true, false, false, false, true);
			// Construct widget layout
			auto* right_parent = Get3D()->GetObjectByName(kRightParent);
			auto  left_parent = Get3D()->GetObjectByName(kLeftParent);
			if (!right_parent || !left_parent)
			{
				SKSE::log::error("Book construction failed: content nodes not found");
				return;
			}

			NiTransform t;

			// Interaction Volume
			t.translate = { -9, -2, 4 };
			auto interaction_volume = AddChild<BasicHitbox>(t, NiPoint3(18, 14, 5));
			interaction_volume->AddBehavior<HandPointing>();
			interaction_volume->AddBehavior<BlockInputOnHover>(InputBlock::kAll);
			interaction_volume->SetPriority(90);

			// Chapter tabs
			auto chapter_parent = Get3D()->GetObjectByName(kChapterParent);
			t.translate = { 0.1f, 0, -0.1f };
			tab_container =
				AddChild<Widget>(t, NiPoint3(1.0, layout.kPageHeight * 0.5f, 0.5), chapter_parent);
			tab_container->SetPriority(10);
			tab_container->AddBehavior<ExclusiveHoverGroup>();

			for (std::size_t i = 0; i < chapters.size(); ++i) AddChapterTab(i);

			// Page Content anchors
			t.translate = layout.kRightPageOrigin;
			right_page_parent = AddChild<Widget>(t,
				NiPoint3(layout.kRightPageWidth * 0.5f, layout.kPageHeight * 0.5f, 1),
				right_parent);
			right_page_parent->SetHitTestEnabled(false);

			t.translate = { -layout.kLeftPageWidth / 2 + layout.kLeftPageOrigin.x, 0, 0 };

			// left parent is upside down
			t.rotate = {
				{ -1.0000000, 0.0000000, 0.0000000 },
				{ 0.0000000, 1.0000000, 0.0000000 },
				{ 0.0000000, 0.0000000, -1.0000000 },
			};

			left_page_parent = AddChild<Widget>(t,
				NiPoint3(layout.kLeftPageWidth * 0.5f, layout.kPageHeight * 0.5f, 1), left_parent);
			left_page_parent->SetHitTestEnabled(false);

			// Grab Node
			t.translate = { 7, 14, -1 };
			t.rotate = {
				{ 1.0000000, 0.0000000, 0.0000000 },
				{ 0.0000000, -1.0000000, 0.0000000 },
				{ 0.0000000, 0.0000000, -1.0000000 },
			};
			grab_node = AddChild<GrabNode>(t, NiPoint3(3, 3, 3));
			grab_node->SetPriority(5);

			animator.PlayImmediately(open, animation_speed, [this]() { DrawCurrentPage(true); });

			DrawCurrentPage(false);
		});

		AddChapter(std::make_unique<Chapter>("3DIRP/Chapters/All.nif"));
		AddChapter(std::make_unique<Chapter>("3DIRP/Chapters/All.nif"));
		AddChapter(std::make_unique<Chapter>("3DIRP/Chapters/All.nif"));
		AddChapter(std::make_unique<Chapter>("3DIRP/Chapters/All.nif"));
		AddChapter(std::make_unique<Chapter>("3DIRP/Chapters/All.nif"));
		AddChapter(std::make_unique<Chapter>("3DIRP/Chapters/All.nif"));
		AddChapter(std::make_unique<Chapter>("3DIRP/Chapters/All.nif"));
		AddChapter(std::make_unique<Chapter>("3DIRP/Chapters/All.nif"));
		AddChapter(std::make_unique<Chapter>("3DIRP/Chapters/All.nif"));
		AddChapter(std::make_unique<Chapter>("3DIRP/Chapters/All.nif"));
		AddChapter(std::make_unique<Chapter>("3DIRP/Chapters/All.nif"));
		AddChapter(std::make_unique<Chapter>("3DIRP/Chapters/All.nif"));
	}

	void Book::TurnPageLeft()
	{
		if (animator.QueuedCount() >= 1) { return; }

		if (chapters.empty() || chapter_index >= chapters.size()) { return; }

		auto& chapter = *chapters[chapter_index];

		if (page_index + 1 < chapter.pages.size()) { ++page_index; }
		else if (chapter_index + 1 < chapters.size())
		{
			++chapter_index;
			page_index = 0;
		}
		else
		{
			return;
		}
		ClearPageView();
		DrawCurrentPage(false);
		animator.Queue(flip_left, animation_speed, [this] {
			if (!animator.HasQueued()) DrawCurrentPage(true);
		});
	}

	void Book::TurnPageRight()
	{
		if (animator.QueuedCount() >= 1) { return; }

		if (chapters.empty() || chapter_index >= chapters.size()) { return; }

		auto& chapter = *chapters[chapter_index];

		if (page_index > 0) { --page_index; }
		else if (chapter_index > 0)
		{
			--chapter_index;
			page_index = chapters[chapter_index]->pages.size() - 1;
		}
		else
		{
			return;
		}
		ClearPageView();
		DrawCurrentPage(true);
		animator.Queue(flip_right, animation_speed, [this] {
			if (!animator.HasQueued()) DrawCurrentPage(false);
		});
	}

	void Book::TurnToChapter(std::size_t a_index)
	{
		if (animator.QueuedCount() >= 2) { return; }
		if (a_index >= chapters.size() || a_index == chapter_index) { return; }

		bool turn_left = a_index > chapter_index;

		const auto& animation = turn_left ? flip_left : flip_right;

		chapter_index = a_index;
		page_index = 0;

		ClearPageView();
		DrawCurrentPage(!turn_left);
		animator.Queue(animation, animation_speed, [this, turn_left] {
			if (!animator.HasQueued()) DrawCurrentPage(turn_left);
		});
	}

	void Book::AddChapter(std::unique_ptr<Chapter> a_chapter)
	{
		if (!a_chapter) { return; }
		a_chapter->MakePages(layout);
		chapters.emplace_back(std::move(a_chapter));
		AddChapterTab(chapters.size() - 1);
	}

	void Book::AddChapterTab(std::size_t a_index)
	{
		if (!tab_container || a_index >= chapters.size() || !chapters[a_index] ||
			chapters[a_index]->tab)
		{
			return;
		}

		NiTransform transform;
		transform.translate = layout.kTabOffset;
		transform.translate.y -= static_cast<float>(a_index) * layout.tab_spacing;
		transform.translate.x -= (a_index % 2) * 0.3;
		transform.translate.z -= 0.1 * a_index;

		auto* tab = tab_container->AddChild<Widget>(transform, NiPoint3(1.0, 0.3, 0.25));
		tab->AddModel(chapters[a_index]->model_path);
		tab->SetPriority(tab_container->GetPriority() + 1);
		tab->AddBehavior<SelectionHighlight>(
			tab_container->GetBehavior<ExclusiveHoverGroup>(),
			[this, a_index] { TurnToChapter(a_index); },
			[this, a_index] { return GetChapterIndex() == a_index; });

		chapters[a_index]->tab = tab;
	}

	void SelectionHighlight::Update(float)
	{
		if (!parent) { return; }

		auto*      model = parent->Get3D();
		const bool selected = is_selected && is_selected();
		const bool should_highlight = selected ||
			(hover_group &&
				(hover_group->IsExclusivelyHovered(parent, false) ||
					hover_group->IsExclusivelyHovered(parent, true)));

		// AddModel is asynchronous, so also apply the current state when its node first appears.
		if (model != highlighted_model || should_highlight != highlighted)
		{
			if (model) { helper::SetGlowMult(model, should_highlight ? 0.9f : 0.0f); }
			highlighted_model = model;
			highlighted = should_highlight;
		}
	}

	void SelectionHighlight::OnClick(bool a_activate, Hand& a_hand, MenuAction a_action)
	{
		if (!a_activate || a_action != MenuAction::kPrimary) { return; }
		if (hover_group) { hover_group->Update(0.0f); }
		if (hover_group && !hover_group->IsExclusivelyHovered(parent, a_hand.IsLeft())) { return; }

		if (on_activate) on_activate();
	}

	void Book::DrawCurrentPage(bool a_left_page)
	{
		if (chapter_index >= chapters.size() || !chapters[chapter_index]) { return; }

		auto& pages = chapters[chapter_index]->pages;
		if (page_index >= pages.size() || !pages[page_index]) { return; }

		PageContext context{ layout, *left_page_parent, *right_page_parent };

		pages[page_index]->Draw(context, a_left_page);

		// Display page numbers
		std::string page_str = std::to_string(page_index + 1);
		page_str.append(" / ").append(std::to_string(pages.size()));
		NiTransform t;
		t.translate = { layout.kRightPageWidth * 0.5f - 0.5f, 0.5f - layout.kPageHeight * 0.5f,
			0.0f };
		auto* page_number = right_page_parent->AddChild<Widget>(t, NiPoint3{});
		page_number->AddText(
			page_str, layout.page_number_character_spacing, layout.page_number_font_model_path);
	}

	void Book::ClearPageView()
	{
		left_page_parent->ClearChildren();
		right_page_parent->ClearChildren();
	}

	void Page::Draw(Book::PageContext a_context, bool a_left_page)
	{
		if (a_left_page)
			a_context.left_page.AddModel("HelperSphere.nif");
		else
			a_context.right_page.AddModel("HelperSphere.nif");
	}

	void Book::Update(float a_delta)
	{
		animator.Update(Get3D(), a_delta);
		if (secondary_press_elapsed)
		{
			*secondary_press_elapsed += a_delta;
			if (*secondary_press_elapsed > secondary_double_tap_threshold)
			{
				secondary_press_elapsed.reset();
			}
		}

		const auto settings = Controller::GetSingleton()->GetSettings();

		const bool secondary_pressed =
			vrinput::GetButtonState(settings.secondary, vrinput::Hand(isLeft),
				vrinput::ActionType::kPress) == vrinput::ButtonState::kButtonDown;

		if (secondary_pressed && !secondary_pressed_during_creation)
		{
			if (secondary_press_elapsed)
			{
				secondary_press_elapsed.reset();
				Close();
			}
			else
			{
				secondary_press_elapsed = 0.0f;
			}
		}

		secondary_pressed_during_creation = secondary_pressed;
	}

	void Book::VirtualParent(NiAVObject* a_new, NiTransform& a_offset)
	{
		if (!a_new) { return; }

		const auto desired_book_world = a_new->world * a_offset;
		if (auto* transform_parent = GetTransformParentNode())
		{
			SetTransform(transform_parent->world.Invert() * desired_book_world);
		}
		else
		{
			SetTransform(desired_book_world);
		}
	}

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
			if (auto* book = dynamic_cast<Book*>(parent))
			{
				book->VirtualParent(follow_target, parent_store);
			}
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
			dynamic_cast<Book*>(parent)->TurnPageLeft();
		}
		else if (a_activate && a_action == MenuAction::kScrollRight)
		{
			dynamic_cast<Book*>(parent)->TurnPageRight();
		}
		else if (a_activate && a_action == MenuAction::kScrollUp)
		{
			auto book = dynamic_cast<Book*>(parent);
			book->TurnToChapter(book->GetChapterIndex() - 1);
		}
		else if (a_activate && a_action == MenuAction::kScrollDown)
		{
			auto book = dynamic_cast<Book*>(parent);
			book->TurnToChapter(book->GetChapterIndex() + 1);
		}
	}

	void Book::DrawExtents(bool show) {}

}
