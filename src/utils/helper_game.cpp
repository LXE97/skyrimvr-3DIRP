#include "helper_game.h"

#include "art_addon.h"
#include "helper_math.h"

#include <iomanip>
#include <limits>
#include <sstream>

namespace helper
{
	struct PlayerCharacter_Update
	{
		static void thunk(RE::PlayerCharacter* a_player, float a_delta)
		{
			func(a_player, a_delta);
			if (my_func) { my_func(); }
		}
		static inline REL::Relocation<decltype(thunk)> func;
		static inline std::function<void(void)>        my_func;
	};

	void InstallPlayerUpdateHook(std::function<void(void)> a_func)
	{
		PlayerCharacter_Update::my_func = a_func;
		RE::write_vfunc<RE::PlayerCharacter, 0xAF, PlayerCharacter_Update>();
	}

	TESForm* LookupByName(FormType a_typeEnum, const char* a_name)
	{
		auto* data = TESDataHandler::GetSingleton();
		auto& forms = data->GetFormArray(a_typeEnum);
		for (auto*& form : forms)
		{
			if (!strcmp(form->GetName(), a_name)) { return form; }
		}
		return nullptr;
	}

	RE::NiPointer<RE::NiPointLight> MakeLight(RE::TESObjectREFR* target, RE::NiNode* attach_node,
		const RE::NiTransform& local, float radius, float fade)
	{
		RE::NiPointer<RE::NiPointLight> result;
		if (!target || !attach_node)
		{
			SKSE::log::trace("cannot create light without a player and attach node");
			return result;
		}

		if (auto* lightForm = TESForm::LookupByEditorID<TESObjectLIGH>("MagicLightLightSpell01"))
		{
			RE::NiPointer<RE::NiLight> generatedLight{ lightForm->GenDynamic(
				target, attach_node, true, true, true) };

			if (!generatedLight)
			{
				SKSE::log::trace("MagicLightLightSpell01 failed to generate a light");
			}
			else if (auto* pointLight = netimmerse_cast<RE::NiPointLight*>(generatedLight.get()))
			{
				pointLight->local = local;
				pointLight->SetLightAttenuation(radius);
				auto& data = pointLight->GetLightRuntimeData();
				 data.ambient = { 0.1f, 0.08f, 0.05f };
				 data.diffuse = { 1.0f, 0.8f, 0.5f };
				data.fade = fade;
				RE::NiUpdateData ctx{};
				pointLight->Update(ctx);

				result.reset(pointLight);
			}
			else
			{
				if (auto* shadowScene = RE::DrawWorld::GetSingleton().mainShadowSceneNode)
				{
					shadowScene->RemoveLight(generatedLight.get());
				}
				if (auto* parent = generatedLight->parent)
				{
					parent->DetachChild(generatedLight.get());
				}
				SKSE::log::trace("MagicLightLightSpell01 did not generate a point light");
			}
		}
		else
		{
			SKSE::log::trace("light form not found");
		}

		return result;
	}

	void DestroyLight(RE::NiPointer<RE::NiPointLight>& runtimeLight)
	{
		if (auto* shadowScene = RE::DrawWorld::GetSingleton().mainShadowSceneNode)
		{
			shadowScene->RemoveLight(runtimeLight.get());
		}

		if (auto* parent = runtimeLight->parent) { parent->DetachChild(runtimeLight.get()); }

		runtimeLight.reset();
	}

	RE::FormID GetFullFormID(uint8_t a_modindex, RE::FormID a_localID)
	{ return (a_modindex << 24) | a_localID; }

	uint8_t GetFormIndex(RE::FormID a_formid) { return a_formid >> 24; }

	uint32_t GetLocalID(RE::FormID a_formid) { return a_formid & 0x00FFFFFF; }

	void HideActivationText(TESObjectREFR* a_target, bool a_hidden)
	{ a_target->extraList.SetExtraFlags(ExtraFlags::Flag::kBlockActivateText, a_hidden); }

	bool SetQuestTracked(RE::TESQuest* a_quest, bool a_tracked)
	{
		if (!a_quest) { return false; }

		auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
		auto* policy = vm ? vm->GetObjectHandlePolicy() : nullptr;
		if (!policy) { return false; }

		const auto handle = policy->GetHandleForObject(a_quest->GetFormType(), a_quest);
		if (handle == policy->EmptyHandle()) { return false; }

		RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
		return vm->DispatchMethodCall(handle, "Quest", "SetActive",
			RE::MakeFunctionArguments(static_cast<bool>(a_tracked)), callback);
	}

	float GetAVPercent(Actor* a_a, ActorValue a_v)
	{
		float current = a_a->AsActorValueOwner()->GetActorValue(a_v);
		float base = a_a->AsActorValueOwner()->GetBaseActorValue(a_v);
		float mod = a_a->GetActorValueModifier(ACTOR_VALUE_MODIFIER::kPermanent, a_v) +
			a_a->GetActorValueModifier(ACTOR_VALUE_MODIFIER::kTemporary, a_v);
		return current / (base + mod);
	}

