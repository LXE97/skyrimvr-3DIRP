#pragma once

#include "vr_gui.h"

#include <deque>
#include <optional>
#include <unordered_map>

namespace vr_gui
{
	class TextManager : public Widget
	{
	public:
		static constexpr std::size_t kQuadsPerModel = 256;
		static constexpr std::size_t kVerticesPerQuad = 4;

		TextManager(Widget* a_parent, RE::NiTransform a_local, std::string_view a_modelPath);

		void AddText(const Widget* a_owner, std::string_view a_text, float a_spacing);
		bool SetCharacter(const Widget* a_owner, std::size_t a_index, char a_character);
		void Update(float a_delta) override;
		void Hide() override;
		void Show() override;

	private:
		struct GlyphLocation
		{
			std::size_t model_index{};
			std::size_t quad_index{};
		};

		struct PendingText
		{
			const Widget*                             owner{};
			std::string                               text;
			float                                     spacing{};
			RE::NiTransform                           transform;
			RE::NiTransform                           cursor;
			std::size_t                               next_character{};
			std::vector<std::optional<GlyphLocation>> glyph_locations;
		};

		struct PoolModel
		{
			art_addon::ArtAddonPtr    addon;
			RE::BSGeometry*           geometry{};
			std::vector<std::uint8_t> source_vertices;
			std::vector<std::uint8_t> vertices;
			std::uint32_t             vertex_size{};
			std::uint32_t             position_offset{};
			std::uint32_t             uv_offset{};
			std::size_t               used_quads{};
			bool                      ready{};
		};

		struct LifetimeToken
		{};

		void CreateModel();
		void OnModelInitialized(std::size_t a_modelIndex, art_addon::ArtAddon* a_model);
		void ProcessQueue();
		void PlaceCharacter(PoolModel& a_model, const PendingText& a_text, char a_character);
		void SetCharacterUV(PoolModel& a_model, std::size_t a_quadIndex, char a_character);
		bool UploadVertices(PoolModel& a_model);
		static float GetGlyphWidth(char a_character);

		static const std::unordered_map<char, float> glyph_widths;

		std::shared_ptr<LifetimeToken>            token = std::make_shared<LifetimeToken>();
		std::string                               model_path;
		std::vector<PoolModel>                    models;
		std::vector<std::unique_ptr<PendingText>> managed_text;
		std::deque<PendingText*>                  pending_text;
	};
}
