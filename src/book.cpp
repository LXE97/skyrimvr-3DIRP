#include "book.h"

#include "helper_game.h"

namespace vr3dirp
{
	using namespace art_addon;

	static constexpr float kChapterGlow = 1.9f;

	namespace
	{
		NiTransform MakeBookLocalTransform(
			bool a_isLeft, const BookSettings& a_settings, float a_scale)
		{
			NiTransform result{};
			result.scale = a_scale;

			NiQuaternion rotation;
			if (a_isLeft)
			{
				result.translate = { a_settings.left_offset_x, a_settings.left_offset_y,
					a_settings.left_offset_z };
				rotation = { a_settings.left_rotate_w, a_settings.left_rotate_x,
					a_settings.left_rotate_y, a_settings.left_rotate_z };
			}
			else
			{
				result.translate = { a_settings.right_offset_x, a_settings.right_offset_y,
					a_settings.right_offset_z };
				rotation = { a_settings.right_rotate_w, a_settings.right_rotate_x,
					a_settings.right_rotate_y, a_settings.right_rotate_z };
			}

			const float length_squared = rotation.Dot(rotation);
			if (length_squared > 0.0f)
			{
				const float inverse_length = 1.0f / std::sqrt(length_squared);
				rotation.w *= inverse_length;
				rotation.x *= inverse_length;
				rotation.y *= inverse_length;
				rotation.z *= inverse_length;
			}
			else
			{
				rotation = { 1.0f, 0.0f, 0.0f, 0.0f };
			}

			result.rotate = rotation.ToRotation();
			return result;
		}
	}

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

