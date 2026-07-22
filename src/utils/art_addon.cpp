#include "art_addon.h"

#include "helper_game.h"
#include "helper_math.h"

#include <codecvt>
#include <filesystem>
#include <locale>
#include <string>

namespace art_addon
{
	using namespace RE;

	static constexpr int kMaxPerFrame = 20;

	std::shared_ptr<ArtAddon> ArtAddon::Make(std::string_view a_model_path, TESObjectREFR* a_target,
		NiAVObject* a_attach_node, NiTransform& a_local, std::function<void(ArtAddon*)> a_callback)
	{
		auto manager = ArtAddonManager::GetSingleton();
		auto art_object = manager->GetArtForm(a_model_path);
		if (!art_object) { SKSE::log::error("Art addon failed: invalid model path"); }
		float id = manager->GetNextId();

		/** Using the duration parameter of the BSTempEffect as an ID */
		if (a_target && a_target->IsHandleValid() && a_attach_node)
		{
			SKSE::GetTaskInterface()->AddTask(
				[a_target, art_object, id]() { a_target->ApplyArtObject(art_object, id); });

			auto new_obj = std::shared_ptr<ArtAddon>(new ArtAddon);
			new_obj->art_object = art_object;
			new_obj->local = a_local;
			new_obj->attach_node = a_attach_node;
			new_obj->target = a_target;
			if (a_callback) { new_obj->callback = a_callback; }

			manager->new_objects.emplace(id, new_obj);
			return new_obj;
		}
		else
		{
			SKSE::log::error("Art addon failed: invalid target");
			return std::shared_ptr<ArtAddon>(new ArtAddon);
		}
	}

	void RemoveCollisionNodes(NiAVObject* a_node)
	{
		if (a_node != nullptr)
		{
			if (a_node->collisionObject)
			{
				//SKSE::log::trace("Removing collision from {}", a_node->name.c_str());
				a_node->collisionObject = nullptr;
			}

			if (auto ninode = a_node->AsNode())
			{
				for (auto c : ninode->GetChildren()) { RemoveCollisionNodes(c.get()); }
			}
		}
	}

	// clone the created NiNode and then delete the ModelReferenceEffect and the original NiNode
	//  using the ProcessList.
	void ArtAddonManager::Update()
	{
		if (!new_objects.empty())
		{
			std::scoped_lock lock(objects_lock);
			if (const auto processLists = ProcessLists::GetSingleton())
			{
				processLists->ForEachModelEffect(
					[count = 0, this](ModelReferenceEffect* a_modelEffect) mutable {
						if (a_modelEffect->Get3D())
						{
							float id = a_modelEffect->lifetime;
							if (new_objects.contains(id))
							{
								if (auto addon = new_objects[id].lock())
								{
									// the id is not unique to this mod but the ArtObject is
									if (addon->art_object == a_modelEffect->artObject)
									{
										addon->root3D = static_cast<RE::NiAVObject*>(
											a_modelEffect->Get3D()->Clone());
										addon->attach_node->AsNode()->AttachChild(addon->root3D);
										a_modelEffect->lifetime = 0;
										addon->root3D->local = std::move(addon->local);

										// .nifs with collision will not be drawn when they're attached to an actor
										RemoveCollisionNodes(addon->root3D);
										//helper::StopControllers(addon->root3D);
										if (addon->callback) { addon->callback(addon.get()); }

										//TODO: push targets to a vector, update each target once per update call
										//NiUpdateData ctx;
										//addon->target->Get3D()->Update(ctx);
									}
								}
								else
								{  // check if it's one of our ArtObjects
									auto it = std::find_if(artobject_cache.begin(),
										artobject_cache.end(), [&a_modelEffect](const auto& pair) {
											return pair.second == a_modelEffect->artObject;
										});
									if (it != artobject_cache.end())
									{  // the artAddon was deleted before initialization finished
										a_modelEffect->lifetime = 0;
										SKSE::log::trace("deleting MRE {} (orphaned)", id);
									}
								}
								// finished with this ArtAddon, no longer need to track it
								new_objects.erase(id);

								if (count++ > kMaxPerFrame)
								{
									return BSContainer::ForEachResult::kStop;
								}
							}
						}
						return BSContainer::ForEachResult::kContinue;
					});
			}
			// TODO: temporary measure to update target only once per frame
			NiUpdateData ctx;
			RE::PlayerCharacter::GetSingleton()->Get3D()->Update(ctx);
		}
	}

