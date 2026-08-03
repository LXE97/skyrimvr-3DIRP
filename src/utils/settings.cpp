#include "settings.h"

#include "main_plugin.h"

#include "rapidjson/document.h"
#include "rapidjson/istreamwrapper.h"
#include "rapidjson/ostreamwrapper.h"
#include "rapidjson/prettywriter.h"

#include <fstream>

namespace settings
{
	QuestFormList g_new_quests_list;

	namespace
	{
		constexpr std::string_view kPapyrusScript = "_3DIRP_Settings";
		constexpr std::uint32_t kSerializationID =
			static_cast<std::uint32_t>('3') | (static_cast<std::uint32_t>('D') << 8) |
			(static_cast<std::uint32_t>('I') << 16) | (static_cast<std::uint32_t>('R') << 24);
		constexpr std::uint32_t kNewQuestsRecord =
			static_cast<std::uint32_t>('N') | (static_cast<std::uint32_t>('Q') << 8) |
			(static_cast<std::uint32_t>('S') << 16) | (static_cast<std::uint32_t>('T') << 24);
		constexpr std::uint32_t kNewQuestsVersion = 1;

		std::shared_mutex g_new_quests_mutex;

		void SerializeQuestForms(SKSE::SerializationInterface* a_intfc)
		{
			if (!a_intfc->OpenRecord(kNewQuestsRecord, kNewQuestsVersion))
			{
				SKSE::log::error("Unable to open new quest serialization record");
				return;
			}

			std::shared_lock lock(g_new_quests_mutex);
			const auto count = static_cast<std::uint32_t>(g_new_quests_list.size());
			if (!a_intfc->WriteRecordData(count))
			{
				SKSE::log::error("Unable to serialize new quest count");
				return;
			}

			for (const auto form_id : g_new_quests_list)
			{
				if (!a_intfc->WriteRecordData(form_id))
				{
					SKSE::log::error("Unable to serialize new quest {:x}", form_id);
					return;
				}
			}
		}

		void DeserializeQuestForms(SKSE::SerializationInterface* a_intfc)
		{
			QuestFormList loaded_quests;
			std::uint32_t type{};
			std::uint32_t version{};
			std::uint32_t length{};

			while (a_intfc->GetNextRecordInfo(type, version, length))
			{
				if (type != kNewQuestsRecord) { continue; }
				if (version != kNewQuestsVersion)
				{
					SKSE::log::warn("Unsupported new quest serialization version {}", version);
					continue;
				}

				std::uint32_t count{};
				if (length < sizeof(count))
				{
					SKSE::log::error("New quest serialization record is truncated");
					continue;
				}
				if (a_intfc->ReadRecordData(count) != sizeof(count))
				{
					SKSE::log::error("Unable to deserialize new quest count");
					continue;
				}
				const auto available_form_ids =
					(length - static_cast<std::uint32_t>(sizeof(count))) / sizeof(RE::FormID);
				if (count > available_form_ids)
				{
					SKSE::log::error("New quest serialization record has an invalid count");
					continue;
				}

				for (std::uint32_t index = 0; index < count; ++index)
				{
					RE::FormID form_id{};
					if (a_intfc->ReadRecordData(form_id) != sizeof(form_id))
					{
						SKSE::log::error("Unable to deserialize new quest at index {}", index);
						break;
					}

					if (a_intfc->ResolveFormID(form_id, form_id)) { loaded_quests.insert(form_id); }
				}
			}

			std::unique_lock lock(g_new_quests_mutex);
			g_new_quests_list = std::move(loaded_quests);
		}

		void RevertQuestForms(SKSE::SerializationInterface*)
		{
			std::unique_lock lock(g_new_quests_mutex);
			g_new_quests_list.clear();
		}

		float PapyrusGetSetting(RE::StaticFunctionTag*, RE::BSFixedString a_key)
		{
			return Manager::GetSingleton()->Get(a_key.c_str());
		}

		bool PapyrusSetSetting(
			RE::StaticFunctionTag*, RE::BSFixedString a_key, float a_value)
		{
			return Manager::GetSingleton()->Set(a_key.c_str(), a_value);
		}

		bool PapyrusReloadSettings(RE::StaticFunctionTag*)
		{
			return Manager::GetSingleton()->Reload();
		}

