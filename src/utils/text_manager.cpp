/* Credit to https://github.com/adamhynek/ for vertex buffer editing */

#include "text_manager.h"

namespace vr_gui
{
	using namespace RE;

	float GetTextWidth(std::string_view a_text, float a_char_scale, float a_char_spacing)
	{
		if (a_text.empty()) { return 0.0F; }

		float width = 0.0F;
		for (const auto character : a_text)
		{
			width += TextManager::GetGlyphWidth(character) * a_char_scale;
		}

		return width + a_char_spacing * a_char_scale * static_cast<float>(a_text.size() - 1);
	}

	void TrimToLine(
		std::string& a_text, float a_char_scale, float a_char_spacing, float a_max_width)
	{
		if (a_text.empty()) { return; }
		if (a_max_width <= 0.0F)
		{
			a_text.clear();
			return;
		}

		float       width = 0.0F;
		std::size_t length = 0;
		for (const auto character : a_text)
		{
			if (character == '\n') { break; }

			float advance = TextManager::GetGlyphWidth(character) * a_char_scale;
			if (length > 0) { advance += a_char_spacing * a_char_scale; }
			if (width + advance > a_max_width) { break; }

			width += advance;
			++length;
		}

		const bool trimmed = length < a_text.size();
		a_text.resize(length);
		if (trimmed)
		{
			const auto ellipsis_length = std::min<std::size_t>(3, a_text.size());
			a_text.replace(a_text.size() - ellipsis_length, ellipsis_length,
				ellipsis_length, '.');
		}
	}

	int FormatParagraph(
		std::string& a_text, float a_char_scale, float a_char_spacing, float a_max_width)
	{
		if (a_text.empty()) { return 0; }

		int line_count = 1;
		if (a_char_scale <= 0.0f || a_max_width <= 0.0f) {
			return line_count + static_cast<int>(std::ranges::count(a_text, '\n'));
		}

		auto get_width = [&a_text, a_char_scale, a_char_spacing](
			std::size_t a_begin, std::size_t a_end) {
			if (a_begin >= a_end) { return 0.0f; }
			return GetTextWidth(
				std::string_view{ a_text }.substr(a_begin, a_end - a_begin),
				a_char_scale, a_char_spacing);
		};

		std::size_t line_start = 0;
		std::size_t last_break = std::string::npos;

		for (std::size_t i = 0; i < a_text.size(); ++i)
		{
			if (a_text[i] == '\n')
			{
				++line_count;
				line_start = i + 1;
				last_break = std::string::npos;
				continue;
			}

			if (a_text[i] == ' ' || a_text[i] == '\t')
			{
				last_break = i;
				continue;
			}

			if (get_width(line_start, i + 1) <= a_max_width || last_break == std::string::npos)
			{
				continue;
			}

			a_text[last_break] = '\n';
			++line_count;
			line_start = last_break + 1;
			last_break = std::string::npos;
		}

		return line_count;
	}


