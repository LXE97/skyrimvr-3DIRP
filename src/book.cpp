#include "book.h"

namespace vr3dui
{
	using namespace art_addon;

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
		local.scale /= a_root->world.scale;

		AddModel(kModelPath, false, [this](ArtAddon* a) {
			ni_animator::SetControllerFlags(Get3D(), true, false, false, false, true);

			this->SetHitTestEnabled(false);

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
			t.translate = { -9, 0, 4 };
			auto interaction_volume = AddChild<BasicHitbox>(isLeft, t, NiPoint3(18, 14, 5));
			interaction_volume->AddBehavior<HandPointing>();
			interaction_volume->SetPriority(90);

			// Chapter tabs
			auto chapter_parent = Get3D()->GetObjectByName(kChapterParent);
			t.translate = { 0.1f, 0, -0.1f };
			tab_container =
				AddChild<Widget>(t, NiPoint3(1.0, layout.kPageHeight * 0.5f, 0.5), chapter_parent);

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

			animator.PlayImmediately(open, animation_speed);

			DrawCurrentPage();
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
			--chapter_index;
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

		chapter_index = a_index;
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

		auto* tab = tab_container->AddChild<Widget>(transform, NiPoint3(1.0, 0.4, 0.5));
		tab->AddModel(chapters[a_index]->model_path);
		chapters[a_index]->tab = tab;
	}

	void Book::DrawCurrentPage()
	{
		if (chapter_index >= chapters.size() || !chapters[chapter_index]) { return; }

		auto& pages = chapters[chapter_index]->pages;
		if (page_index >= pages.size() || !pages[page_index]) { return; }

		PageContext context{ layout, *left_page_parent, *right_page_parent };

		pages[page_index]->Draw(context);

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

	void Page::Draw(Book::PageContext a_context)
	{
		a_context.left_page.AddModel("HelperSphere.nif");
		a_context.right_page.AddModel("HelperSphere.nif");
	}

	void Book::Update(float a_delta)
	{
		Widget::Update(a_delta);
		animator.Update(Get3D(), a_delta);
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
