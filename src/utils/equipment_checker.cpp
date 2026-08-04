#include "equipment_checker.h"

namespace equipment_checker
{

	bool g_equipment_dirty = true;
	bool g_has_equipped_ammo = false;

	bool HasEquippedAmmo(RE::PlayerCharacter* a_player)
	{
		if (!g_equipment_dirty) { return g_has_equipped_ammo; }

		g_has_equipped_ammo = false;
		if (auto* inventory_changes = a_player->GetInventoryChanges(false);
			inventory_changes && inventory_changes->entryList)
		{
			for (auto* entry : *inventory_changes->entryList)
			{
				if (entry && entry->object && entry->object->IsAmmo() && entry->IsWorn())
				{
					g_has_equipped_ammo = true;
					break;
				}
			}
		}

		g_equipment_dirty = false;
		return g_has_equipped_ammo;
	}

	void OnEquipEvent(const RE::TESEquipEvent* a_event)
	{
		if (!a_event || a_event->actor.get() != RE::PlayerCharacter::GetSingleton()) { return; }

		g_equipment_dirty = true;
	}

	bool IsHandEmpty(bool a_isLeft, bool a_allow_empty_arrow_hand, bool a_left_hand_mode)
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player) { return true; }

		if (!player->IsWeaponDrawn()) { return true; }

		const bool equipment_left = a_isLeft != a_left_hand_mode;
		auto*      equipped = player->GetEquippedObject(equipment_left);
		if (equipped && equipped->As<RE::SpellItem>()) { return true; }

		auto* main_hand = player->GetEquippedObject(false);
		auto* main_hand_weapon = main_hand ? main_hand->As<RE::TESObjectWEAP>() : nullptr;

		if (main_hand_weapon)
		{
			const bool is_bow = main_hand_weapon->IsBow();
			const bool is_two_handed = is_bow || main_hand_weapon->IsCrossbow() ||
				main_hand_weapon->IsTwoHandedSword() || main_hand_weapon->IsTwoHandedAxe();

			if (is_two_handed &&
				((is_bow && !equipment_left &&
					 (!HasEquippedAmmo(player) && a_allow_empty_arrow_hand)) ||
					(!is_bow && equipment_left)))
			{
				return true;
			}
		}

		if (!equipped)
		{
			auto*      left_equipped = player->GetEquippedObject(true);
			auto*      armor = left_equipped ? left_equipped->As<RE::TESObjectARMO>() : nullptr;
			const bool has_shield = armor && armor->IsShield();
			return !has_shield || !equipment_left;
		}

		return false;
	}

}
