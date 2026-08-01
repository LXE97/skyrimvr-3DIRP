/** Library for sticking arbitrary nif files to ObjectReferences.
 * It uses temporary BGSArtObject forms that are recycled on exit to main menu or desktop.
 */
#pragma once

#include <chrono>
#include <functional>
#include <memory>

namespace art_addon
{
	static constexpr const char* kEmptyNif = "effects/fxemptyobject.nif";
	static constexpr const char* kFontAtlas = "3DIRP/char_2048.nif";

	class ArtAddon;
	using ArtAddonPtr = std::shared_ptr<ArtAddon>;

	class ArtAddon
	{
		friend class ArtAddonManager;

	public:
		using OnInitialized = std::function<void(ArtAddon*)>;

		/** Returns: shared_ptr, or nullptr if modelPath or target is invalid
		 * 
		 * Constructs a new ArtAddon and queues the creation of its 3D model. The model is not 
		 * saved to the savefile but it will persist through game loads for the lifetime of the
		 * returned pointer.
         * 
         * a_model_path:	path to the *.nif file relative to Data/meshes/
         * a_target:		object to attach the 3D to
         * a_attach_node:	parent node for the new 3D, must be a 3rd person node for the player
         * a_local:      	transform relative to the attachNode
		 * a_callback:		Callback executed once, as soon as the artaddon's 3D is valid. Can be used
		 * 					to set other NIF properties besides transform
		 */
		[[nodiscard]] static ArtAddonPtr Make(std::string_view a_model_path,
			RE::TESObjectREFR* a_target, RE::NiAVObject* a_attach_node,
			const RE::NiTransform& a_local, OnInitialized a_callback = nullptr,
			bool a_do_deep_clone = false);

		~ArtAddon()
		{
			if (root3D && target && target->Is3DLoaded() && root3D->parent)
			{
				root3D->parent->DetachChild(root3D);
			}
		}

		/** Returns: Pointer to the attached NiAVObject. nullptr if initialization hasn't finished. */
		RE::NiAVObject* Get3D() { return root3D; }
		RE::NiAVObject* GetParent() { return attach_node; }
		RE::TESObjectREFR* GetTarget() { return target; }

		/** Keeps the addon at the requested world transform regardless of its physical parent. */
		void SetWorldTransform(const RE::NiTransform& a_world);

	protected:
		ArtAddon() = default;
		ArtAddon(const ArtAddon&) = delete;
		ArtAddon(ArtAddon&&) = delete;
		ArtAddon& operator=(const ArtAddon&) = delete;
		ArtAddon& operator=(ArtAddon&&) = delete;

		RE::NiAVObject*                root3D = nullptr;
		RE::TESObjectREFR*             target = nullptr;
		RE::BGSArtObject*              art_object = nullptr;
		RE::NiAVObject*                attach_node = nullptr;
		RE::NiTransform                local;
		std::function<void(ArtAddon*)> callback;
		bool                           deep_clone = false;
	};

	class ArtAddonManager
	{
		friend ArtAddon;

	public:
		/** Must be called every frame. It only takes 1 frame to create all the models and remove 
		 * them from the processing queue so this will usually do nothing. */
		void Update();
		/* This is just for cleaning up dangling art objects, should be called on game load */
		void OnGameLoad();

		static ArtAddonManager* GetSingleton()
		{
			static ArtAddonManager singleton;
			return &singleton;
		}

		RE::BGSArtObject* GetBaseObject() { return base_artobject; }

		bool IsTempArtObject(RE::BGSArtObject* a_artobject) const
		{ return temp_artobjects.contains(a_artobject); }

	private:
		ArtAddonManager();
		~ArtAddonManager() = default;
		ArtAddonManager(const ArtAddonManager&) = delete;
		ArtAddonManager(ArtAddonManager&&) = delete;
		ArtAddonManager& operator=(const ArtAddonManager&) = delete;
		ArtAddonManager& operator=(ArtAddonManager&&) = delete;

		RE::BGSArtObject* GetArtForm(std::string_view a_modelPath);
		float             GetNextId();

		std::unordered_map<float, std::weak_ptr<ArtAddon>> new_objects;
		std::mutex                                         objects_lock;
		std::unordered_map<std::string, RE::BGSArtObject*> artobject_cache;
		RE::BGSArtObject*                                  base_artobject;
		int                                                next_id = 1;
		std::unordered_set<RE::BGSArtObject*>              temp_artobjects;
	};

	struct NifChar
	{
	public:
		static constexpr float       kUVOffset_x = 0.0625f;
		static constexpr float       kUVOffset_y = 0.125f;
		static constexpr float       kCharacterWidth = 0.5f;
		static constexpr const char* kFontModelPath = kFontAtlas;
		static constexpr const char* kNodeName = "char";

		static inline RE::NiPoint2 AsciiToXY(char a_ascii)
		{
			char temp = a_ascii - ' ';

			return RE::NiPoint2((temp % 16) * kUVOffset_x, (temp / 16) * kUVOffset_y);
		}
	};

	/* For creation of floating text.*/
	class AddonTextBox
	{
	public:
		static constexpr float kLineSpacing = 0.6f;
		AddonTextBox(std::string_view a_string, float a_spacing, RE::TESObjectREFR* a_target,
			RE::NiAVObject* a_attach_to, const RE::NiTransform& a_local, std::string font_path);
		AddonTextBox(std::string_view a_string, float a_spacing, RE::NiAVObject* a_attach_to,
			const RE::NiTransform& a_local, std::string font_path) :
			AddonTextBox(a_string, a_spacing, RE::PlayerCharacter::GetSingleton(), a_attach_to,
				a_local, std::move(font_path))
		{}

		RE::NiAVObject* Get3D() { return root ? root->Get3D() : nullptr; }
		void            SetWorldTransform(const RE::NiTransform& a_world);

	private:
		struct LifetimeToken
		{
			bool alive = true;
		};

		void MakeString(std::string font_path);

		std::shared_ptr<LifetimeToken> token = std::make_shared<LifetimeToken>();

		ArtAddonPtr              root;
		std::vector<ArtAddonPtr> characters;
		RE::TESObjectREFR*       target{};
		std::string              string;
		const float              spacing;
		std::string              font;
	};

}