	// Estimated from the font atlas. Widths are normalized to the widest glyph (W = 1.0).
	const std::unordered_map<char, float> TextManager::glyph_widths{
		{ ' ', 0.45F }, { '!', 0.30F }, { '"', 0.32F }, { '#', 0.62F },
		{ '$', 0.58F }, { '%', 0.90F }, { '&', 0.80F }, { '\'', 0.20F },
		{ '(', 0.38F }, { ')', 0.38F }, { '*', 0.45F }, { '+', 0.68F },
		{ ',', 0.28F }, { '-', 0.48F }, { '.', 0.25F }, { '/', 0.45F },
		{ '0', 0.65F }, { '1', 0.42F }, { '2', 0.58F }, { '3', 0.58F },
		{ '4', 0.62F }, { '5', 0.58F }, { '6', 0.62F }, { '7', 0.58F },
		{ '8', 0.62F }, { '9', 0.62F }, { ':', 0.26F }, { ';', 0.30F },
		{ '<', 0.62F }, { '=', 0.62F }, { '>', 0.62F }, { '?', 0.55F },
		{ '@', 0.80F }, { 'A', 0.69F }, { 'B', 0.62F }, { 'C', 0.68F },
		{ 'D', 0.70F }, { 'E', 0.60F }, { 'F', 0.58F }, { 'G', 0.72F },
		{ 'H', 0.72F }, { 'I', 0.34F }, { 'J', 0.45F }, { 'K', 0.68F },
		{ 'L', 0.56F }, { 'M', 0.92F }, { 'N', 0.76F }, { 'O', 0.78F },
		{ 'P', 0.62F }, { 'Q', 0.80F }, { 'R', 0.68F }, { 'S', 0.62F },
		{ 'T', 0.66F }, { 'U', 0.74F }, { 'V', 0.76F }, { 'W', 1.00F },
		{ 'X', 0.73F }, { 'Y', 0.70F }, { 'Z', 0.66F }, { '[', 0.34F },
		{ '\\', 0.45F }, { ']', 0.34F }, { '^', 0.50F }, { '_', 0.70F },
		{ '`', 0.25F }, { 'a', 0.58F }, { 'b', 0.62F }, { 'c', 0.54F },
		{ 'd', 0.65F }, { 'e', 0.55F }, { 'f', 0.42F }, { 'g', 0.64F },
		{ 'h', 0.63F }, { 'i', 0.30F }, { 'j', 0.34F }, { 'k', 0.59F },
		{ 'l', 0.30F }, { 'm', 0.82F }, { 'n', 0.62F }, { 'o', 0.61F },
		{ 'p', 0.63F }, { 'q', 0.63F }, { 'r', 0.43F }, { 's', 0.51F },
		{ 't', 0.42F }, { 'u', 0.63F }, { 'v', 0.58F }, { 'w', 0.80F },
		{ 'x', 0.59F }, { 'y', 0.58F }, { 'z', 0.52F }, { '{', 0.38F },
		{ '|', 0.22F }, { '}', 0.38F }, { '~', 0.63F }
	};

	TextManager::TextManager(Widget* a_parent, NiTransform a_local, std::string_view a_modelPath) :
		Widget(a_parent, std::move(a_local), NiPoint3{}),
		model_path(a_modelPath)
	{ CreateModel(); }

	float TextManager::GetGlyphWidth(char a_character)
	{
		const auto glyph = glyph_widths.find(a_character);
		if (glyph == glyph_widths.end()) { return 1.0F; }
		return glyph->second * art_addon::NifChar::kCharacterWidth;
	}

	void TextManager::AddText(const Widget* a_owner, std::string_view a_text, float a_spacing)
	{
		if (!a_owner || a_text.empty()) { return; }

		auto text = std::make_unique<PendingText>();
		text->owner = a_owner;
		text->text = a_text;
		text->spacing = a_spacing;
		text->transform = GetWorld().Invert() * a_owner->GetWorld();
		text->glyph_locations.resize(text->text.size());

		pending_text.push_back(text.get());
		managed_text.emplace_back(std::move(text));
		ProcessQueue();
	}

	bool TextManager::SetCharacter(const Widget* a_owner, std::size_t a_index, char a_character)
	{
		if (!a_owner || a_character == '\n' || a_character == ' ') { return false; }

		for (auto& text : managed_text)
		{
			if (text->owner != a_owner) { continue; }
			if (a_index >= text->text.size() || text->text[a_index] == '\n' ||
				text->text[a_index] == ' ')
			{
				return false;
			}

			text->text[a_index] = a_character;
			const auto& location = text->glyph_locations[a_index];
			if (!location) { return true; }
			if (location->model_index >= models.size()) { return false; }

			auto& pool = models[location->model_index];
			if (!pool.ready || location->quad_index >= pool.used_quads) { return false; }

			SetCharacterUV(pool, location->quad_index, a_character);
			return UploadVertices(pool);
		}

		return false;
	}

	void TextManager::CreateModel()
	{
		auto* object = GetObjectReference();
		auto* target = GetModelAttachmentNode();
		if (!object || !target || model_path.empty()) { return; }

		const auto model_index = models.size();
		models.emplace_back();

		std::weak_ptr<LifetimeToken> weak_token = token;
		const auto                   transform = target->world.Invert() * GetWorld();
		models.back().addon = art_addon::ArtAddon::Make(model_path, object, target, transform,
			[weak_token, this, model_index](art_addon::ArtAddon* a_model) {
				if (weak_token.lock()) { OnModelInitialized(model_index, a_model); }
			}, true);
	}

