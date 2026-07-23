#pragma once

#include "exclusive_hover_group.h"
#include "ni_animator.h"
#include "vr_gui.h"

#include <concepts>
#include <utility>

namespace vr3dui
{
	using namespace RE;
	using namespace vr_gui;

	class Book;
	class ChapterTab;
	class Page;
	class Chapter;
	class GrabNode;
	class BasicHitob;

	Window* SummonFloatingBook(bool isLeft);

	void DismissBook(Window* a_book_window);

	class ChapterTab : public Widget, ExclusiveHoverItem
	{
	public:
		ChapterTab(Widget* a_parent, NiTransform a_local, NiPoint3 a_halfExtents,
			std::size_t a_chapterIndex, ExclusiveHoverGroup* a_group) :
			Widget(a_parent, std::move(a_local), a_halfExtents),
			ExclusiveHoverItem(a_group),
			chapter_index(a_chapterIndex)
		{}

		void OnClick(bool a_activate, Hand& a_hand, MenuAction a_action) override;

		void OnHover(bool a_activate, Hand& a_hand) override;
		void OnHoverExclusive(bool a_activate, Hand& a_hand) override;

		void OnSelected(bool a_selected);

		virtual void UpdateSelectionVisual();

		bool HandStateFilter(Hand& a_hand) const override
		{ return parent && parent->HandStateFilter(a_hand); }

	private:
		std::size_t chapter_index{};
		bool        selected{ false };
	};

	class Book : public Widget
	{
	public:
		Book(Widget* a_parent, std::string a_model_path, bool a_isLeft, NiTransform a_transform);

		struct Layout
		{
			const NiPoint3 kLeftPageOrigin = { 3, 10.4, 0.01 };
			const NiPoint3 kRightPageOrigin = { 0, 10.4, 0.01 };
			const NiPoint3 kRightPageParentOffset = { 9.0, 0.0, 1.21 };

			static constexpr float kRightPageWidth = 15.5;
			static constexpr float kLeftPageWidth = 13.7;
			static constexpr float kPageHeight = 10.4 * 2;

			const NiPoint3 kTabOffset = { 0.0f, 9.5f, 0.0f };

			float tab_spacing = 1.8f;
			float top_margin = 0.0f;
			float bottom_margin = 0.0f;
			float horizontal_margin = 2.0f;

			float quest_line_spacing = 1.5f;
			float body_text_scale = 2.0f;
			float heading_text_scale = 1.0f;
			float body_character_spacing = -0.1f;
			float page_number_character_spacing = -0.6f;

			std::string_view body_font_model_path = "3DIRP/char_2048.nif";
			std::string_view page_number_font_model_path = "3DIRP/char_2048.nif";
		};

		struct PageContext
		{
			const Layout& layout;
			ExclusiveClickContainer&       left_page;
			ExclusiveClickContainer&       right_page;
		};

		static constexpr int   kChapterTabGroupPriority = 10;
		static constexpr int   kPageGroupPriority = 15;
		static constexpr float kDefaultWindowRadius = 40.f;

		void AddChapter(std::unique_ptr<Chapter> a_chapter);

		// Advance through pages
		void TurnPageLeft();
		// Reverse through pages
		void TurnPageRight();

		void TurnToChapter(std::size_t a_index);

		std::size_t GetChapterIndex() const { return chapter_index; }
		std::size_t GetPageIndex() const { return page_index; }

		void Close();

		void Update(float a_delta) override;

		void VirtualParent(NiAVObject* a_new, NiTransform& a_offset);

		void DrawExtents(bool show);

		bool HandStateFilter(Hand& a_hand) const override { return a_hand.IsLeft() != isLeft; }

	protected:
		static constexpr const char* kLeftPageNodeName = "Book CoverPage Turn04";
		static constexpr const char* kRightPageNodeName = "Book Pages";
		static constexpr const char* kChapterTabModel = "ChapterTab.nif";

		static constexpr ni_animator::AnimationRange open{ 0.0f, 1.f, 1.f };
		static constexpr ni_animator::AnimationRange close{ 3.f, 3.97f, 0.0f };
		static constexpr ni_animator::AnimationRange flip_left{ 1.f, 2.f, 1.f };
		static constexpr ni_animator::AnimationRange flip_right{ 2.f, 3.f, 1.f };

		std::vector<std::unique_ptr<Chapter>>    chapters;
		std::size_t                              chapter_index{};
		std::size_t                              page_index{};
		std::unique_ptr<art_addon::AddonTextBox> page_numbers;

		ExclusiveClickContainer* left_page_parent{};
		ExclusiveClickContainer* right_page_parent{};

		void AddChapterTab(std::size_t a_index);
		void DrawCurrentPage();

