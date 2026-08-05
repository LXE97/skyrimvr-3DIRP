#pragma once

#include "exclusive_hover_group.h"
#include "ni_animator.h"
#include "vr_gui.h"
#include "vr_gui_input_block.h"

#include <concepts>
#include <utility>

namespace vr3dirp
{
	using namespace RE;
	using namespace vr_gui;

	struct BackpackSettings
	{
		float       light_fade = 1.f;
		float       window_radius = 50.f;
		float       scale = 1.0f;
		std::string model_path = "3DIRP/Backpack/backpack_player.nif";
	};

	struct BackpackCallbacks
	{};

	class BackpackSettingsOwner
	{
	protected:
		BackpackSettingsOwner(BackpackSettings a_settings, BackpackCallbacks a_callbacks);

		BackpackSettings  settings;
		BackpackCallbacks callbacks;
	};

	class Backpack : public BackpackSettingsOwner, public Widget
	{
		friend class BackpackGrabNode;

	public:
		Backpack(std::string_view a_model_path, bool a_isLeft, TESObjectREFR* a_objectReference,
			BackpackSettings a_settings, BackpackCallbacks a_callbacks = {});
		~Backpack() override;

		static constexpr float kDefaultWindowRadius = 80.f;

	protected:
		bool initialHandIsLeft;
	};

	class BackpackGrabNode : public Widget
	{
	public:
		using Widget::Widget;

		void OnClick(bool a_activate, Hand& a_hand, MenuAction a_action) override;

		void OnHover(bool a_activate, Hand& a_hand) override;

		void Update(float delta) override;

		void StartGrab();

	private:
		static constexpr float kGrabHoldTime = 0.3f;

		void Release();
		void UpdateGrabTransform();

		bool        isGrabbed = false;
		bool        isGrabButtonHeld = false;
		float       grabHoldTime = 0.0f;
		Hand*       grabHand{};

		ModeHandle hand_mode;
	};
}
