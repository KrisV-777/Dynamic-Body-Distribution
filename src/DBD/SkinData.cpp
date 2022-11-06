#include "Skindata.h"

namespace DBD
{
	SkinData* SkinData::InitializeTexture(const fs::directory_entry& a_texturefolder)
	{
		// Create a SkinData object an populate it
		auto texturename = a_texturefolder.path().filename().string();
		logger::info("Creating Texture Set = {}", texturename);
		ToLower(texturename);
		auto data = new SkinData{ texturename };
		// Given File Path is [ Data/Textures/BDB/... ]
		const auto GetTextureRoot = [](const fs::directory_entry& a_file) {
			return a_file.path().string().substr(14);
		};

		for (auto& directory : fs::directory_iterator{ a_texturefolder }) {
			if (!directory.is_directory())
				continue;
			// subfolders in the texture directory; "male" or "argonianfemale"
			auto folder = directory.path().filename().string();
			logger::info("Reading Texture Folder = {}", folder);
			ToLower(folder);

			const auto ProcessDefaultFolder = [&](RaceType a_type, RE::SEX a_sex) {
				auto it = fs::directory_iterator{ directory };
				if (it._At_end()) {
					logger::info("Directory is empty");
					return true;
				} else {
					auto path = it->path().string();
					ToLower(path);
					if (path.ends_with("ap.dds")) {
						auto error = std::error_code{};
						it = it.increment(error);
						if (error.value() != static_cast<int>(__std_win_error::_Success) || it._At_end()) {
							logger::info("Directory is empty");
							return true;
						}
					}
				}
				if (data->Type != a_type && data->Type != RaceType::Unknown) {
					logger::error("Invalid directory. May only use one of Argonian, Khajiit or Default (female/male)");
					return false;
				} else if (!data->InitializeTextureSets(a_sex)) {
					logger::error("Failed to initialize texture set objects");
					return false;
				}
				data->Type = a_type;

				for (auto& file : fs::directory_iterator{ directory }) {
					auto texture = file.path().filename().string();
					logger::info("Reading file = {}", texture);
					ToLower(texture);

					if (texture.find("body") != std::string::npos) {
						SetTexture(data->Textures[BodyPart::Body][a_sex], GetTextureRoot(file), true);
					} else if (texture.find("hand") != std::string::npos) {
						SetTexture(data->Textures[BodyPart::Hands][a_sex], GetTextureRoot(file), true);
					} else if (texture.find("headvampire") != std::string::npos) {
						SetTexture(data->Textures[BodyPart::FaceVampire][a_sex], GetTextureRoot(file), true);
					} else if (texture.find("head") != std::string::npos) {
						SetTexture(data->Textures[BodyPart::Face][a_sex], GetTextureRoot(file), true);
						SetTexture(data->Textures[BodyPart::FaceVampire][a_sex], GetTextureRoot(file), false);
					}
				}
				// skipping over DetailMap in consistency check as it seems unused by the game, so if the author didnt add it idc
				constexpr std::array validtypes{ TextureType::kDiffuse, TextureType::kNormal, TextureType::kSubsurfaceTint, TextureType::kDetailMap, TextureType::kSpecular };
				for (auto& textype : validtypes) {
					// check that the given texturepath isnt empty and if it is, try to find a backup path or return false
					const auto control = [&](const BodyPart a_bodypart, const std::string& a_altpath = ""s) {
						RE::BSFixedString& name = data->Textures[a_bodypart][a_sex]->textures[textype].textureName;
						if (!name.empty())
							return true;
						if (!a_altpath.empty() && fs::exists(a_texturefolder.path().string() + a_altpath)) {
							name = a_altpath;
							return true;
						} else if (auto v = GetVanillaTexture(textype, a_sex, a_bodypart, a_type); *v != '\0') {
							name = v;
							return true;
						}
						logger::error("Unable to find texture of type = {}", textype);	// TODO: Magic enum integration
						return false;
					};
					if (!control(BodyPart::Face))
						return false;
					if (!control(BodyPart::FaceVampire))
						return false;
					// ---
					const auto altpath = textype == TextureType::kSpecular ?
											 a_sex == RE::SEX::kMale ? "\\Male\\MaleBody_1_sk.dds"s : "\\Female\\FemaleBody_1_sk.dds"s :
											 ""s;
					if (!control(BodyPart::Body, altpath))
						return false;
					if (!control(BodyPart::Hands, altpath))
						return false;
				}
				return true;
			};

			if (folder.find("khajiit") != std::string::npos) {
				if (!ProcessDefaultFolder(RaceType::Khajiit, folder.find("female") ? RE::SEX::kFemale : RE::SEX::kMale)) {
					logger::error("Failed to create Texture Data");
					return nullptr;
				}
			} else if (folder.find("argonian") != std::string::npos) {
				if (!ProcessDefaultFolder(RaceType::Argonian, folder.find("female") ? RE::SEX::kFemale : RE::SEX::kMale)) {
					logger::error("Failed to create Texture Data");
					return nullptr;
				}
			} else if (const bool male = folder == "male"; male || folder == "female") {
				if (!ProcessDefaultFolder(RaceType::Default, male ? RE::SEX::kMale : RE::SEX::kFemale)) {
					logger::error("Failed to create Texture Data");
					return nullptr;
				}
			} else {
				auto extra = SkinData::ExtraTexture{ GetTextureRoot(directory) };
				for (auto& file : fs::directory_iterator{ directory }) {
					auto filename = file.path().filename().string();
					ToLower(filename);
					logger::info("Reading file = {}", filename);
					const auto type = GetTextureType(filename);
					if (type) {
						extra.Textures.push_back(std::make_pair(filename, *type));
					} else {
						logger::error("Unrecognized file type");
					}
				}
				data->AdditionalTextures.push_back(extra);
			}
		}
		return data;
	}


