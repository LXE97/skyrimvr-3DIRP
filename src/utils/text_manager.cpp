#include "text_manager.h"

namespace vr_gui
{
	using namespace RE;

	TextManager::TextManager(
		Widget* a_parent, NiTransform a_local, std::string_view a_modelPath) :
		Widget(a_parent, std::move(a_local), NiPoint3{}),
		model_path(a_modelPath)
	{
		CreateModel();
	}

	void TextManager::AddText(
		const Widget* a_owner, std::string_view a_text, float a_spacing)
	{
		if (!a_owner || a_text.empty()) { return; }

		auto text = std::make_unique<PendingText>();
		text->owner = a_owner;
		text->text = a_text;
		text->spacing = a_spacing;
		text->transform = a_owner->GetTransform();
		text->glyph_locations.resize(text->text.size());

		pending_text.push_back(text.get());
		managed_text.emplace_back(std::move(text));
		ProcessQueue();
	}

	bool TextManager::SetCharacter(
		const Widget* a_owner, std::size_t a_index, char a_character)
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
		const auto transform = target->world.Invert() * GetWorld();
		models.back().addon = art_addon::ArtAddon::Make(model_path, object, target, transform,
			[weak_token, this, model_index](art_addon::ArtAddon* a_model) {
				if (weak_token.lock()) { OnModelInitialized(model_index, a_model); }
			});
	}

	void TextManager::OnModelInitialized(
		std::size_t a_modelIndex, art_addon::ArtAddon* a_model)
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
		auto* geometry_data = geometry->GetGeometryRuntimeData().rendererData;
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
			if (!pool.ready) { return; }

			bool modified = false;
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
					text.cursor.translate.x +=
						art_addon::NifChar::kCharacterWidth + text.spacing;
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
				text.cursor.translate.x += art_addon::NifChar::kCharacterWidth + text.spacing;
				modified = true;
			}

			if (modified) { UploadVertices(pool); }
			pending_text.pop_front();
		}
	}

	void TextManager::PlaceCharacter(
		PoolModel& a_model, const PendingText& a_text, char a_character)
	{
		const auto glyph_transform = a_text.transform * a_text.cursor;
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

	void TextManager::SetCharacterUV(
		PoolModel& a_model, std::size_t a_quadIndex, char a_character)
	{
		const auto uv_offset = art_addon::NifChar::AsciiToXY(a_character);
		const auto first_vertex = a_quadIndex * kVerticesPerQuad;

		for (std::size_t i = 0; i < kVerticesPerQuad; ++i)
		{
			const auto byte_offset = (first_vertex + i) * a_model.vertex_size;
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
			if (pool.addon) { pool.addon->SetWorldTransform(GetWorld()); }
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
