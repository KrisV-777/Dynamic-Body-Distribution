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
			if (HasBodyTextures(directory)) {
				bodyTexturePath = directory.path().filename().string();
				Util::ToLower(bodyTexturePath);
				ReadTextureFiles(directory);
			} else {
				ReadTextureFilesExtra(directory);
			}
		}
	}

	bool TextureProfile::HasBodyTextures(const fs::directory_entry& a_textureFolder)
	{
		for (const auto& file : fs::directory_iterator{ a_textureFolder }) {
			auto filename = file.path().filename().string();
			Util::ToLower(filename);
			if (filename.find("body") != std::string::npos) {
				return true;
			}
		}
		return false;
	}

	void TextureProfile::ReadTextureFiles(const fs::directory_entry& a_textureFolder)
	{
		for (const auto& file : fs::directory_iterator{ a_textureFolder }) {
			auto filename = file.path().filename().string();
			if (!filename.ends_with(".dds")) {
				continue;
			}
			logger::info("Reading file: {}", filename);
			const auto type = GetTextureType(filename);
			if (!type) {
				logger::error("Unrecognized texture type for file: {}", filename);
				continue;
			}
			Util::ToLower(filename);
			const auto path = file.path().string();
			const auto cutPath = path.substr(PATH_PREFIX_CUT.size());
			const auto cutPathCStr = cutPath.c_str();
			if (filename.find("body") != std::string::npos) {
				textureSetBody[*type] = cutPathCStr;
			} else if (filename.find("hand") != std::string::npos) {
				textureSetHands[*type] = cutPathCStr;
			} else if (filename.find("headvampire") != std::string::npos) {
				textureSetHeadVampire[*type] = cutPathCStr;
			} else if (filename.find("head") != std::string::npos) {
				textureSetHead[*type] = cutPathCStr;
				textureSetHeadVampire[*type] = cutPathCStr;
			}
		}
	}

	void TextureProfile::ReadTextureFilesExtra(const fs::directory_entry& a_textureFolder)
	{
		const auto key = a_textureFolder.path().filename().string();
		for (const auto& file : fs::directory_iterator{ a_textureFolder }) {
			auto filename = file.path().filename().string();
			if (!filename.ends_with(".dds")) {
				continue;
			}
			logger::info("Reading file: {}", filename);
			const auto type = GetTextureType(filename);
			if (!type) {
				logger::error("Unrecognized texture type for file: {}", filename);
				continue;
			}
			Util::ToLower(filename);
			if (filename.find("vampire") != std::string::npos) {
				headNormalsVampire.insert_or_assign(key, file.path().string());
			} else if (filename.find("head") != std::string::npos) {
				headNormals.insert_or_assign(key, file.path().string());
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

	std::optional<TextureProfile::Texture> TextureProfile::GetTextureType(const std::string_view a_file)
	{
		if (a_file.ends_with("_msn.dds"))
			return Texture::kNormal;
		else if (a_file.ends_with("_s.dds"))
			return Texture::kSpecular;
		else if (a_file.ends_with("_sk.dds"))
			return Texture::kSubsurfaceTint;
		else if (a_file.ends_with("map.dds"))
			return Texture::kHeight;
		else if (a_file.ends_with(".dds"))
			return Texture::kDiffuse;
		return std::nullopt;
	}

	bool TextureProfile::IsApplicable(RE::Actor* a_target) const
	{
		using VisitControl = RE::BSVisit::BSVisitControl;

		std::string raceName{ "" };
		const auto rootObject = a_target->Get3D();
		RE::BSVisit::TraverseScenegraphGeometries(rootObject, [&](RE::BSGeometry* geometry) -> VisitControl {
			auto effect = geometry->GetGeometryRuntimeData().properties[RE::BSGeometry::States::State::kEffect].get();
			auto lightingShader = effect ? netimmerse_cast<RE::BSLightingShaderProperty*>(effect) : nullptr;
			if (!lightingShader) {
				return VisitControl::kContinue;
			}
			const auto material = static_cast<MaterialBase*>(lightingShader->material);
			if (material->materialAlpha <= 0.0039f) {
				return VisitControl::kContinue;
			}
			std::string normal{ material->textureSet.get()->GetTexturePath(Texture::kNormal) };
			Util::ToLower(normal);
			if (normal.find("body") == std::string::npos) {
				return VisitControl::kContinue;
			}
			logger::info("IsApplicable: Found texture {}: {}", name, normal);
			raceName = GetSubfolderKey(normal);
			if (!raceName.empty()) {
				logger::info("IsApplicable: Extracted race: {}", raceName);
				return VisitControl::kStop;
			}
			return VisitControl::kContinue;
		});

		return raceName == bodyTexturePath;
	}

	void TextureProfile::Apply(RE::Actor* a_target) const
	{
		assert(a_target);
		a_target->DoReset3D(true);
		a_target->UpdateSkinColor();
		ApplySkinTexture(a_target);
		ApplyHeadTexture(a_target);
		a_target->Update3DModel();
	}

	void TextureProfile::ApplyHeadTexture(RE::Actor* a_target) const
	{
		const auto headPart = a_target->GetHeadPartObject(RE::BGSHeadPart::HeadPartType::kFace);
		if (!headPart) {
			const auto errMsg = std::format("Actor {} has no head part", a_target->GetName());
			throw std::runtime_error(errMsg);
		}
		std::string raceName{ "" };

		using VisitControl = RE::BSVisit::BSVisitControl;
		RE::BSVisit::TraverseScenegraphGeometries(headPart, [&](RE::BSGeometry* geometry) -> RE::BSVisit::BSVisitControl {
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

		const auto& appliedTextureSet = a_target->HasKeywordString("Vampire") ? textureSetHeadVampire : textureSetHead;
		if (auto headNormal = headNormals.find(raceName); headNormal != headNormals.end()) {
			ApplyTextureImpl(headPart, appliedTextureSet, headNormal->second);
		} else {
			ApplyTextureImpl(headPart, appliedTextureSet);
		}
	}

	void TextureProfile::ApplySkinTexture(RE::Actor* a_target) const
	{
		const auto skin = a_target->GetSkin();
		if (!skin) {
			logger::error("Load3DPlayer: No skin found for Character: {}", a_target->formID);
			return;
		}

		const auto factory = RE::IFormFactory::GetConcreteFormFactoryByType<RE::TESObjectARMO>();
		const auto factoryARMA = RE::IFormFactory::GetConcreteFormFactoryByType<RE::TESObjectARMA>();
		const auto factoryTexture = RE::IFormFactory::GetConcreteFormFactoryByType<RE::BGSTextureSet>();
		auto newSkin = factory ? factory->Create() : nullptr;
		if (!newSkin || !factoryARMA || !factoryTexture) {
			logger::error("Load3DPlayer: Failed to create duplicate skin for Character: {}", a_target->formID);
			return;
		}
		newSkin->Copy(skin);

		const auto base = a_target->GetActorBase();
		if (!base) {
			logger::error("Load3DPlayer: No ActorBase found for Character: {}", a_target->formID);
			return;
		}
		const auto sex = base->GetSex();
		const auto race = base->GetRace();

		const auto applyOverwrite = [&](RE::TESObjectARMA*& sourceArma, const TextureData& a_texture) {
			auto& armaTextures = sourceArma->skinTextures[sex];
			auto newArma = factoryARMA->Create();
			if (!newArma || !armaTextures) {
				logger::error("Load3DPlayer: Failed to create duplicate body for ArmorAddon: {}", sourceArma->formID);
				return;
			}
			newArma->data = sourceArma->data;
			newArma->race = sourceArma->race;
			newArma->bipedModel1stPersons[RE::SEX::kMale] = sourceArma->bipedModel1stPersons[RE::SEX::kMale];
			newArma->bipedModel1stPersons[RE::SEX::kFemale] = sourceArma->bipedModel1stPersons[RE::SEX::kFemale];
			newArma->bipedModels[RE::SEX::kMale] = sourceArma->bipedModels[RE::SEX::kMale];
			newArma->bipedModels[RE::SEX::kFemale] = sourceArma->bipedModels[RE::SEX::kFemale];
			newArma->skinTextures[RE::SEX::kMale] = sourceArma->skinTextures[RE::SEX::kMale];
			newArma->skinTextures[RE::SEX::kFemale] = sourceArma->skinTextures[RE::SEX::kFemale];
			newArma->bipedModelData = sourceArma->bipedModelData;
			newArma->additionalRaces = sourceArma->additionalRaces;
			newArma->footstepSet = sourceArma->footstepSet;
			const auto newTexture = factoryTexture->Create();
			if (!newTexture) {
				logger::error("Load3DPlayer: Failed to create duplicate texture for ArmorAddon: {}", sourceArma->formID);
				return;
			}
			newTexture->flags = armaTextures->flags;
			using Texture = RE::BSTextureSet::Texture;
			for (size_t i = 0; i < Texture::kTotal; i++) {
				const auto t = static_cast<Texture>(i);
				if (const char* path = a_texture[t].data()) {
					newTexture->SetTexturePath(t, path);
				} else {
					newTexture->SetTexturePath(t, armaTextures->GetTexturePath(t));
				}
			}
			newArma->skinTextures[sex] = newTexture;
			sourceArma = newArma;
		};

		using SlotMask = RE::BGSBipedObjectForm::BipedObjectSlot;
		for (auto& arma : newSkin->armorAddons) {
			if (!arma || !arma->IsValidRace(race)) {
				continue;
			} else if (arma->HasPartOf(SlotMask::kBody) || arma->HasPartOf(SlotMask::kFeet) || arma->HasPartOf(SlotMask::kTail)) {
				applyOverwrite(arma, textureSetBody);
			} else if (arma->HasPartOf(SlotMask::kHands)) {
				applyOverwrite(arma, textureSetHands);
			}
		}
		base->skin = newSkin;
	}

	void TextureProfile::ApplyTextureImpl(RE::NiAVObject* a_object, const TextureData& a_texture, const std::string& a_normal) const
	{
		using VisitControl = RE::BSVisit::BSVisitControl;

		RE::BSVisit::TraverseScenegraphGeometries(a_object, [&](RE::BSGeometry* a_geometry) -> VisitControl {
			const auto effect = a_geometry->GetGeometryRuntimeData().properties[RE::BSGeometry::States::kEffect];
			const auto lightingShader = netimmerse_cast<RE::BSLightingShaderProperty*>(effect.get());
			if (!lightingShader || !lightingShader->material) {
				return VisitControl::kContinue;
			}
			const auto material = static_cast<MaterialBase*>(lightingShader->material);
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
				logger::error("Failed to create face texture set");
				return VisitControl::kContinue;
			}
			bool checkOldOnly = false;
			for (size_t i = 0; i < Texture::kTotal; i++) {
				const auto t = static_cast<Texture>(i);
				if (t == Texture::kNormal && !a_normal.empty()) {
					materialTextureNew->SetTexturePath(t, a_normal.c_str());
					continue;
				}
				if (!checkOldOnly && !a_texture[t].empty()) {
					materialTextureNew->SetTexturePath(t, a_texture[t].data());
				} else {
					const auto path = materialTexture->GetTexturePath(t);
					materialTextureNew->SetTexturePath(t, path);
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
