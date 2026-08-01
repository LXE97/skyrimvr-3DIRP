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
		using Callback = std::function<void()>;

		struct TimedCallback
		{
			float    time{};
			Callback callback{};
		};

		using CallbackQueue = std::deque<TimedCallback>;

		void PlayImmediately(const AnimationRange& a_anim, float a_speed = 1.f,
			CallbackQueue a_callbacks = {});
		void Queue(const AnimationRange& a_anim, float a_speed = 1.f,
			CallbackQueue a_callbacks = {});

		void Update(RE::NiAVObject* a_target, float a_delta);

		void Stop();
		void ClearQueue();
		void Clear();

		void SetSpeed(float a_new_speed)
		{
			if (current) { current->speed = a_new_speed; }
		}

		float GetSpeed() const { return current ? current->speed : 1.0f; }

		[[nodiscard]] bool        IsPlaying() const;
		[[nodiscard]] bool        HasQueued() const;
		[[nodiscard]] std::size_t QueuedCount() const;
		[[nodiscard]] bool        IsBusy() const;

	private:
		struct Entry
		{
			const AnimationRange* anim{};
			float                 time{};
			float                 speed{ 1.f };
			CallbackQueue         callbacks;
		};

		std::optional<Entry> current;
		std::deque<Entry>    queue;
	};
}