	void TextManager::OnModelInitialized(std::size_t a_modelIndex, art_addon::ArtAddon* a_model)
	{
		if (a_modelIndex >= models.size() || !a_model || !a_model->Get3D()) { return; }

		auto* root = a_model->Get3D();
		auto* geometry_object = root->GetObjectByName(art_addon::NifChar::kNodeName);
		auto* geometry = geometry_object ? geometry_object->AsGeometry() : root->AsGeometry();
		auto* trishape = geometry ? geometry->AsTriShape() : nullptr;
		if (!trishape)
		{
			SKSE::log::error("Managed text model does not contain a '{}' tri-shape",
				art_addon::NifChar::kNodeName);
			return;
		}

		const auto vertex_count = trishape->GetTrishapeRuntimeData().vertexCount;
		auto*      geometry_data = geometry->GetGeometryRuntimeData().rendererData;
		if (!geometry_data || !geometry_data->rawVertexData ||
			vertex_count < kQuadsPerModel * kVerticesPerQuad)
		{
			SKSE::log::error("Managed text model requires at least {} vertices",
				kQuadsPerModel * kVerticesPerQuad);
			return;
		}

		auto vertex_desc = geometry_data->vertexDesc;
		if (!vertex_desc.HasFlag(RE::BSGraphics::Vertex::Flags::VF_VERTEX) ||
			!vertex_desc.HasFlag(RE::BSGraphics::Vertex::Flags::VF_UV))
		{
			SKSE::log::error("Managed text model requires position and UV vertex attributes");
			return;
		}

		auto& pool = models[a_modelIndex];
		pool.geometry = geometry;
		pool.vertex_size = vertex_desc.GetSize();
		pool.position_offset =
			vertex_desc.GetAttributeOffset(RE::BSGraphics::Vertex::Attribute::VA_POSITION);
		pool.uv_offset =
			vertex_desc.GetAttributeOffset(RE::BSGraphics::Vertex::Attribute::VA_TEXCOORD0);

		const auto buffer_size = static_cast<std::size_t>(vertex_count) * pool.vertex_size;
		pool.source_vertices.assign(
			geometry_data->rawVertexData, geometry_data->rawVertexData + buffer_size);
		pool.vertices = pool.source_vertices;

		for (std::uint16_t i = 0; i < vertex_count; ++i)
		{
			auto* position = reinterpret_cast<NiPoint3*>(pool.vertices.data() +
				static_cast<std::size_t>(i) * pool.vertex_size + pool.position_offset);
			*position = NiPoint3{};
		}

		pool.ready = UploadVertices(pool);
		if (!pool.ready) { return; }

		a_model->SetWorldTransform(GetWorld());
		if (hide) { root->SetAppCulled(true); }
		ProcessQueue();
	}

	void TextManager::ProcessQueue()
	{
		while (!pending_text.empty())
		{
			if (models.empty())
			{
				CreateModel();
				return;
			}

			auto& pool = models.back();
			if (!pool.ready)
			{
				return;
			}

			bool  modified = false;
			auto& text = *pending_text.front();
			while (text.next_character < text.text.size())
			{
				const auto character = text.text[text.next_character];
				if (character == '\n')
				{
					++text.next_character;
					text.cursor.translate.y -= art_addon::AddonTextBox::kLineSpacing;
					text.cursor.translate.x = 0.0F;
					continue;
				}
				if (character == ' ')
				{
					++text.next_character;
					text.cursor.translate.x += GetGlyphWidth(character) + text.spacing;
					continue;
				}

				if (pool.used_quads == kQuadsPerModel)
				{
					if (modified) { UploadVertices(pool); }
					CreateModel();
					return;
				}

				PlaceCharacter(pool, text, character);
				text.glyph_locations[text.next_character] =
					GlyphLocation{ models.size() - 1, pool.used_quads };
				++pool.used_quads;
				++text.next_character;
				text.cursor.translate.x += GetGlyphWidth(character) + text.spacing;
				modified = true;
			}

			if (modified) { UploadVertices(pool); }
			pending_text.pop_front();
		}
	}

