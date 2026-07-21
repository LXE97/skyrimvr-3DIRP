#include "ni_animator.h"
#include "vr_gui.h"

namespace vr3dui
{
	using namespace RE;
	using namespace vr_gui;

	class Book;
	class GrabNode;
	class BasicHitob;

	Window* SummonAttachedBook(bool isLeft);
	Window* SummonFloatingBook(bool isLeft);

	void DismissBook(Window* a_book_window);

	class Book : public Widget
	{
	public:
		Book(Widget* a_parent, std::string a_model_path, bool a_isLeft, NiTransform a_transform);

		bool HandStateFilter(Hand& a_hand) const override { return a_hand.IsLeft() != isLeft; }

		static constexpr float kDefaultWindowRadius = 40.f;

		static constexpr ni_animator::AnimationRange open{ 0.0f, 1.f, 1.f };
		static constexpr ni_animator::AnimationRange close{ 3.f, 3.97f, 0.0f };
		static constexpr ni_animator::AnimationRange flip_left{ 1.f, 2.f, 1.f };
		static constexpr ni_animator::AnimationRange flip_right{ 2.f, 3.f, 1.f };

		void Update(float a_delta) override;

		void VirtualParent(NiAVObject* a_new, NiTransform& a_offset);

		void TurnPage(bool a_direction_left);

		void Close();

		void DrawExtents(bool show);

	protected:
		struct Layout
		{
			int   items_per_row = 5;
			float padding = 1;
			float page_width = 15;
			float page_height = 20;
		};

		static constexpr const char* kLeftPageNodeName = "Book CoverPage Turn04";
		static constexpr const char* kRightPageNodeName = "Book TurnPage2";

		GrabNode* grab_node;

		ni_animator::NiAnimator animator{};

		bool   isLeft;
		int    page_index = 0;
		Layout layout;

		float animation_speed = 3;
	};

	class GrabNode : public Widget
	{
	public:
		GrabNode(Widget* a_parent, bool isLeft, NiTransform a_local);

		bool HandStateFilter(Hand& a_hand) const override
		{
			if (a_hand.IsLeft() != isLeft)
			{
				// only grab when palm is facing along this Y axis
				const NiPoint3 local_y{ 0.0f, 1.0f, 0.0f };
				const auto     hand_normal = a_hand.GetTransform().rotate * local_y;
				const auto     grab_normal = GetWorld().rotate * local_y;
				if (hand_normal.Dot(grab_normal) < 0.0f) { return true; }
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
		{ priority = 5; }

		bool HandStateFilter(Hand& a_hand) const override { return a_hand.IsLeft() != isLeft; }

		void OnClick(bool a_activate, Hand& a_hand, MenuAction a_action) override;

		virtual bool TestOverlap(Hand& a_hand) const override;

	private:
		bool isLeft;
	};

}