	float GetChargePercent(Actor* a_a, bool isLeft)
	{
		if (auto equipped = a_a->GetEquippedObject(isLeft))
		{
			if (equipped->IsWeapon())
			{
				float current = a_a->AsActorValueOwner()->GetActorValue(
					isLeft ? ActorValue::kLeftItemCharge : ActorValue::kRightItemCharge);

				// player made items
				if (auto entryData = a_a->GetEquippedEntryData(isLeft))
				{
					for (auto& x : *(entryData->extraLists))
					{
						if (auto e = x->GetByType<ExtraEnchantment>())
						{
							return std::clamp(current / e->charge, 0.f, 100.f);
						}
					}
				}

				// prefab items
				if (auto ench = equipped->As<TESEnchantableForm>())
				{
					return std::clamp(current / (float)(ench->amountofEnchantment), 0.f, 100.f);
				}
			}
		}
		return 0;
	}

	float GetGameHour()
	{
		if (auto c = Calendar::GetSingleton()) { return c->GetHour(); }
		return 0;
	}

	float GetAmmoPercent(Actor* a_a, float a_ammoCountMult)
	{
		if (auto ammo = a_a->GetCurrentAmmo())
		{
			auto countmap = a_a->GetInventoryCounts();
			if (countmap[ammo]) { return std::clamp(countmap[ammo] * a_ammoCountMult, 0.f, 100.f); }
		}
		return 0;
	}

	float GetShoutCooldownPercent(Actor* a_a, float a_MaxCDTime)
	{ return std::clamp(a_a->GetVoiceRecoveryTime() / a_MaxCDTime, 0.f, 100.f); }

	void SetGlowMult(NiAVObject* a_target, float a_glow_mult)
	{
		if (a_target)
		{
			// if target has no geometry, try first child
			BSGeometry* geom = a_target->AsGeometry();
			if (!geom) { geom = a_target->AsNode()->GetChildren().front()->AsGeometry(); }
			if (auto shaderProp = GetShaderProperty(geom))
			{
				if (auto shader = netimmerse_cast<RE::BSLightingShaderProperty*>(shaderProp))
				{
					shader->emissiveMult = a_glow_mult;
				}
			}
		}
	}

	void SetGlowColor(NiAVObject* a_target, int a_color_hex)
	{
		if (a_target)
		{
			// if target has no geometry, try first child
			BSGeometry* geom = a_target->AsGeometry();
			if (!geom) { geom = a_target->AsNode()->GetChildren().front()->AsGeometry(); }
			if (auto shaderProp = GetShaderProperty(geom))
			{
				if (auto shader = netimmerse_cast<RE::BSLightingShaderProperty*>(shaderProp))
				{
					NiColor temp(a_color_hex);
					*(shader->emissiveColor) = temp;
				}
			}
		}
	}

	void SetUVCoords(NiAVObject* a_target, float a_x, float a_y)
	{
		if (a_target)
		{
			if (auto geometry =
					a_target->GetFirstGeometryOfShaderType(BSShaderMaterial::Feature::kNone))
			{
				if (auto shaderProp = geometry->GetGeometryRuntimeData().shaderProperty)
				{
					shaderProp->material->texCoordOffset[0].x = a_x;
					shaderProp->material->texCoordOffset[0].y = a_y;
					shaderProp->material->texCoordOffset[1].x = a_x;
					shaderProp->material->texCoordOffset[1].y = a_y;
				}
			}
		}
	}

	void SetUvUnique(NiAVObject* a_target, float a_x, float a_y, const char* a_nodename)
	{
		if (auto shader = helper::GetShaderProperty(a_target, a_nodename))
		{
			auto oldmat = shader->material;
			auto newmat = oldmat->Create();
			newmat->CopyMembers(oldmat);
			shader->material = newmat;
			newmat->IncRef();
			oldmat->DecRef();

			newmat->texCoordOffset[0].x = a_x;
			newmat->texCoordOffset[0].y = a_y;
			newmat->texCoordOffset[1].x = newmat->texCoordOffset[0].x;
			newmat->texCoordOffset[1].y = newmat->texCoordOffset[0].y;
		}
	}

	void SetSpecularMult() {}
	void SetSpecularColor() {}

	void CastSpellInstant(Actor* src, Actor* a_target, SpellItem* a_spell)
	{
		if (src && a_target && a_spell)
		{
			if (auto caster = src->GetMagicCaster(MagicSystem::CastingSource::kInstant))
			{
				caster->CastSpellImmediate(a_spell, false, a_target, 1.0, false, 1.0, src);
			}
		}
	}

	void Dispel(Actor* src, Actor* a_target, SpellItem* a_spell)
	{
		if (src && a_target && a_spell)
		{
			if (auto handle = src->GetHandle())
			{
				a_target->GetMagicTarget()->DispelEffect(a_spell, handle);
			}
		}
	}

	void PrintActorModelEffects(RE::TESObjectREFR* a_actor)
	{
		if (const auto processLists = RE::ProcessLists::GetSingleton())
		{
			int player = 0;
			int dangling = 0;
			processLists->ForEachModelEffect([&](RE::ModelReferenceEffect* a_modelEffect) {
				if (a_modelEffect->artObject)
				{
					SKSE::log::debug("MRE:{}  AO:{:x} lifetime:{}", (void*)&a_modelEffect,
						a_modelEffect->artObject->GetFormID(), a_modelEffect->lifetime);
					player++;
				}
				else
				{
					SKSE::log::debug("MRE:{}  AO:{}  lifetime:{}", (void*)&a_modelEffect,
						(void*)(a_modelEffect->artObject), a_modelEffect->lifetime);
					dangling++;
				}

				return RE::BSContainer::ForEachResult::kContinue;
			});
			SKSE::log::debug("{} effects and {} dangling MRE", player, dangling);
		}
	}

