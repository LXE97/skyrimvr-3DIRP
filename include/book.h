#pragma once

#include "exclusive_hover_group.h"
#include "ni_animator.h"
#include "vr_gui.h"
#include "vr_gui_input_block.h"

#include <concepts>
#include <utility>

namespace vr3dirp
{
	using namespace RE;
	using namespace vr_gui;

	class Book;
	class Page;
	class Chapter;
	class GrabNode;
	class BasicHitbox;
	class SelectionHighlight;

	struct BookSettings
	{
		float light_fade = 1.f;
		float page_brightness = 0.5f;
		float font_size = 1.f;
		float book_scale = 1.0f;
		float rightpage_text_z_offset = 1.25f;
		float leftpage_text_z_offset = 0.03f;
		float rightpage_text_z_offset_righthand = 1.25f;
		float leftpage_text_z_offset_righthand = 0.03f;
		float horizontal_margin = 0.9f;
		float top_margin = 1.0f;
		int   close_button = 1;
		int   close_action = 1;
		float close_timing = 1.0f;
		float floating_despawn_distance = 500.0f;

		float right_offset_x = 8.862305f;
		float right_offset_y = 4.031250f;
		float right_offset_z = 12.304688f;
		float right_rotate_w = 0.569704532f;
		float right_rotate_x = 0.552828061f;
		float right_rotate_y = 0.424522750f;
		float right_rotate_z = 0.435428886f;

		float left_offset_x = 10.912109f;
		float left_offset_y = -11.599609f;
		float left_offset_z = 6.068359f;
		float left_rotate_w = 0.655093787f;
		float left_rotate_x = 0.717480487f;
		float left_rotate_y = -0.110829458f;
		float left_rotate_z = -0.209262307f;
	};

	struct BookCallbacks
	{
		std::function<void(bool, const NiTransform&)> transform_changed;
	};

	class BookSettingsOwner
	{
	protected:
		BookSettingsOwner(BookSettings a_settings, BookCallbacks a_callbacks);

		BookSettings  settings;
		BookCallbacks callbacks;
	};

	class Book : private BookSettingsOwner, public Widget
	{
		friend class GrabNode;

	public:
		Book(std::string_view a_model_path, vrinput::Hand a_hand, TESObjectREFR* a_objectReference,
			NiAVObject* a_root, BookSettings a_settings, BookCallbacks a_callbacks = {},
			std::optional<float> a_scale_override = std::nullopt);
		~Book() override;

		struct Layout
		{
			NiPoint3 left_page_origin = { -3.8, 0.0, 0 };
			NiPoint3 right_page_origin = { 8.6, -0.2, 0 };
			NiPoint3 tab_origin = { 0.0f, 9.5f, 1.1f };
			float tab_scale = 1.f;

			static constexpr float kRightPageWidth = 16.3;
			static constexpr float kLeftPageWidth = 6.94 * 2;
			static constexpr float kRightPageHeight = 11.9 * 2;
			static constexpr float kLeftPageHeight = 11.69 * 2;

			float tab_spacing = 1.8f;
			float bottom_margin = 0.0f;
			float top_margin = 1.0f;
			float horizontal_margin = 0.9f;
			float body_text_scale = 1.0f;
			float heading_text_scale = 1.2f;

			static constexpr float body_character_spacing = 0.0f;
			static constexpr float page_number_character_spacing = -0.15f;

			float quest_line_spacing = 0.6f;
			float objective_spacing =
				0.2f + art_addon::AddonTextBox::kLineSpacing * body_text_scale;

			std::string_view body_font_model_path = "3DIRP/vr_gui/char_2048.nif";
		};

		struct PageContext
		{
			const Layout& layout;
			Widget&       left_page;
			Widget&       right_page;
			int           page_index;
			int           chapter_index;
			int           num_pages;
		};

