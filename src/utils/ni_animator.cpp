#include "ni_animator.h"

#include "RE/N/NiAVObject.h"

namespace ni_animator
{
	void SetControllerTime(RE::NiAVObject* a_obj, float a_time)
	{
		if (!a_obj) { return; }

		for (auto* controller = a_obj->GetControllers(); controller;
			controller = controller->next.get())
		{
			controller->scaledTime = a_time;
		}

		if (auto* node = a_obj->AsNode())
		{
			for (auto& child : node->children) { SetControllerTime(child.get(), a_time); }
		}
	}

	void SetControllerFlags(
		RE::NiAVObject* a_obj, bool active, bool apptime, bool scaledtime, bool clamp, bool loop)
	{
		if (!a_obj) { return; }

		for (auto* controller = a_obj->GetControllers(); controller;
			controller = controller->next.get())
		{
			controller->flags.set(clamp, RE::NiTimeController::Flag::kCycleType_Clamp);
			controller->flags.set(loop, RE::NiTimeController::Flag::kCycleType_Loop);
			//controller->flags.set(true, RE::NiTimeController::Flag::kForceUpdate);
			controller->flags.set(active, RE::NiTimeController::Flag::kActive);
			controller->flags.set(apptime, RE::NiTimeController::Flag::kAnimType_AppTime);
			controller->flags.set(scaledtime, RE::NiTimeController::Flag::kComputeScaledTime);
		}

		if (auto* node = a_obj->AsNode())
		{
			for (auto& child : node->children)
			{
				SetControllerFlags(child.get(), active, apptime, scaledtime, clamp, loop);
			}
		}
	}

	void NiAnimator::PlayImmediately(const AnimationRange& a_anim, float a_speed, OnFinish a_on_finish)
	{
		queue.clear();

		Entry entry;
		entry.anim = std::addressof(a_anim);
		entry.time = a_anim.start;
		entry.speed = a_speed;
		entry.on_finish = std::move(a_on_finish);

		current = std::move(entry);
	}

	void NiAnimator::Queue(const AnimationRange& a_anim, float a_speed, OnFinish a_on_finish)
	{
		Entry entry;
		entry.anim = std::addressof(a_anim);
		entry.time = a_anim.start;
		entry.speed = a_speed;
		entry.on_finish = std::move(a_on_finish);

		queue.push_back(std::move(entry));
	}

	void NiAnimator::Update(RE::NiAVObject* a_target, float a_delta)
	{
		if (!current && !queue.empty())
		{
			current = std::move(queue.front());
			queue.pop_front();
		}

		if (!current || !a_target) { return; }

		auto& entry = *current;

		entry.time += a_delta * entry.speed;

		RE::NiUpdateData ctx{};

		if (entry.time > entry.anim->end)
		{
			SetControllerTime(a_target, entry.anim->steady);
			a_target->Update(ctx);

			// copy the callback in case it marks the parent for deletion
			auto on_finish = std::move(entry.on_finish);
			current.reset();

			if (on_finish) { on_finish(); }

			return;
		}

		SetControllerTime(a_target, entry.time);
		a_target->Update(ctx);
	}

	void NiAnimator::Stop() { current.reset(); }

	void NiAnimator::ClearQueue() { queue.clear(); }

	void NiAnimator::Clear()
	{
		current.reset();
		queue.clear();
	}

	bool NiAnimator::IsPlaying() const { return current.has_value(); }

	bool NiAnimator::HasQueued() const { return !queue.empty(); }

	std::size_t NiAnimator::QueuedCount() const { return queue.size(); }

	bool NiAnimator::IsBusy() const { return IsPlaying() || HasQueued(); }
}
