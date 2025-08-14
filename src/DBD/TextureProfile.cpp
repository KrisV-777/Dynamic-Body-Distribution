#include "TextureProfile.h"

#include "Util/StringUtil.h"

namespace DBD
{
	TextureProfile::TextureProfile(const fs::directory_entry& a_textureFolder) :
		name(a_textureFolder.path().filename().string()), isPrivate(!name.empty() && name[0] == '.')
	{
		if (name.empty()) {
			throw std::runtime_error("TextureProfile name is empty");
		} else if (name[0] == '.') {
			std::string str{ name.data() };
			name = str.substr(1);
		}
		logger::info("Creating texture-set: {}", name);

		const auto factory = RE::IFormFactory::GetConcreteFormFactoryByType<RE::BGSTextureSet>();
		if (!factory) {
			throw std::runtime_error("Failed to create factory");
		}
		const auto createTextureSet = [&](RE::BGSTextureSet*& dest) {
			dest = factory->Create();
			if (!dest) {
				throw std::runtime_error("Failed to create texture set");
			}
			dest->flags.set(RE::BGSTextureSet::Flag::kFacegenTextures, RE::BGSTextureSet::Flag::kHasModelSpaceNormalMap);
		};
		createTextureSet(textureSetHead);
		createTextureSet(textureSetHeadVampire);
		createTextureSet(textureSetBody);
		createTextureSet(textureSetHands);

		for (auto& directory : fs::directory_iterator{ a_textureFolder }) {
			if (!directory.is_directory())
				continue;
			// subfolders in the texture directory; "male" or "argonianfemale"
			logger::info("Reading texture sub-folder: {}", directory.path().string());

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
				continue;  // Skip non-DDS files
			}
			logger::info("Reading file: {}", filename);
			const auto type = GetTextureType(filename);
			if (!type) {
				logger::error("Unrecognized texture type for file: {}", filename);
				continue;
			}
			Util::ToLower(filename);
			if (filename.find("body") != std::string::npos) {
				textureSetBody->SetTexturePath(*type, file.path().string().c_str());
			} else if (filename.find("hand") != std::string::npos) {
				textureSetHands->SetTexturePath(*type, file.path().string().c_str());
			} else if (filename.find("headvampire") != std::string::npos) {
				textureSetHeadVampire->SetTexturePath(*type, file.path().string().c_str());
			} else if (filename.find("head") != std::string::npos) {
				textureSetHead->SetTexturePath(*type, file.path().string().c_str());
				textureSetHeadVampire->SetTexturePath(*type, file.path().string().c_str());
			}
		}
	}

	void TextureProfile::ReadTextureFilesExtra(const fs::directory_entry& a_textureFolder)
	{
		const auto key = a_textureFolder.path().filename().string();
		for (const auto& file : fs::directory_iterator{ a_textureFolder }) {
			auto filename = file.path().filename().string();
			if (!filename.ends_with(".dds")) {
				continue;  // Skip non-DDS files
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
		Util::ToLower(a_path);
		// Extract the race from the normal path
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
			return Texture::kDetailMap;
		// return Texture::kSubsurfaceTint;
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

	void TextureProfile::ApplyTexture(RE::Actor* a_target) const
	{
		ApplyHeadTexture(a_target);
		ApplySkinTexture(a_target);
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
			if (material->materialAlpha <= 0.0039f) {
				return VisitControl::kContinue;
			}
			std::string normal{ material->textureSet.get()->GetTexturePath(Texture::kNormal) };
			logger::info("Found texture {}: {}", name, normal);
			auto raceName = GetSubfolderKey(normal);
			if (!raceName.empty()) {
				logger::info("Extracted race: {}", raceName);
				return VisitControl::kStop;
			}
			return VisitControl::kContinue;
		});

		if (raceName.empty()) {
			const auto actorBase = a_target->GetActorBase();
			const bool isFemale = actorBase && actorBase->IsFemale();
			raceName = isFemale ? "female" : "male";
		}

		auto headNormal = headNormals.at(raceName);
		RE::BGSTextureSet* appliedTextureSet = nullptr;
		if (a_target->HasKeywordWithType(RE::DefaultObjectID::kVampireRace))
			appliedTextureSet = textureSetHeadVampire;
		else {
			appliedTextureSet = textureSetHead;
		}
		appliedTextureSet->SetTexturePath(Texture::kNormal, headNormal.c_str());
		ApplyTextureImpl(headPart, appliedTextureSet);
	}

	void TextureProfile::ApplySkinTexture(RE::Actor* a_target) const
	{
		const auto skin = a_target->GetSkin();
		if (!skin) {
			const auto errMsg = std::format("Actor {} has no skin", a_target->GetName());
			throw std::runtime_error(errMsg);
		}
		for (auto&& arma : skin->armorAddons) {
			a_target->VisitArmorAddon(skin, arma, [&](bool, RE::NiAVObject& a_obj) {
				if (arma->HasPartOf(RE::BGSBipedObjectForm::BipedObjectSlot::kHands)) {
					ApplyTextureImpl(&a_obj, textureSetHands);
				} else {
					ApplyTextureImpl(&a_obj, textureSetBody);
				}
			});
		}
	}

	void TextureProfile::ApplyTextureImpl(RE::NiAVObject* a_object, RE::BGSTextureSet* a_texture) const
	{
		using VisitControl = RE::BSVisit::BSVisitControl;

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
				return VisitControl::kContinue;
			}
			newMaterial->CopyMembers(material);
			newMaterial->ClearTextures();

			if (feature == Feature::kFaceGen) {
				const auto faceTextureSet = RE::BSShaderTextureSet::Create();
				if (!faceTextureSet) {
					logger::error("Failed to create face texture set");
					return VisitControl::kContinue;
				}
				for (size_t i = 0; i < Texture::kTotal; i++) {
					const std::string_view path{ a_texture->GetTexturePath(static_cast<Texture>(i)) };
					if (path.empty())
						continue;
					faceTextureSet->SetTexturePath(static_cast<Texture>(i), path.data());
				}
				newMaterial->OnLoadTextureSet(0, faceTextureSet);
			} else {
				newMaterial->OnLoadTextureSet(0, a_texture);
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
