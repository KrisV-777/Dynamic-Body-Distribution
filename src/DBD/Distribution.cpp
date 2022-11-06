#include "Distribution.h"


namespace DBD
{
	void Distribution::Initialize()
	{
		logger::info("Loading Texture Sets");
		const auto path = fs::path{ "Data\\Textures\\DBD" };
		if (!fs::exists(path)) {
			logger::critical("Path to textures does not exist");
		} else {
			// Data/Textures/DBD/<ModName>/<gender>/<file>
			const auto texroot = [](const fs::directory_entry& a_path) { return a_path.path().string().substr(15); };
			for (auto& texturemod : fs::directory_iterator{ path }) {
				if (!texturemod.is_directory())
					continue;
				auto data = SkinData::InitializeTexture(texturemod);
				if (data) {
					auto name = data->GetID();
					logger::info("Added Texture Set = {}", name);
					skins.emplace(std::make_pair(name, data));
				} else {
					logger::error("Failed to add Texture Set = {}", texturemod.path().filename().string());
				}
			}
		}
		logger::info("Loading Body Presets");
		// TODO: implement
	}

	bool Distribution::DistributeTexture(RE::Actor* a_target, const std::string& a_skinid)
	{
		auto request = skins.find(a_skinid);
		if (request == skins.end()) {
			logger::error("Invalid Request = {}. No Skin with such ID exists", a_skinid);
			return false;
		}
		auto& data = request->second;
		auto race = std::string(a_target->GetRace()->GetFormEditorID());
		ToLower(race);
		if (race.find("khajiit") != std::string::npos && data->Type != RaceType::Khajiit) {
			logger::error("Skin is of invalid type. Expected \'Khajiit\' but got \'{}\'", data->Type);	// TODO: Magic enum integration			
			return false;
		} else if (race.find("argonian") != std::string::npos && data->Type != RaceType::Argonian) {
			logger::error("Skin is of invalid type. Expected \'Argonian\' but got \'{}\'", data->Type);	// TODO: Magic enum integration			
			return false;
		} else if (data->Type != RaceType::Default) {
			logger::error("Skin is of invalid type. Expected \'Default\' but got \'{}\'", data->Type);	// TODO: Magic enum integration			
			return false;
		}
		if (!data->NakedSkin) {
			RE::TESObjectARMO* skin;
			switch (data->Type) {
			case RaceType::Default:
				skin = RE::TESForm::LookupByID<RE::TESObjectARMO>(0xD64);
				break;
			case RaceType::Argonian:
			case RaceType::Khajiit:
				skin = RE::TESForm::LookupByID<RE::TESObjectARMO>(0x69CE3);
				break;
			default:
				logger::error("No default Skin for Skin = {}", data->Type);	// TODO: Magic enum integration
				return false;
			}
			const auto factory = RE::IFormFactory::GetConcreteFormFactoryByType<RE::TESObjectARMO>();
			const auto factoryARMA = RE::IFormFactory::GetConcreteFormFactoryByType<RE::TESObjectARMA>();
			data->NakedSkin = factory ? factory->Create() : nullptr;
			if (!data->NakedSkin || !factoryARMA) {
				logger::error("Failed to create new Skin");
				return false;
			}
			data->NakedSkin->Copy(skin);
			for (uint32_t i = 0; i < skin->armorAddons.size(); i++) {
				auto& addon_og = skin->armorAddons[i];
				const auto ActorTypeNPC = RE::TESForm::LookupByID<RE::BGSKeyword>(0x13794);
				if (addon_og->race->GetFormID() != 0x19 && !addon_og->race->HasKeyword(ActorTypeNPC)) {
					// 0x19 = DefaultRace
					continue;
				} else {
					switch (data->Type) {
					case RaceType::Default:
						if (race.find("khajiit") != std::string::npos || race.find("argonian") != std::string::npos)
							continue;
						break;
					case RaceType::Khajiit:
						if (race.find("khajiit") == std::string::npos)
							continue;
						break;
					case RaceType::Argonian:
						if (race.find("argonian") == std::string::npos)
							continue;
						break;
					}
				}
				auto addon = factoryARMA->Create();
				if (!addon) {
					logger::error("Failed to create new Skin");
					return false;
				}
				data->NakedSkin->armorAddons[i] = addon;
				// Copy over the data from the og Addon to the new Addon object
				addon->data = addon_og->data;
				addon->race = addon_og->race;
				addon->bipedModel1stPersons[RE::SEX::kMale] = addon_og->bipedModel1stPersons[RE::SEX::kMale];
				addon->bipedModel1stPersons[RE::SEX::kFemale] = addon_og->bipedModel1stPersons[RE::SEX::kFemale];
				addon->bipedModels[RE::SEX::kMale] = addon_og->bipedModels[RE::SEX::kMale];
				addon->bipedModels[RE::SEX::kFemale] = addon_og->bipedModels[RE::SEX::kFemale];
				addon->bipedModelData = addon_og->bipedModelData;
				addon->additionalRaces = addon_og->additionalRaces;
				const auto Overwrite = [&](RE::SEX a_sex, BodyPart a_part) {
					const auto replica = data->Textures[a_part][a_sex];
					addon->skinTextures[a_sex] = replica ? replica : addon_og->skinTextures[a_sex];
				};
				if (addon->HasPartOf(RE::BGSBipedObjectForm::BipedObjectSlot::kBody) ||
					addon->HasPartOf(RE::BGSBipedObjectForm::BipedObjectSlot::kFeet) ||
					addon->HasPartOf(RE::BGSBipedObjectForm::BipedObjectSlot::kTail)) {
					Overwrite(RE::SEX::kMale, BodyPart::Body);
					Overwrite(RE::SEX::kFemale, BodyPart::Body);
				} else if (addon->HasPartOf(RE::BGSBipedObjectForm::BipedObjectSlot::kHands)) {
					Overwrite(RE::SEX::kMale, BodyPart::Hands);
					Overwrite(RE::SEX::kFemale, BodyPart::Hands);
				}
			}
		}
		auto base = a_target->GetActorBase();
		if (!base) {
			logger::error("Unable to find ActorBase of given target");
			return false;
		}

		auto facetex = [&]() -> RE::BGSTextureSet* {
			const bool vampire = IsVampire(a_target);
			const auto sex = base->GetSex();

			auto ret = vampire ? data->Textures[BodyPart::FaceVampire][sex] : data->Textures[BodyPart::Face][sex];
			for (auto& extra : data->AdditionalTextures) {
				// does add. tex apply()
				std::string copy{ extra.TexturePath };
				copy.erase(0, copy.rfind("\\"));
				if (auto where = copy.find("female"); where != std::string::npos) {
					copy.erase(where, where + "female"s.size());
				} else if (where = copy.find("male"); where != std::string::npos) {
					copy.erase(where, where + "male"s.size());
				}
				if (race.find(copy) == std::string::npos)
					continue;
				auto& extraset = extra.ExtraSet[vampire];
				if (!extraset) {
					const auto factory = RE::IFormFactory::GetConcreteFormFactoryByType<RE::BGSTextureSet>();
					extraset = factory ? factory->Create() : nullptr;
					if (!extraset) {
						logger::error("Unable to create TextureSet for custom Headpart of type = {}, returning to default", extra.TexturePath);
						break;
					}
					extraset->Copy(ret);
					for (auto& texture : extra.Textures) {
						// Dont override Vampire diffuse
						if (vampire && texture.second == TextureType::kDiffuse)
							continue;
						extraset->textures[texture.second].textureName = extra.TexturePath + texture.first;
					}
				}
				return extraset;
			}
			return ret;
		}();

		// if (auto tex = base->headRelatedData ? base->headRelatedData->faceDetails : nullptr; tex) {
		// 	constexpr std::array validtypes{ TextureType::kDiffuse, TextureType::kNormal, TextureType::kDetailMap, TextureType::kHeight, TextureType::kSpecular };
		// 	for (auto& t : validtypes) {
		// 		auto& path = tex->textures[t].textureName;
		// 		auto& newpath = newface->textures[t].textureName;
		// 		if (path.empty() || newpath.empty())
		// 			continue;
		// 		path = newpath;
		// 	}
		// } else {
		// 	base->SetFaceTexture(newface);
		// }

		SetFaceTextureSet(a_target, facetex);
		base->skin = data->NakedSkin;

		a_target->Update3DModel();
		return true;
	}

}  // namespace DBD

RE::BSGeometry* GetFirstShaderType(RE::NiAVObject* a_object, RE::BSShaderMaterial::Feature a_type)
{
	return a_object->GetFirstGeometryOfShaderType(a_type);
}