	void PrintPlayerShaderEffects()
	{
		if (const auto processLists = RE::ProcessLists::GetSingleton())
		{
			int player = 0;
			int dangling = 0;
			processLists->ForEachShaderEffect([&](RE::ShaderReferenceEffect* a_shaderEffect) {
				if (a_shaderEffect->target.get()->AsReference() ==
					RE::PlayerCharacter::GetSingleton()->AsReference())
				{
					if (a_shaderEffect->effectData)
					{
						SKSE::log::debug("SRE:{}  AO:{}", (void*)&a_shaderEffect,
							(void*)a_shaderEffect->effectData);
						player++;
					}
					else
					{
						dangling++;
					}
				}
				return RE::BSContainer::ForEachResult::kContinue;
			});
			SKSE::log::debug("{} player effects and {} dangling SRE", player, dangling);
		}
	}

	std::filesystem::path GetGamePath()
	{
		HMODULE handle = GetModuleHandle(NULL);
		char    exe_path[MAX_PATH];
		// Get the full path of the DLL
		if (GetModuleFileNameA(handle, exe_path, (sizeof(exe_path))))
		{
			std::filesystem::path file_path(exe_path);

			return file_path.parent_path() / "Data\\";
		}
		return "";
	}

	float ReadFloatFromIni(std::ifstream& a_file, std::string a_setting)
	{
		if (a_file.is_open())
		{
			std::string line;
			while (std::getline(a_file, line))
			{
				if (line[0] != '#' && line.find(a_setting) == 0)
				{
					auto found = line.find('=');
					if (found != std::string::npos)
					{
						try
						{
							auto val = std::stof(line.substr(found + 1));
							a_file.clear();
							a_file.seekg(0, std::ios::beg);
							SKSE::log::trace("{} : {}", a_setting, val);
							return val;
						} catch (std::out_of_range)
						{
						} catch (std::invalid_argument) {}
						SKSE::log::error("Bad mod ini, please reset it");
						a_file.clear();
						a_file.seekg(0, std::ios::beg);
						return 0.f;
					}
				}
			}
			SKSE::log::error("ini error: {} not found", a_setting);
			a_file.clear();
			a_file.seekg(0, a_file.beg);
		}

		return 0.f;
	}

	bool WriteFloatToIni(
		const std::filesystem::path& a_path, std::string_view a_setting, float a_value)
	{
		std::vector<std::string> lines;
		std::ifstream            input(a_path);
		std::string              line;
		bool                     found = false;

		while (std::getline(input, line))
		{
			const auto delimiter = line.find('=');
			if (delimiter != std::string::npos)
			{
				auto       key = std::string_view(line).substr(0, delimiter);
				const auto key_begin = key.find_first_not_of(" \t");
				const auto key_end = key.find_last_not_of(" \t");

				if (key_begin != std::string_view::npos)
				{
					key = key.substr(key_begin, key_end - key_begin + 1);
					if (key == a_setting)
					{
						std::ostringstream value;
						value << std::setprecision(std::numeric_limits<float>::max_digits10)
							  << a_value;
						line = std::string(a_setting) + '=' + value.str();
						found = true;
					}
				}
			}

			lines.emplace_back(std::move(line));
		}

		if (!found)
		{
			std::ostringstream value;
			value << std::setprecision(std::numeric_limits<float>::max_digits10) << a_value;
			lines.emplace_back(std::string(a_setting) + '=' + value.str());
		}

		input.close();
		std::ofstream output(a_path, std::ios::trunc);
		if (!output.is_open())
		{
			SKSE::log::error("Unable to write INI setting {}", a_setting);
			return false;
		}

		for (const auto& output_line : lines) { output << output_line << '\n'; }
		return output.good();
	}

	bool WriteStringToIni(
		const std::filesystem::path& a_path, std::string_view a_setting, std::string_view a_value)
	{
		std::vector<std::string> lines;
		std::ifstream            input(a_path);
		std::string              line;
		bool                     found = false;

		while (std::getline(input, line))
		{
			const auto delimiter = line.find('=');
			if (delimiter != std::string::npos)
			{
				auto       key = std::string_view(line).substr(0, delimiter);
				const auto key_begin = key.find_first_not_of(" \t");
				const auto key_end = key.find_last_not_of(" \t");

				if (key_begin != std::string_view::npos)
				{
					key = key.substr(key_begin, key_end - key_begin + 1);
					if (key == a_setting)
					{
						line = std::string(a_setting) + '=' + std::string(a_value);
						found = true;
					}
				}
			}

			lines.emplace_back(std::move(line));
		}

		if (!found) { lines.emplace_back(std::string(a_setting) + '=' + std::string(a_value)); }

		input.close();
		std::ofstream output(a_path, std::ios::trunc);
		if (!output.is_open())
		{
			SKSE::log::error("Unable to write INI setting {}", a_setting);
			return false;
		}

		for (const auto& output_line : lines) { output << output_line << '\n'; }
		return output.good();
	}

