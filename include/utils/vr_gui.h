#pragma once

#include "art_addon.h"
#include "helper_math.h"
#include "higgsinterface001.h"
#include "vr_gui_hand.h"
#include "vrikinterface001.h"
#include "vrinput.h"

#include <unordered_set>

namespace vr_gui
{
	const std::string kPluginName = "VRGUItest.esp";
	const RE::FormID  kActivatorID = 0xD98;
	const RE::FormID  kMarkerModspaceID = 0xD99;

	using namespace RE;
	class Widget;
	class Hand;
	class Controller;

	enum class MenuAction
	{
		kPrimary,
		kSecondary,
		kScrollUp,
		kScrollDown,
		kScrollLeft,
		kScrollRight
	};

	struct ActivatorOverride
	{
		Widget*             owner;
		const Widget*       root;
		bool                isLeft;
		RE::TESBoundObject* base;
		RE::ExtraDataList*  extradata;
		std::string         text;
	};

	class Behavior
	{
	public:
		Behavior(Widget* a_parent) : parent(a_parent) {}

		virtual ~Behavior() = default;

		virtual void Update(float delta) {};

		virtual void OnHover(bool activate, Hand& hand) {};

		virtual void OnDetach() {};

		void Remove()
		{
			marked_for_removal = true;
			OnDetach();
		}

		bool IsMarkedForRemoval() const { return marked_for_removal; }

	protected:
		Widget* parent{};

	private:
		bool marked_for_removal = false;
	};

	// Base View class
	class Widget
	{
	public:
		static constexpr int MAX_DEPTH = 20;

		Widget(Widget* a_parent, NiTransform a_local, NiPoint3 a_halfextents, float a_radius,
			NiAVObject* a_transformParentNode = nullptr);

		Widget(Widget* a_parent, NiTransform a_local, NiPoint3 a_halfextents,
			NiAVObject* a_transformParentNode = nullptr) :
			Widget(a_parent, std::move(a_local), a_halfextents,
				helper::ComputeRadius(a_halfextents), a_transformParentNode)
		{
			base_radius = radius;
			base_extents = extents;
		}

		Widget(float a_radius, TESObjectREFR* a_objectReference,
			NiAVObject*                a_transformParentNode = nullptr,
			std::optional<NiTransform> a_local = std::nullopt);

		virtual ~Widget();

		// Events
		virtual void OnClick(bool a_activate, Hand& a_hand, MenuAction a_action);
		virtual void OnHover(bool a_activate, Hand& a_hand);
		virtual void Update(float a_delta);

		// Tree construction and lookup
		template <typename T, typename... Args>
		T* AddChild(Args&&... args)
		{
			static_assert(std::is_base_of_v<Widget, T>, "T must inherit from Widget");

			children.emplace_back(std::make_unique<T>(this, std::forward<Args>(args)...));
			auto* child = static_cast<T*>(children.back().get());
			OnChildAdded(*child);
			return child;
		}

		template <typename T, typename... Args>
		T* AddBehavior(Args&&... args)
		{
			static_assert(std::is_base_of_v<Behavior, T>, "T must inherit from Behavior");

			behaviors.emplace_back(std::make_unique<T>(this, std::forward<Args>(args)...));
			return static_cast<T*>(behaviors.back().get());
		}

		template <typename T>
		T* GetBehavior()
		{
			static_assert(std::is_base_of_v<Behavior, T>, "T must inherit from Behavior");

			for (const auto& behavior : behaviors)
			{
				if (auto* result = dynamic_cast<T*>(behavior.get())) { return result; }
			}

			return nullptr;
		}

		template <class T>
		T* FindChild()
		{
			for (const auto& c : children)
			{
				if (auto* result = dynamic_cast<T*>(c.get())) { return result; }

				if (auto* result = c->FindChild<T>()) { return result; }
			}

			return nullptr;
		}

		template <class T>
		T* FindEnabledChild()
		{
			for (const auto& c : children)
			{
				if (!c->IsEnabled()) { continue; }

				if (auto* result = dynamic_cast<T*>(c.get())) { return result; }

				if (auto* result = c->FindEnabledChild<T>()) { return result; }
			}

			return nullptr;
		}

		// State and hierarchy accessors
		void SetEnabled(bool a_enabled) { enabled = a_enabled; }
		bool IsEnabled() const { return enabled; }

		void SetHitTestEnabled(bool a_enabled) { hit_test_enabled = a_enabled; }
		bool IsHitTestEnabled() const { return hit_test_enabled; }

