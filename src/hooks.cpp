#include "hooks.h"
#include "art_addon.h"

namespace hooks
{

	void ModelReferenceEffect_SaveGameHook::Install()
	{
		REL::Relocation<std::uintptr_t> vtbl{ RE::ModelReferenceEffect::VTABLE[0] };
		_original = vtbl.write_vfunc(0x2D, Hook);
	}

	void ModelReferenceEffect_SaveGameHook::Hook(
		RE::ModelReferenceEffect* a_this, RE::BGSSaveGameBuffer* a_buf)
	{
		if (a_this)
		{
			if (auto manager = art_addon::ArtAddonManager::GetSingleton())
			{
				if (manager->IsTempArtObject(a_this->artObject))

					a_this->artObject = manager->GetBaseObject();
			}
		}

		_original(a_this, a_buf);
	}
}