	void TextManager::PlaceCharacter(
		PoolModel& a_model, const PendingText& a_text, char a_character)
	{
		auto glyph_cursor = a_text.cursor;
		glyph_cursor.translate.x -=
			(art_addon::NifChar::kCharacterWidth - GetGlyphWidth(a_character)) * 0.5F;
		const auto glyph_transform = a_text.transform * glyph_cursor;
		const auto first_vertex = a_model.used_quads * kVerticesPerQuad;

		for (std::size_t i = 0; i < kVerticesPerQuad; ++i)
		{
			const auto vertex = first_vertex + i;
			const auto byte_offset = vertex * a_model.vertex_size;

			const auto* source_position = reinterpret_cast<const NiPoint3*>(
				a_model.source_vertices.data() + byte_offset + a_model.position_offset);
			auto* position = reinterpret_cast<NiPoint3*>(
				a_model.vertices.data() + byte_offset + a_model.position_offset);
			*position = glyph_transform * *source_position;
		}

		SetCharacterUV(a_model, a_model.used_quads, a_character);
	}

	void TextManager::SetCharacterUV(PoolModel& a_model, std::size_t a_quadIndex, char a_character)
	{
		const auto uv_offset = art_addon::NifChar::AsciiToXY(a_character);
		const auto first_vertex = a_quadIndex * kVerticesPerQuad;

		for (std::size_t i = 0; i < kVerticesPerQuad; ++i)
		{
			const auto  byte_offset = (first_vertex + i) * a_model.vertex_size;
			const auto* source_uv = reinterpret_cast<const std::uint16_t*>(
				a_model.source_vertices.data() + byte_offset + a_model.uv_offset);
			auto* uv = reinterpret_cast<std::uint16_t*>(
				a_model.vertices.data() + byte_offset + a_model.uv_offset);
			uv[0] = DirectX::PackedVector::XMConvertFloatToHalf(
				DirectX::PackedVector::XMConvertHalfToFloat(source_uv[0]) + uv_offset.x);
			uv[1] = DirectX::PackedVector::XMConvertFloatToHalf(
				DirectX::PackedVector::XMConvertHalfToFloat(source_uv[1]) + uv_offset.y);
		}
	}

	bool TextManager::UploadVertices(PoolModel& a_model)
	{
		if (!a_model.geometry || a_model.vertices.empty()) { return false; }

		auto* geometry_data = a_model.geometry->GetGeometryRuntimeData().rendererData;
		auto* device = RE::BSGraphics::Renderer::GetDevice();
		if (!geometry_data || !geometry_data->rawVertexData || !device) { return false; }

		REX::W32::D3D11_BUFFER_DESC buffer_desc{};
		buffer_desc.byteWidth = static_cast<std::uint32_t>(a_model.vertices.size());
		buffer_desc.usage = REX::W32::D3D11_USAGE_DEFAULT;
		buffer_desc.bindFlags = REX::W32::D3D11_BIND_VERTEX_BUFFER;

		REX::W32::D3D11_SUBRESOURCE_DATA initial_data{};
		initial_data.sysMem = a_model.vertices.data();

		REX::W32::ID3D11Buffer* vertex_buffer = nullptr;
		const auto result = device->CreateBuffer(&buffer_desc, &initial_data, &vertex_buffer);
		if (result < 0 || !vertex_buffer)
		{
			SKSE::log::error("Failed to create managed text vertex buffer (HRESULT {:#x})",
				static_cast<std::uint32_t>(result));
			return false;
		}

		std::memcpy(geometry_data->rawVertexData, a_model.vertices.data(), a_model.vertices.size());

		auto* old_vertex_buffer =
			reinterpret_cast<REX::W32::ID3D11Buffer*>(geometry_data->vertexBuffer);
		geometry_data->vertexBuffer = reinterpret_cast<RE::ID3D11Buffer*>(vertex_buffer);
		if (old_vertex_buffer) { old_vertex_buffer->Release(); }
		return true;
	}

	void TextManager::Update(float a_delta)
	{
		for (auto& pool : models)
		{
			//if (pool.addon) { pool.addon->SetWorldTransform(GetWorld()); }
		}
	}

	void TextManager::Hide()
	{
		Widget::Hide();
		for (auto& pool : models)
		{
			if (auto* node = pool.addon ? pool.addon->Get3D() : nullptr)
			{
				node->SetAppCulled(true);
			}
		}
	}

	void TextManager::Show()
	{
		Widget::Show();
		for (auto& pool : models)
		{
			if (auto* node = pool.addon ? pool.addon->Get3D() : nullptr)
			{
				node->SetAppCulled(false);
			}
		}
	}
}