	bool SkinData::InitializeTextureSets(RE::SEX a_sex)
	{
		const auto factory = RE::IFormFactory::GetConcreteFormFactoryByType<RE::BGSTextureSet>();
		if (!factory) {
			logger::error("Failed to create factory");
			return false;
		}
		for (auto& texture : Textures) {
			texture[a_sex] = factory->Create();
			if (!texture[a_sex])
				return false;
			texture[a_sex]->flags.set(RE::BGSTextureSet::Flag::kFacegenTextures, RE::BGSTextureSet::Flag::kHasModelSpaceNormalMap);
		}
		return true;
	}

	std::optional<TextureType> SkinData::GetTextureType(const std::string& a_file)
	{
		if (a_file.ends_with("_msn.dds"))
			return TextureType::kNormal;
		else if (a_file.ends_with("_s.dds"))
			return TextureType::kSpecular;
		else if (a_file.ends_with("_sk.dds"))
			return TextureType::kDetailMap;
		// return TextureType::kSubsurfaceTint;
		else if (a_file.ends_with("map.dds"))
			return TextureType::kHeight;
		else if (a_file.ends_with(".dds"))
			return TextureType::kDiffuse;
		else
			return std::nullopt;
	}

	void SkinData::SetTexture(RE::BGSTextureSet* a_set, const std::string& a_texture, bool a_force)
	{
		auto type = GetTextureType(a_texture);
		if (!type) {
			logger::error("Cannot find type of texture = {}", a_texture);
			return;
		}
		auto& entry = a_set->textures[*type].textureName;
		if (a_force || entry.empty()) {
			entry = a_texture;
		}
	}

