#pragma once

namespace equipment_checker
{
	bool IsHandEmpty(bool a_isLeft, bool a_allow_empty_arrow_hand = true);
	
	bool HasEquippedAmmo(RE::PlayerCharacter* a_player);

	void OnEquipEvent(const RE::TESEquipEvent* a_event);
}
