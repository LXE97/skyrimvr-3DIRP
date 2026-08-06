#include "inventory_manager.h"

#include "RE/E/ExtraUniqueID.h"
#include "higgsinterface001.h"
#include "menu_checker.h"

namespace vr3dirp
{
	namespace
	{
		constexpr std::uint32_t MakeRecordType(char a, char b, char c, char d)
		{
			return static_cast<std::uint32_t>(a) | (static_cast<std::uint32_t>(b) << 8) |
				(static_cast<std::uint32_t>(c) << 16) | (static_cast<std::uint32_t>(d) << 24);
		}

		constexpr std::uint32_t kShadowItemsRecord = MakeRecordType('S', 'H', 'D', 'W');
		constexpr std::uint32_t kIgnoredFormsRecord = MakeRecordType('I', 'G', 'N', 'R');
		constexpr std::uint32_t kRecordVersion = 1;
		constexpr std::uint32_t kTransformFloatCount = 13;

		bool WriteTransform(SKSE::SerializationInterface* a_intfc, const RE::NiTransform& a_transform)
		{
			for (std::size_t row = 0; row < 3; ++row)
			{
				for (std::size_t column = 0; column < 3; ++column)
				{
					if (!a_intfc->WriteRecordData(a_transform.rotate.entry[row][column])) { return false; }
				}
			}

			return a_intfc->WriteRecordData(a_transform.translate.x) &&
				a_intfc->WriteRecordData(a_transform.translate.y) &&
				a_intfc->WriteRecordData(a_transform.translate.z) &&
				a_intfc->WriteRecordData(a_transform.scale);
		}

		bool ReadTransform(SKSE::SerializationInterface* a_intfc, RE::NiTransform& a_transform)
		{
			for (std::size_t row = 0; row < 3; ++row)
			{
				for (std::size_t column = 0; column < 3; ++column)
				{
					if (a_intfc->ReadRecordData(a_transform.rotate.entry[row][column]) != sizeof(float))
					{
						return false;
					}
				}
			}

			return a_intfc->ReadRecordData(a_transform.translate.x) == sizeof(float) &&
				a_intfc->ReadRecordData(a_transform.translate.y) == sizeof(float) &&
				a_intfc->ReadRecordData(a_transform.translate.z) == sizeof(float) &&
				a_intfc->ReadRecordData(a_transform.scale) == sizeof(float);
		}

		bool SameInventoryIdentity(const ShadowItem& a_left, const ShadowItem& a_right)
		{
			if (a_left.id != a_right.id) { return false; }
			if (a_left.unique_id || a_right.unique_id)
			{
				return a_left.unique_id && a_left.unique_id == a_right.unique_id;
			}
			if (a_left.extradata && a_right.extradata)
			{
				return a_left.extradata == a_right.extradata;
			}
			return true;
		}

		bool SameInventoryValue(const ShadowItem& a_left, const ShadowItem& a_right)
		{
			return SameInventoryIdentity(a_left, a_right) &&
				a_left.placement3d == a_right.placement3d &&
				a_left.extradata == a_right.extradata && a_left.count == a_right.count &&
				a_left.stackable == a_right.stackable && a_left.source == a_right.source;
		}
	}

	InventoryManager* InventoryManager::GetSingleton()
	{
		static InventoryManager singleton;
		return &singleton;
	}