		void ClearPageView();

		GrabNode* grab_node;

		ni_animator::NiAnimator animator{};

		bool   isLeft;
		Layout layout;

		float animation_speed = 2;

		ExclusiveClickContainer* tab_container{};
	};

	class Page
	{
	public:
		virtual ~Page() = default;
		virtual void Draw(Book::PageContext a_context);
	};

	class Chapter
	{
	public:
		Chapter(std::string a_model_path) : model_path(std::move(a_model_path)) {}
		virtual ~Chapter() = default;

		virtual void MakePages(const Book::Layout&)
		{
			// for testing
			pages.emplace_back(std::make_unique<Page>());
			pages.emplace_back(std::make_unique<Page>());
		};

		virtual void OnSelected(bool a_selected)
		{
			if (tab) { tab->OnSelected(a_selected); }
		}

		std::vector<std::unique_ptr<Page>> pages;
		ChapterTab*                        tab{};
		std::string                        model_path;
	};

	class GrabNode : public Widget
	{
	public:
		GrabNode(Widget* a_parent, bool isLeft, NiTransform a_local) :
			isLeft(isLeft),
			Widget(a_parent, a_local, NiPoint3(4, 4, 4))
		{}

		bool HandStateFilter(Hand& a_hand) const override
		{
			if (a_hand.IsLeft() != isLeft)
			{
				// only grab when palm is facing along this Y axis
				const NiPoint3  local_y{ 0.0f, 1.0f, 0.0f };
				const auto      hand_normal = a_hand.GetTransform().rotate * local_y;
				const auto      grab_normal = GetWorld().rotate * local_y;
				constexpr float kCos45Degrees = 0.70710678f;
				if (hand_normal.Dot(grab_normal) < -kCos45Degrees) { return true; }
			}
			return false;
		}

		void OnClick(bool a_activate, Hand& a_hand, MenuAction a_action) override;

		void OnHover(bool a_activate, Hand& a_hand) override;

		void Update(float delta) override;

	private:
		static constexpr float kGrabHoldTime = 1.0f;

		bool        isGrabbed = false;
		bool        isGrabButtonHeld = false;
		bool        isLeft = false;
		float       grabHoldTime = 0.0f;
		Hand*       grabHand{};
		NiAVObject* follow_target{};
		NiTransform parent_store{};

		ModeHandle hand_mode;
	};

	class HandPointing : public Behavior
	{
	public:
		HandPointing(Widget* a_parent, bool isLeft) : Behavior(a_parent), isLeft(isLeft) {}

		void OnHover(bool a_activate, Hand& a_hand) override
		{
			if (a_hand.IsLeft() == isLeft)
			{
				if (a_activate)
				{
					hand_mode =
						a_hand.RequestMode(Hand::Mode::kPointing, Hand::ModePriority::kPassive);
				}
				else
				{
					hand_mode.Release();
				}
			}
		}

	private:
		bool       isLeft;
		ModeHandle hand_mode;
	};

	// widget for interaction volumes that only uses the Hand's radius for hit detection
	class BasicHitbox : public Widget
	{
	public:
		BasicHitbox(Widget* a_parent, bool a_isLeft, NiTransform a_local, NiPoint3 a_extents) :
			Widget(a_parent, a_local, a_extents),
			isLeft(a_isLeft)
		{}

		bool HandStateFilter(Hand& a_hand) const override
		{ return parent && parent->HandStateFilter(a_hand); }

		void OnClick(bool a_activate, Hand& a_hand, MenuAction a_action) override;

		virtual bool TestOverlap(Hand& a_hand) const override;

	private:
		bool isLeft;
	};

	template <class T, class... Args>
		requires std::derived_from<T, Book> &&
		std::constructible_from<T, Widget*, Args..., bool, NiTransform>
	Window* SummonAttachedBook(bool a_isLeft, Args&&... a_args)
	{
		NiTransform transform;
		transform.scale = 1.0f;
		transform.translate = { 5.915527f, -10.583008f, 10.284607f };
		transform.rotate = { { 0.839558f, -0.198012f, -0.493753f },
			{ -0.528744f, -0.127261f, -0.825465f }, { 0.089592f, 0.956704f, -0.210660f } };

		auto* window = Controller::GetSingleton()->AddWindow(std::make_unique<AttachedWindow>(
			Book::kDefaultWindowRadius, PlayerCharacter::GetSingleton(),
			vrinput::GetHandNode(vrinput::Hand(a_isLeft), false)->AsNode(), transform));

		NiTransform book_transform;
		book_transform.scale = 0.9f;
		window->AddChild<T>(std::forward<Args>(a_args)..., a_isLeft, book_transform);

		return window;
	}



}
