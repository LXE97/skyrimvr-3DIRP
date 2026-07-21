#include "ni_animator.h"
#include "vr_gui.h"
#include "book.h"

namespace spellbook
{
	using namespace RE;
	using namespace vr_gui;
	using namespace vr3dui;

	class Spellbook;

	class Spellbook : public Book
	{
	public:

		void DisplaySpellInfo(std::string font, float z_offset = -0.03f);


	private:
		struct Layout
		{
			int   items_per_row = 5;
			float padding = 1;
			float page_width = 15;
			float page_height = 20;

			bool use_dividers;
		};

		std::unique_ptr<art_addon::AddonTextBox> description;
		std::unique_ptr<art_addon::AddonTextBox> spell_cost;
		std::unique_ptr<art_addon::AddonTextBox> spell_magnitude;
		std::unique_ptr<art_addon::AddonTextBox> spell_duration;
		GridContainer*                           spell_icon_container;

		ni_animator::NiAnimator animator{};

		bool     isLeft;
		int      page_index = 0;
		Layout   layout;

		float animation_speed = 3;
	};

}