	int ReadIntFromIni(std::ifstream& a_file, std::string a_setting)
	{
		if (a_file.is_open())
		{
			std::string line;
			while (std::getline(a_file, line))
			{
				if (line.find(a_setting) == 0)
				{
					auto found = line.find('=');
					if (found != std::string::npos)
					{
						try
						{
							auto val = std::stoi(line.substr(found + 1));
							a_file.clear();
							a_file.seekg(0, std::ios::beg);
							SKSE::log::trace("{} : {}", a_setting, val);
							return val;
						} catch (std::out_of_range)
						{
						} catch (std::invalid_argument) {}
						SKSE::log::error("Bad mod ini, please reset it");
						a_file.clear();
						a_file.seekg(0, std::ios::beg);
						return 0;
					}
				}
			}
			SKSE::log::error("ini error: {} not found", a_setting);
			a_file.clear();
			a_file.seekg(0, std::ios::beg);
		}

		return 0;
	}

	std::string ReadStringFromIni(std::ifstream& a_file, std::string a_setting)
	{
		if (a_file.is_open())
		{
			std::string line;
			while (std::getline(a_file, line))
			{
				if (line.find(a_setting) == 0)
				{
					auto found = line.find('=');
					if (found != std::string::npos)
					{
						a_file.clear();
						a_file.seekg(0, std::ios::beg);

						// Extract the substring after '=' and trim any leading/trailing whitespace
						std::string val = line.substr(found + 1);
						val = val.erase(
							0, val.find_first_not_of(" \t\n\r"));  // Trim leading whitespace
						val = val.erase(
							val.find_last_not_of(" \t\n\r") + 1);  // Trim trailing whitespace

						SKSE::log::trace("{} : {}", a_setting, val);
						return val;
					}
				}
			}
			SKSE::log::error("ini error: {} not found", a_setting);
			a_file.clear();
			a_file.seekg(0, std::ios::beg);
		}

		return "";
	}

	void StopControllers(RE::NiAVObject* a_obj)
	{
		if (!a_obj) { return; }

		for (auto* controller = a_obj->GetControllers(); controller;
			controller = controller->next.get())
		{
			controller->Stop();
		}

		if (auto* node = a_obj->AsNode())
		{
			for (auto& child : node->children) { StopControllers(child.get()); }
		}
	}

	bool InitializeSound(BSSoundHandle& a_handle, std::string a_editorID)
	{
		auto* manager = BSAudioManager::GetSingleton();
		if (!manager) { return false; }

		manager->GetSoundHandleByName(a_handle, a_editorID.c_str(), 0x10);
		return a_handle.IsValid();
	}

	bool PlaySound(BSSoundHandle& a_handle, float a_volume, RE::NiPoint3& a_position,
		RE::NiAVObject* a_follow_node)
	{
		a_handle.SetPosition(a_position);
		a_handle.SetObjectToFollow(a_follow_node);
		a_handle.SetVolume(a_volume);
		a_handle.Play();
		return a_handle.IsPlaying();
	}

	const char* GetObjectModelPath(RE::TESBoundObject* a_obj)
	{
		if (!a_obj) { return nullptr; }

		switch (a_obj->GetFormType())
		{
		case RE::FormType::Armor:
			if (auto armor = a_obj->As<RE::TESObjectARMO>())
			{
				if (auto path = armor->worldModels[0].GetModel(); path && path[0] != '\0')
				{
					return path;
				}
				if (auto path = armor->worldModels[1].GetModel(); path && path[0] != '\0')
				{
					return path;
				}
			}
			break;

		case RE::FormType::Book:
			if (auto book = a_obj->As<RE::TESObjectBOOK>())
			{
				if (auto model = book->inventoryModel)
				{
					if (auto data = model->As<RE::TESModelTextureSwap>())
					{
						return data->GetModel();
					}
				}
			}
			break;

		case RE::FormType::Ammo:
		case RE::FormType::KeyMaster:
		case RE::FormType::Weapon:
		case RE::FormType::AlchemyItem:
		case RE::FormType::SoulGem:
		case RE::FormType::Misc:
		case RE::FormType::Ingredient:
		case RE::FormType::Scroll:
		default:
			if (auto model = a_obj->As<RE::TESModelTextureSwap>())
			{
				if (auto path = model->GetModel(); path && path[0] != '\0') { return path; }
			}
		}

		SKSE::log::trace("No model found for formID {} with formtype {}", a_obj->GetFormID(),
			RE::FormTypeToString(a_obj->GetFormType()));
		return nullptr;
	}

	void DrawBox(art_addon::ArtAddon* box, const RE::NiPoint3& dimensions)
	{
		if (box)
		{
			RE::NiAVObject* geom = box->Get3D();
			for (int i : { 0, 1 })
				for (int j : { 0, 1 })
					for (int k : { 0, 1 })
					{
						char name[4] = { char('0' + i), char('0' + j), char('0' + k), 0 };

						if (auto node = geom->GetObjectByName(name))
						{
							float x = (i ? +dimensions.x : -dimensions.x);
							float y = (j ? +dimensions.y : -dimensions.y);
							float z = (k ? +dimensions.z : -dimensions.z);

							node->local.translate = { x, y, z };
						}
					}
		}
	}