		bool PapyrusLoadProfile(RE::StaticFunctionTag*, RE::BSFixedString a_profile)
		{
			return Manager::GetSingleton()->LoadProfile(a_profile.c_str());
		}

		bool PapyrusCreateProfile(RE::StaticFunctionTag*, RE::BSFixedString a_profile)
		{
			return Manager::GetSingleton()->CreateProfile(a_profile.c_str());
		}

		bool PapyrusSaveSettings(RE::StaticFunctionTag*)
		{
			const bool saved = Manager::GetSingleton()->Save();
			if (saved) { vr3dirp::ApplySettings(); }
			return saved;
		}

		std::vector<RE::BSFixedString> PapyrusGetProfiles(RE::StaticFunctionTag*)
		{
			std::vector<RE::BSFixedString> result;
			for (const auto& profile : Manager::GetSingleton()->GetProfiles())
			{
				result.emplace_back(profile);
			}
			return result;
		}

		RE::BSFixedString PapyrusGetActiveProfile(RE::StaticFunctionTag*)
		{
			return Manager::GetSingleton()->GetActiveProfile();
		}
	}

	void PushNewQuest(RE::FormID a_form_id)
	{
		if (a_form_id == 0) { return; }

		std::unique_lock lock(g_new_quests_mutex);
		g_new_quests_list.insert(a_form_id);
	}

	QuestFormList GetNewQuestList()
	{
		std::shared_lock lock(g_new_quests_mutex);
		return g_new_quests_list;
	}

	void CommitNewQuestList(
		const QuestFormList& a_list_at_open, const QuestFormList& a_current_list)
	{
		std::unique_lock lock(g_new_quests_mutex);
		for (const auto form_id : a_list_at_open)
		{
			if (!a_current_list.contains(form_id)) { g_new_quests_list.erase(form_id); }
		}
		g_new_quests_list.insert(a_current_list.begin(), a_current_list.end());
	}

	void InitQuestSerialization()
	{
		auto* serialization = SKSE::GetSerializationInterface();
		serialization->SetUniqueID(kSerializationID);
		serialization->SetSaveCallback(SerializeQuestForms);
		serialization->SetLoadCallback(DeserializeQuestForms);
		serialization->SetRevertCallback(RevertQuestForms);
	}

	bool Manager::Init(std::filesystem::path a_path, std::string a_profile)
	{
		{
			std::unique_lock lock(mutex);
			path = std::move(a_path);
			active_profile = std::move(a_profile);
		}

		return Reload();
	}

	bool Manager::Reload()
	{
		Profiles loaded_profiles;
		if (!ReadFile(loaded_profiles)) { return false; }

		std::unique_lock lock(mutex);
		profiles = std::move(loaded_profiles);
		if (!profiles.contains(active_profile)) { active_profile = "Default"; }
		dirty = false;
		return profiles.contains(active_profile);
	}

	float Manager::Get(std::string_view a_key) const
	{
		std::shared_lock lock(mutex);
		const auto profile = profiles.find(active_profile);
		if (profile == profiles.end()) { return -1.0f; }

		const auto setting = profile->second.find(a_key);
		return setting != profile->second.end() ? setting->second : -1.0f;
	}

	bool Manager::Set(std::string_view a_key, float a_value)
	{
		std::unique_lock lock(mutex);
		const auto profile = profiles.find(active_profile);
		if (profile == profiles.end()) { return false; }

		profile->second[std::string(a_key)] = a_value;
		dirty = true;
		return true;
	}

	bool Manager::LoadProfile(std::string_view a_profile)
	{
		std::unique_lock lock(mutex);
		if (!profiles.contains(a_profile)) { return false; }

		active_profile = a_profile;
		return true;
	}

	bool Manager::CreateProfile(std::string_view a_profile)
	{
		if (a_profile.empty()) { return false; }

		std::unique_lock lock(mutex);
		if (profiles.contains(a_profile)) { return false; }

		const auto source = profiles.find(active_profile);
		if (source == profiles.end()) { return false; }

		profiles.emplace(std::string(a_profile), source->second);
		active_profile = a_profile;
		dirty = true;
		return true;
	}

	bool Manager::Save()
	{
		std::unique_lock lock(mutex);
		if (path.empty()) { return false; }
		if (!dirty) { return true; }
		if (!WriteFile(profiles)) { return false; }

		dirty = false;
		return true;
	}

