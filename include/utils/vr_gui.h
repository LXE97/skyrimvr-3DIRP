#pragma once

#include "art_addon.h"
#include "helper_math.h"
#include "higgsinterface001.h"
#include "vrikinterface001.h"
#include "vrinput.h"
#include "vr_gui_hand.h"

#include <unordered_set>

namespace vr_gui
{
	const std::string kPluginName = "VRGUItest.esp";
	const RE::FormID  kActivatorID = 0xD98;
	const RE::FormID  kMarkerModspaceID = 0xD99;

	using namespace RE;
	class Widget;
	class Hand;
	class Window;
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
		const Window*       parent;
		bool                isLeft;
		RE::TESBoundObject* base;
		RE::ExtraDataList*  extradata;
		std::string         text;
	};

	// observes and caches input device state
	

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
		Widget(Widget* a_parent, NiTransform a_local, NiPoint3 a_halfextents, float a_radius);

		Widget(Widget* a_parent, NiTransform a_local, NiPoint3 a_halfextents) :
			Widget(
				a_parent, std::move(a_local), a_halfextents, helper::ComputeRadius(a_halfextents))
		{
			base_radius = radius;
			base_extents = extents;
		}
		virtual ~Widget() = default;

		void SetEnabled(bool a_enabled) { enabled = a_enabled; };
		bool IsEnabled() const { return enabled; };

		virtual bool TestOverlap(Hand& a_hand) const;

		virtual bool HandStateFilter(Hand& a_hand) const { return true; }

		virtual void OnClick(bool a_activate, Hand& a_hand, MenuAction a_action);
		virtual void OnHover(bool a_activate, Hand& a_hand);
		void         OnHoverImpl(bool a_activate, Hand& a_hand);

		virtual void Hide();
		virtual void Show();
		bool         IsHidden() const { return hide; }

		void         MoveTo(NiPoint3 a_translate_local);
		virtual void Resize(float a_scale);

		bool IsHovered(bool a_isLeft) const { return hover_state[a_isLeft]; };

		virtual void Update(float delta);

		template <typename T, typename... Args>
		T* AddChild(Args&&... args)
		{
			static_assert(std::is_base_of_v<Widget, T>, "T must inherit from Widget");

			children.emplace_back(std::make_unique<T>(this, std::forward<Args>(args)...));
			return static_cast<T*>(children.back().get());
		}

		virtual void DrawExtents(bool show);
		void         ShowHitboxes(bool show);

		Widget*                                     GetParent() const { return parent; }
		const std::vector<std::unique_ptr<Widget>>& GetChildren() const { return children; }

		// finds the base of the hierarchy tree
		const Window* GetWindow() const;
		Window*       GetWindow();

		NiTransform& GetTransform() { return local; }
		void         SetTransform(NiTransform a_t)
		{
			NiUpdateData ctx;
			local = a_t;
			if (auto node = Get3D())
			{
				node->local.rotate = a_t.rotate;
				node->local.scale = a_t.scale;
				node->Update(ctx);
			}
		}

		virtual NiTransform GetWorld(int depth = 0) const;
		inline NiTransform  GetLocal() const { return local; }

		int GetPriority() const { return priority; };

		virtual void AddModel(const std::string& a_path, bool a_tempeffect = false,
			std::function<void(art_addon::ArtAddon*)> a_3DInitializedCallback = nullptr);

		NiAVObject* Get3D() { return model ? model->Get3D() : nullptr; }

		void UpdateImpl(float delta);

		void UpdateModelTransform();

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

	protected:
		NiTransform local;
		NiPoint3    extents;
		float       radius = 1.f;
		bool        hide = false;

		NiPoint3 base_extents;
		float    base_radius;

		Widget*                                parent{};
		std::vector<std::unique_ptr<Widget>>   children;
		std::vector<std::unique_ptr<Behavior>> behaviors;
		art_addon::ArtAddonPtr                 model;

		std::vector<art_addon::ArtAddonPtr> visual_effects;
		bool                                enabled = true;
		bool                                hover_state[2] = { false, false };
		int                                 priority = 9;

		void RemoveChild(Widget* a_child);

		NiTransform GetLocalToRoot() const;

		static constexpr int MAX_DEPTH = 20;

		friend class Controller;
	};

	/* Root of all Widget trees, has an ObjectReference for attaching models.
    The radius represents the range at which UI collision detection will be activated.
    */
	class Window : public Widget
	{
	public:
		Window(float a_radius, RE::TESObjectREFR* a_rootobj, RE::NiNode* a_parent_node,
			std::optional<RE::NiTransform> a_local = std::nullopt) :
			Widget(nullptr, a_local.value_or(RE::NiTransform{}), RE::NiPoint3{}, a_radius),
			rootobj(a_rootobj),
			parent_node(a_parent_node)
		{}
		virtual ~Window() = default;

		TESObjectREFR* GetObjRef() const { return rootobj; }

		virtual NiNode* GetRootNode() const
		{ return parent_node ? parent_node : rootobj->Get3D()->AsNode(); }

		bool TestOverlap(Hand& a_Hand) const override;

		void DrawExtents(bool show) override;

		void OnHover(bool a_activate, Hand& a_hand) override;

		virtual NiTransform GetWorld(int depth = 0) const;

		void AddModel(const std::string& a_path, bool a_tempeffect = false,
			std::function<void(art_addon::ArtAddon*)> a_3DInitializedCallback = nullptr);

	protected:
		TESObjectREFR* rootobj{};
		NiNode*        parent_node{};
	};

	/* Window which stays attached to a node on the target ObjectReference. If no node is specified,
	defaults to the root of the ObjectReference skeleton */
	class AttachedWindow : public Window
	{
	public:
		AttachedWindow(float a_radius, RE::TESObjectREFR* a_rootobj, RE::NiNode* a_parent_node,
			std::optional<RE::NiTransform> a_local = std::nullopt);
		virtual ~AttachedWindow() = default;
	};

	/* Window that floats in a desired world position. It is actually attached to the ObjectReference,
	but uses an empty .nif as the parent of all child Widgets, which is updated every frame to remain in place
	Because this .nif takes 1 frame to create, an optional initialization callback is provided*/
	class FloatingWindow : public Window
	{
	public:
		FloatingWindow(float a_radius, RE::TESObjectREFR* a_rootobj, RE::NiTransform a_world,
			std::function<void(FloatingWindow*)> a_3DInitializedCallback = nullptr);
		virtual ~FloatingWindow() = default;

		NiTransform GetWorld(int depth = 0) const;

		NiNode* GetRootNode() const;

	protected:
		void                                 Update(float delta);
		std::function<void(FloatingWindow*)> InitializedCallback;
	};

	/* Interacts via button presses */
	class SimpleButton : public Widget
	{
		using Callback = std::function<void(SimpleButton*, bool activate, Hand&, MenuAction)>;

	public:
		SimpleButton(Widget* a_parent, NiTransform a_local, NiPoint3 a_halfextents, Callback cb) :
			Widget(a_parent, std::move(a_local), a_halfextents),
			callback_(std::move(cb))
		{}

		void OnClick(bool a_activate, Hand& a_hand, MenuAction a_action) override
		{
			if (callback_) callback_(this, a_activate, a_hand, a_action);
		}

	private:
		Callback callback_;
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

		const std::string kRolloverNodeName = "WSActivateRollover";

		void Update();
		void Init();
		void Cleanup();
		void HandleHUDOverrides();

		Window* AddWindow(std::unique_ptr<Window> a_new)
		{
			windows.emplace_back(std::move(a_new));
#ifdef HUD_OVERRIDES
			SKSE::GetTaskInterface()->AddTask([this]() {
				if (activator_obj)
				{
					activator_obj->MoveTo(RE::PlayerCharacter::GetSingleton()->AsReference());
				}
			});
#endif
			return windows.back().get();
		}

		void ShowHitboxes(bool show);

		void MarkForDelete(Widget* w);

		void PushActivatorOverride(ActivatorOverride&& a_data);
		void RemoveActivatorOverride(Widget* a_owner);
		void RemoveActivatorOverride(Widget* a_owner, bool a_isLeft);
		void RemoveActivatorOverride(Window* a_parent);

		void AcquireInputBlock();
		void ReleaseInputBlock(bool a_force = false);

		RE::TESObjectREFR* GetActivator() { return activator_obj; }

		static Controller* GetSingleton()
		{
			static Controller singleton;
			return &singleton;
		}

		Hand* GetHand(bool isLeft) { return &hands[isLeft]; }

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
		void RemoveWindow(Window* a_del);

		bool IsValid(Widget* a_target) const;
		bool IsValid(Widget* a_root, Widget* a_target) const;

		std::vector<std::unique_ptr<Window>> windows;
		std::vector<Widget*>                 widgets_to_delete;
		std::vector<ButtonEvent>             button_queue;
		std::vector<ActivatorOverride>       activator_overrides;

		std::unordered_map<Hand*, std::unordered_map<Window*, std::vector<Widget*>>> hovered_map;

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

	inline RE::NiNode* GetControllerNode(bool isLeft)
	{
		return isLeft ?
			RE::PlayerCharacter::GetSingleton()->GetVRNodeData()->LeftWandNode->AsNode() :
			RE::PlayerCharacter::GetSingleton()->GetVRNodeData()->RightWandNode->AsNode();
	}

}