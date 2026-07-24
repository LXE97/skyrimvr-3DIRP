#include "vr_gui_input_block.h"

#include "higgsinterface001.h"

#include <memory>
#include <utility>

namespace vr_gui
{
	namespace
	{
		constexpr std::array kBlockTypes{ InputBlock::kPrimary, InputBlock::kSecondary,
			InputBlock::kJoystick, InputBlock::kHiggs };

		[[nodiscard]] constexpr bool Contains(InputBlock a_blocks, InputBlock a_block) noexcept
		{ return (a_blocks & a_block) != InputBlock::kNone; }
	}

	InputBlockHandle::InputBlockHandle(
		InputBlockManager* a_manager, bool a_isLeft, InputBlock a_blocks) noexcept :
		manager(a_manager),
		blocks(a_blocks),
		is_left(a_isLeft)
	{}

	InputBlockHandle::InputBlockHandle(InputBlockHandle&& a_rhs) noexcept :
		manager(std::exchange(a_rhs.manager, nullptr)),
		blocks(std::exchange(a_rhs.blocks, InputBlock::kNone)),
		is_left(a_rhs.is_left)
	{}

	InputBlockHandle& InputBlockHandle::operator=(InputBlockHandle&& a_rhs) noexcept
	{
		if (this != std::addressof(a_rhs))
		{
			Release();
			manager = std::exchange(a_rhs.manager, nullptr);
			blocks = std::exchange(a_rhs.blocks, InputBlock::kNone);
			is_left = a_rhs.is_left;
		}
		return *this;
	}

	InputBlockHandle::~InputBlockHandle() { Release(); }

	void InputBlockHandle::Release() noexcept
	{
		if (manager)
		{
			manager->Release(is_left, blocks);
			manager = nullptr;
			blocks = InputBlock::kNone;
		}
	}

	void InputBlockManager::Init()
	{
		if (initialized) { return; }

		activate_pick_length_setting = RE::GetINISetting("fActivatePickLength:Interface");

		if (activate_pick_length_setting)
		{
			activate_pick_length_default = activate_pick_length_setting->data.f;

			initialized = true;
		}
	}

	InputBlockHandle InputBlockManager::Acquire(bool a_isLeft, InputBlock a_blocks)
	{
		a_blocks = a_blocks & InputBlock::kAll;
		if (a_blocks == InputBlock::kNone) { return {}; }

		const auto hand = static_cast<std::size_t>(a_isLeft);
		for (std::size_t i = 0; i < kBlockTypes.size(); ++i)
		{
			if (!Contains(a_blocks, kBlockTypes[i])) { continue; }

			const auto previous = block_counts[hand][i].fetch_add(1, std::memory_order_relaxed);
			if (kBlockTypes[i] == InputBlock::kHiggs && previous == 0 && g_higgsInterface)
			{
				g_higgsInterface->DisableHand(a_isLeft);
			}
		}

		if (Contains(a_blocks, InputBlock::kActivatePickLength))
		{
			if (activate_pick_length_blocks++ == 0 && activate_pick_length_setting)
			{
				activate_pick_length_setting->data.f = 0.0f;
			}
		}

		return InputBlockHandle(this, a_isLeft, a_blocks);
	}

	bool InputBlockManager::IsBlocked(bool a_isLeft, InputBlock a_block) const noexcept
	{
		if (Contains(a_block, InputBlock::kActivatePickLength) && activate_pick_length_blocks != 0)
		{
			return true;
		}
		
		const auto hand = static_cast<std::size_t>(a_isLeft);
		for (std::size_t i = 0; i < kBlockTypes.size(); ++i)
		{
			if (Contains(a_block, kBlockTypes[i]) &&
				block_counts[hand][i].load(std::memory_order_relaxed) != 0)
			{
				return true;
			}
		}
		return false;
	}

	void InputBlockManager::Release(bool a_isLeft, InputBlock a_blocks) noexcept
	{
		const auto hand = static_cast<std::size_t>(a_isLeft);
		for (std::size_t i = 0; i < kBlockTypes.size(); ++i)
		{
			if (!Contains(a_blocks, kBlockTypes[i])) { continue; }

			const auto previous = block_counts[hand][i].fetch_sub(1, std::memory_order_relaxed);
			if (kBlockTypes[i] == InputBlock::kHiggs && previous == 1 && g_higgsInterface)
			{
				g_higgsInterface->EnableHand(a_isLeft);
			}
		}

		if (Contains(a_blocks, InputBlock::kActivatePickLength))
		{
			if (--activate_pick_length_blocks == 0 && activate_pick_length_setting)
			{
				activate_pick_length_setting->data.f = activate_pick_length_default;
			}
		}
	}
}