		if (book_light) { helper::DestroyLight(book_light); }
	}

	void Book::Close()
	{
		if (animator.HasQueued()) { animator.ClearQueue(); }
		animator.PlayImmediately(close, animation_speed * 1.2,
			{ { close.end, [this]() { Controller::GetSingleton()->MarkForDelete(this); } } });

		RE::BSSoundHandle sound;
		auto              world_pos = GetWorld().translate;
		helper::InitializeSound(sound, kBookCloseSd);
		helper::PlaySound(sound, 1, world_pos, Get3D());
	}

	Book::Book(std::string_view a_model_path, bool a_isLeft, TESObjectREFR* a_objectReference,
		NiAVObject* a_root, BookSettings& a_settings, std::optional<float> a_scale_override) :
		Widget(kDefaultWindowRadius, a_objectReference, a_root,
			MakeBookLocalTransform(
				a_isLeft, a_settings, a_scale_override.value_or(a_settings.book_scale))),
		settings(a_settings),
		isLeft(a_isLeft)
	{
		layout.right_page_origin.z = a_isLeft ? settings.rightpage_text_z_offset :
												settings.rightpage_text_z_offset_righthand;
		layout.left_page_origin.z -=
			a_isLeft ? settings.leftpage_text_z_offset : settings.leftpage_text_z_offset_righthand;
		layout.top_margin = settings.top_margin;
		layout.horizontal_margin = settings.horizontal_margin;
		layout.body_text_scale = settings.font_size;
		layout.heading_text_scale *= settings.font_size;
		layout.objective_spacing =
			0.2f + art_addon::AddonTextBox::kLineSpacing * layout.body_text_scale;

		// if (settings.light_radius > 0.1f)
		// {
		// 	NiTransform book_transform;
		// 	book_transform.translate = { 0, 0, 10 };
		// 	book_light = helper::MakeLight(a_objectReference, a_root->AsNode(), book_transform,
		// 		settings.light_radius, settings.light_fade);
		// }

		// store state of dismissal button when summoned to avoid instant closing
		const auto vrgui_settings = Controller::GetSingleton()->GetSettings();
		secondary_pressed_during_creation =
			vrinput::GetButtonState(vrgui_settings.secondary, vrinput::Hand(a_isLeft),
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

			// Light
			if (settings.light_fade > 0.01f)
			{
				NiTransform book_transform;
				book_transform.translate = { -9, 0, 15 };
				book_light = helper::MakeLight(
					a->GetTarget(), a->Get3D()->AsNode(), book_transform, 5.0, settings.light_fade);
			}

			// Interaction Volume
			t.translate = { -7, -2, 4 };
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
			t.translate = layout.right_page_origin;
			right_page_parent = AddChild<Widget>(t,
				NiPoint3(layout.kRightPageWidth * 0.5f, layout.kRightPageHeight * 0.5f, 1),
				right_parent);
			right_page_parent->AddBehavior<ExclusiveHoverGroup>();

			t.translate = layout.left_page_origin;

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
			if (isLeft) { t.translate = { 7, 16, -4 }; }
			else
			{
				t.translate = { -23, 16, -4 };
			}

			t.rotate = {
				{ 1.0000000, 0.0000000, 0.0000000 },
				{ 0.0000000, -1.0000000, 0.0000000 },
				{ 0.0000000, 0.0000000, -1.0000000 },
			};
			grab_node = AddChild<GrabNode>(t, NiPoint3(2, 2, 4));
			grab_node->SetPriority(5);

			const auto initial_chapter = static_cast<int>(chapter_index);
			const auto initial_page = static_cast<int>(page_index);
			animator.PlayImmediately(
				open, animation_speed, { { open.end, [this, initial_chapter, initial_page]() {
											  DrawPage(initial_chapter, initial_page, true);
										  } } });

			RE::BSSoundHandle sound;
			auto              world_pos = GetWorld().translate;
			helper::InitializeSound(sound, kBookOpenSd);
			helper::PlaySound(sound, 1, world_pos, Get3D());

			DrawPage(initial_chapter, initial_page, false);
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
		if (a_index == (int)chapter_index) { return; }
		if (a_index >= (int)chapters.size()) a_index = 0;
		if (a_index < 0) a_index = (int)chapters.size() - 1;
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

		float anim_speed_adjust = animation_speed;
		if (animator.IsBusy())
		{
			anim_speed_adjust *= 1.2f;
			animator.SetSpeed(anim_speed_adjust);
		}

		animator.ClearQueue();

		const auto& animation = turn_left ? flip_left : flip_right;
		const auto  draw_chapter = static_cast<int>(a_chapter_index);
		const auto  draw_page = static_cast<int>(a_page_index);
		animator.Queue(animation, anim_speed_adjust,
			{ { 0.2f,
				  [this, turn_left, draw_chapter, draw_page] {
					  ClearPageView(!turn_left);
					  DrawPage(draw_chapter, draw_page, !turn_left);
				  } },
				{ turn_left ? 1.8f : 2.7f, [this, turn_left, draw_chapter, draw_page] {
					 ClearPageView(turn_left);
					 DrawPage(draw_chapter, draw_page, turn_left);
				 } } });

		RE::BSSoundHandle sound;
		auto              world_pos = GetWorld().translate;
		helper::InitializeSound(sound, turn_left ? kBookFlipLeftSnd : kBookFlipRightSnd);
		helper::PlaySound(sound, 1, world_pos, Get3D());
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
		transform.translate.x += 0.1;
		transform.translate.x -= (a_index % 2) * 0.2;
		transform.translate.z -= 0.1 * a_index;

		auto* tab = tab_container->AddChild<Widget>(transform, NiPoint3(1.0, 0.3, 0.25));
		tab->AddModel(chapters[a_index]->model_path, false, [this, a_index](ArtAddon* a_model) {
			helper::SetGlowMult(a_model ? a_model->Get3D() : nullptr,
				chapter_index == a_index ? kChapterGlow : 0.0f);
		});
		tab->SetPriority(tab_container->GetPriority() + 1);
		tab->AddBehavior<SelectionHighlight>(
			tab_container->GetBehavior<ExclusiveHoverGroup>(),
			[tab](bool highlight) {
				helper::SetGlowMult(tab->Get3D(), highlight ? kChapterGlow : 0.0f);
			},
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

	void Book::DrawPage(int a_chapter, int a_page, bool a_left_page)
	{
		if (a_chapter < 0 || a_page < 0) { return; }

		const auto chapter = static_cast<std::size_t>(a_chapter);
		const auto page = static_cast<std::size_t>(a_page);
		if (chapter >= chapters.size() || !chapters[chapter]) { return; }

		auto& pages = chapters[chapter]->pages;
		if (page >= pages.size() || !pages[page]) { return; }

		PageContext context{ layout, *left_page_parent, *right_page_parent, a_page,
			(int)pages.size() };

		pages[page]->Draw(context, a_left_page);
	}

	void Book::ClearPageView(bool a_left)
	{
		if (a_left) { left_page_parent->ClearChildren(); }
		else
		{
			right_page_parent->ClearChildren();
		}
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

		const auto vrgui_settings = Controller::GetSingleton()->GetSettings();

		const bool secondary_pressed =
			vrinput::GetButtonState(vrgui_settings.secondary, vrinput::Hand(isLeft),
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
				if (isGrabbed)
				{
					auto*      book = static_cast<Book*>(GetRoot());
					const auto transform = book->GetTransform();
					const auto quat = helper::Mat2Quat(transform.rotate);

					if (book->isLeft)
					{
						book->settings.left_offset_x = transform.translate.x;
						book->settings.left_offset_y = transform.translate.y;
						book->settings.left_offset_z = transform.translate.z;

						book->settings.left_rotate_w = quat.w;
						book->settings.left_rotate_x = quat.x;
						book->settings.left_rotate_y = quat.y;
						book->settings.left_rotate_z = quat.z;
					}
					else
					{
						book->settings.right_offset_x = transform.translate.x;
						book->settings.right_offset_y = transform.translate.y;
						book->settings.right_offset_z = transform.translate.z;

						book->settings.right_rotate_w = quat.w;
						book->settings.right_rotate_x = quat.x;
						book->settings.right_rotate_y = quat.y;
						book->settings.right_rotate_z = quat.z;
					}

					helper::PrintTransform(parent->GetTransform());
				}
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
