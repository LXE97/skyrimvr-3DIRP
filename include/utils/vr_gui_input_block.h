#pragma once

#include <array>
#include <atomic>
#include <cstdint>

namespace vr_gui
{
	enum class InputBlock : std::uint8_t
	{
		kNone = 0,
		kPrimary = 1 << 0,
		kSecondary = 1 << 1,
		kJoystick = 1 << 2,
		kHiggs = 1 << 3,
		kActivatePickLength = 1 << 4,
		kAll = 0x1F
	};

	[[nodiscard]] constexpr InputBlock operator|(InputBlock a_lhs, InputBlock a_rhs) noexcept
	{
		return static_cast<InputBlock>(
			static_cast<std::uint8_t>(a_lhs) | static_cast<std::uint8_t>(a_rhs));
	}

	[[nodiscard]] constexpr InputBlock operator&(InputBlock a_lhs, InputBlock a_rhs) noexcept
	{
		return static_cast<InputBlock>(
			static_cast<std::uint8_t>(a_lhs) & static_cast<std::uint8_t>(a_rhs));
	}

	constexpr InputBlock& operator|=(InputBlock& a_lhs, InputBlock a_rhs) noexcept
	{
		a_lhs = a_lhs | a_rhs;
		return a_lhs;
	}

	class InputBlockManager;

	class InputBlockHandle
	{
	public:
		InputBlockHandle() = default;
		InputBlockHandle(const InputBlockHandle&) = delete;
		InputBlockHandle& operator=(const InputBlockHandle&) = delete;
		InputBlockHandle(InputBlockHandle&& a_rhs) noexcept;
		InputBlockHandle& operator=(InputBlockHandle&& a_rhs) noexcept;
		~InputBlockHandle();

		void Release() noexcept;

		[[nodiscard]] explicit operator bool() const noexcept { return manager != nullptr; }

	private:
		friend class InputBlockManager;

		InputBlockHandle(InputBlockManager* a_manager, bool a_isLeft, InputBlock a_blocks) noexcept;

		InputBlockManager* manager{};
		InputBlock         blocks{ InputBlock::kNone };
		bool               is_left{};
	};

	class InputBlockManager
	{
	public:
		static InputBlockManager* GetSingleton()
		{
			static InputBlockManager singleton;
			return &singleton;
		}

		[[nodiscard]] InputBlockHandle Acquire(bool a_isLeft, InputBlock a_blocks);
		[[nodiscard]] bool             IsBlocked(bool a_isLeft, InputBlock a_block) const noexcept;
		void                           Init();

	private:
		friend class InputBlockHandle;

		static constexpr std::size_t kHandCount = 2;
		static constexpr std::size_t kBlockCount = 4;

		InputBlockManager() = default;
		~InputBlockManager() = default;
		InputBlockManager(const InputBlockManager&) = delete;
		InputBlockManager(InputBlockManager&&) = delete;
		InputBlockManager& operator=(const InputBlockManager&) = delete;
		InputBlockManager& operator=(InputBlockManager&&) = delete;

		void Release(bool a_isLeft, InputBlock a_blocks) noexcept;

		std::array<std::array<std::atomic_uint32_t, kBlockCount>, kHandCount> block_counts{};

		RE::Setting* activate_pick_length_setting{};
		float        activate_pick_length_default{};
		bool         initialized{};

		std::uint32_t activate_pick_length_blocks{};
	};
}