		static constexpr std::string_view kModelPath =
			"clutter/books/book02/character assets/spelltomealteration.nif";
		static constexpr const char* kDefaultParent = "Book Pages Nub";
		static constexpr const char* kChapterParent = "Book Pages Nub";
		static constexpr const char* kRightParent = "Book Pages";
		static constexpr const char* kLeftParent = "Book CoverPage Turn04";
		static constexpr float       kDefaultWindowRadius = 40.f;

		static constexpr const char* kBookOpenSd = "ITMBookOpenSD";
		static constexpr const char* kBookCloseSd = "ITMBookCloseSD";
		static constexpr const char* kBookFlipLeftSnd = "ITMBookPageTurnForwardSD";
		static constexpr const char* kBookFlipRightSnd = "ITMBookPageTurnBackwardSD";

		void AddChapter(std::unique_ptr<Chapter> a_chapter);

		// Advance through pages
		void TurnPageLeft();
		// Reverse through pages
		void TurnPageRight();

		void TurnToChapter(int a_index);

		std::size_t GetChapterIndex() const { return chapter_index; }
		std::size_t GetPageIndex() const { return page_index; }

		void Close();

		void Update(float a_delta) override;
		void OnHover(bool a_activate, Hand& a_hand) override;

		void VirtualParent(NiAVObject* a_new, NiTransform& a_offset);

		void DrawExtents(bool show);

		bool HandStateFilter(Hand& a_hand) const override
		{
			return hand == vrinput::Hand::kBoth || a_hand.IsLeft() != isLeft;
		}

		bool IsAnimating() const { return animator.IsBusy(); }

	protected:
		enum class TurnDirection
		{
			kLeft,
			kRight
		};

		static constexpr ni_animator::AnimationRange open{ 0.0f, 0.98f, 0.98f };
		static constexpr ni_animator::AnimationRange close{ 3.f, 3.97f, 0.0f };
		static constexpr ni_animator::AnimationRange flip_left{ 0.98f, 2.f, 0.98f };
		static constexpr ni_animator::AnimationRange flip_right{ 2.f, 3.f, 0.98f };

		std::vector<std::unique_ptr<Chapter>> chapters;
		std::size_t                           chapter_index{};
		std::size_t                           page_index{};
		Widget*                               left_page_parent{};
		Widget*                               right_page_parent{};

		void AddChapterTab(std::size_t a_index);
		void TurnToPage(
			std::size_t a_chapter_index, std::size_t a_page_index, TurnDirection a_direction);

		void DrawPage(int a_chapter, int a_page, bool a_left_page);

		void ClearPageView(bool a_left);
		void RestoreSpellVisual();
		void InitializeHandState();

		GrabNode* grab_node;

		ni_animator::NiAnimator animator{};

		vrinput::Hand hand;
		bool          isFloating;
		bool          isClosing{};
		bool          usesLeftLayout;
		bool          isLeft;
		Layout        layout;

		float animation_speed = 2;

		Widget* tab_container{};

		InputBlockHandle block_handle;

		bool                 secondary_pressed_during_creation{};
		std::optional<float> secondary_press_elapsed{};
		float                secondary_double_tap_threshold = 1.0f;

		RE::SpellItem* stored_spell{};

		ModeHandle hand_mode;

		RE::NiPointer<RE::NiPointLight> book_light;

		NiAVObject* follow_node{};
	};

	class Page
	{
	public:
		virtual ~Page() = default;
		virtual void OnSelected() {}
		virtual void Draw(Book::PageContext a_context, bool a_left_page);
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
		virtual void OnSelected(const Book::Layout& layout) {MakePages(layout);}

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

		void Release();

		bool        isGrabbed = false;
		bool        isGrabButtonHeld = false;
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
		using Widget::Widget;

		virtual bool TestOverlap(Hand& a_hand) const override;
	};

	class BookPageTurn : public Behavior
	{
	public:
		using Behavior::Behavior;

