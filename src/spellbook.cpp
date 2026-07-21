#include "spellbook.h"

namespace spellbook
{
	using namespace art_addon;

	void Spellbook::DisplaySpellInfo(std::string font, float z_offset)
	{
		NiTransform t{};
		t.translate = { 2, 10, z_offset };
		t.rotate.SetEulerAnglesXYZ(0, 3.141593, 3.141593);
		if (auto node = model->Get3D()->GetObjectByName(kLeftPageNodeName))
		{
			SKSE::log::trace("drawing text");
			description = std::make_unique<AddonTextBox>(
				"Accusamus magnam neque est libero. Ipsam quia aut in \n"
				"recusandae assumenda consequatur illo. Muae nostrum\n"
				"praesentium illo id magni.\n Dolorem et similique saepe ut\n"
				"voluptatum exercitationem sit.aut et culpa quidem."
				"Inventore alias at quidem dolorem\n aut et culpa quidem.\n"
				"Ut non est dicta. A qui molestiae sit reprehenderit voluptatem\n"
				"at mollitia. Werum possimus\n consequuntur architecto. Officia\n"
				"ipsum soluta cum suscipit.\n\n"
				"Hic ex sed cupiditate voluptatum.\n Earum officia eaque quis in\n"
				"perferendis et dolorem sint.\n Et vero temporibus sed. Wpsum\n"
				"ea ex blanditiis totam \ndoloremque similique.\n"
				"at mollitia. Werum possimus consequuntur architecto. Officia\n"
				"Hic ex sed cupiditate voluptatum. Earum officia eaque quis",

				-0.2f, node, t, font);
		}
		else
		{
			SKSE::log::trace("node not found");
		}
	}

}