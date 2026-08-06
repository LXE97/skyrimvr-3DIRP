#pragma once

#include <deque>
#include <functional>
#include <optional>
#include <shared_mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace vr3dirp
{
	enum class InventoryActionSource : std::uint8_t
	{
		kUnknown,
		kBackpackDrop,
		kWorldPickup,
		kLiveNPCTransfer,
		kPausedTransfer,
		kHiggsStash
	};

	struct ShadowItem
	{
		RE::FormID                     id{};
		std::optional<RE::NiTransform> placement3d;
		RE::ExtraDataList*             extradata{};
		std::int32_t                   count{};
		bool                           stackable{};
		std::uint16_t                  unique_id{};
		InventoryActionSource          source{ InventoryActionSource::kUnknown };
	};

	enum class InventoryChangeType : std::uint8_t
	{
		kAdded,
		kUpdated,
		kRemoved
	};

	struct InventoryChange
	{
		InventoryChangeType type{ InventoryChangeType::kUpdated };
		ShadowItem          item;
	};

	class InventoryManager
	{
	public:
		using InventoryChangedCallback = std::function<void(const std::vector<InventoryChange>&)>;

		static InventoryManager* GetSingleton();

		void OnContainerChanged(const RE::TESContainerChangedEvent* a_event);
		void OnHiggsStashed(bool a_is_left, RE::TESForm* a_form);
		void OnGameLoad();
		void Update();

		void ExpectAddition(RE::FormID a_form_id, std::int32_t a_count,
			InventoryActionSource          a_source,
			std::optional<RE::NiTransform> a_placement = std::nullopt,
			RE::ObjectRefHandle            a_reference = {});
		void ExpectRemoval(RE::FormID a_form_id, std::int32_t a_count,
			InventoryActionSource a_source, RE::ObjectRefHandle a_reference = {});
		void QueueHiggsGrab(RE::ObjectRefHandle a_reference, bool a_is_left);

		// Called by SKSE's serialization save event
		void OnSaveGame(SKSE::SerializationInterface* a_intfc) const;
		bool LoadRecord(SKSE::SerializationInterface* a_intfc, std::uint32_t a_type,
			std::uint32_t a_version, std::uint32_t a_length);
		void Revert();

		[[nodiscard]] std::vector<ShadowItem>        GetItems() const;
		[[nodiscard]] std::unordered_set<RE::FormID> GetIgnoredForms() const;
		[[nodiscard]] std::uint64_t                  GetRevision() const;
		std::uint64_t AddInventoryChangedListener(InventoryChangedCallback a_callback);
		void RemoveInventoryChangedListener(std::uint64_t a_listener_id);

		bool SetPlacement(RE::FormID a_form_id, std::optional<RE::NiTransform> a_placement);
		void IgnoreForm(RE::FormID a_form_id);
		void UnignoreForm(RE::FormID a_form_id);
		[[nodiscard]] bool IsIgnored(RE::FormID a_form_id) const;

	private:
		struct PendingInventoryAction
		{
			RE::FormID                     form_id{};
			std::int32_t                   remaining_count{};
			InventoryActionSource          source{ InventoryActionSource::kUnknown };
			std::optional<RE::NiTransform> placement;
			RE::ObjectRefHandle            reference{};
			std::uint16_t                  unique_id{};
			bool                           addition{};
			std::uint64_t                  frame{};
			std::uint64_t                  sequence{};
		};

		struct QueuedContainerEvent
		{
			RE::FormID          old_container{};
			RE::FormID          new_container{};
			RE::FormID          form_id{};
			std::int32_t        count{};
			RE::ObjectRefHandle reference{};
			std::uint16_t       unique_id{};
			bool                game_paused{};
			std::uint64_t       frame{};
			std::uint64_t       sequence{};
		};

		struct PendingHiggsGrab
		{
			RE::ObjectRefHandle reference{};
			bool                is_left{};
			std::uint64_t       frame{};
		};

		InventoryManager() = default;
		~InventoryManager() = default;
		InventoryManager(const InventoryManager&) = delete;
		InventoryManager(InventoryManager&&) = delete;
		InventoryManager& operator=(const InventoryManager&) = delete;
		InventoryManager& operator=(InventoryManager&&) = delete;

		void                  ScanPlayerInventory(bool a_remove_missing);
		InventoryActionSource ClassifyAddition(const QueuedContainerEvent& a_event) const;
		std::optional<PendingInventoryAction> ConsumePendingAction(
			const QueuedContainerEvent& a_event, bool a_addition);
		void ApplyAction(const QueuedContainerEvent& a_event,
			const PendingInventoryAction* a_pending, InventoryActionSource a_source);
		void QueueExpectedAction(RE::FormID a_form_id, std::int32_t a_count,
			InventoryActionSource a_source, bool a_addition,
			std::optional<RE::NiTransform> a_placement, RE::ObjectRefHandle a_reference);
		void NotifyInventoryChanged();

		mutable std::shared_mutex          mutex_;
		std::vector<ShadowItem>            items_;
		std::unordered_set<RE::FormID>     ignored_forms_;
		std::deque<PendingInventoryAction> pending_actions_;
		std::deque<QueuedContainerEvent>   container_events_;
		std::deque<PendingHiggsGrab>       pending_grabs_;
		std::uint64_t                      frame_{};
		std::uint64_t                      next_sequence_{};
		std::uint64_t                      revision_{};
		std::uint64_t                      next_listener_id_{ 1 };
		std::unordered_map<std::uint64_t, InventoryChangedCallback> inventory_listeners_;
		std::vector<ShadowItem>            last_notified_items_;
	};
}