	std::vector<std::string> Manager::GetProfiles() const
	{
		std::shared_lock lock(mutex);
		std::vector<std::string> result;
		result.reserve(profiles.size());
		for (const auto& [name, profile] : profiles) { result.push_back(name); }
		return result;
	}

	std::string Manager::GetActiveProfile() const
	{
		std::shared_lock lock(mutex);
		return active_profile;
	}

	bool Manager::ReadFile(Profiles& a_profiles) const
	{
		std::filesystem::path settings_path;
		{
			std::shared_lock lock(mutex);
			settings_path = path;
		}

		std::ifstream input(settings_path);
		if (!input.is_open())
		{
			SKSE::log::error("Unable to open settings file: {}", settings_path.string());
			return false;
		}

		rapidjson::IStreamWrapper stream(input);
		rapidjson::Document document;
		document.ParseStream(stream);
		if (document.HasParseError() || !document.IsObject())
		{
			SKSE::log::error("Invalid settings JSON: {}", settings_path.string());
			return false;
		}

		for (auto profile = document.MemberBegin(); profile != document.MemberEnd(); ++profile)
		{
			if (!profile->value.IsObject()) { continue; }

			auto& destination = a_profiles[profile->name.GetString()];
			for (auto setting = profile->value.MemberBegin();
				 setting != profile->value.MemberEnd(); ++setting)
			{
				if (setting->value.IsNumber())
				{
					destination[setting->name.GetString()] = setting->value.GetFloat();
				}
			}
		}

		if (!a_profiles.contains("Default"))
		{
			SKSE::log::error("Settings JSON does not contain a Default profile");
			return false;
		}

		return true;
	}

	bool Manager::WriteFile(const Profiles& a_profiles) const
	{
		std::ofstream output(path, std::ios::trunc);
		if (!output.is_open())
		{
			SKSE::log::error("Unable to write settings file: {}", path.string());
			return false;
		}

		rapidjson::Document document(rapidjson::kObjectType);
		auto& allocator = document.GetAllocator();
		for (const auto& [profile_name, profile] : a_profiles)
		{
			rapidjson::Value profile_object(rapidjson::kObjectType);
			for (const auto& [key, value] : profile)
			{
				rapidjson::Value json_key;
				json_key.SetString(key.c_str(), static_cast<rapidjson::SizeType>(key.size()), allocator);
				profile_object.AddMember(json_key, value, allocator);
			}

			rapidjson::Value json_profile_name;
			json_profile_name.SetString(profile_name.c_str(),
				static_cast<rapidjson::SizeType>(profile_name.size()), allocator);
			document.AddMember(json_profile_name, profile_object, allocator);
		}

		rapidjson::OStreamWrapper stream(output);
		rapidjson::PrettyWriter<rapidjson::OStreamWrapper> writer(stream);
		writer.SetIndent(' ', 2);
		const bool written = document.Accept(writer);
		output.flush();
		return written && output.good();
	}

	void Manager::PrintSettings() const
	{
		std::shared_lock lock(mutex);
		const auto profile = profiles.find(active_profile);
		if (profile == profiles.end())
		{
			SKSE::log::trace("Settings profile not loaded");
			return;
		}

		SKSE::log::trace("Settings profile: {}", active_profile);
		for (const auto& [key, value] : profile->second)
		{
			SKSE::log::trace("{} : {}", key, value);
		}
	}

	bool RegisterPapyrusFunctions(RE::BSScript::IVirtualMachine* a_vm)
	{
		a_vm->RegisterFunction("GetSetting", kPapyrusScript, PapyrusGetSetting);
		a_vm->RegisterFunction("SetSetting", kPapyrusScript, PapyrusSetSetting);
		a_vm->RegisterFunction("ReloadSettings", kPapyrusScript, PapyrusReloadSettings);
		a_vm->RegisterFunction("LoadProfile", kPapyrusScript, PapyrusLoadProfile);
		a_vm->RegisterFunction("CreateProfile", kPapyrusScript, PapyrusCreateProfile);
		a_vm->RegisterFunction("SaveSettings", kPapyrusScript, PapyrusSaveSettings);
		a_vm->RegisterFunction("GetProfiles", kPapyrusScript, PapyrusGetProfiles);
		a_vm->RegisterFunction("GetActiveProfile", kPapyrusScript, PapyrusGetActiveProfile);
		return true;
	}
}
