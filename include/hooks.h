#pragma once
#include "hooks.h"


namespace hooks
{

	class ModelReferenceEffect_SaveGameHook
	{
	public:
		static void Install();

	private:
		static void Hook(RE::ModelReferenceEffect* a_this, RE::BGSSaveGameBuffer* a_buf);

		static inline REL::Relocation<decltype(Hook)> _original;
	};
}