	const char* GetObjectModelPath(RE::TESObjectREFR* a_obj)
	{
		if (auto boundobj = a_obj->GetBaseObject())
		{
			if (auto model = boundobj->As<TESModelTextureSwap>()) { return model->GetModel(); }
			else if (boundobj->GetFormType() == FormType::Armor)
			{
				if (auto bipedmodel = boundobj->As<TESBipedModelForm>())
				{
					return bipedmodel->worldModels[0].GetModel();
				}
			}
		}
		return nullptr;
	}

	RE::TESForm* GetForm(const RE::FormID a_lower_id, std::string a_mod_name)
	{
		if (auto file = RE::TESDataHandler::GetSingleton()->LookupModByName(a_mod_name))
		{
			if (auto idx = file->GetPartialIndex(); idx != 0xff)
			{
				return RE::TESForm::LookupByID(idx << (file->IsLight() ? 12 : 24) | a_lower_id);
			}
		}
		return nullptr;
	}

	/* Code based on https://github.com/adamhynek/higgs/ */

	void UpdateVertices(RE::BSGeometry* a_geom, const RE::NiTransform& a_geometryToRoot,
		std::vector<RE::NiPoint3>& a_vertices)
	{
		if (!a_geom) { return; }

		auto* trishape = a_geom->AsTriShape();
		if (!trishape) { return; }

		const auto vertex_count = trishape->GetTrishapeRuntimeData().vertexCount;

		auto* geom_data = a_geom->GetGeometryRuntimeData().rendererData;
		if (!geom_data || !geom_data->rawVertexData || vertex_count == 0) { return; }

		auto vertex_desc = geom_data->vertexDesc;

		if (!vertex_desc.HasFlag(RE::BSGraphics::Vertex::Flags::VF_VERTEX)) { return; }

		auto*      verts = geom_data->rawVertexData;
		const auto vertex_size = vertex_desc.GetSize();
		const auto pos_offset =
			vertex_desc.GetAttributeOffset(RE::BSGraphics::Vertex::Attribute::VA_POSITION);

		for (std::uint16_t i = 0; i < vertex_count; ++i)
		{
			auto* pos = reinterpret_cast<RE::NiPoint3*>(verts + i * vertex_size + pos_offset);

			a_vertices.push_back(a_geometryToRoot * (*pos));
		}
	}

	void UpdateVertices(RE::NiSkinInstance* a_skin, const RE::NiTransform& a_geometryToRoot,
		std::vector<RE::NiPoint3>& a_vertices)
	{
		if (!a_skin) { return; }

		auto& part = a_skin->skinPartition;
		if (!part) { return; }

		for (const auto& data : part->partitions)
		{
			const auto vertex_count = data.vertices;

			auto* geom_data = data.buffData;
			if (!geom_data || !geom_data->rawVertexData || vertex_count == 0) { continue; }

			auto vertex_desc = geom_data->vertexDesc;

			if (!vertex_desc.HasFlag(RE::BSGraphics::Vertex::Flags::VF_VERTEX)) { continue; }

			auto*      verts = geom_data->rawVertexData;
			const auto vertex_size = vertex_desc.GetSize();
			const auto pos_offset =
				vertex_desc.GetAttributeOffset(RE::BSGraphics::Vertex::Attribute::VA_POSITION);

			for (std::uint16_t i = 0; i < vertex_count; ++i)
			{
				auto* pos = reinterpret_cast<RE::NiPoint3*>(verts + i * vertex_size + pos_offset);

				a_vertices.push_back(a_geometryToRoot * (*pos));
			}
		}
	}

	void GetVerticesRecursive(RE::NiAVObject* a_obj, const RE::NiTransform& a_parentToRoot,
		std::vector<RE::NiPoint3>& a_vertices)
	{
		if (!a_obj) { return; }

		const RE::NiTransform object_to_root = a_parentToRoot * a_obj->local;

		if (auto* geom = a_obj->AsGeometry())
		{
			if (auto* skin = geom->skinInstance.get())
			{
				UpdateVertices(skin, object_to_root, a_vertices);
				return;
			}
			else
			{
				UpdateVertices(geom, object_to_root, a_vertices);
				return;
			}
		}

		if (auto* node = a_obj->AsNode())
		{
			for (auto& child : node->children)
			{
				GetVerticesRecursive(child.get(), object_to_root, a_vertices);
			}
		}
	}

	void GetVertices(RE::NiAVObject* a_root, std::vector<RE::NiPoint3>& a_vertices)
	{
		if (!a_root) { return; }
		if (auto node = a_root->AsNode())
		{
			for (auto& c : node->children) { GetVerticesRecursive(c.get(), {}, a_vertices); }
		}
	}

