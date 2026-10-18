#include "TexturePack.h"

#include <glaze/glaze.hpp>
#include <shared/KrisV/Util/String.h>

namespace DBD::Data
{
	namespace detail
	{
		struct TextureMapping
		{
			std::string VanillaTexture;
			std::string ReplacementTexture;
		};
		static_assert(glz::reflectable<TextureMapping>);

		struct TextureData
		{
			std::string Uid;
			std::string Name;
			std::string Description;
			std::vector<TextureMapping> Mappings;
			std::chrono::system_clock::time_point LastUpdatedUtc;
		};
		static_assert(glz::reflectable<TextureData>);

		std::string NormalizeTexturePath(const char* a_pathCStr)
		{
			std::string path{ a_pathCStr ? Util::CastLower(a_pathCStr) : "" };
			constexpr std::string_view dataPrefix{ "data" };
			constexpr std::string_view texturesPrefix{ "textures" };
			if (path.starts_with(dataPrefix)) {
				path = path.substr(dataPrefix.size() + 1);
			}
			if (path.starts_with(texturesPrefix)) {
				path = path.substr(texturesPrefix.size() + 1);
			}
			std::replace(path.begin(), path.end(), '\\', '/');
			return path;
		}
	}

	TexturePack::TexturePack(const fs::path& a_jsonFilePath)
	{
		const auto jsonPath = a_jsonFilePath.string();
		detail::TextureData data;
		if (auto ec = glz::read_file_json(data, jsonPath, std::string{})) {
			throw std::runtime_error(
				std::format("Failed to load TexturePack from {} with Error Code {}: {}",
					a_jsonFilePath.string(), std::to_underlying(ec.ec), ec.custom_error_message));
		}
		const auto parentFolder = a_jsonFilePath.parent_path().filename().string();

		_name = data.Name;
		_description = data.Description;
		for (const auto& mapping : data.Mappings) {
			_textureMappings[mapping.VanillaTexture] = std::format("dbd/{}/{}", parentFolder, mapping.ReplacementTexture);
		}
	}

	bool TexturePack::WriteMap(RE::Actor* a_actor, TextureMap& a_textureOverrides) const
	{
		assert(a_actor);
		if (auto model = a_actor->Get3D()) {
			return WriteMap(model, a_textureOverrides);
		}
		return false;
	}

	bool TexturePack::WriteMap(RE::NiAVObject* a_object, TextureMap& a_textureOverrides) const
	{
		assert(a_object);
		auto ret = false;
		RE::BSVisit::TraverseScenegraphGeometries(a_object, [&](RE::BSGeometry* a_geometry) {
			const auto lightingShader = a_geometry->lightingShaderProp_cast();
			if (!lightingShader) {
				return RE::BSVisit::BSVisitControl::kContinue;
			}
			constexpr auto alphaThreshold = 1.f / 255.f;
			const auto material = static_cast<RE::BSLightingShaderMaterialBase*>(lightingShader->material);
			if (material->materialAlpha < alphaThreshold) {
				return RE::BSVisit::BSVisitControl::kContinue;
			}
			const auto materialTexture = material->GetTextureSet();
			using Texture = RE::BSTextureSet::Texture;
			for (size_t i = 0; i < Texture::kTotal; i++) {
				const auto t = static_cast<Texture>(i);
				const char* pathCStr = materialTexture->GetTexturePath(t);
				std::string path = detail::NormalizeTexturePath(pathCStr);
				const auto it = _textureMappings.find(path);
				if (it != _textureMappings.end()) {
					a_textureOverrides[it->first] = it->second;
					ret = true;
				}
			}
			return RE::BSVisit::BSVisitControl::kContinue;
		});
		return ret;
	}

	bool TexturePack::HasReplacements(RE::Actor* a_actor) const
	{
		assert(a_actor);
		TextureMap textureOverrides;
		return WriteMap(a_actor, textureOverrides);
	}

	bool TexturePack::HasReplacements(RE::NiAVObject* a_object) const
	{
		assert(a_object);
		TextureMap textureOverrides;
		return WriteMap(a_object, textureOverrides);
	}

}  // namespace DBD::Data