	void ArtAddonManager::OnGameLoad()
	{
		// Clean up any dangling MREs that slipped through the cracks.
		// They will be deleted next time the game is saved
		if (const auto processLists = RE::ProcessLists::GetSingleton())
		{
			int dangling = 0;
			processLists->ForEachModelEffect([&](RE::ModelReferenceEffect* a_modelEffect) {
				// Temporary artobjects will not be saved, so any MREs that reference them will have
				// a null artobject reference.
				if (!a_modelEffect->artObject)
				{
					// Set the artobject to a valid reference, otherwise the MRE won't be deleted
					a_modelEffect->artObject = base_artobject;
					a_modelEffect->lifetime = 0;
					dangling++;
				}
				return RE::BSContainer::ForEachResult::kContinue;
			});
			if (dangling) { SKSE::log::trace("{} dang MREs deleted", dangling); }
		}
	}

	BGSArtObject* ArtAddonManager::GetArtForm(std::string_view a_model_path)
	{
		auto key = std::string(a_model_path);

		if (auto it = artobject_cache.find(key); it != artobject_cache.end()) { return it->second; }

		if (!base_artobject)
		{
			SKSE::log::error("base ArtObject not found");
			return nullptr;
		}

		auto dupe = base_artobject->CreateDuplicateForm(false, nullptr);
		if (!dupe)
		{
			SKSE::log::error("error creating form for: {}", key);
			return nullptr;
		}

		auto temp = dupe->As<BGSArtObject>();
		if (!temp)
		{
			SKSE::log::error("duplicate form was not a BGSArtObject for: {}", key);
			return nullptr;
		}

		auto [it, inserted] = artobject_cache.emplace(std::move(key), temp);
		temp_artobjects.insert(temp);

		// use the cached std::string storage
		temp->SetModel(it->first.c_str());

		return temp;
	}

	float ArtAddonManager::GetNextId()
	{
		static constexpr float kStep = 1.0f / 1024.0f;
		if (next_id > 1000) { next_id = 0; }
		return 1.0f + static_cast<float>(next_id++) * kStep;
	}

	ArtAddonManager::ArtAddonManager()
	{
		// arbitrary, this is a form from the skyrim trailer that's unlikely to be used by anything else
		constexpr FormID kBaseArtobjectId = 0x9405f;

		base_artobject = TESForm::LookupByID(kBaseArtobjectId)->As<BGSArtObject>();
		if (base_artobject && base_artobject->GetModel())
		{
			SKSE::log::trace("base art object {}", base_artobject->GetModel());
			base_artobject->SetModel(kEmptyNif);
		}
	}

	AddonTextBox::AddonTextBox(std::string_view a_string, const float a_spacing,
		RE::NiAVObject* a_attach_to, RE::NiTransform& a_local, std::string font_path) :
		string(a_string),
		spacing(a_spacing),
		font(font_path)
	{
		std::weak_ptr<LifetimeToken> weak_token = token;

		root = ArtAddon::Make(kEmptyNif, PlayerCharacter::GetSingleton(),
			(a_attach_to ? a_attach_to : PlayerCharacter::GetSingleton()->Get3D()), a_local,
			[weak_token, this](ArtAddon* a) {
				if (auto token = weak_token.lock(); token && token->alive) { MakeString(font); }
			});
	}

	void AddonTextBox::MakeString(std::string font_path)
	{
		if (auto root_node = root ? root->Get3D() : nullptr)
		{
			NiTransform t;
			for (int i = 0; string[i] != '\0'; i++)
			{
				if (string[i] == 0x0A)
				{
					t.translate.y -= kLineSpacing;
					t.translate.x = 0.f;
				}
				else
				{
					characters.push_back(ArtAddon::Make(font_path, PlayerCharacter::GetSingleton(),
						root_node, t, [c = string[i]](ArtAddon* m) {
							if (auto shader =
									helper::GetShaderProperty(m->Get3D(), NifChar::kNodeName))
							{
								auto oldmat = shader->material;
								auto newmat = oldmat->Create();
								newmat->CopyMembers(oldmat);
								shader->material = newmat;
								//newmat->IncRef();
								oldmat->DecRef();

								auto temp = NifChar::AsciiToXY(c);

								newmat->texCoordOffset[0].x = temp.x;
								newmat->texCoordOffset[0].y = temp.y;
								newmat->texCoordOffset[1].x = temp.x;
								newmat->texCoordOffset[1].y = temp.y;
							}
						}));
					t.translate.x += NifChar::kCharacterWidth + spacing;
				}
			}
		}
	}
}