		bool IsHidden() const { return hide; }
		bool IsHovered(bool a_isLeft) const { return hover_state[a_isLeft]; }

		int  GetPriority() const { return priority; }
		void SetPriority(int a_priority) { priority = a_priority; }

		Widget*                                     GetParent() const { return parent; }
		const std::vector<std::unique_ptr<Widget>>& GetChildren() const { return children; }
		Widget*                                     GetRoot();
		const Widget*                               GetRoot() const;

		TESObjectREFR* GetObjectReference() const;
		NiAVObject*    GetTransformParentNode() const { return transform_parent_node; }
		void           SetTransformParentNode(NiAVObject* a_node);

		NiTransform&       GetTransform() { return local; }
		const NiTransform& GetTransform() const { return local; }
		NiTransform        GetWorld(int a_depth = 0) const;

		NiAVObject* Get3D() { return model ? model->Get3D() : nullptr; }

		// Widget  operations
		void         SetTransform(NiTransform a_transform);
		void         MoveTo(NiPoint3 a_translateLocal);
		virtual void Resize(float a_scale);
		void         ClearChildren();
		void         AddModel(const std::string_view a_path, bool a_temporaryEffect = false,
			art_addon::ArtAddon::OnInitialized a_callback = nullptr);
		art_addon::AddonTextBox* AddText(
			std::string_view a_text, float a_spacing, std::string_view a_fontPath);

		virtual void Hide();
		virtual void Show();

		// Collision checking
		virtual bool TestOverlap(Hand& a_hand) const;
		virtual bool HandStateFilter(Hand& a_hand) const
		{
			if (parent) return parent->HandStateFilter(a_hand);
			return true;
		}

		virtual void DrawExtents(bool a_show);
		void         ShowHitboxes(bool a_show);

	protected:
		virtual void OnChildAdded(Widget&) {}

		NiTransform local;
		NiPoint3    extents;
		float       radius = 1.f;
		bool        hide = false;

		NiPoint3 base_extents;
		float    base_radius;

		Widget*                                               parent{};
		NiAVObject*                                           transform_parent_node{};
		TESObjectREFR*                                        object_reference{};
		std::vector<std::unique_ptr<Widget>>                  children;
		std::vector<std::unique_ptr<Behavior>>                behaviors;
		art_addon::ArtAddonPtr                                model;
		std::vector<std::unique_ptr<art_addon::AddonTextBox>> text_boxes;

		std::vector<art_addon::ArtAddonPtr> visual_effects;
		bool                                enabled = true;
		bool                                hit_test_enabled = true;
		bool                                hover_state[2] = { false, false };
		int                                 priority = 50;

		// Internal implementation helpers
		void OnHoverImpl(bool a_activate, Hand& a_hand);
		void UpdateImpl(float a_delta);
		void UpdateModelTransform();
		void UpdateOwnModelTransform();

		void RemoveChild(Widget* a_child);

		bool        IsWorldAnchored() const;
		bool        IsTreeWorldAnchored() const;
		bool        TestRootOverlap(Hand& a_hand) const;
		NiAVObject* GetModelAttachmentNode() const;

		friend class Controller;
	};

	class Container : public Widget
	{
		using Widget::Widget;

	public:
		using FilterFunc = std::function<bool(const Widget&)>;
		using SortFunc = std::function<bool(const Widget&, const Widget&)>;

		/* Hides children that don't match the filter function */
		virtual void Filter(FilterFunc f);

		virtual void Sort(SortFunc f);
	};

	/* Automatically arranges children in a grid pattern */
	class GridContainer : public Container
	{
	public:
		GridContainer(Widget* a_parent, NiTransform a_local, NiPoint3 a_halfextents,
			int a_items_per_row, float a_padding) :
			Container(a_parent, std::move(a_local), a_halfextents),
			items_per_row(a_items_per_row),
			padding(a_padding)
		{}

		template <typename T, typename... Args>
		T* AddItem(Args&&... args)
		{
			static_assert(std::is_base_of_v<Widget, T>, "T must inherit from Widget");
			auto* item = AddChild<T>(std::forward<Args>(args)...);
			redraw_counter = 3;
			return item;
		}

		void RemoveGridChild(Widget* a_child);

		void Scroll(int a_adjustment);

		void Update(float delta) override;

		void Filter(FilterFunc f) override;

