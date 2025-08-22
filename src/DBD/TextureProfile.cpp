#include "TextureProfile.h"

#include "Util/StringUtil.h"

namespace DBD
{
	TextureProfile::TextureProfile(const fs::directory_entry& a_textureFolder) :
		ProfileBase(a_textureFolder.path().filename().string())
	{
		logger::info("Creating texture-set: {}", name);
		for (auto& directory : fs::directory_iterator{ a_textureFolder }) {
			if (!directory.is_directory())
				continue;
			const auto isBodyTextureFolder = std::ranges::any_of(
				fs::directory_iterator{ directory }, [](const auto& file) {
					auto filename = Util::CastLower(file.path().filename().string());
					return filename.find("body") != std::string::npos;
				});
			if (isBodyTextureFolder) {
				const auto rootPath = directory.path().filename().string();
				if (!textureRoot.empty()) {
					const auto err = std::format("Found multiple body texture directories: {} and {}", textureRoot, rootPath);
					throw std::runtime_error(err);
				}
				textureRoot = rootPath;
			}
			for (const auto& file : fs::directory_iterator{ directory }) {
				if (!file.is_regular_file())
					continue;
				const auto fileName = Util::CastLower(file.path().filename().string());
				if (!fileName.ends_with(".dds")) {
					logger::warn("Invalid texture file: {}", fileName);
					continue;
				}
				const auto path = file.path().string();
				const auto cutPath = path.substr(PATH_PREFIX_CUT.size());
				if (isBodyTextureFolder || cutPath.find("head") == std::string::npos && textures.find(fileName) == textures.end()) {
					textures[fileName] = cutPath;
				} else {
					const auto raceRootKey = GetSubfolderKey(cutPath);
					if (fileName.find("vampire") != std::string::npos) {
						headNormalsVampire[raceRootKey] = cutPath;
					} else {
						headNormals[raceRootKey] = cutPath;
					}
				}
			}
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
			const auto pathCStr = armaTextures->GetTexturePath(Texture::kDiffuse);
			std::string path{ pathCStr ? pathCStr : ""s };
			Util::ToLower(path);
			if (path.find("body") == std::string::npos) {
				continue;
			}
			const auto subFolderPath = GetSubfolderKey(path);
			if (!subFolderPath.empty()) {
				return textureRoot == std::string_view{ subFolderPath };
			}
		}
		return false;
	}

	void TextureProfile::Apply(RE::Actor*) const
	{
		throw std::runtime_error("Apply method not implemented for TextureProfiles.");
	}

	void TextureProfile::ApplyHeadTexture(RE::Actor* a_target) const
	{
		const auto headPart = a_target->GetHeadPartObject(RE::BGSHeadPart::HeadPartType::kFace);
		if (!headPart) {
			const auto errMsg = std::format("Actor {} has no head part", a_target->GetName());
			// throw std::runtime_error(errMsg);
			logger::error("{}", errMsg);
			return;
		}

		std::string raceName{ "" };
		RE::BSVisit::TraverseScenegraphGeometries(headPart, [&](RE::BSGeometry* geometry) -> VisitControl {
			auto effect = geometry->GetGeometryRuntimeData().properties[RE::BSGeometry::States::State::kEffect].get();
			auto lightingShader = effect ? netimmerse_cast<RE::BSLightingShaderProperty*>(effect) : nullptr;
			if (!lightingShader) {
				return VisitControl::kContinue;
			}
			const auto material = static_cast<MaterialBase*>(lightingShader->material);
			std::string normal{ material->textureSet.get()->GetTexturePath(Texture::kNormal) };
			Util::ToLower(normal);
			if (normal.find("head") != std::string::npos) {
				logger::info("Found texture {}: {}", name, normal);
				raceName = GetSubfolderKey(normal);
				if (!raceName.empty()) {
					logger::info("Extracted race: {}", raceName);
					return VisitControl::kStop;
				}
			}
			return VisitControl::kContinue;
		});

		const auto vampire = a_target->HasKeywordString("Vampire");
		const auto& table = vampire ? headNormalsVampire : headNormals;
		if (auto headNormal = table.find(raceName); headNormal != table.end()) {
			ApplyTextureImpl(headPart, headNormal->second);
		} else {
			ApplyTextureImpl(headPart);
		}
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
			for (size_t i = 0; i < Texture::kTotal; i++) {
				const auto t = static_cast<Texture>(i);
				const char* pathCStr = armaTextures->GetTexturePath(t);
				const std::string_view path{ pathCStr ? pathCStr : ""sv };
				const auto filename = fs::path(path).filename().string();
				const auto it = textures.find(filename);
				if (it == textures.end()) {
					newTexture->SetTexturePath(t, pathCStr);
				} else {
					newTexture->SetTexturePath(t, it->second.c_str());
				}
			}
			newArma->skinTextures[sex] = newTexture;
			arma = newArma;
		}
		base->skin = newSkin;
	}

	void TextureProfile::ApplyTextureImpl(RE::NiAVObject* a_object, const std::string& a_normalPath) const
	{
		const auto normalFile = fs::path(a_normalPath).filename().string();
		RE::BSVisit::TraverseScenegraphGeometries(a_object, [&](RE::BSGeometry* a_geometry) -> VisitControl {
			const auto effect = a_geometry->GetGeometryRuntimeData().properties[RE::BSGeometry::States::kEffect];
			const auto lightingShader = netimmerse_cast<RE::BSLightingShaderProperty*>(effect.get());
			if (!lightingShader || !lightingShader->material) {
				return VisitControl::kContinue;
			}
			const auto material = static_cast<MaterialBase*>(lightingShader->material);
			if (material->materialAlpha <= 0.0039f) {
				return VisitControl::kContinue;
			}
			auto const feature = material->GetFeature();
			const auto textureSet = material->textureSet;
			if (!textureSet || feature != Feature::kFaceGenRGBTint && feature != Feature::kFaceGen)
				return VisitControl::kContinue;
			const auto newMaterial = static_cast<MaterialBase*>(material->Create());
			if (!newMaterial) {
				logger::error("Failed to create new material for texture application");
				return VisitControl::kContinue;
			}
			newMaterial->CopyMembers(material);
			newMaterial->ClearTextures();

			const auto materialTexture = material->GetTextureSet();
			const auto materialTextureNew = RE::BSShaderTextureSet::Create();
			if (!materialTextureNew) {
				logger::error("Failed to create texture set");
				return VisitControl::kContinue;
			}
			for (size_t i = 0; i < Texture::kTotal; i++) {
				const auto t = static_cast<Texture>(i);
				const char* pathCStr = materialTexture->GetTexturePath(t);
				const std::string_view path{ pathCStr ? pathCStr : ""sv };
				const auto filename = fs::path(path).filename().string();
				if (t == Texture::kNormal && !a_normalPath.empty() && filename == normalFile) {
					materialTextureNew->SetTexturePath(t, a_normalPath.c_str());
					continue;
				}
				const auto it = textures.find(filename);
				if (it == textures.end()) {
					materialTextureNew->SetTexturePath(t, pathCStr);
				} else {
					materialTextureNew->SetTexturePath(t, it->second.c_str());
				}
			}
			newMaterial->OnLoadTextureSet(0, materialTextureNew);

			lightingShader->SetMaterial(newMaterial, true);
			lightingShader->SetupGeometry(a_geometry);
			lightingShader->FinishSetupGeometry(a_geometry);

			newMaterial->~BSLightingShaderMaterialBase();
			RE::free(newMaterial);

			return VisitControl::kContinue;
		});
	}

}  // namespace DBD
