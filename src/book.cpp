#include "book.h"

#include "helper_game.h"

namespace vr3dirp
{
	using namespace art_addon;
	static constexpr float kHiddenSpellScale = 0.001f;

	Book::~Book()
	{
		if (auto* actor = GetObjectReference()->As<RE::Actor>())
		{
			if (stored_spell && helper::IsHandEmpty(isLeft))
			{
				if (auto* equip_manager = RE::ActorEquipManager::GetSingleton())
				{
					equip_manager->EquipSpell(
						actor, stored_spell, helper::GetHandEquipSlot(isLeft));
				}
			}
		}
	}

	void Book::Close()
	{
		if (animator.HasQueued()) { animator.ClearQueue(); }
		animator.PlayImmediately(close, animation_speed * 1.2,
			[this]() { Controller::GetSingleton()->MarkForDelete(this); });

		RE::BSSoundHandle sound;
		auto              world_pos = GetWorld().translate;
		helper::InitializeSound(sound, kBookCloseSd);
		helper::PlaySound(sound, 1, world_pos, Get3D());
	}

	Book::Book(std::string_view a_model_path, bool a_isLeft, TESObjectREFR* a_objectReference,
		NiAVObject* a_root, NiTransform a_local) :
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

		AddModel(a_model_path, false, [this](ArtAddon* a) {
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
			interaction_volume->AddBehavior<HandInteractionMode>();
			interaction_volume->AddBehavior<BlockInputOnHover>(InputBlock::kAll);
			interaction_volume->AddBehavior<BookPageTurn>();
			interaction_volume->SetPriority(90);

			// Chapter tabs
			auto chapter_parent = Get3D()->GetObjectByName(kChapterParent);
			t.translate = { 0.1f, 0, -0.1f };
			tab_container = AddChild<Widget>(
				t, NiPoint3(1.0, layout.kRightPageHeight * 0.5f, 0.5), chapter_parent);
			tab_container->SetPriority(10);
			tab_container->AddBehavior<ExclusiveHoverGroup>();

			for (std::size_t i = 0; i < chapters.size(); ++i) AddChapterTab(i);

			// Page Content anchors
			t.translate = layout.kRightPageOrigin;
			right_page_parent = AddChild<Widget>(t,
				NiPoint3(layout.kRightPageWidth * 0.5f, layout.kRightPageHeight * 0.5f, 1),
				right_parent);
			right_page_parent->AddBehavior<ExclusiveHoverGroup>();

			t.translate = layout.kLeftPageOrigin;

			// left parent is upside down
			t.rotate = {
				{ -1.0000000, 0.0000000, 0.0000000 },
				{ 0.0000000, 1.0000000, 0.0000000 },
				{ 0.0000000, 0.0000000, -1.0000000 },
			};

			left_page_parent = AddChild<Widget>(t,
				NiPoint3(layout.kLeftPageWidth * 0.5f, layout.kLeftPageHeight * 0.5f, 1),
				left_parent);
			left_page_parent->AddBehavior<ExclusiveHoverGroup>();

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

			RE::BSSoundHandle sound;
			auto              world_pos = GetWorld().translate;
			helper::InitializeSound(sound, kBookOpenSd);
			helper::PlaySound(sound, 1, world_pos, Get3D());

			DrawCurrentPage(false);
		});
	}

	void Book::TurnPageLeft()
	{
		if (chapter_index >= chapters.size() || !chapters[chapter_index]) { return; }

		if (page_index + 1 < chapters[chapter_index]->pages.size())
		{
			TurnToPage(chapter_index, page_index + 1, TurnDirection::kLeft);
		}
		else if (chapter_index + 1 < chapters.size())
		{
			TurnToPage(chapter_index + 1, 0, TurnDirection::kLeft);
		}
	}

	void Book::TurnPageRight()
	{
		if (chapter_index >= chapters.size() || !chapters[chapter_index]) { return; }

		if (page_index > 0) { TurnToPage(chapter_index, page_index - 1, TurnDirection::kRight); }
		else if (chapter_index > 0)
		{
			const auto previous_chapter = chapter_index - 1;
			if (!chapters[previous_chapter] || chapters[previous_chapter]->pages.empty())
			{
				return;
			}

			TurnToPage(previous_chapter, chapters[previous_chapter]->pages.size() - 1,
				TurnDirection::kRight);
		}
	}

	void Book::TurnToChapter(int a_index)
	{
		SKSE::log::trace("{}", a_index);
		if (a_index == (int)chapter_index) { return; }
		if (a_index >= (int)chapters.size()) a_index = 0;
		if (a_index < 0) a_index = (int)chapters.size() - 1;
		SKSE::log::trace("  {}", a_index);
		TurnToPage(a_index, 0,
			a_index > (int)chapter_index ? TurnDirection::kLeft : TurnDirection::kRight);
	}

	void Book::TurnToPage(
		std::size_t a_chapter_index, std::size_t a_page_index, TurnDirection a_direction)
	{
		if (a_chapter_index >= chapters.size() || !chapters[a_chapter_index] ||
			a_page_index >= chapters[a_chapter_index]->pages.size() ||
			!chapters[a_chapter_index]->pages[a_page_index] ||
			(a_chapter_index == chapter_index && a_page_index == page_index))
		{
			return;
		}

		chapter_index = a_chapter_index;
		page_index = a_page_index;

		const bool turn_left = a_direction == TurnDirection::kLeft;
		ClearPageView();
		DrawCurrentPage(!turn_left);

		const auto  queued = animator.QueuedCount();
		const float anim_speed_adjust =
			animation_speed * std::pow(1.5f, static_cast<float>(queued));
		if (queued) { animator.SetSpeed(anim_speed_adjust); }
		if (queued <= 1)
		{
			const auto& animation = turn_left ? flip_left : flip_right;
			animator.Queue(animation, anim_speed_adjust, [this, turn_left] {
				if (!animator.HasQueued()) DrawCurrentPage(turn_left);
			});

			RE::BSSoundHandle sound;
			auto              world_pos = GetWorld().translate;
			helper::InitializeSound(sound, turn_left ? kBookFlipLeftSnd : kBookFlipRightSnd);
			helper::PlaySound(sound, 1, world_pos, Get3D());
		}
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
		tab->AddModel(chapters[a_index]->model_path, false, [this, a_index](ArtAddon* a_model) {
			helper::SetGlowMult(
				a_model ? a_model->Get3D() : nullptr, chapter_index == a_index ? 0.9f : 0.0f);
		});
		tab->SetPriority(tab_container->GetPriority() + 1);
		tab->AddBehavior<SelectionHighlight>(
			tab_container->GetBehavior<ExclusiveHoverGroup>(),
			[tab](bool highlight) { helper::SetGlowMult(tab->Get3D(), highlight ? 0.9f : 0.0f); },
			[this, a_index] { TurnToChapter((int)a_index); },
			[this, a_index] { return GetChapterIndex() == a_index; });

		chapters[a_index]->tab = tab;
	}

	void SelectionHighlight::Update(float)
	{
		if (!parent) { return; }

		const bool selected = is_selected && is_selected();
		const bool should_highlight = selected ||
			(hover_group &&
				(hover_group->IsExclusivelyHovered(parent, false) ||
					hover_group->IsExclusivelyHovered(parent, true)));

		if (should_highlight != highlighted)
		{
			if (on_highlight) { on_highlight(should_highlight); }
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

		// Display page numbers on right side only
		if (!a_left_page)
		{
			std::string page_str = std::to_string(page_index + 1);
			page_str.append("/").append(std::to_string(pages.size()));
			NiTransform t;
			t.translate = { layout.kRightPageWidth * 0.5f - 2.5f,
				0.5f - layout.kRightPageHeight * 0.5f, 0.0f };
			auto* page_number = right_page_parent->AddChild<Widget>(t, NiPoint3{});
			page_number->AddText(
				page_str, layout.page_number_character_spacing, layout.page_number_font_model_path);
		}
	}

	void Book::ClearPageView()
	{
		left_page_parent->ClearChildren();
		right_page_parent->ClearChildren();
	}

	void Page::Draw(Book::PageContext a_context, bool a_left_page)
	{
		if (a_left_page) {}
		else
		{}
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
				if (isGrabbed) { helper::PrintTransform(parent->GetTransform()); }
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

	void Book::DrawExtents(bool show) {}
}
