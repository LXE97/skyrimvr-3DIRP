#include "ni_animator.h"
#include "vr_gui.h"

namespace spellbook
{
	using namespace RE;
	using namespace vr_gui;

	class Spellbook;
	class GrabNode;
	class BasicHitbox;

	Spellbook* Summon(bool isLeft);
	void       Dismiss(Spellbook* a_spbk);

	const RE::NiTransform default_spellbook();

	class Spellbook : public AttachedWindow
	{
	public:
		enum class Category
		{
			kAll,
			kFavorites,
			kConjuration,
			kIllusion,
			kAlteration,
			kDestruction,
			kRestoration,
			kActiveEffects,
			kPowers,
			kShouts,
		};

		Spellbook(bool a_isLeft, NiTransform a_transform);

		static constexpr ni_animator::AnimationRange open{ 0.0f, 0.95f, 0.95f };
		static constexpr ni_animator::AnimationRange close{ 2.97f, 3.971f, 0.0f };
		static constexpr ni_animator::AnimationRange flip_left{ 0.95f, 1.96f, 0.95f };
		static constexpr ni_animator::AnimationRange flip_right{ 1.96f, 2.97f, 0.95f };

		void Update(float a_delta) override;

		void VirtualParent(NiAVObject* a_new, NiTransform& a_offset);

		void DisplaySpellInfo(std::string font, float z_offset = -0.03f);

		void TurnPage(bool a_direction_left);

		void Close();

	private:
		struct Layout
		{
			int   items_per_row = 5;
			float padding = 1;
			float page_width = 15;
			float page_height = 20;

			bool use_dividers;
		};

		static constexpr float       kDefaultWindowRadius = 15.f;
		static constexpr const char* kLeftPageNodeName = "Book CoverPage Turn04";
		static constexpr const char* kRightPageNodeName = "Book TurnPage2";

		void AttachButtons();
		void SetupGrid(Category a_category);

		std::unique_ptr<art_addon::AddonTextBox> description;
		std::unique_ptr<art_addon::AddonTextBox> spell_cost;
		std::unique_ptr<art_addon::AddonTextBox> spell_magnitude;
		std::unique_ptr<art_addon::AddonTextBox> spell_duration;
		GridContainer*                           spell_icon_container;
		GrabNode*                                grab_node;
		BasicHitbox*                             interaction_volume;

		ni_animator::NiAnimator animator{};

		bool     isLeft;
		int      page_index = 0;
		Category selected;
		Layout   layout;

		float animation_speed = 3;
	};

	class GrabNode : public Widget
	{
	public:
		GrabNode(Widget* a_parent, bool isLeft, NiPoint3 a_offset);

		void OnClick(bool a_activate, Hand& a_hand, MenuAction a_action) override;

		void OnHover(bool a_activate, Hand& a_hand) override;

		void Update(float delta) override;

	private:
		bool        isGrabbed = false;
		bool        isLeft = false;
		NiAVObject* follow_target{};
		NiPoint3    offset;
		NiTransform parent_store{};

		ModeHandle hand_mode;
	};

	class BasicHitbox : public Widget
	{
	public:
		BasicHitbox(Widget* a_parent, NiTransform a_local, NiPoint3 a_extents, float a_radius) :
			Widget(a_parent, a_local, a_extents, a_radius) {};

		void OnClick(bool a_activate, Hand& a_hand, MenuAction a_action) override;

		virtual bool TestOverlap(Hand& a_hand) const override;
	};

	class SpellIcon : public Widget
	{};

	class CategoryTab : public Widget
	{};

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

	class HoverExitVelocityTracker : public Behavior
	{
	public:
		HoverExitVelocityTracker(Widget* a_parent, bool isLeft) : Behavior(a_parent), isLeft(isLeft)
		{}

		void OnHover(bool a_activate, Hand& a_hand) override;

	private:
		std::vector<NiPoint3> pos_history;
		bool                  isLeft;
	};

}