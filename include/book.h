#pragma once

#include "ni_animator.h"
#include "vr_gui.h"
#include "vr_gui_input_block.h"

#include <concepts>
#include <utility>

namespace vr3dui
{
	using namespace RE;
	using namespace vr_gui;

	class Book;
	class Page;
	class Chapter;
	class GrabNode;
	class BasicHitbox;

	class Book : public Widget
	{
	public:
		Book(bool a_isLeft, TESObjectREFR* a_objectReference, NiAVObject* a_root,
			NiTransform a_local);

		struct Layout
		{
			const NiPoint3 kLeftPageOrigin = { 3, 10.4, 0.02 };
			const NiPoint3 kRightPageOrigin = { 9.2, 0.0, 1.21 };
			const NiPoint3 kTabOffset = { 0.0f, 9.5f, 1.1f };

			static constexpr float kRightPageWidth = 15.5;
			static constexpr float kLeftPageWidth = 13.7;
			static constexpr float kPageHeight = 10.4 * 2;

			float tab_spacing = 1.8f;
			float top_margin = 0.0f;
			float bottom_margin = 0.0f;
			float horizontal_margin = 2.0f;

			float quest_line_spacing = 1.5f;
			float body_text_scale = 1.5f;
			float heading_text_scale = 2.0f;
			float body_character_spacing = -0.1f;
			float page_number_character_spacing = -0.6f;

			std::string_view body_font_model_path = "3DIRP/char_2048.nif";
			std::string_view page_number_font_model_path = "3DIRP/char_2048.nif";
		};

		struct PageContext
		{
			const Layout& layout;
			Widget&       left_page;
			Widget&       right_page;
		};

		static constexpr std::string_view kModelPath = "3DIRP/custom.nif";
		static constexpr const char*      kDefaultParent = "Book Pages Nub";
		static constexpr const char*      kChapterParent = "Book Pages Nub";
		static constexpr const char*      kRightParent = "Book Pages";
		static constexpr const char*      kLeftParent = "Book CoverPage Turn04";
		static constexpr float            kDefaultWindowRadius = 40.f;

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
		static constexpr ni_animator::AnimationRange open{ 0.0f, 1.f, 1.f };
		static constexpr ni_animator::AnimationRange close{ 3.f, 3.97f, 0.0f };
		static constexpr ni_animator::AnimationRange flip_left{ 1.f, 2.f, 1.f };
		static constexpr ni_animator::AnimationRange flip_right{ 2.f, 3.f, 1.f };

		std::vector<std::unique_ptr<Chapter>> chapters;
		std::size_t                           chapter_index{};
		std::size_t                           page_index{};
		Widget*                               left_page_parent{};
		Widget*                               right_page_parent{};

		void AddChapterTab(std::size_t a_index);
		void DrawCurrentPage();

		void ClearPageView();

		GrabNode* grab_node;

		ni_animator::NiAnimator animator{};

		bool   isLeft;
		Layout layout;

		float animation_speed = 2;

		Widget* tab_container{};

		InputBlockHandle block_handle;
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

		std::vector<std::unique_ptr<Page>> pages;
		Widget*                            tab{};
		std::string                        model_path;
	};

	class GrabNode : public Widget
	{
	public:
		using Widget::Widget;

		bool HandStateFilter(Hand& a_hand) const override
		{
			if (parent->HandStateFilter(a_hand))
			{
				// only grab when palm is facing along this Y axis
				const NiPoint3  local_y{ 0.0f, 1.0f, 0.0f };
				const auto      hand_normal = a_hand.GetTransform().rotate * local_y;
				const auto      grab_normal = GetWorld().rotate * local_y;
				constexpr float kCos60Degrees = 0.5f;
				if (hand_normal.Dot(grab_normal) < -kCos60Degrees) { return true; }
			}
			return false;
		}

		void OnClick(bool a_activate, Hand& a_hand, MenuAction a_action) override;

		void OnHover(bool a_activate, Hand& a_hand) override;

		void Update(float delta) override;

	private:
		// TODO: make setting
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

	// widget for interaction volumes that only uses the Hand's radius for hit detection
	class BasicHitbox : public Widget
	{
	public:
		BasicHitbox(Widget* a_parent, bool a_isLeft, NiTransform a_local, NiPoint3 a_extents) :
			Widget(a_parent, a_local, a_extents),
			isLeft(a_isLeft)
		{}

		void OnClick(bool a_activate, Hand& a_hand, MenuAction a_action) override;

		virtual bool TestOverlap(Hand& a_hand) const override;

	private:
		bool isLeft;
	};

	class HandPointing : public Behavior
	{
	public:
		HandPointing(Widget* a_parent) : Behavior(a_parent) {}

		void OnHover(bool a_activate, Hand& a_hand) override
		{
			if (a_activate)
			{
				hand_mode = a_hand.RequestMode(Hand::Mode::kPointing, Hand::ModePriority::kPassive);
			}
			else
			{
				hand_mode.Release();
			}
		}

	private:
		ModeHandle hand_mode;
	};

	class BlockInputOnHover : public Behavior
	{
	public:
		BlockInputOnHover(Widget* a_parent, InputBlock a_blocks) :
			Behavior(a_parent),
			blocks(a_blocks)
		{}

		void OnHover(bool a_activate, Hand& a_hand) override
		{
			auto& handle = handles[a_hand.IsLeft()];

			if (a_activate) { handle = InputBlockManager::GetSingleton()->Acquire(a_hand.IsLeft(), blocks); }
			else
			{
				handle.Release();
			}
		}

	private:
		InputBlock                  blocks;
		std::array<InputBlockHandle, 2> handles;
	};

}
