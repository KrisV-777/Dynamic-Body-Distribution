#include "TexturePreset.h"

#include "shared/KrisV/Util/String.h"

namespace DBD::Data
{
	std::string TexturePreset::GetName() const
	{
		return std::ranges::fold_left(packs,
			std::string{}, [](std::string&& a, const auto& b) {
				return a + std::string{ b->GetName() };
			});
	}


	bool TexturePreset::Apply(RE::Actor* a_actor) const
	{
		assert(a_actor);
		if (auto model = a_actor->Get3D()) {
			return Apply(model);
		}
		return false;
	}

	bool TexturePreset::Apply(RE::NiAVObject* a_object) const
	{
		assert(a_object);
		auto ret = false;
		RE::BSVisit::TraverseScenegraphGeometries(a_object, [&](RE::BSGeometry* a_geometry) {
			ret |= OverrideGeometry(a_geometry);
			return RE::BSVisit::BSVisitControl::kContinue;
		});
		return ret;
	}


	bool TexturePreset::OverrideGeometry(RE::BSGeometry* a_geometry) const
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

	RE::BSTextureSet* TexturePreset::BuildReplacementTextureSet(RE::BSTextureSet* a_originalTextureSet) const
	{
		using Texture = RE::BSTextureSet::Texture;
		std::array<const char*, Texture::kTotal> texturePaths;
		bool hasOverwritten = false;
		constexpr auto DATA_PREFIX = "data"sv;
		constexpr auto TEXTURE_PREFIX = "textures"sv;
		for (size_t i = 0; i < Texture::kTotal; i++) {
			const auto t = static_cast<Texture>(i);
			const char* pathCStr = a_originalTextureSet->GetTexturePath(t);
			std::string path{ pathCStr ? Util::CastLower(pathCStr) : "" };
			if (path.starts_with(DATA_PREFIX))
				path = path.substr(DATA_PREFIX.size() + 1);
			if (path.starts_with(TEXTURE_PREFIX)) {
				path = path.substr(TEXTURE_PREFIX.size() + 1);
			}
			std::replace(path.begin(), path.end(), '\\', '/');
			const auto it = data.find(path);
			if (it != data.end()) {
				texturePaths[i] = it->second.data();
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
		return materialTextureNew;
	}

	void TexturePreset::CopyFeatureSpecificTextures(RE::BSLightingShaderMaterialBase* a_source, RE::BSLightingShaderMaterialBase* a_target)
	{
		using Feature = RE::BSLightingShaderMaterialBase::Feature;
		assert(a_source && a_target);

#define ASSIGN_SAFE_PTR(dest, src)                                                        \
	if (!dest) {                                                                          \
		if (src)                                                                          \
			dest = src;                                                                   \
		else                                                                              \
			logger::error("TexturePack: skipping invalid pointer for field '{}'", #dest); \
	}

		const auto feature = a_source->GetFeature();
		switch (feature) {
		case Feature::kDefault:
			break;
		case Feature::kEnvironmentMap:
			{
				const auto oldEnvMap = static_cast<RE::BSLightingShaderMaterialEnvmap*>(a_source);
				const auto newEnvMap = static_cast<RE::BSLightingShaderMaterialEnvmap*>(a_target);
				ASSIGN_SAFE_PTR(newEnvMap->envTexture, oldEnvMap->envTexture);
				ASSIGN_SAFE_PTR(newEnvMap->envMaskTexture, oldEnvMap->envMaskTexture);
			}
			break;
		case Feature::kEye:
			{
				const auto oldEye = static_cast<RE::BSLightingShaderMaterialEye*>(a_source);
				const auto newEye = static_cast<RE::BSLightingShaderMaterialEye*>(a_target);
				ASSIGN_SAFE_PTR(newEye->envTexture, oldEye->envTexture);
				ASSIGN_SAFE_PTR(newEye->envMaskTexture, oldEye->envMaskTexture);
			}
			break;
		case Feature::kFaceGen:
			{
				const auto oldFacegen = static_cast<RE::BSLightingShaderMaterialFacegen*>(a_source);
				const auto newFacegen = static_cast<RE::BSLightingShaderMaterialFacegen*>(a_target);
				ASSIGN_SAFE_PTR(newFacegen->tintTexture, oldFacegen->tintTexture);
				ASSIGN_SAFE_PTR(newFacegen->detailTexture, oldFacegen->detailTexture);
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
				ASSIGN_SAFE_PTR(newEye->glowTexture, oldEye->glowTexture);
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
				ASSIGN_SAFE_PTR(newParallax->layerTexture, oldParallax->layerTexture);
				ASSIGN_SAFE_PTR(newParallax->envTexture, oldParallax->envTexture);
				ASSIGN_SAFE_PTR(newParallax->envMaskTexture, oldParallax->envMaskTexture);
			}
			break;
		case Feature::kParallax:
			{
				const auto oldParallax = static_cast<RE::BSLightingShaderMaterialParallax*>(a_source);
				const auto newParallax = static_cast<RE::BSLightingShaderMaterialParallax*>(a_target);
				ASSIGN_SAFE_PTR(newParallax->heightTexture, oldParallax->heightTexture);
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
				ASSIGN_SAFE_PTR(newParallaxOcc->heightTexture, oldParallaxOcc->heightTexture);
				newParallaxOcc->parallaxOccMaxPasses = oldParallaxOcc->parallaxOccMaxPasses;
				newParallaxOcc->parallaxOccScale = oldParallaxOcc->parallaxOccScale;
			}
			break;
		default:
			logger::error("Unsupported material feature: {}", magic_enum::enum_name<Feature>(feature));
			assert(false && "Unsupported material feature");
			break;
		}
#undef ASSIGN_SAFE_PTR
	}

} // namespace DBD::Data
