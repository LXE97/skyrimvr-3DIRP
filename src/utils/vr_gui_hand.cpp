#include "vr_gui_hand.h"

#include "vr_gui.h"

namespace vr_gui
{
	using namespace RE;

	const NiTransform Hand::GetTransform() const
	{
		if (transform) { return *transform * offset; }
		else
		{
			return offset;
		}
	}

		const NiTransform Hand::GetBoxTransform() const
	{
		if (transform) { return *transform * offset_box; }
		else
		{
			return offset_box;
		}
	}

	bool Hand::Init()
	{
		if (initialized) { return true; }
		// check player 3d loaded
		if (auto pc = RE::PlayerCharacter::GetSingleton()->Get3D(false); pc)
		{
			// link transforms
			if (auto hand_node = pc->GetObjectByName(vrinput::kControllerNodeName[isLeft]);
				hand_node)
			{
				node = hand_node;
				transform = &node->world;
				initialized = true;
				ApplyMode(Mode::kNormal);
				return true;
			}
		}
		node = nullptr;
		transform = nullptr;
		initialized = false;
		return false;
	}

	bool Hand::Update()
	{
		if (Init())
		{
			//query HIGGS state
			if (g_higgsInterface->IsHandInGrabbableState(isLeft)) { state = State::kReady; }
			else if (g_higgsInterface->GetGrabbedObject(isLeft)) { state = State::kGrabbing; }
			else
			{
				state = State::kWeapon;
			}
			return true;
		}
		return false;
	}

	ModeHandle Hand::RequestMode(Mode a_mode, ModePriority a_priority)
	{
				SKSE::log::trace("requesting mode on hand {}", this->IsLeft() ? "left" : "right");
		const auto id = next_mode_request_id++;

		mode_requests.push_back(ModeRequest{ .id = id, .mode = a_mode, .priority = static_cast<int>(a_priority) });

		RefreshMode();

		return ModeHandle{ this, id };
	}

	void Hand::ReleaseMode(std::uint64_t a_id)
	{
		std::erase_if(
			mode_requests, [a_id](const ModeRequest& a_request) { return a_request.id == a_id; });

		RefreshMode();
	}

	void Hand::RefreshMode()
	{
		Mode best_mode = Mode::kNormal;
		int  best_priority = std::numeric_limits<int>::min();

		for (const auto& request : mode_requests)
		{
			if (request.priority >= best_priority)
			{
				best_priority = request.priority;
				best_mode = request.mode;
			}
		}

		if (best_mode == current_mode) { return; }

		current_mode = best_mode;
		ApplyMode(current_mode);
	}

	void Hand::ApplyMode(Mode a_mode)
	{
		switch (a_mode)
		{
		case Mode::kNormal:
			radius = 6.f;
			extents = { 3, 1, 5 };
			offset.rotate = NiMatrix3();
			offset.translate = isLeft ? NiPoint3{ 0, 0.5, 5 } : NiPoint3{ 0, -0.5, 5 };
			offset_box = offset;
			g_vrikInterface->restoreFingers(isLeft);
			break;

		case Mode::kOpen:
			radius = 6.f;
			extents = { 3, 1, 5 };
			offset.translate = isLeft ? NiPoint3{ 0, 0.5, 5 } : NiPoint3{ 0, -0.5, 5 };
			offset.rotate = NiMatrix3();
			offset_box = offset;
			g_vrikInterface->setFingerRange(isLeft, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1);
			break;

		case Mode::kFist:
			radius = 6.f;
			extents = { 3, 1, 5 };
			offset.translate = isLeft ? NiPoint3{ 0, 0.5, 5 } : NiPoint3{ 0, -0.5, 5 };
			offset.rotate = NiMatrix3();
			offset_box = offset;
			g_vrikInterface->setFingerRange(isLeft, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
			break;

		case Mode::kPointing:
			radius = 7.f;
			extents = { 0.7, 0.7, 3.5 };
			offset.translate = isLeft ? NiPoint3{ 0, 0.5, 7.5 } : NiPoint3{ 0, -0.5, 7.5 };
			offset.rotate = NiMatrix3();
			offset_box.translate = isLeft ? NiPoint3{ -2.5, -1.6, 12 } : NiPoint3{ 2.5, -1.6, 12 };
			offset_box.rotate.SetEulerAnglesXYZ(-0.3141593, isLeft ? 0.1396263 : -0.1396263, 0);
			g_vrikInterface->setFingerRange(isLeft, 0.1, 0.1, 1, 1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1);
			break;
		}
	}

	void ModeHandle::Release()
	{
		if (hand)
		{
			hand->ReleaseMode(id);
			hand = nullptr;
			id = 0;
		}
	}
}