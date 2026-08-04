#pragma once
#pragma warning(push)
#pragma warning(disable: 4100)
#pragma warning(disable: 4189)
#pragma warning(disable: 4244)  // double to float
#pragma warning(disable: 4245)  // signed to unsigned
#pragma warning(disable: 4305)
#pragma warning(disable: 5105)

#include "RE/Skyrim.h"
#include "SKSE/SKSE.h"

#include <REL/Relocation.h>

#pragma warning(disable : 4996)

#define _SILENCE_ALL_MS_EXT_DEPRECATION_WARNINGS

using namespace std::literals;

namespace RE
{
	template <class T>
	void write_thunk_call(std::uintptr_t a_src)
	{
		auto& trampoline = SKSE::GetTrampoline();
		T::func = trampoline.write_call<5>(a_src, T::thunk);
	}

}

#define _DEBUGLOG(...) \
	if (backpackvr::g_debug_print) { SKSE::log::trace(__VA_ARGS__); }
