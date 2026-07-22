#include "book.h"

namespace vr3dui
{
	using namespace art_addon;

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
		animator.PlayImmediately(close, animation_speed * 1.2,
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
				tab_parent = node->GetObjectByName(kTabParentNodeName);

				for (std::size_t i = 0; i < chapters.size(); ++i) { AddChapterTab(i); }

				animator.PlayImmediately(open, animation_speed);
				DrawCurrentPage();
			}
		});

		this->SetPriority(100);

		NiTransform t;

		t.translate = { 10, -18, -4 };
		grab_node = AddChild<GrabNode>(isLeft, t);
		grab_node->SetPriority(5);

		t.translate = { -10, 0, 5 };
		auto interaction_volume = AddChild<BasicHitbox>(isLeft, t, NiPoint3(26, 17, 8));
		interaction_volume->AddBehavior<HandPointing>(!isLeft);
		interaction_volume->SetPriority(90);
	}

	void Book::TurnPageLeft()
	{
		if (animator.QueuedCount() >= 1) { return; }

		if (chapters.empty() || chapter_index >= chapters.size()) { return; }

		auto& chapter = *chapters[chapter_index];

		if (page_index + 1 < chapter.pages.size()) { ++page_index; }
		else if (chapter_index + 1 < chapters.size())
		{
			chapters[chapter_index]->OnSelected(false);
			++chapter_index;
			chapters[chapter_index]->OnSelected(true);
			page_index = 0;
		}
		else
		{
			return;
		}
		ClearPageView();
		animator.Queue(flip_left, animation_speed, [this] {
			if (!animator.HasQueued()) DrawCurrentPage();
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
			chapters[chapter_index]->OnSelected(false);
			--chapter_index;
			chapters[chapter_index]->OnSelected(true);
			page_index = chapters[chapter_index]->pages.size() - 1;
		}
		else
		{
			return;
		}
		ClearPageView();
		animator.Queue(flip_right, animation_speed, [this] {
			if (!animator.HasQueued()) DrawCurrentPage();
		});
	}

	void Book::TurnToChapter(std::size_t a_index)
	{
		if (animator.QueuedCount() >= 2) { return; }
		if (a_index >= chapters.size() || a_index == chapter_index) { return; }

		const auto& animation = a_index < chapter_index ? flip_right : flip_left;

		chapters[chapter_index]->OnSelected(false);
		chapter_index = a_index;
		chapters[chapter_index]->OnSelected(true);
		page_index = 0;

		ClearPageView();
		animator.Queue(animation, animation_speed, [this] {
			if (!animator.HasQueued()) DrawCurrentPage();
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
		if (!tab_parent || a_index >= chapters.size() || !chapters[a_index] ||
			chapters[a_index]->tab)
		{
			return;
		}

		NiTransform transform;
		transform.translate = layout.kTabOffset;
		transform.translate.y -= static_cast<float>(a_index) * layout.tab_spacing;
		transform.translate.x -= (a_index % 2) * 0.3;

		auto* tab = AddChild<ChapterTab>(
			tab_parent, transform, NiPoint3(1.0, 0.4, 0.5), a_index, &chapter_tab_group);
		tab->AddModel(chapters[a_index]->model_path);
		tab->SetPriority(10 + (int)a_index);
		chapters[a_index]->tab = tab;
		if (a_index == chapter_index) { chapters[a_index]->OnSelected(true); }
	}

	void ChapterTab::OnClick(bool a_activate, Hand&, MenuAction a_action)
	{
		if (a_activate && a_action == MenuAction::kPrimary)
		{
			if (auto* book = dynamic_cast<Book*>(parent)) { book->TurnToChapter(chapter_index); }
		}
	}

	void Book::DrawCurrentPage()
	{
		if (chapter_index >= chapters.size() || !chapters[chapter_index]) { return; }

		auto& pages = chapters[chapter_index]->pages;
		if (page_index >= pages.size() || !pages[page_index]) { return; }

		// Create parents for page content
		NiTransform p{};
		p.translate = { layout.kRightPageWidth / 2, 0, 0 };
		NiPoint3 e = { layout.kRightPageWidth / 2, layout.kPageHeight / 2, 2 };
		right_page_parent =
			AddChild<ModelDrivenWidget>(model->Get3D()->GetObjectByName(kRightPageNodeName), p, e);

		p.translate = { -layout.kLeftPageWidth / 2 + layout.kLeftPageOrigin.x, 0, 0 };

		// left parent is upside down
		p.rotate = { { -1.0000000, 0.0000000, 0.0000000 }, { 0.0000000, 1.0000000, 0.0000000 },
			{ -0.0000000, 0.0000000, -1.0000000 } };

		left_page_parent =
			AddChild<ModelDrivenWidget>(model->Get3D()->GetObjectByName(kLeftPageNodeName), p, e);

		PageContext context{ layout, *left_page_parent, *right_page_parent };

		right_page_parent->SetPriority(100);
		left_page_parent->SetPriority(100);

		pages[page_index]->Draw(context);

		// Display page numbers
		std::string page_str = std::to_string(page_index + 1);
		page_str.append(" / ").append(std::to_string(pages.size()));
		NiTransform t;
		t.translate = layout.kRightPageOrigin;
		t.translate += { layout.kRightPageWidth - 0.5, 0.5 - layout.kPageHeight, 0 };
		page_numbers =
			std::make_unique<AddonTextBox>(page_str, layout.page_number_character_spacing,
				model->Get3D()->GetObjectByName(kRightPageNodeName), t,
				std::string{ layout.page_number_font_model_path });
	}

	void Book::ClearPageView()
	{
		auto* left = std::exchange(left_page_parent, nullptr);
		auto* right = std::exchange(right_page_parent, nullptr);

		auto* controller = vr_gui::Controller::GetSingleton();
		controller->MarkForDelete(left);
		controller->MarkForDelete(right);
	}

	void Page::Draw(Book::PageContext a_context)
	{
		a_context.left_page.AddModel("HelperSphere.nif");
		a_context.right_page.AddModel("HelperSphere.nif");
	}

	void Book::Update(float a_delta)
	{
		Widget::Update(a_delta);
		animator.Update(Get3D(), a_delta);
		chapter_tab_group.Update();
		page_group.Update();
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

	void ChapterTab::OnHover(bool a_activate, Hand& a_hand)
	{
		if (!group) { return; }

		if (a_activate) { group->Request(this, GetWorld().translate, a_hand); }
		else
		{
			group->Release(this, a_hand);
		}
	}

	void ChapterTab::OnHoverExclusive(bool a_activate, Hand& a_hand) { UpdateSelectionVisual(); }

	void ChapterTab::OnSelected(bool a_selected)
	{
		selected = a_selected;
		UpdateSelectionVisual();
	}

	void ChapterTab::UpdateSelectionVisual()
	{
		if (auto* node = Get3D())
		{
			const bool highlighted =
				selected || IsExclusivelyHovered(false) || IsExclusivelyHovered(true);

			helper::SetGlowMult(node, highlighted ? 0.9f : 0.0f);
		}
	}
}