	constexpr const char* SkinData::GetVanillaTexture(TextureType a_texturetype, RE::SEX a_sex, BodyPart a_part, RaceType a_racetype)
	{
		// aint nothin more beautiful than nested switch cases and a billion returns zzz
		switch (a_part) {
		case BodyPart::FaceVampire:
			{
				if (a_racetype == RaceType::Default) {
					switch (a_texturetype) {
					case TextureType::kDiffuse:
						{
							if (a_sex == RE::SEX::kMale)
								return "Actors\\Character\\Male\\MaleHeadVampire.dds";
							else
								return "Actors\\Character\\Female\\FemaleHeadVampire.dds";
						}
					case TextureType::kNormal:
						{
							if (a_sex == RE::SEX::kMale)
								return "Actors\\Character\\Male\\MaleHeadVampire_msn.dds";
							else
								return "Actors\\Character\\Female\\FemaleHeadVampire_msn.dds";
						}
					}
				}
			}
			__fallthrough;
		case BodyPart::Face:
			switch (a_texturetype) {
			case TextureType::kDiffuse:
				switch (a_racetype) {
				case RaceType::Default:
					{
						if (a_sex == RE::SEX::kMale)
							return "Actors\\Character\\Male\\MaleHead.dds";
						else
							return "Actors\\Character\\Female\\FemaleHead.dds";
					}
				case RaceType::Khajiit:
					{
						if (a_sex == RE::SEX::kMale)
							return "Actors\\Character\\KhajiitMale\\KhajiitMaleHead.dds";
						else
							return "Actors\\Character\\KhajiitFemale\\FemaleHead.dds";
					}
				case RaceType::Argonian:
					{
						if (a_sex == RE::SEX::kMale)
							return "Actors\\Character\\ArgonianMale\\ArgonianMaleHead.dds";
						else
							return "Actors\\Character\\ArgonianFemale\\ArgonianFemaleHead.dds";
					}
				}
			case TextureType::kNormal:
				switch (a_racetype) {
				case RaceType::Default:
					{
						if (a_sex == RE::SEX::kMale)
							return "Actors\\Character\\Male\\MaleHead_msn.dds";
						else
							return "Actors\\Character\\Female\\FemaleHead_msn.dds";
					}
				case RaceType::Khajiit:
					{
						if (a_sex == RE::SEX::kMale)
							return "Actors\\Character\\KhajiitMale\\KhajiitMaleHead_msn.dds";
						else
							return "Actors\\Character\\KhajiitFemale\\FemaleHead_msn.dds";
					}
				case RaceType::Argonian:
					{
						if (a_sex == RE::SEX::kMale)
							return "Actors\\Character\\ArgonianMale\\ArgonianMaleHead_msn.dds";
						else
							return "Actors\\Character\\ArgonianFemale\\ArgonianFemaleHead_msn.dds";
					}
				}
			case TextureType::kSubsurfaceTint:	// race independent
				{
					if (a_sex == RE::SEX::kMale)
						return "Actors\\Character\\Male\\MaleHead_sk.dds";
					else
						return "Actors\\Character\\Female\\FemaleHead_sk.dds";
				}
			case TextureType::kDetailMap:
				switch (a_racetype) {
				case RaceType::Default:	 // male & female use same here
					{
						return "Actors\\Character\\Male\\BlankDetailmap.dds";
					}
				case RaceType::Khajiit:
					{
						if (a_sex == RE::SEX::kMale)
							return "Actors\\Character\\KhajiitMale\\BlankDetailmap.dds";
						else
							return "Actors\\Character\\KhajiitFemale\\BlankDetailmap.dds";
					}
				case RaceType::Argonian:
					{
						if (a_sex == RE::SEX::kMale)
							return "Actors\\Character\\ArgonianMale\\BlankDetailmap.dds";
						else
							return "Actors\\Character\\ArgonianFemale\\BlankDetailmap.dds";
					}
				}
			case TextureType::kSpecular:
				switch (a_racetype) {
				case RaceType::Default:
					{
						if (a_sex == RE::SEX::kMale)
							return "Actors\\Character\\Male\\MaleHead_s.dds";
						else
							return "Actors\\Character\\Female\\FemaleHead_s.dds";
					}
				case RaceType::Khajiit:
					{
						if (a_sex == RE::SEX::kMale)
							return "Actors\\Character\\KhajiitMale\\KhajiitMaleHead_s.dds";
						else
							return "Actors\\Character\\KhajiitFemale\\FemaleHead_s.dds";
					}
				case RaceType::Argonian:
					{
						if (a_sex == RE::SEX::kMale)
							return "Actors\\Character\\ArgonianMale\\ArgonianMaleHead_s.dds";
						else
							return "Actors\\Character\\ArgonianFemale\\ArgonianFemaleHead_s.dds";
					}
				}
			}
			break;
		// --------------------------------
		case BodyPart::Body:
			switch (a_texturetype) {
			case TextureType::kDiffuse:
				switch (a_racetype) {
				case RaceType::Default:
					{
						if (a_sex == RE::SEX::kMale)
							return "Actors\\Character\\Male\\MaleBody_1.dds";
						else
							return "Actors\\Character\\Female\\FemaleBody_1.dds";
					}
				case RaceType::Khajiit:
					{
						if (a_sex == RE::SEX::kMale)
							return "Actors\\Character\\KhajiitMale\\BodyMale.dds";
						else
							return "Actors\\Character\\KhajiitFemale\\FemaleBody.dds";
					}
				case RaceType::Argonian:
					{
						if (a_sex == RE::SEX::kMale)
							return "Actors\\Character\\ArgonianMale\\ArgonianMaleBody.dds";
						else
							return "Actors\\Character\\ArgonianFemale\\ArgonianFemaleBody.dds";
					}
				}
			case TextureType::kNormal:
				switch (a_racetype) {
				case RaceType::Default:
					{
						if (a_sex == RE::SEX::kMale)
							return "Actors\\Character\\Male\\MaleBody_1_msn.dds";
						else
							return "Actors\\Character\\Female\\FemaleBody_1_msn.dds";
					}
				case RaceType::Khajiit:
					{
						if (a_sex == RE::SEX::kMale)
							return "Actors\\Character\\KhajiitMale\\BodyMale_msn.dds";
						else
							return "Actors\\Character\\KhajiitFemale\\FemaleBody_msn.dds";
					}
				case RaceType::Argonian:
					{
						if (a_sex == RE::SEX::kMale)
							return "Actors\\Character\\ArgonianMale\\ArgonianMaleBody_msn.dds";
						else
							return "Actors\\Character\\ArgonianFemale\\ArgonianFemaleBody_msn.dds";
					}
				}
			case TextureType::kSubsurfaceTint:
				{
					if (a_sex == RE::SEX::kMale)
						return "Actors\\Character\\Male\\MaleBody_1_sk.dds";
					else
						return "Actors\\Character\\Female\\FemaleBody_1_sk.dds";
				}
			case TextureType::kSpecular:
				switch (a_racetype) {
				case RaceType::Default:
					{
						if (a_sex == RE::SEX::kMale)
							return "Actors\\Character\\Male\\MaleBody_1_s.dds";
						else
							return "Actors\\Character\\Female\\FemaleBody_1_s.dds";
					}
				case RaceType::Khajiit:
					{
						if (a_sex == RE::SEX::kMale)
							return "Actors\\Character\\KhajiitMale\\BodyMale_s.dds";
						else
							return "Actors\\Character\\KhajiitFemale\\FemaleBody_s.dds";
					}
				case RaceType::Argonian:
					{
						if (a_sex == RE::SEX::kMale)
							return "Actors\\Character\\ArgonianMale\\ArgonianMaleBody_s.dds";
						else
							return "Actors\\Character\\ArgonianFemale\\ArgonianFemaleBody_s.dds";
					}
				}
			}
			break;
		// --------------------------------
		case BodyPart::Hands:
			switch (a_texturetype) {
			case TextureType::kDiffuse:
				switch (a_racetype) {
				case RaceType::Default:
					{
						if (a_sex == RE::SEX::kMale)
							return "Actors\\Character\\Male\\MaleHands_1.dds";
						else
							return "Actors\\Character\\Female\\FemaleHands_1.dds";
					}
				case RaceType::Khajiit:
					{
						if (a_sex == RE::SEX::kMale)
							return "Actors\\Character\\KhajiitMale\\HandsMale.dds";
						else
							return "Actors\\Character\\KhajiitFemale\\FemaleHands.dds";
					}
				case RaceType::Argonian:
					{
						if (a_sex == RE::SEX::kMale)
							return "Actors\\Character\\ArgonianMale\\ArgonianMaleHands.dds";
						else
							return "Actors\\Character\\ArgonianFemale\\ArgonianFemaleHands.dds";
					}
				}
			case TextureType::kNormal:
				switch (a_racetype) {
				case RaceType::Default:
					{
						if (a_sex == RE::SEX::kMale)
							return "Actors\\Character\\Male\\MaleHands_1_msn.dds";
						else
							return "Actors\\Character\\Female\\FemaleHands_1_msn.dds";
					}
				case RaceType::Khajiit:	 // male & female use same here
					{
						return "Actors\\Character\\KhajiitMale\\HandsMale_msn.dds";
					}
				case RaceType::Argonian:
					{
						if (a_sex == RE::SEX::kMale)
							return "Actors\\Character\\ArgonianMale\\ArgonianMaleHands_msn.dds";
						else
							return "Actors\\Character\\ArgonianFemale\\ArgonianFemaleHands_msn.dds";
					}
				}
			case TextureType::kSubsurfaceTint:
				{
					if (a_sex == RE::SEX::kMale)
						return "Actors\\Character\\Male\\MaleHands_1_sk.dds";
					else
						return "Actors\\Character\\Female\\FemaleHands_1_sk.dds";
				}
			case TextureType::kSpecular:
				switch (a_racetype) {
				case RaceType::Default:
					{
						if (a_sex == RE::SEX::kMale)
							return "Actors\\Character\\Male\\MaleHands_1_S.dds";
						else
							return "Actors\\Character\\Female\\FemaleHands_1_S.dds";
					}
				case RaceType::Khajiit:
					{
						if (a_sex == RE::SEX::kMale)
							return "Actors\\Character\\KhajiitMale\\HandsMale_s.dds";
						else
							return "Actors\\Character\\KhajiitFemale\\FemaleHands_s.dds";
					}
				case RaceType::Argonian:
					{
						if (a_sex == RE::SEX::kMale)
							return "Actors\\Character\\ArgonianMale\\ArgonianMaleHands_s.dds";
						else
							return "Actors\\Character\\ArgonianFemale\\ArgonianFemaleHands_s.dds";
					}
				}
			}
			break;
		}
		return "";
	}
}  // namespace DBD