	void InventoryManager::OnContainerChanged(const RE::TESContainerChangedEvent* a_event)
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!a_event || !player) { return; }

		const auto player_id = player->GetFormID();
		if (a_event->oldContainer != player_id && a_event->newContainer != player_id) { return; }

		std::unique_lock lock(mutex_);
		container_events_.push_back({ .old_container = a_event->oldContainer,
			.new_container = a_event->newContainer,
			.form_id = a_event->baseObj,
			.count = std::max(1, std::abs(a_event->itemCount)),
			.reference = a_event->reference,
			.unique_id = a_event->uniqueID,
			.game_paused = menuchecker::isGameStopped(),
			.frame = frame_,
			.sequence = next_sequence_++ });
	}

	void InventoryManager::OnHiggsStashed(bool, RE::TESForm* a_form)
	{
		if (a_form)
		{
			ExpectAddition(a_form->GetFormID(), 1, InventoryActionSource::kHiggsStash);
		}
	}

	void InventoryManager::ExpectAddition(RE::FormID a_form_id, std::int32_t a_count,
		InventoryActionSource a_source, std::optional<RE::NiTransform> a_placement,
		RE::ObjectRefHandle a_reference)
	{
		QueueExpectedAction(a_form_id, a_count, a_source, true, std::move(a_placement),
			std::move(a_reference));
	}

	void InventoryManager::ExpectRemoval(RE::FormID a_form_id, std::int32_t a_count,
		InventoryActionSource a_source, RE::ObjectRefHandle a_reference)
	{
		QueueExpectedAction(a_form_id, a_count, a_source, false, std::nullopt,
			std::move(a_reference));
	}

	void InventoryManager::QueueHiggsGrab(RE::ObjectRefHandle a_reference, bool a_is_left)
	{
		if (!a_reference) { return; }
		std::unique_lock lock(mutex_);
		pending_grabs_.push_back(
			{ .reference = std::move(a_reference), .is_left = a_is_left, .frame = frame_ });
	}

	void InventoryManager::QueueExpectedAction(RE::FormID a_form_id, std::int32_t a_count,
		InventoryActionSource a_source, bool a_addition,
		std::optional<RE::NiTransform> a_placement, RE::ObjectRefHandle a_reference)
	{
		if (!a_form_id || a_count <= 0) { return; }
		std::uint16_t unique_id{};
		if (auto reference = a_reference.get())
		{
			if (const auto* unique_data = reference->extraList.GetByType<RE::ExtraUniqueID>())
			{
				unique_id = unique_data->uniqueID;
			}
		}

		std::unique_lock lock(mutex_);
		pending_actions_.push_back({ .form_id = a_form_id,
			.remaining_count = a_count,
			.source = a_source,
			.placement = std::move(a_placement),
			.reference = std::move(a_reference),
			.unique_id = unique_id,
			.addition = a_addition,
			.frame = frame_,
			.sequence = next_sequence_++ });
	}

	std::optional<InventoryManager::PendingInventoryAction>
	InventoryManager::ConsumePendingAction(
		const QueuedContainerEvent& a_event, bool a_addition)
	{
		std::unique_lock lock(mutex_);
		auto best = pending_actions_.end();
		int best_score = -1;
		for (auto action = pending_actions_.begin(); action != pending_actions_.end(); ++action)
		{
			if (action->addition != a_addition || action->form_id != a_event.form_id) { continue; }
			if (action->unique_id && a_event.unique_id && action->unique_id != a_event.unique_id)
			{
				continue;
			}
			if (action->reference && a_event.reference && action->reference != a_event.reference)
			{
				continue;
			}

			int score = 0;
			if (action->unique_id && action->unique_id == a_event.unique_id) { score += 4; }
			if (action->reference && action->reference == a_event.reference) { score += 4; }
			if (action->placement) { score += 2; }
			if (score > best_score)
			{
				best = action;
				best_score = score;
			}
		}

		if (best == pending_actions_.end()) { return std::nullopt; }
		auto result = *best;
		best->remaining_count -= std::min(best->remaining_count, a_event.count);
		if (best->remaining_count <= 0) { pending_actions_.erase(best); }
		return result;
	}

	InventoryActionSource InventoryManager::ClassifyAddition(
		const QueuedContainerEvent& a_event) const
	{
		if (a_event.game_paused) { return InventoryActionSource::kPausedTransfer; }
		if (a_event.reference) { return InventoryActionSource::kWorldPickup; }

		if (a_event.old_container)
		{
			auto* old_container = RE::TESForm::LookupByID(a_event.old_container);
			if (old_container && old_container->As<RE::Actor>())
			{
				return InventoryActionSource::kLiveNPCTransfer;
			}
			return InventoryActionSource::kWorldPickup;
		}

		return InventoryActionSource::kUnknown;
	}

	void InventoryManager::ApplyAction(const QueuedContainerEvent& a_event,
		const PendingInventoryAction* a_pending, InventoryActionSource a_source)
	{
		std::unique_lock lock(mutex_);
		const auto unique_id =
			a_event.unique_id ? a_event.unique_id : a_pending ? a_pending->unique_id : 0;
		auto item = std::ranges::find_if(items_, [&a_event, unique_id](const ShadowItem& a_item) {
			return a_item.id == a_event.form_id &&
				(!unique_id || !a_item.unique_id || a_item.unique_id == unique_id);
		});
		if (item == items_.end()) { return; }

		switch (a_source)
		{
		case InventoryActionSource::kBackpackDrop:
			item->source = InventoryActionSource::kBackpackDrop;
			if (a_pending && a_pending->placement) { item->placement3d = a_pending->placement; }
			break;
		case InventoryActionSource::kWorldPickup:
			item->source = InventoryActionSource::kWorldPickup;
			break;
		case InventoryActionSource::kLiveNPCTransfer:
			item->source = InventoryActionSource::kLiveNPCTransfer;
			break;
		case InventoryActionSource::kPausedTransfer:
			item->source = InventoryActionSource::kPausedTransfer;
			break;
		case InventoryActionSource::kHiggsStash:
			item->source = InventoryActionSource::kHiggsStash;
			break;
		case InventoryActionSource::kUnknown:
		default:
			item->source = InventoryActionSource::kUnknown;
			break;
		}
		++revision_;
	}

	void InventoryManager::Update()
	{
		constexpr std::uint64_t kCorrelationDelayFrames = 2;
		constexpr std::uint64_t kPendingLifetimeFrames = 8;
		std::vector<QueuedContainerEvent> ready_events;
		{
			std::unique_lock lock(mutex_);
			++frame_;
			for (auto event = container_events_.begin(); event != container_events_.end();)
			{
				if (frame_ - event->frame < kCorrelationDelayFrames)
				{
					++event;
					continue;
				}
				ready_events.push_back(*event);
				event = container_events_.erase(event);
			}
		}

		auto* player = RE::PlayerCharacter::GetSingleton();
		const auto player_id = player ? player->GetFormID() : 0;
		for (const auto& event : ready_events)
		{
			const bool addition = event.new_container == player_id;
			auto pending = ConsumePendingAction(event, addition);
			const auto source = pending ? pending->source :
				addition             ? ClassifyAddition(event) : InventoryActionSource::kUnknown;

			ScanPlayerInventory(true);
			if (addition) { ApplyAction(event, pending ? &*pending : nullptr, source); }
		}
		if (!ready_events.empty()) { NotifyInventoryChanged(); }

		std::deque<PendingHiggsGrab> grabs;
		std::uint64_t current_frame{};
		{
			std::unique_lock lock(mutex_);
			std::erase_if(pending_actions_, [this](const PendingInventoryAction& a_action) {
				return frame_ - a_action.frame > kPendingLifetimeFrames;
			});
			current_frame = frame_;
			grabs.swap(pending_grabs_);
		}

		std::deque<PendingHiggsGrab> retry_grabs;
		for (auto& grab : grabs)
		{
			auto reference = grab.reference.get();
			if (g_higgsInterface && reference && reference->Get3D(false) &&
				g_higgsInterface->CanGrabObject(grab.is_left))
			{
				g_higgsInterface->GrabObject(reference.get(), grab.is_left);
			}
			else if (current_frame - grab.frame <= kPendingLifetimeFrames)
			{
				retry_grabs.push_back(std::move(grab));
			}
		}

		if (!retry_grabs.empty())
		{
			std::unique_lock lock(mutex_);
			for (auto& grab : retry_grabs) { pending_grabs_.push_back(std::move(grab)); }
		}
	}

	void InventoryManager::OnGameLoad()
	{
		ScanPlayerInventory(false);
		NotifyInventoryChanged();
	}

	void InventoryManager::ScanPlayerInventory(bool a_remove_missing)
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player) { return; }

		struct InventoryItem
		{
			RE::FormID         id;
			RE::ExtraDataList* extra_data;
			std::int32_t       count;
			bool               stackable;
			std::uint16_t      unique_id;
		};

		std::vector<InventoryItem> inventory_items;
		for (auto& [object, inventory_data] : player->GetInventory())
		{
			const auto count = inventory_data.first;
			auto*      entry = inventory_data.second.get();
			if (!object || !entry || count <= 0) { continue; }

			std::int32_t represented_count = 0;
			if (entry->extraLists)
			{
				for (auto* list : *entry->extraLists)
				{
					if (!list || represented_count >= count) { continue; }
					const auto list_count =
						std::min(count - represented_count, std::max(1, list->GetCount()));
					const auto* unique_data = list->GetByType<RE::ExtraUniqueID>();
					const std::uint16_t unique_id =
						unique_data ? unique_data->uniqueID : std::uint16_t{};
					inventory_items.push_back(
						InventoryItem{ object->GetFormID(), list, list_count, false, unique_id });
					represented_count += list_count;
				}
			}

			if (represented_count < count)
			{
				inventory_items.push_back(
					{ object->GetFormID(), nullptr, count - represented_count, true, 0 });
			}
		}

		std::unique_lock lock(mutex_);
		for (const auto& inventory_item : inventory_items)
		{
			if (ignored_forms_.contains(inventory_item.id)) { continue; }

			auto item = std::ranges::find_if(items_, [&inventory_item](const ShadowItem& a_item) {
				if (a_item.id != inventory_item.id) { return false; }
				if (inventory_item.unique_id)
				{
					return !a_item.unique_id || a_item.unique_id == inventory_item.unique_id;
				}
				if (inventory_item.extra_data)
				{
					return a_item.extradata == inventory_item.extra_data ||
						(!a_item.extradata && !a_item.unique_id);
				}
				return !a_item.extradata && !a_item.unique_id;
			});
			if (item == items_.end())
			{
				items_.push_back({ .id = inventory_item.id,
					.placement3d = std::nullopt,
					.extradata = inventory_item.extra_data,
					.count = inventory_item.count,
					.stackable = inventory_item.stackable,
					.unique_id = inventory_item.unique_id,
					.source = InventoryActionSource::kUnknown });
			}
			else
			{
				item->extradata = inventory_item.extra_data;
				item->count = inventory_item.count;
				item->stackable = inventory_item.stackable;
				item->unique_id = inventory_item.unique_id;
			}
		}

		if (a_remove_missing)
		{
			std::erase_if(items_, [this, &inventory_items](const ShadowItem& a_item) {
				if (ignored_forms_.contains(a_item.id)) { return true; }
				return std::ranges::none_of(inventory_items, [&a_item](const InventoryItem& a_live) {
					if (a_live.id != a_item.id) { return false; }
					if (a_item.unique_id) { return a_live.unique_id == a_item.unique_id; }
					if (a_item.extradata) { return a_live.extra_data == a_item.extradata; }
					return !a_live.extra_data && !a_live.unique_id;
				});
			});
		}
		++revision_;
	}

	void InventoryManager::OnSaveGame(SKSE::SerializationInterface* a_intfc) const
	{
		std::vector<ShadowItem>        placed_items;
		std::unordered_set<RE::FormID> ignored_forms;
		{
			std::shared_lock lock(mutex_);
			for (const auto& item : items_)
			{
				if (item.placement3d) { placed_items.push_back(item); }
			}
			ignored_forms = ignored_forms_;
		}

		if (a_intfc->OpenRecord(kShadowItemsRecord, kRecordVersion))
		{
			const auto count = static_cast<std::uint32_t>(placed_items.size());
			bool       success = a_intfc->WriteRecordData(count);
			for (const auto& item : placed_items)
			{
				success = success && a_intfc->WriteRecordData(item.id) &&
					a_intfc->WriteRecordData(item.count) &&
					a_intfc->WriteRecordData(item.stackable) &&
					WriteTransform(a_intfc, *item.placement3d);
			}
			if (!success) { SKSE::log::error("Unable to serialize shadow inventory"); }
		}
		else
		{
			SKSE::log::error("Unable to open shadow inventory serialization record");
		}

		if (a_intfc->OpenRecord(kIgnoredFormsRecord, kRecordVersion))
		{
			const auto count = static_cast<std::uint32_t>(ignored_forms.size());
			bool       success = a_intfc->WriteRecordData(count);
			for (const auto form_id : ignored_forms)
			{
				success = success && a_intfc->WriteRecordData(form_id);
			}
			if (!success) { SKSE::log::error("Unable to serialize ignored inventory forms"); }
		}
		else
		{
			SKSE::log::error("Unable to open ignored inventory forms serialization record");
		}
	}

	bool InventoryManager::LoadRecord(SKSE::SerializationInterface* a_intfc,
		std::uint32_t a_type, std::uint32_t a_version, std::uint32_t a_length)
	{
		if (a_type != kShadowItemsRecord && a_type != kIgnoredFormsRecord) { return false; }
		if (a_version != kRecordVersion)
		{
			SKSE::log::warn("Unsupported inventory serialization version {}", a_version);
			return true;
		}

		std::uint32_t count{};
		if (a_length < sizeof(count) || a_intfc->ReadRecordData(count) != sizeof(count))
		{
			SKSE::log::error("Unable to read inventory serialization record header");
			return true;
		}

		if (a_type == kIgnoredFormsRecord)
		{
			const auto available = (a_length - sizeof(count)) / sizeof(RE::FormID);
			if (count > available)
			{
				SKSE::log::error("Invalid ignored inventory forms record");
				return true;
			}

			std::unordered_set<RE::FormID> loaded_forms;
			for (std::uint32_t index = 0; index < count; ++index)
			{
				RE::FormID form_id{};
				if (a_intfc->ReadRecordData(form_id) != sizeof(form_id)) { return true; }
				if (a_intfc->ResolveFormID(form_id, form_id)) { loaded_forms.insert(form_id); }
			}

			std::unique_lock lock(mutex_);
			ignored_forms_ = std::move(loaded_forms);
			return true;
		}

		constexpr auto serialized_item_size = sizeof(RE::FormID) + sizeof(std::int32_t) +
			sizeof(bool) + kTransformFloatCount * sizeof(float);
		const auto available = (a_length - sizeof(count)) / serialized_item_size;
		if (count > available)
		{
			SKSE::log::error("Invalid shadow inventory record");
			return true;
		}

		std::vector<ShadowItem> loaded_items;
		loaded_items.reserve(count);
		for (std::uint32_t index = 0; index < count; ++index)
		{
			RE::FormID     form_id{};
			std::int32_t   item_count{};
			bool           stackable{};
			RE::NiTransform placement{};
			if (a_intfc->ReadRecordData(form_id) != sizeof(form_id) ||
				a_intfc->ReadRecordData(item_count) != sizeof(item_count) ||
				a_intfc->ReadRecordData(stackable) != sizeof(stackable) ||
				!ReadTransform(a_intfc, placement))
			{
				SKSE::log::error("Unable to deserialize shadow inventory item");
				return true;
			}

			if (a_intfc->ResolveFormID(form_id, form_id))
			{
				loaded_items.push_back({ .id = form_id,
					.placement3d = placement,
					.extradata = nullptr,
					.count = item_count,
					.stackable = stackable,
					.unique_id = 0,
					.source = InventoryActionSource::kUnknown });
			}
		}

		{
			std::unique_lock lock(mutex_);
			items_ = std::move(loaded_items);
			++revision_;
		}
		NotifyInventoryChanged();
		return true;
	}

	void InventoryManager::Revert()
	{
		{
			std::unique_lock lock(mutex_);
			items_.clear();
			ignored_forms_.clear();
			pending_actions_.clear();
			container_events_.clear();
			pending_grabs_.clear();
			frame_ = 0;
			next_sequence_ = 0;
			++revision_;
		}
		NotifyInventoryChanged();
	}

	std::vector<ShadowItem> InventoryManager::GetItems() const
	{
		std::shared_lock lock(mutex_);
		return items_;
	}

	std::unordered_set<RE::FormID> InventoryManager::GetIgnoredForms() const
	{
		std::shared_lock lock(mutex_);
		return ignored_forms_;
	}

	std::uint64_t InventoryManager::GetRevision() const
	{
		std::shared_lock lock(mutex_);
		return revision_;
	}

	std::uint64_t InventoryManager::AddInventoryChangedListener(
		InventoryChangedCallback a_callback)
	{
		if (!a_callback) { return 0; }
		std::unique_lock lock(mutex_);
		const auto listener_id = next_listener_id_++;
		inventory_listeners_.emplace(listener_id, std::move(a_callback));
		return listener_id;
	}

	void InventoryManager::RemoveInventoryChangedListener(std::uint64_t a_listener_id)
	{
		std::unique_lock lock(mutex_);
		inventory_listeners_.erase(a_listener_id);
	}

	void InventoryManager::NotifyInventoryChanged()
	{
		std::vector<InventoryChange> changes;
		std::vector<InventoryChangedCallback> listeners;
		{
			std::unique_lock lock(mutex_);
			for (const auto& previous : last_notified_items_)
			{
				if (std::ranges::none_of(items_, [&previous](const ShadowItem& a_item) {
						return SameInventoryIdentity(previous, a_item);
					}))
				{
					changes.push_back({ InventoryChangeType::kRemoved, previous });
				}
			}

			for (const auto& item : items_)
			{
				auto previous = std::ranges::find_if(last_notified_items_,
					[&item](const ShadowItem& a_previous) {
						return SameInventoryIdentity(item, a_previous);
					});
				if (previous == last_notified_items_.end())
				{
					changes.push_back({ InventoryChangeType::kAdded, item });
				}
				else if (!SameInventoryValue(*previous, item))
				{
					changes.push_back({ InventoryChangeType::kUpdated, item });
				}
			}

			last_notified_items_ = items_;
			if (changes.empty()) { return; }
			listeners.reserve(inventory_listeners_.size());
			for (const auto& [id, listener] : inventory_listeners_)
			{
				if (listener) { listeners.push_back(listener); }
			}
		}

		for (const auto& listener : listeners) { listener(changes); }
	}

	bool InventoryManager::SetPlacement(
		RE::FormID a_form_id, std::optional<RE::NiTransform> a_placement)
	{
		{
			std::unique_lock lock(mutex_);
			auto item = std::ranges::find(items_, a_form_id, &ShadowItem::id);
			if (item == items_.end()) { return false; }
			if (item->placement3d == a_placement) { return true; }
			item->placement3d = std::move(a_placement);
			++revision_;
		}
		NotifyInventoryChanged();
		return true;
	}

	void InventoryManager::IgnoreForm(RE::FormID a_form_id)
	{
		bool changed{};
		{
			std::unique_lock lock(mutex_);
			changed = ignored_forms_.insert(a_form_id).second;
			const auto removed = std::erase_if(
				items_, [a_form_id](const ShadowItem& a_item) { return a_item.id == a_form_id; });
			changed = changed || removed > 0;
			if (!changed) { return; }
			++revision_;
		}
		NotifyInventoryChanged();
	}

	void InventoryManager::UnignoreForm(RE::FormID a_form_id)
	{
		{
			std::unique_lock lock(mutex_);
			if (ignored_forms_.erase(a_form_id) == 0) { return; }
			++revision_;
		}
		NotifyInventoryChanged();
	}

	bool InventoryManager::IsIgnored(RE::FormID a_form_id) const
	{
		std::shared_lock lock(mutex_);
		return ignored_forms_.contains(a_form_id);
	}
}