		void OnClick(bool a_activate, Hand& a_hand, MenuAction a_action) override
		{
			if (auto book = dynamic_cast<Book*>(parent->GetRoot()); book && a_activate)
			{
				switch (a_action)
				{
				case MenuAction::kScrollLeft:
					book->TurnPageLeft();
					break;
				case MenuAction::kScrollRight:
					book->TurnPageRight();
					break;
				case MenuAction::kScrollUp:
					book->TurnToChapter((int)book->GetChapterIndex() - 1);
					break;
				case MenuAction::kScrollDown:
					book->TurnToChapter((int)book->GetChapterIndex() + 1);
				}
			}
		}
	};

	class HandInteractionMode : public Behavior
	{
	public:
		HandInteractionMode(Widget* a_parent) : Behavior(a_parent) {}

		void OnHover(bool a_activate, Hand& a_hand) override
		{
			hover_hand = std::addressof(a_hand);
			pending_active = a_activate;
			transition_elapsed = 0.0f;
		}

		void Update(float a_delta) override
		{
			if (!hover_hand || pending_active == pointing_active) { return; }

			transition_elapsed += a_delta;
			if (transition_elapsed < kTransitionDelay) { return; }

			auto* pc = RE::PlayerCharacter::GetSingleton();
			if (pending_active)
			{
				hand_mode =
					hover_hand->RequestMode(Hand::Mode::kPointing, Hand::ModePriority::kPassive);

				if (auto* equipped = pc->GetEquippedObject(hover_hand->IsLeft());
					pc->IsWeaponDrawn() && equipped && equipped->As<RE::SpellItem>())
				{
					pc->DrawWeaponMagicHands(false);
					force_sheathed = true;
				}

				smoothing_handle_l = Controller::GetSingleton()->GetHand(true)->RequestSmoothing();
				smoothing_handle_r = Controller::GetSingleton()->GetHand(false)->RequestSmoothing();
			}
			else
			{
				hand_mode.Release();
				if (auto* equipped = pc->GetEquippedObject(hover_hand->IsLeft());
					equipped && equipped->As<RE::SpellItem>() && force_sheathed)
				{
					pc->DrawWeaponMagicHands(true);
					force_sheathed = false;
				}

				smoothing_handle_l.Release();
				smoothing_handle_r.Release();
			}

			pointing_active = pending_active;
			transition_elapsed = 0.0f;
		}

	private:
		static constexpr float kTransitionDelay = 0.2f;

		ModeHandle      hand_mode;
		SmoothingHandle smoothing_handle_l;
		SmoothingHandle smoothing_handle_r;
		Hand*           hover_hand{};
		bool            force_sheathed{ false };
		bool            pending_active{ false };
		bool            pointing_active{ false };
		float           transition_elapsed{};
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

			if (a_activate)
			{
				handle = InputBlockManager::GetSingleton()->Acquire(a_hand.IsLeft(), blocks);
			}
			else
			{
				handle.Release();
			}
		}

	private:
		InputBlock                      blocks;
		std::array<InputBlockHandle, 2> handles;
	};

	class SelectionHighlight : public Behavior
	{
	public:
		using OnHighlight = std::function<void(bool)>;
		using OnActivate = std::function<void()>;
		using IsSelected = std::function<bool()>;
		using CanInteract = std::function<bool()>;

		SelectionHighlight(Widget* a_parent, ExclusiveHoverGroup* a_hover_group,
			OnHighlight a_on_highlight, OnActivate a_on_activate = {},
			IsSelected a_is_selected = {}, CanInteract a_can_interact = {}) :
			Behavior(a_parent),
			hover_group(a_hover_group),
			is_selected(std::move(a_is_selected)),
			on_activate(std::move(a_on_activate)),
			on_highlight(std::move(a_on_highlight)),
			can_interact(std::move(a_can_interact))
		{}

		void Update(float a_delta) override;
		void OnClick(bool a_activate, Hand& a_hand, MenuAction a_action) override;

	private:
		ExclusiveHoverGroup* hover_group{};
		bool                 highlighted{};
		IsSelected           is_selected{};
		OnActivate           on_activate{};
		OnHighlight          on_highlight{};
		CanInteract          can_interact{};
	};

}
