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
	}

	TexturePack::TexturePack(std::string_view a_jsonFilePath)
	{
		detail::TextureData data;
		if (auto ec = glz::read_file_json(data, a_jsonFilePath.data(), std::string{})) {
			throw std::runtime_error(
				std::format("Failed to load TexturePack from {} with Error Code {}: {}",
					a_jsonFilePath, ec.ec, ec.custom_error_message));
		}

		_name = data.Name;
		_description = data.Description;
		for (const auto& mapping : data.Mappings) {
			_textureMappings[mapping.VanillaTexture] = mapping.ReplacementTexture;
		}
	}

    TexturePack::~TexturePack() noexcept
    {
        for (const auto& [original, replacement] : _createdTextureSets) {
            if (replacement) {
                replacement->~BSTextureSet();
                RE::free(replacement);
            }
        }
    }

	bool TexturePack::ReplaceTextures(RE::Actor* a_actor) const
	{
		assert(a_actor);
		if (auto model = a_actor->Get3D()) {
			return ReplaceTextures(model);
		}
		return false;
	}

	bool TexturePack::ReplaceTextures(RE::NiAVObject* a_object) const
	{
        assert(a_object);
        auto ret = false;
		RE::BSVisit::TraverseScenegraphGeometries(a_object, [&](RE::BSGeometry* a_geometry) {
			ret |= OverrideGeometry(a_geometry);
			return RE::BSVisit::BSVisitControl::kContinue;
		});
		return ret;
	}

	bool TexturePack::HasReplacements(RE::Actor* a_actor) const
	{
		assert(a_actor);
        if (auto model = a_actor->Get3D()) {
            return HasReplacements(model);
        }
        return false;
	}

	bool TexturePack::HasReplacements(RE::NiAVObject* a_object) const
	{
        assert(a_object);
		auto ret = false;
		using VisitControl = RE::BSVisit::BSVisitControl;
		RE::BSVisit::TraverseScenegraphGeometries(a_object, [&](RE::BSGeometry* a_geometry) {
			const auto lightingShader = a_geometry->lightingShaderProp_cast();
			if (!lightingShader) {
				return VisitControl::kContinue;
			}
			constexpr auto alphaThreshold = 1.f / 255.f;
			const auto material = static_cast<RE::BSLightingShaderMaterialBase*>(lightingShader->material);
			if (material->materialAlpha < alphaThreshold) {
				return VisitControl::kContinue;
			}
			const auto materialTexture = material->GetTextureSet();
			const auto replacement = BuildReplacementTextureSet(materialTexture.get());
            if (!replacement) {
				return VisitControl::kContinue;
			}
			ret = true;
			return VisitControl::kStop;
		});
		return ret;
	}

	bool TexturePack::OverrideGeometry(RE::BSGeometry* a_geometry) const
	{
		const auto lightingShader = a_geometry->lightingShaderProp_cast();
		if (!lightingShader) {
			return false;
		}
		constexpr auto alphaThreshold = 1.f / 255.f;
		const auto material = static_cast<RE::BSLightingShaderMaterialBase*>(lightingShader->material);
		if (material->materialAlpha < alphaThreshold) {
			return false;
		}
		const auto materialTexture = material->GetTextureSet();
		const auto materialTextureNew = BuildReplacementTextureSet(materialTexture.get());
		if (!materialTextureNew) {
			return false;
		}
		const auto newMaterial = static_cast<RE::BSLightingShaderMaterialBase*>(material->Create());
		if (!newMaterial) {
			return false;
		}
		newMaterial->CopyMembers(material);
		newMaterial->ClearTextures();

		newMaterial->OnLoadTextureSet(0, materialTextureNew);
		CopyFeatureSpecificTextures(material, newMaterial);

		lightingShader->SetMaterial(newMaterial, true);
		lightingShader->SetupGeometry(a_geometry);
		lightingShader->FinishSetupGeometry(a_geometry);

		newMaterial->~BSLightingShaderMaterialBase();
		RE::free(newMaterial);

		return true;
	}

	RE::BSTextureSet* TexturePack::BuildReplacementTextureSet(RE::BSTextureSet* a_originalTextureSet) const
	{
		const auto existingReplacementIt = _createdTextureSets.find(a_originalTextureSet);
		if (existingReplacementIt != _createdTextureSets.end()) {
            assert(existingReplacementIt->second);
			return existingReplacementIt->second;
		}

		using Texture = RE::BSTextureSet::Texture;
		std::array<const char*, Texture::kTotal> texturePaths;
		bool hasOverwritten = false;
		constexpr auto DATA_PREFIX = "data"sv;
		constexpr auto TEXTURE_PREFIX = "textures"sv;
		for (size_t i = 0; i < Texture::kTotal; i++) {
			const auto t = static_cast<Texture>(i);
			const char* pathCStr = a_originalTextureSet->GetTexturePath(t);
			std::string path{ pathCStr ? Util::CastLower(pathCStr) : "" };
			constexpr auto DATA_PREFIX = "data"sv;
			constexpr auto TEXTURE_PREFIX = "textures"sv;
			if (path.starts_with(DATA_PREFIX))
				path = path.substr(DATA_PREFIX.size() + 1);
			if (path.starts_with(TEXTURE_PREFIX)) {
				path = path.substr(TEXTURE_PREFIX.size() + 1);
			}
			const auto it = _textureMappings.find(path);
			if (it != _textureMappings.end()) {
				texturePaths[i] = it->second.c_str();
				hasOverwritten = true;
			} else {
				texturePaths[i] = pathCStr;
			}
		}
		if (!hasOverwritten)
			return nullptr;
		const auto materialTextureNew = RE::BSShaderTextureSet::Create();
		if (!materialTextureNew)
			return nullptr;
		for (size_t i = 0; i < Texture::kTotal; i++) {
			materialTextureNew->SetTexturePath(static_cast<Texture>(i), texturePaths[i]);
		}
		_createdTextureSets[a_originalTextureSet] = materialTextureNew;
		return materialTextureNew;
	}

	void TexturePack::CopyFeatureSpecificTextures(RE::BSLightingShaderMaterialBase* a_source, RE::BSLightingShaderMaterialBase* a_target) const
	{
		using Feature = RE::BSLightingShaderMaterialBase::Feature;
		assert(a_source && a_target);

		const auto feature = a_source->GetFeature();
		switch (feature) {
		case Feature::kDefault:
			break;
		case Feature::kEnvironmentMap:
			{
				const auto oldEnvMap = static_cast<RE::BSLightingShaderMaterialEnvmap*>(a_source);
				const auto newEnvMap = static_cast<RE::BSLightingShaderMaterialEnvmap*>(a_target);
				if (!newEnvMap->envTexture)
					newEnvMap->envTexture = oldEnvMap->envTexture;
				if (!newEnvMap->envMaskTexture) {
					newEnvMap->envMaskTexture = oldEnvMap->envMaskTexture;
				}
			}
			break;
		case Feature::kEye:
			{
				const auto oldEye = static_cast<RE::BSLightingShaderMaterialEye*>(a_source);
				const auto newEye = static_cast<RE::BSLightingShaderMaterialEye*>(a_target);
				if (!newEye->envTexture)
					newEye->envTexture = oldEye->envTexture;
				if (!newEye->envMaskTexture) {
					newEye->envMaskTexture = oldEye->envMaskTexture;
				}
			}
			break;
		case Feature::kFaceGen:
			{
				const auto oldFacegen = static_cast<RE::BSLightingShaderMaterialFacegen*>(a_source);
				const auto newFacegen = static_cast<RE::BSLightingShaderMaterialFacegen*>(a_target);
				newFacegen->tintTexture = oldFacegen->tintTexture;
				newFacegen->detailTexture = oldFacegen->detailTexture;
			}
			break;
		case Feature::kFaceGenRGBTint:
			{
				const auto oldFacegen = static_cast<RE::BSLightingShaderMaterialFacegenTint*>(a_source);
				const auto newFacegen = static_cast<RE::BSLightingShaderMaterialFacegenTint*>(a_target);
				newFacegen->tintColor = oldFacegen->tintColor;
			}
			break;
		case Feature::kGlowMap:
			{
				const auto oldEye = static_cast<RE::BSLightingShaderMaterialGlowmap*>(a_source);
				const auto newEye = static_cast<RE::BSLightingShaderMaterialGlowmap*>(a_target);
				if (!newEye->glowTexture)
					newEye->glowTexture = oldEye->glowTexture;
			}
			break;
		case Feature::kHairTint:
			{
				const auto oldFacegen = static_cast<RE::BSLightingShaderMaterialHairTint*>(a_source);
				const auto newFacegen = static_cast<RE::BSLightingShaderMaterialHairTint*>(a_target);
				newFacegen->tintColor = oldFacegen->tintColor;
			}
			break;
		case Feature::kMultilayerParallax:
			{
				const auto oldParallax = static_cast<RE::BSLightingShaderMaterialMultiLayerParallax*>(a_source);
				const auto newParallax = static_cast<RE::BSLightingShaderMaterialMultiLayerParallax*>(a_target);
				if (!newParallax->layerTexture)
					newParallax->layerTexture = oldParallax->layerTexture;
				if (!newParallax->envTexture)
					newParallax->envTexture = oldParallax->envTexture;
				if (!newParallax->envMaskTexture)
					newParallax->envMaskTexture = oldParallax->envMaskTexture;
			}
			break;
		case Feature::kParallax:
			{
				const auto oldParallax = static_cast<RE::BSLightingShaderMaterialParallax*>(a_source);
				const auto newParallax = static_cast<RE::BSLightingShaderMaterialParallax*>(a_target);
				if (!newParallax->heightTexture)
					newParallax->heightTexture = oldParallax->heightTexture;
			}
			break;
		case Feature::kUnknown:
		case Feature::kMultiIndexTriShapeSnow:
			{
				const auto oldSnow = static_cast<RE::BSLightingShaderMaterialSnow*>(a_source);
				const auto newSnow = static_cast<RE::BSLightingShaderMaterialSnow*>(a_target);
				newSnow->sparkleParams = oldSnow->sparkleParams;
			}
			break;
		case Feature::kParallaxOcc:
			{
				const auto oldParallaxOcc = static_cast<RE::BSLightingShaderMaterialParallaxOcc*>(a_source);
				const auto newParallaxOcc = static_cast<RE::BSLightingShaderMaterialParallaxOcc*>(a_target);
				if (!newParallaxOcc->heightTexture)
					newParallaxOcc->heightTexture = oldParallaxOcc->heightTexture;
				newParallaxOcc->parallaxOccMaxPasses = oldParallaxOcc->parallaxOccMaxPasses;
				newParallaxOcc->parallaxOccScale = oldParallaxOcc->parallaxOccScale;
			}
			break;
		default:
			logger::error("Unsupported material feature: {}", magic_enum::enum_name<Feature>(feature));
			assert(false && "Unsupported material feature");
			break;
		}
	}

}  // namespace DBD::Data