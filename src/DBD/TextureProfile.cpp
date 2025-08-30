#include "TextureProfile.h"

#include "Util/StringUtil.h"

namespace DBD
{
	TextureProfile::TextureProfile(const fs::directory_entry& a_textureFolder) :
		ProfileBase(a_textureFolder.path().filename().string())
	{
		logger::info("Creating texture-set: {}", name);
		const auto profilePrefix(std::format("DBD/{}{}", isPrivate ? "." : "", name.c_str()));
		for (auto& file : fs::recursive_directory_iterator{ a_textureFolder }) {
			if (!file.is_regular_file())
				continue;
			const auto fileName = Util::CastLower(file.path().filename().string());
			if (!fileName.ends_with(".dds")) {
				logger::warn("Invalid texture file: {}", fileName);
				continue;
			}
			// Idk if the c_str when setting texture path (see 'FillTextureSet') needs to be persistent, so I'd rather
			// save the full path here, despite it being easily constructible from filePath, to avoid potential UB.
			const auto filePathRaw = file.path().string();
			const auto filePathFull = filePathRaw.substr(PREFIX_PATH.size() + 1);  // Remove "Data/Textures/"
			const auto filePath = filePathFull.substr(profilePrefix.size() + 1);   // Remove "Data/Textures/DBD/<profile_name>/"
			textures[filePath] = filePathFull;
		}
	}

	std::string TextureProfile::GetSubfolderKey(std::string a_path)
	{
		auto lastSlash = a_path.find_last_of("\\/");
		if (lastSlash != std::string::npos) {
			auto prevSlash = a_path.find_last_of("\\/", lastSlash - 1);
			if (prevSlash != std::string::npos && lastSlash > prevSlash + 1) {
				return a_path.substr(prevSlash + 1, lastSlash - prevSlash - 1);
			}
		}
		return std::string{};
	}

