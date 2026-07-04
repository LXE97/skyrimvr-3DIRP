#pragma once
#include "vrikinterface001.h"

namespace vr_gui
{
	using namespace RE;

	class Hand
	{
	public:
		friend class ModeHandle;

		enum class State
		{
			kReady,
			kGrabbing,
			kWeapon,
			kInvalid
		};

		enum class Mode
		{
			kNormal,
			kPointing,
			kOpen,
			kFist
		};

        enum class ModePriority
        {
            kPassive,
            kGrab,
            kForce
        };

		Hand(bool a_left) : isLeft(a_left) {}

		// intializes device state, returns true if success
		bool Init();
		// fetch higgs state
		bool Update();

		ModeHandle RequestMode(Mode a_mode, ModePriority a_priority);
		void       ClearModes();

		const NiTransform GetTransform() const;
		const State       GetState() const { return state; }
		const float       GetRadius() const { return radius; }
		const NiPoint3*   GetExtents() const { return &extents; }
		const bool        IsLeft() const { return isLeft; }

	private:
		struct ModeRequest
		{
			std::uint64_t id{};
			Mode          mode{ Mode::kNormal };
			int           priority{};
		};

		std::vector<ModeRequest> mode_requests;
		std::uint64_t            next_mode_request_id{ 1 };

		Mode current_mode{ Mode::kNormal };

		bool         isLeft = false;
		NiAVObject*  node{};
		NiTransform* transform{};
		float        radius = 6.f;
		State        state = State::kReady;
		NiPoint3     extents = { 3, 1, 5 };
		NiTransform  offset;
		bool         initialized = false;

		void ReleaseMode(std::uint64_t a_id);
		void RefreshMode();
		void ApplyMode(Mode a_mode);
	};

	class ModeHandle
	{
	public:
		ModeHandle() = default;

		ModeHandle(Hand* a_hand, std::uint64_t a_id) : hand(a_hand), id(a_id) {}

		ModeHandle(const ModeHandle&) = delete;
		ModeHandle& operator=(const ModeHandle&) = delete;

		ModeHandle(ModeHandle&& a_rhs) noexcept
		{
			hand = a_rhs.hand;
			id = a_rhs.id;

			a_rhs.hand = nullptr;
			a_rhs.id = 0;
		}

		ModeHandle& operator=(ModeHandle&& a_rhs) noexcept
		{
			if (this != std::addressof(a_rhs))
			{
				Release();

				hand = a_rhs.hand;
				id = a_rhs.id;

				a_rhs.hand = nullptr;
				a_rhs.id = 0;
			}

			return *this;
		}

		~ModeHandle() { Release(); }

		void Release();

	private:
		Hand*         hand{};
		std::uint64_t id{};
	};

}