		void Sort(SortFunc f) override;

		void Redraw();

		int GetScrollPosition() const { return scroll_position; }

		int GetVisibleRows() const { return visible_index / items_per_row; }

	private:
		int redraw_counter = 0;

		int   items_per_row = 1;
		int   visible_index = 0;
		float padding = 0.f;
		int   scroll_position = 0.f;
		int   last_scroll_position = 0.f;
		float item_radius = 1.f;

		SortFunc   current_sort{};
		FilterFunc current_filter{};
	};

	class Controller
	{
	public:
		const std::string kRolloverNodeName = "WSActivateRollover";

		struct ButtonEvent
		{
			bool       isLeft;
			MenuAction button;
			bool       down;
		};

		struct Settings
		{
			vr::EVRButtonId primary = vr::EVRButtonId::k_EButton_SteamVR_Trigger;
			vr::EVRButtonId secondary = vr::EVRButtonId::k_EButton_Grip;
		};

		static Controller* GetSingleton()
		{
			static Controller singleton;
			return &singleton;
		}

		RE::TESObjectREFR* GetActivator() { return activator_obj; }
		Hand*              GetHand(bool a_isLeft) { return &hands[a_isLeft]; }

		Widget* AddRoot(std::unique_ptr<Widget> a_root)
		{
			roots.emplace_back(std::move(a_root));
#ifdef HUD_OVERRIDES
			SKSE::GetTaskInterface()->AddTask([this]() {
				if (activator_obj)
				{
					activator_obj->MoveTo(RE::PlayerCharacter::GetSingleton()->AsReference());
				}
			});
#endif
			return roots.back().get();
		}

		void Update();
		void Init();
		void Cleanup();
		void HandleHUDOverrides();

		void ShowHitboxes(bool a_show);

		void MarkForDelete(Widget* a_widget);

		void PushActivatorOverride(ActivatorOverride&& a_data);
		void RemoveActivatorOverride(Widget* a_owner);
		void RemoveActivatorOverride(Widget* a_owner, bool a_isLeft);

		void AcquireInputBlock();
		void ReleaseInputBlock(bool a_force = false);

	private:
		Controller() = default;
		~Controller() = default;
		Controller(const Controller&) = delete;
		Controller(Controller&&) = delete;
		Controller& operator=(const Controller&) = delete;
		Controller& operator=(Controller&&) = delete;

		void HandleInput();
		void HandleEvents();
		void HandleDeletionQueue();

		bool        InputEventHandler(const vrinput::ModInputEvent& e);
		static bool InputEventHandlerStatic(const vrinput::ModInputEvent& e);

		void TraverseCollision(Widget& a_w, Hand& a_hand, std::vector<Widget*>& a_hover_list);

		// HUD override handling
		void ClearHUDOverride();
		void SetHUDOverride(ActivatorOverride& a_data);
		void ClearExtraData(RE::TESObjectREFR* a_obj);
		void ToggleActivator(bool a_enabled);
		void RemoveActivatorOverridesForRoot(Widget* a_root);
		void RemoveRoot(Widget* a_root);

		bool IsValid(Widget* a_target) const;
		bool IsValid(Widget* a_root, Widget* a_target) const;

		std::vector<std::unique_ptr<Widget>> roots;
		std::vector<Widget*>                 widgets_to_delete;
		std::vector<ButtonEvent>             button_queue;
		std::vector<ActivatorOverride>       activator_overrides;

		std::unordered_map<Hand*, std::unordered_map<Widget*, std::vector<Widget*>>> hovered_map;

		RE::TESObjectREFR*  activator_obj = nullptr;
		RE::TESObjectREFR*  modspacemarker_obj = nullptr;
		RE::TESForm*        player_form_ref = nullptr;
		RE::TESBoundObject* activator_default_base = nullptr;

		int input_block_counter = 0;

		std::chrono::steady_clock::time_point last_update_time{ std::chrono::steady_clock::now() };

		// element 0 = right hand, 1 = left hand
		std::vector<Hand> hands;

		// higgs save
		double FarCastDistance;
		double NearCastDistance;

		Settings settings;

		RE::NiTransform rollover_default_hand;
		RE::NiPoint3    rollover_default_hand_pos;
		RE::NiMatrix3   rollover_default_hand_rot;
		float           factivatepicklength_default;

		bool initialized = false;
	};

	inline void PostWandUpdate() { Controller::GetSingleton()->HandleHUDOverrides(); }

}
