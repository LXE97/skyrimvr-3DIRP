#pragma once

#include <filesystem>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace settings
{
	using QuestFormList = std::unordered_set<RE::FormID>;

	extern QuestFormList g_new_quests_list;

	void PushNewQuest(RE::FormID a_form_id);
	QuestFormList GetNewQuestList();
	void CommitNewQuestList(
		const QuestFormList& a_list_at_open, const QuestFormList& a_current_list);
	void InitQuestSerialization();

	class Manager
	{
	public:
		using Profile = std::map<std::string, float, std::less<>>;

		static Manager* GetSingleton()
		{
			static Manager singleton;
			return &singleton;
		}

		bool Init(std::filesystem::path a_path, std::string a_profile = "Default");
		bool Reload();

		float Get(std::string_view a_key) const;
		bool  Set(std::string_view a_key, float a_value);

		bool LoadProfile(std::string_view a_profile);
		bool CreateProfile(std::string_view a_profile);
		bool Save();

		std::vector<std::string> GetProfiles() const;
		std::string              GetActiveProfile() const;

		void PrintSettings() const;

	private:
		Manager() = default;
		~Manager() = default;
		Manager(const Manager&) = delete;
		Manager(Manager&&) = delete;
		Manager& operator=(const Manager&) = delete;
		Manager& operator=(Manager&&) = delete;

		using Profiles = std::map<std::string, Profile, std::less<>>;

		bool ReadFile(Profiles& a_profiles) const;
		bool WriteFile(const Profiles& a_profiles) const;

		mutable std::shared_mutex mutex;
		std::filesystem::path    path;
		Profiles                 profiles;
		std::string              active_profile = "Default";
		bool                     dirty{};
	};

	bool RegisterPapyrusFunctions(RE::BSScript::IVirtualMachine* a_vm);
}
