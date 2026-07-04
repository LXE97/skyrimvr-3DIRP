#pragma once

#include <deque>
#include <functional>
#include <optional>

namespace RE
{
	class NiAVObject;
}

namespace ni_animator
{
	struct AnimationRange
	{
		float start{};
		float end{};
		float steady{};
	};

	void SetControllerFlags(
		RE::NiAVObject* a_obj, bool active, bool apptime, bool scaledtime, bool clamp, bool loop);

	void SetControllerTime(RE::NiAVObject* a_obj, float a_time);

	class NiAnimator
	{
	public:
		using OnFinish = std::function<void()>;

		void PlayImmediately(const AnimationRange& a_anim, float a_speed = 1.f, OnFinish a_on_finish = {});
		void Queue(const AnimationRange& a_anim, float a_speed = 1.f, OnFinish a_on_finish = {});

		void Update(RE::NiAVObject* a_target, float a_delta);

		void Stop();
		void ClearQueue();
		void Clear();

		[[nodiscard]] bool IsPlaying() const;
		[[nodiscard]] bool HasQueued() const;
		[[nodiscard]] bool IsBusy() const;

	private:
		struct Entry
		{
			const AnimationRange* anim{};
			float                 time{};
			float                 speed{ 1.f };
			OnFinish              on_finish{};
		};

		std::optional<Entry> current;
		std::deque<Entry>    queue;
	};
}