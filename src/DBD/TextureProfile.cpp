#include "TextureProfile.h"

#include "Util/StringUtil.h"

namespace DBD
{
	TextureProfile::TextureProfile(const fs::directory_entry& a_textureFolder) :
		ProfileBase(a_textureFolder.path().filename().string())
	{
		logger::info("Creating texture-set: {}", name);
		const auto profilePrefix(std::format("DBD/{}", name.c_str()));
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

	void TextureProfile::FillTextureSet(RE::BSTextureSet* a_sourceSet, RE::BSTextureSet* a_targetSet) const
	{
		constexpr auto TEXTURE_PREFIX = "textures"sv;
		for (size_t i = 0; i < Texture::kTotal; i++) {
			const auto t = static_cast<Texture>(i);
			const char* pathCStr = a_sourceSet->GetTexturePath(t);
			std::string path{ pathCStr ? pathCStr : ""sv };
			if (path.starts_with(TEXTURE_PREFIX)) {
				path = path.substr(TEXTURE_PREFIX.size() + 1);
			}
			if (const auto it = textures.find(path); it != textures.end()) {
				a_targetSet->SetTexturePath(t, it->second.c_str());
			} else {
				a_targetSet->SetTexturePath(t, pathCStr);
			}
		}
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
			if (const auto pathCStr = armaTextures->GetTexturePath(Texture::kDiffuse)) {
				return textures.contains({ pathCStr });
			}
		}
		return false;
	}

	void TextureProfile::Apply(RE::Actor* a_target) const
	{
		ApplySkinTexture(a_target);
		ApplyHeadTexture(a_target);
	}

	void TextureProfile::ApplyHeadTexture(RE::Actor* a_target) const
	{
		logger::info("Applying head texture to actor: {}", a_target->formID);
		const auto headPart = a_target->GetHeadPartObject(RE::BGSHeadPart::HeadPartType::kFace);
		const auto geometry = headPart ? headPart->AsGeometry() : nullptr;
		if (!geometry) {
			logger::error("Actor {} has no head part geometry", a_target->GetName());
			return;
		}

		auto effect = geometry->GetGeometryRuntimeData().properties[RE::BSGeometry::States::State::kEffect].get();
		auto lightingShader = effect ? netimmerse_cast<RE::BSLightingShaderProperty*>(effect) : nullptr;
		if (!lightingShader) {
			logger::error("Actor {} has no lighting shader on head part", a_target->GetName());
			return;
		}
		const auto material = static_cast<MaterialBase*>(lightingShader->material);
		const auto feature = material->GetFeature();
		if (feature != Feature::kFaceGen && feature != Feature::kFaceGenRGBTint) {
			logger::error("Actor {} head material is not FaceGen", a_target->GetName());
			return;
		}

		const auto newMaterial = static_cast<MaterialBase*>(material->Create());
		if (!newMaterial) {
			logger::error("Actor {} failed to create new material for texture application", a_target->GetName());
			return;
		}
		newMaterial->CopyMembers(material);
		newMaterial->ClearTextures();

		const auto materialTexture = material->GetTextureSet();
		const auto materialTextureNew = RE::BSShaderTextureSet::Create();
		if (!materialTextureNew) {
			logger::error("Actor {} failed to create texture set", a_target->GetName());
			return;
		}

		FillTextureSet(materialTexture.get(), materialTextureNew);
		newMaterial->OnLoadTextureSet(0, materialTextureNew);

		if (feature == Feature::kFaceGen) {
			const auto oldFacegen = static_cast<MaterialFacegen*>(material);
			const auto newFacegen = static_cast<MaterialFacegen*>(newMaterial);
			if (!newFacegen->tintTexture)
				newFacegen->tintTexture = oldFacegen->tintTexture;
			if (!newFacegen->detailTexture)
				newFacegen->detailTexture = oldFacegen->detailTexture;
		}

		lightingShader->SetMaterial(newMaterial, true);
		lightingShader->SetupGeometry(geometry);
		lightingShader->FinishSetupGeometry(geometry);

		newMaterial->~BSLightingShaderMaterialBase();
		RE::free(newMaterial);
	}

	void TextureProfile::ApplySkinTexture(RE::Actor* a_target) const
	{
		const auto skin = a_target->GetSkin();
		if (!skin) {
			logger::error("No skin found for Character: {}", a_target->formID);
			return;
		}

		const auto factory = RE::IFormFactory::GetConcreteFormFactoryByType<RE::TESObjectARMO>();
		const auto factoryARMA = RE::IFormFactory::GetConcreteFormFactoryByType<RE::TESObjectARMA>();
		const auto factoryTexture = RE::IFormFactory::GetConcreteFormFactoryByType<RE::BGSTextureSet>();
		auto newSkin = factory ? factory->Create() : nullptr;
		if (!newSkin || !factoryARMA || !factoryTexture) {
			logger::error("Failed to create duplicate skin for Character: {}", a_target->formID);
			return;
		}
		newSkin->Copy(skin);

		const auto base = a_target->GetActorBase();
		if (!base) {
			logger::error("No ActorBase found for Character: {}", a_target->formID);
			return;
		}
		const auto sex = base->GetSex();
		const auto race = base->GetRace();

		for (auto& arma : newSkin->armorAddons) {
			if (!arma || !arma->IsValidRace(race))
				continue;
			auto& armaTextures = arma->skinTextures[sex];
			auto newArma = factoryARMA->Create();
			if (!newArma || !armaTextures) {
				logger::error("Failed to create duplicate body for ArmorAddon: {}", arma->formID);
				continue;
			}
			newArma->data = arma->data;
			newArma->race = arma->race;
			newArma->bipedModel1stPersons[RE::SEX::kMale] = arma->bipedModel1stPersons[RE::SEX::kMale];
			newArma->bipedModel1stPersons[RE::SEX::kFemale] = arma->bipedModel1stPersons[RE::SEX::kFemale];
			newArma->bipedModels[RE::SEX::kMale] = arma->bipedModels[RE::SEX::kMale];
			newArma->bipedModels[RE::SEX::kFemale] = arma->bipedModels[RE::SEX::kFemale];
			newArma->skinTextures[RE::SEX::kMale] = arma->skinTextures[RE::SEX::kMale];
			newArma->skinTextures[RE::SEX::kFemale] = arma->skinTextures[RE::SEX::kFemale];
			newArma->bipedModelData = arma->bipedModelData;
			newArma->additionalRaces = arma->additionalRaces;
			newArma->footstepSet = arma->footstepSet;
			const auto newTexture = factoryTexture->Create();
			if (!newTexture) {
				logger::error("Failed to create duplicate texture for ArmorAddon: {}", arma->formID);
				continue;
			}
			newTexture->flags = armaTextures->flags;

			FillTextureSet(armaTextures, newTexture);
			newArma->skinTextures[sex] = newTexture;
			arma = newArma;
		}
		base->skin = newSkin;
		a_target->Update3DModel();
	}

}  // namespace DBD