	RE::BSTextureSet* TextureProfile::CreateOverwriteTextureSet(RE::BSTextureSet* a_sourceSet) const
	{
		std::array<const char*, Texture::kTotal> texturePaths;
		bool hasOverwritten = false;
		constexpr auto DATA_PREFIX = "data"sv;
		constexpr auto TEXTURE_PREFIX = "textures"sv;
		for (size_t i = 0; i < Texture::kTotal; i++) {
			const auto t = static_cast<Texture>(i);
			const char* pathCStr = a_sourceSet->GetTexturePath(t);
			std::string path{ pathCStr ? pathCStr : ""sv };
			Util::ToLower(path);
			if (path.starts_with(DATA_PREFIX))
				path = path.substr(DATA_PREFIX.size() + 1);
			if (path.starts_with(TEXTURE_PREFIX)) {
				path = path.substr(TEXTURE_PREFIX.size() + 1);
			}
			if (const auto it = textures.find(path); it != textures.end()) {
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
		return materialTextureNew;
	}

	bool TextureProfile::IsApplicable(RE::Actor* a_target) const
	{
		const auto skin = a_target->GetSkin();
		const auto race = a_target->GetRace();
		if (!skin || !race) {
			logger::error("Failed to get skin or race for actor: {}", a_target->formID);
			return false;
		}
		const auto base = a_target->GetActorBase();
		const auto sex = base ? base->GetSex() : RE::SEX::kMale;
		for (auto&& arma : skin->armorAddons) {
			if (!arma || !arma->IsValidRace(race) || !arma->HasPartOf(RE::BGSBipedObjectForm::BipedObjectSlot::kBody))
				continue;
			const auto armaTextures = arma->skinTextures[sex];
			if (!armaTextures)
				continue;
			for (size_t i = 0; i < Texture::kTotal; i++) {
				const auto pathCStr = armaTextures->GetTexturePath(static_cast<Texture>(i));
				if (pathCStr && textures.contains({ pathCStr })) {
					return true;
				}
			}
		}
		return false;
	}

	void TextureProfile::Apply(RE::Actor* a_target) const
	{
		logger::info("Applying texture profile {} to actor {}", name.data(), a_target->formID);
		const auto faceNode = a_target->GetFaceNodeSkinned();
		const auto bodyNode = a_target->Get3D();
		if (!faceNode) {
			logger::error("Actor {} has no face node", a_target->formID);
			return;
		} else if (!bodyNode) {
			logger::error("Actor {} has no body node", a_target->formID);
			return;
		}
		OverrideObjectTextures(faceNode);
		OverrideObjectTextures(bodyNode);
	}

	void TextureProfile::OverrideObjectTextures(RE::NiAVObject* a_object) const
	{
		using VisitControl = RE::BSVisit::BSVisitControl;
		RE::BSVisit::TraverseScenegraphGeometries(a_object, [&](RE::BSGeometry* a_geometry) {
			auto effect = a_geometry->GetGeometryRuntimeData().properties[RE::BSGeometry::States::State::kEffect].get();
			auto lightingShader = effect ? netimmerse_cast<RE::BSLightingShaderProperty*>(effect) : nullptr;
			if (!lightingShader) {
				return VisitControl::kContinue;
			}
			const auto material = static_cast<MaterialBase*>(lightingShader->material);
			if (material->materialAlpha < 1.f / 255.f) {
				return VisitControl::kContinue;
			}
			const auto feature = material->GetFeature();
			constexpr std::array supportedFeatures{
				Feature::kDefault,
				Feature::kEnvironmentMap,
				Feature::kEye,
				Feature::kFaceGen,
				Feature::kFaceGenRGBTint,
				Feature::kGlowMap,
				Feature::kHairTint,
				// No clue if Skyrim even supports that for armor/body meshes, but meh...
				Feature::kMultilayerParallax,
				Feature::kParallax
			};
			if (!std::ranges::contains(supportedFeatures, feature)) {
				logger::error("Unsupported material feature: {}", magic_enum::enum_name(feature));
				return VisitControl::kContinue;
			}
			const auto newMaterial = static_cast<MaterialBase*>(material->Create());
			if (!newMaterial) {
				return VisitControl::kContinue;
			}
			newMaterial->CopyMembers(material);
			newMaterial->ClearTextures();

			const auto materialTexture = material->GetTextureSet();
			const auto materialTextureNew = CreateOverwriteTextureSet(materialTexture.get());
			if (!materialTextureNew) {
				return VisitControl::kContinue;
			}
			newMaterial->OnLoadTextureSet(0, materialTextureNew);
			switch (feature) {
			case Feature::kEnvironmentMap:
				{
					const auto oldEnvMap = static_cast<RE::BSLightingShaderMaterialEnvmap*>(material);
					const auto newEnvMap = static_cast<RE::BSLightingShaderMaterialEnvmap*>(newMaterial);
					if (!newEnvMap->envTexture)
						newEnvMap->envTexture = oldEnvMap->envTexture;
					if (!newEnvMap->envMaskTexture) {
						newEnvMap->envMaskTexture = oldEnvMap->envMaskTexture;
					}
				}
				break;
			case Feature::kEye:
				{
					const auto oldEye = static_cast<RE::BSLightingShaderMaterialEye*>(material);
					const auto newEye = static_cast<RE::BSLightingShaderMaterialEye*>(newMaterial);
					if (!newEye->envTexture)
						newEye->envTexture = oldEye->envTexture;
					if (!newEye->envMaskTexture) {
						newEye->envMaskTexture = oldEye->envMaskTexture;
					}
				}
				break;
			case Feature::kFaceGen:
				{
					const auto oldFacegen = static_cast<MaterialFacegen*>(material);
					const auto newFacegen = static_cast<MaterialFacegen*>(newMaterial);
					if (!newFacegen->tintTexture)
						newFacegen->tintTexture = oldFacegen->tintTexture;
					if (!newFacegen->detailTexture) {
						newFacegen->detailTexture = oldFacegen->detailTexture;
					}
				}
				break;
			case Feature::kFaceGenRGBTint:
				{
					const auto oldFacegen = static_cast<RE::BSLightingShaderMaterialFacegenTint*>(material);
					const auto newFacegen = static_cast<RE::BSLightingShaderMaterialFacegenTint*>(newMaterial);
					newFacegen->tintColor = oldFacegen->tintColor;
				}
				break;
			case Feature::kGlowMap:
				{
					const auto oldEye = static_cast<RE::BSLightingShaderMaterialGlowmap*>(material);
					const auto newEye = static_cast<RE::BSLightingShaderMaterialGlowmap*>(newMaterial);
					if (!newEye->glowTexture)
						newEye->glowTexture = oldEye->glowTexture;
				}
				break;
			case Feature::kHairTint:
				{
					const auto oldFacegen = static_cast<RE::BSLightingShaderMaterialHairTint*>(material);
					const auto newFacegen = static_cast<RE::BSLightingShaderMaterialHairTint*>(newMaterial);
					newFacegen->tintColor = oldFacegen->tintColor;
				}
				break;
			case Feature::kMultilayerParallax:
				{
					const auto oldParallax = static_cast<RE::BSLightingShaderMaterialMultiLayerParallax*>(material);
					const auto newParallax = static_cast<RE::BSLightingShaderMaterialMultiLayerParallax*>(newMaterial);
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
					const auto oldParallax = static_cast<RE::BSLightingShaderMaterialParallax*>(material);
					const auto newParallax = static_cast<RE::BSLightingShaderMaterialParallax*>(newMaterial);
					if (!newParallax->heightTexture)
						newParallax->heightTexture = oldParallax->heightTexture;
				}
				break;
			}
			lightingShader->SetMaterial(newMaterial, true);
			lightingShader->SetupGeometry(a_geometry);
			lightingShader->FinishSetupGeometry(a_geometry);

			newMaterial->~BSLightingShaderMaterialBase();
			RE::free(newMaterial);

			return VisitControl::kContinue;
		});
	}

}  // namespace DBD