	void CalculateSphereBounds(
		const std::vector<RE::NiPoint3>& a_vertices, float& a_radiusOut, RE::NiPoint3& a_centerOut)
	{
		a_radiusOut = 0.0f;
		a_centerOut = { 0.0f, 0.0f, 0.0f };

		if (a_vertices.empty()) { return; }

		RE::NiPoint3 min = a_vertices.front();
		RE::NiPoint3 max = a_vertices.front();

		for (const auto& v : a_vertices)
		{
			min.x = std::min(min.x, v.x);
			min.y = std::min(min.y, v.y);
			min.z = std::min(min.z, v.z);

			max.x = std::max(max.x, v.x);
			max.y = std::max(max.y, v.y);
			max.z = std::max(max.z, v.z);
		}

		a_centerOut = { (min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f, (min.z + max.z) * 0.5f };

		for (const auto& v : a_vertices)
		{
			const auto  delta = v - a_centerOut;
			const float distSq = delta.x * delta.x + delta.y * delta.y + delta.z * delta.z;
			a_radiusOut = std::max(a_radiusOut, std::sqrt(distSq));
		}
	}

	void CalculateAABB(const std::vector<RE::NiPoint3>& a_vertices, RE::NiPoint3& a_center,
		RE::NiPoint3& a_extents)
	{
		if (a_vertices.empty())
		{
			a_center = {};
			a_extents = {};
			return;
		}

		auto a_min = a_vertices.front();
		auto a_max = a_vertices.front();

		for (const auto& v : a_vertices)
		{
			a_min.x = std::min(a_min.x, v.x);
			a_min.y = std::min(a_min.y, v.y);
			a_min.z = std::min(a_min.z, v.z);

			a_max.x = std::max(a_max.x, v.x);
			a_max.y = std::max(a_max.y, v.y);
			a_max.z = std::max(a_max.z, v.z);
		}

		a_center = (a_min + a_max) * 0.5f;
		a_extents = (a_max - a_min) * 0.5f;
	}

	void CalculateExtentsAndRadius(const std::vector<RE::NiPoint3>& a_vertices,
		RE::NiPoint3& a_center, RE::NiPoint3& a_extents, float& a_radius)
	{
		if (a_vertices.empty()) return;
	}

	struct BoundsAccumulator
	{
		bool         initialized = false;
		RE::NiPoint3 min{};
		RE::NiPoint3 max{};

		void Add(const RE::NiPoint3& a_point)
		{
			if (!initialized)
			{
				min = a_point;
				max = a_point;
				initialized = true;
				return;
			}

			min.x = std::min(min.x, a_point.x);
			min.y = std::min(min.y, a_point.y);
			min.z = std::min(min.z, a_point.z);

			max.x = std::max(max.x, a_point.x);
			max.y = std::max(max.y, a_point.y);
			max.z = std::max(max.z, a_point.z);
		}
	};

	void UpdateBoundsDirect(RE::BSGeometry* a_geom, const RE::NiTransform& a_geometryToRoot,
		BoundsAccumulator& a_bounds)
	{
		if (!a_geom) { return; }

		auto* trishape = a_geom->AsTriShape();
		if (!trishape) { return; }

		const auto vertex_count = trishape->GetTrishapeRuntimeData().vertexCount;

		auto* geom_data = a_geom->GetGeometryRuntimeData().rendererData;
		if (!geom_data || !geom_data->rawVertexData || vertex_count == 0) { return; }

		auto vertex_desc = geom_data->vertexDesc;

		if (!vertex_desc.HasFlag(RE::BSGraphics::Vertex::Flags::VF_VERTEX)) { return; }

		auto*      verts = geom_data->rawVertexData;
		const auto vertex_size = vertex_desc.GetSize();

		const auto pos_offset =
			vertex_desc.GetAttributeOffset(RE::BSGraphics::Vertex::Attribute::VA_POSITION);

		for (std::uint16_t i = 0; i < vertex_count; ++i)
		{
			auto* pos = reinterpret_cast<RE::NiPoint3*>(verts + i * vertex_size + pos_offset);

			a_bounds.Add(a_geometryToRoot * (*pos));
		}
	}

	void UpdateBoundsDirect(RE::NiSkinInstance* a_skin, const RE::NiTransform& a_geometryToRoot,
		BoundsAccumulator& a_bounds)
	{
		if (!a_skin) { return; }

		auto& part = a_skin->skinPartition;
		if (!part) { return; }

		for (const auto& data : part->partitions)
		{
			const auto vertex_count = data.vertices;

			auto* geom_data = data.buffData;
			if (!geom_data || !geom_data->rawVertexData || vertex_count == 0) { continue; }

			auto vertex_desc = geom_data->vertexDesc;

			if (!vertex_desc.HasFlag(RE::BSGraphics::Vertex::Flags::VF_VERTEX)) { continue; }

			auto*      verts = geom_data->rawVertexData;
			const auto vertex_size = vertex_desc.GetSize();

			const auto pos_offset =
				vertex_desc.GetAttributeOffset(RE::BSGraphics::Vertex::Attribute::VA_POSITION);

			for (std::uint16_t i = 0; i < vertex_count; ++i)
			{
				auto* pos = reinterpret_cast<RE::NiPoint3*>(verts + i * vertex_size + pos_offset);

				a_bounds.Add(a_geometryToRoot * (*pos));
			}
		}
	}

	void CalculateBoundsDirectRecursive(
		RE::NiAVObject* a_obj, const RE::NiTransform& a_parentToRoot, BoundsAccumulator& a_bounds)
	{
		if (!a_obj) { return; }

		const RE::NiTransform object_to_root = a_parentToRoot * a_obj->local;

		if (auto* geom = a_obj->AsGeometry())
		{
			if (auto* skin = geom->skinInstance.get())
			{
				UpdateBoundsDirect(skin, object_to_root, a_bounds);
			}
			else
			{
				UpdateBoundsDirect(geom, object_to_root, a_bounds);
			}

			return;
		}

		if (auto* node = a_obj->AsNode())
		{
			for (auto& child : node->children)
			{
				CalculateBoundsDirectRecursive(child.get(), object_to_root, a_bounds);
			}
		}
	}

	void CalculateBoundsDirect(RE::NiAVObject* a_root, float& a_radiusOut,
		RE::NiPoint3& a_centerOut, RE::NiPoint3& a_extentsOut)
	{
		a_radiusOut = 0.f;
		a_centerOut = {};
		a_extentsOut = {};

		if (!a_root) { return; }

		auto* root_node = a_root->AsNode();
		if (!root_node) { return; }

		BoundsAccumulator bounds;

		const RE::NiTransform root_identity{};

		for (auto& child : root_node->children)
		{
			CalculateBoundsDirectRecursive(child.get(), root_identity, bounds);
		}

		if (!bounds.initialized) { return; }

		a_centerOut = (bounds.min + bounds.max) * 0.5f;
		a_extentsOut = (bounds.max - bounds.min) * 0.5f;
		a_radiusOut = a_extentsOut.Length();
	}

#pragma push_macro("GetObject")
#undef GetObject

	RE::BGSEquipSlot* GetHandEquipSlot(bool a_isLeft)
	{
		auto* default_objects = RE::BGSDefaultObjectManager::GetSingleton();
		return default_objects ? default_objects->GetObject<RE::BGSEquipSlot>(a_isLeft ?
										 RE::DEFAULT_OBJECT::kLeftHandEquip :
										 RE::DEFAULT_OBJECT::kRightHandEquip) :
								 nullptr;
	}

#pragma pop_macro("GetObject")

	/* adapted from Shizof's WeaponThrowVR and SpellWheelVR*/
	void UnequipSpell(RE::Actor* a_actor, RE::SpellItem* a_spell, bool a_isLeft)
	{
		using func_t = void (*)(RE::BSScript::IVirtualMachine*, std::uint32_t, RE::Actor*,
			RE::SpellItem*, std::int32_t);

		static REL::Relocation<func_t> func{ REL::Offset(0x984D00) };

		auto* skyrim_vm = RE::SkyrimVM::GetSingleton();
		if (skyrim_vm && skyrim_vm->impl)
		{
			func(skyrim_vm->impl.get(), 0, a_actor, a_spell, a_isLeft ? 0 : 1);
		}
	}

	// Writes information about a node to the log file
	void logNode(int depth, NiAVObject* node)
	{
		if (!node) { return; }

		const auto* rtti = node->GetRTTI();
		const auto* type_name = rtti ? rtti->GetName() : nullptr;
		const auto* node_name = node->name.c_str();
		const auto  indentation = std::string(static_cast<std::size_t>(std::max(depth, 0)), '.');

		SKSE::log::trace("{}: {}{} (RTTI: {})", depth, indentation,
			node_name && node_name[0] ? node_name : "<unnamed>",
			type_name ? type_name : "<unknown>");
	}

	// Lists all parents of a bone to the log file
	void logParents(NiAVObject* bone)
	{
		NiNode* node = bone ? bone->AsNode() : nullptr;
		int     depth = 1;
		while (node)
		{
			logNode(depth, node);
			node = node->parent;
			++depth;
		}
	}

	// Lists all children of a bone to the log file, filtering by RTTI type name
	void logChildren(NiAVObject* bone, int depth, int maxDepth, const char* filter)
	{
		if (!bone) return;

		if (filter && filter[0])
		{
			const auto* rtti = bone->GetRTTI();
			const auto* type_name = rtti ? rtti->GetName() : nullptr;
			if (!type_name || std::strcmp(type_name, filter) != 0) { return; }
		}

		logNode(depth, bone);
		NiNode* node = bone->AsNode();
		if (!node) return;

		if (depth < maxDepth || maxDepth < 0)
		{
			for (const auto& child : node->GetChildren())
			{
				logChildren(child.get(), depth + 1, maxDepth, filter);
			}
		}
	}

	/* Quest related functions */
	const RE::BGSQuestInstanceText* FindQuestInstanceText(
		const RE::TESQuest* a_quest, std::uint32_t a_instanceID)
	{
		if (!a_quest) { return nullptr; }

		for (const auto* instance : a_quest->instanceData)
		{
			if (instance && instance->id == a_instanceID) { return instance; }
		}

		return nullptr;
	}

	RE::BGSBaseAlias* FindQuestAlias(const RE::TESQuest* a_quest, std::string_view a_aliasName)
	{
		if (!a_quest) { return nullptr; }

		for (auto* alias : a_quest->aliases)
		{
			if (alias && a_aliasName == alias->aliasName.c_str()) { return alias; }
		}

		return nullptr;
	}

	RE::TESForm* FindStoredAliasNameForm(
		const RE::TESQuest* a_quest, std::uint32_t a_instanceID, const RE::BGSBaseAlias* a_alias)
	{
		const auto* instance = FindQuestInstanceText(a_quest, a_instanceID);
		if (!instance || !a_alias) { return nullptr; }

		for (const auto& entry : instance->stringData)
		{
			if (entry.aliasID == a_alias->aliasID)
			{
				return RE::TESForm::LookupByID(entry.fullNameFormID);
			}
		}

		return nullptr;
	}

	std::string ResolveReferenceName(RE::TESObjectREFR* a_reference, bool a_shortName)
	{
		if (!a_reference) { return "[...]"; }

		if (a_shortName)
		{
			if (auto* actor = a_reference->As<RE::Actor>())
			{
				if (auto* npc = actor->GetActorBase(); npc && !npc->shortName.empty())
				{
					return npc->shortName.c_str();
				}
			}
		}

		const auto* name = a_reference->GetDisplayFullName();
		return name && *name ? name : "[...]";
	}

	std::string ResolveAliasName(const RE::TESQuest* a_quest, std::uint32_t a_instanceID,
		std::string_view a_aliasName, bool a_shortName)
	{
		if (a_aliasName == "Player")
		{
			return ResolveReferenceName(RE::PlayerCharacter::GetSingleton(), a_shortName);
		}

		auto* alias = FindQuestAlias(a_quest, a_aliasName);
		if (!alias) { return "[...]"; }

		if (auto* nameForm = FindStoredAliasNameForm(a_quest, a_instanceID, alias))
		{
			if (a_shortName)
			{
				if (auto* npc = nameForm->As<RE::TESNPC>(); npc && !npc->shortName.empty())
				{
					return npc->shortName.c_str();
				}
			}

			const auto* name = nameForm->GetName();
			if (name && *name) { return name; }
		}

		if (auto* refAlias = skyrim_cast<RE::BGSRefAlias*>(alias))
		{
			return ResolveReferenceName(refAlias->GetReference(), a_shortName);
		}

		return "[...]";
	}

	const RE::TESGlobal* FindTextGlobal(const RE::TESQuest* a_quest, std::string_view a_editorID)
	{
		if (!a_quest || !a_quest->textGlobals) { return nullptr; }

		for (const auto* global : *a_quest->textGlobals)
		{
			if (global && a_editorID == global->GetFormEditorID()) { return global; }
		}

		return nullptr;
	}

	std::optional<float> GetStoredGlobalValue(
		const RE::TESQuest* a_quest, std::uint32_t a_instanceID, const RE::TESGlobal* a_global)
	{
		const auto* instance = FindQuestInstanceText(a_quest, a_instanceID);
		if (!instance || !a_global) { return std::nullopt; }

		for (const auto& entry : instance->valueData)
		{
			if (entry.global == a_global) { return entry.value; }
		}

		return std::nullopt;
	}

	std::string ResolveGlobalValue(
		const RE::TESQuest* a_quest, std::uint32_t a_instanceID, std::string_view a_editorID)
	{
		const auto* global = FindTextGlobal(a_quest, a_editorID);
		if (!global) { return "[...]"; }

		const float value =
			GetStoredGlobalValue(a_quest, a_instanceID, global).value_or(global->value);

		std::ostringstream result;
		if (global->type == RE::TESGlobal::Type::kFloat)
		{
			result << std::fixed << std::setprecision(2) << value;
		}
		else
		{
			result << std::fixed << std::setprecision(0) << value;
		}

		return result.str();
	}

	std::unordered_set<RE::FormID> ParseFormIDList(
		std::string_view a_list, std::string_view a_setting_name)
	{
		std::unordered_set<RE::FormID> result;

		while (!a_list.empty())
		{
			const auto delimiter = a_list.find(',');
			auto       token = a_list.substr(0, delimiter);

			const auto token_begin = token.find_first_not_of(" \t\r\n");
			if (token_begin != std::string_view::npos)
			{
				const auto token_end = token.find_last_not_of(" \t\r\n");
				token = token.substr(token_begin, token_end - token_begin + 1);
				if (token.starts_with("0x") || token.starts_with("0X"))
				{
					token.remove_prefix(2);
				}

				RE::FormID form_id{};
				const auto [end, error] =
					std::from_chars(token.data(), token.data() + token.size(), form_id, 16);
				if (error == std::errc{} && end == token.data() + token.size())
				{
					result.insert(form_id);
				}
				else
				{
					SKSE::log::warn("Invalid form ID in {}: {}", a_setting_name, token);
				}
			}

			if (delimiter == std::string_view::npos) { break; }
			a_list.remove_prefix(delimiter + 1);
		}

		return result;
	}

	std::string SerializeFormIDList(const std::unordered_set<RE::FormID>& a_form_ids)
	{
		std::vector<RE::FormID> sorted_ids(a_form_ids.begin(), a_form_ids.end());
		std::ranges::sort(sorted_ids);

		std::string result;
		for (const auto form_id : sorted_ids)
		{
			if (!result.empty()) { result.push_back(','); }
			result.append(std::format("{:x}", form_id));
		}
		return result;
	}
}
