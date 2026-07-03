#include "vr_gui.h"

namespace spellbook
{
	using namespace RE;
	using namespace vr_gui;

	class Spellbook;
	class GrabNode;

	Spellbook* Summon(bool isLeft);
	void       Dismiss(Spellbook* a_spbk);

	const RE::NiTransform default_spellbook();

	class Spellbook : public AttachedWindow
	{
	public:
		struct Anim
		{
			float start;
			float end;
			float steady;
		};

		enum class Categories
		{
			kAll,
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

		static constexpr Anim open{ 0.0f, 0.95f, 0.95f };
		static constexpr Anim close{ 2.97f, 3.971f, 0.0f };
		static constexpr Anim flip_left{ 0.95f, 1.96f, 1.96f };
		static constexpr Anim flip_right{ 1.96f, 2.97f, 2.97f };

		bool HandStateFilter(Hand& a_hand) const override;

		void OnClick(bool a_activate, Hand& a_hand, MenuAction a_action) override;

		void VirtualParent(NiAVObject* a_new, NiTransform& a_offset);

	private:
		static constexpr float kDefaultWindowRadius = 15.f;

		bool                                       isLeft;
		std::unordered_map<Categories, SpellItem*> spell_pages;

		GridContainer* spell_icon_container;
		GrabNode*      grab_node;

		std::unique_ptr<art_addon::AddonTextBox> description;
		std::unique_ptr<art_addon::AddonTextBox> spell_cost;
		std::unique_ptr<art_addon::AddonTextBox> spell_magnitude;
		std::unique_ptr<art_addon::AddonTextBox> spell_duration;

		void BuildPages();
		void AttachButtons();
		void SetupGrid();
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
	};

	class SpellIcon : public Widget
	{};

	class CategoryTab : public Widget
	{};

	class BookAnimator : public Behavior
	{
	public:
		BookAnimator(Widget* a_parent, const Spellbook::Anim& a_keyframes, float a_speed = 1.f) :

			Behavior(a_parent),
			keyframes(a_keyframes),
			accumulate(a_keyframes.start),
            speed(a_speed)
		{}

		void Update(float delta) override;
		void OnDetach() override;

	private:
		const Spellbook::Anim& keyframes;
		float                  accumulate{};
        float speed{};
	};

	class HandPointing : public Behavior
	{
	public:
		HandPointing(Widget* a_parent, bool isLeft) : Behavior(a_parent), isLeft(isLeft) {}

		~HandPointing() { Controller::GetSingleton()->GetHand(isLeft)->RestoreMode(); }

		void OnHover(bool a_activate, Hand& a_hand) override
		{
			if (a_activate) { a_hand.SetMode(); }
			else
			{
				a_hand.RestoreMode();
			}
		}

	private:
		bool isLeft;
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