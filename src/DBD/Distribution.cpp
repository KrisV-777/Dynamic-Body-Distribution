#include "Distribution.h"

#include "Util/FormLookup.h"
#include "Util/Random.h"

namespace DBD
{
	void Distribution::Initialize()
	{
		if (const auto intfc = SKEE::GetInterfaceMap()) {
			actorUpdateManager = SKEE::GetActorUpdateManager(intfc);
			morphInterface = SKEE::GetBodyMorphInterface(intfc);
			if (actorUpdateManager) {
				actorUpdateManager->AddInterface(this);
			} else {
				logger::error("Failed to get ActorUpdateManager. Some textures will not be updated");
			}
		} else {
			logger::error("Failed to get SKEE interface map");
		}

		LoadTextureProfiles();
		LoadSliderProfiles();
		LoadConditions();

		const auto player = RE::PlayerCharacter::GetSingleton();
		const auto playerNPC = player->GetActorBase();
		playerSexPreChargen = playerNPC ? playerNPC->GetSex() : RE::SEX::kMale;
	}

	ProfileArray<std::shared_ptr<const ProfileBase>> Distribution::SelectProfiles(RE::Actor* a_target)
	{
		ProfileArray<std::shared_ptr<const ProfileBase>> selectedProfiles{};
		if (excludedForms.contains(a_target->formID)) {
			return selectedProfiles;
		}

		const auto cacheIt = cache.find(a_target->formID);
		if (cacheIt != cache.end()) {
			if (a_target->IsPlayerRef()) {
				const auto npc = a_target->GetActorBase();
				if (npc && npc->GetSex() != playerSexPreChargen) {
					logger::info("Player sex changed from {} to {}", std::to_underlying(playerSexPreChargen), std::to_underlying(npc->GetSex()));
					playerSexPreChargen = npc->GetSex();
					goto SkipCaching;
				}
			}
			selectedProfiles = cacheIt->second;
		}

SkipCaching:
		using Priority = Configuration::MatchPriority;
		std::vector<const Configuration*> validConfigs;
		Priority priority{ Priority::None };
		for (const auto& config : configurations) {
			const auto tmpPriority = config.GetMatchPriority(a_target);
			if (tmpPriority < priority) {
				validConfigs = { &config };
				priority = tmpPriority;
			} else if (tmpPriority == priority) {
				validConfigs.push_back(&config);
			}
		}
		Random::shuffle(validConfigs);
		for (size_t i = 0; i < selectedProfiles.size(); i++) {
			if (selectedProfiles[i])
				continue;
			for (const auto& config : validConfigs) {
				if (const auto& profile = config->SelectProfile(a_target, ProfileType(i))) {
					selectedProfiles[i] = profile;
					break;
				}
			}
		}

		cache[a_target->formID] = selectedProfiles;
		return selectedProfiles;
	}

	bool Distribution::ApplyProfile(RE::Actor* a_target, const std::string& a_profileId, ProfileType a_type)
	{
		auto it = profileMap[a_type].find(a_profileId);
		if (it != profileMap[a_type].end() && it->second.get()->IsApplicable(a_target)) {
			cache[a_target->formID][a_type] = it->second;
			excludedForms.erase(a_target->formID);
			a_target->DoReset3D(false);
			return true;
		}
		return false;
	}

	bool Distribution::ApplyTextureProfile(RE::Actor* a_target, const std::string& a_textureId)
	{
		return ApplyProfile(a_target, a_textureId, ProfileType::Textures);
	}

	bool Distribution::ApplySliderProfile(RE::Actor* a_target, const std::string& a_sliderId)
	{
		return ApplyProfile(a_target, a_sliderId, ProfileType::Sliders);
	}

	std::shared_ptr<const ProfileBase> Distribution::GetProfile(std::string a_profileId, ProfileType a_type) const
	{
		const auto& source = profileMap[a_type];
		auto it = source.find(a_profileId);
		if (it != source.end()) {
			return it->second;
		}
		return nullptr;
	}

	void Distribution::ForEachProfile(const std::function<void(std::shared_ptr<const ProfileBase>)>& a_callback, ProfileType a_type) const
	{
		const auto& source = profileMap[a_type];
		for (const auto& [id, profile] : source) {
			a_callback(profile);
		}
	}

	void Distribution::ForEachTextureProfile(const std::function<void(const TextureProfile*)>& a_callback) const
	{
		ForEachProfile([&](std::shared_ptr<const ProfileBase> profile) {
			a_callback(static_cast<const TextureProfile*>(profile.get()));
		},
			ProfileType::Textures);
	}

	void Distribution::ForEachSliderProfile(const std::function<void(const SliderProfile*)>& a_callback) const
	{
		ForEachProfile([&](std::shared_ptr<const ProfileBase> profile) {
			a_callback(static_cast<const SliderProfile*>(profile.get()));
		},
			ProfileType::Sliders);
	}

	ProfileArray<std::shared_ptr<const ProfileBase>> Distribution::GetProfiles(RE::Actor* a_target) const
	{
		const auto it = cache.find(a_target->formID);
		if (it != cache.end()) {
			ProfileArray<std::shared_ptr<const ProfileBase>> result{};
			for (size_t i = 0; i < it->second.size(); ++i) {
				result[i] = it->second[i];
			}
			return result;
		}
		return ProfileArray<std::shared_ptr<const ProfileBase>>{};
	}

	void Distribution::ClearProfiles(RE::Actor* a_target, bool a_exclude)
	{
		const auto formID = a_target->formID;
		excludedForms.insert(formID);
		cache.erase(formID);
		SliderProfile::DeleteMorphs(a_target, morphInterface);
		a_target->DoReset3D(false);
		if (!a_exclude) {
			excludedForms.erase(formID);
		}
	}

	void Distribution::LoadTextureProfiles()
	{
		logger::info("Loading Texture Sets");
		if (!fs::exists(TEXTURE_ROOT_PATH)) {
			logger::critical("Path to textures does not exist");
		} else {
			for (auto& folder : fs::directory_iterator{ TEXTURE_ROOT_PATH }) {
				if (!folder.is_directory())
					continue;
				try {
					auto profile = std::make_unique<TextureProfile>(folder);
					auto name = std::string{ profile->GetName() };
					profileMap[ProfileType::Textures][name] = std::move(profile);
					logger::info("Added Texture Set: {}", name);
				} catch (const std::exception& e) {
					logger::error("Failed to add Texture Set: {}. Error: {}", folder.path().filename().string(), e.what());
				}
			}
			logger::info("Loaded {} Texture Sets", profileMap[ProfileType::Textures].size());
		}
	}

	void Distribution::LoadSliderProfiles()
	{
		logger::info("Loading Slider Sets");
		if (!morphInterface) {
			logger::critical("Missing morph interface. Skipping slider profile initialization");
			return;
		}
		for (auto& type : std::vector{ "male", "female" }) {
			const auto rootFolder = std::format("{}/{}", SLIDER_ROOT_PATH, type);
			if (!fs::exists(rootFolder)) {
				logger::critical("Path to slider '{}' does not exist", type);
				continue;
			}
			for (auto& file : fs::directory_iterator{ rootFolder }) {
				if (!file.is_regular_file())
					continue;
				const auto fileName = file.path().filename().string();
				if (!fileName.ends_with(".xml")) {
					logger::warn("Skipping non-XML file: {}", fileName);
					continue;
				}
				try {
					auto profile = std::make_unique<SliderProfile>(file.path().string(), (type == "male"s), morphInterface);
					auto name = std::string{ profile->GetName() };
					profileMap[ProfileType::Sliders][name] = std::move(profile);
					logger::info("Added Slider Set: {}", name);
				} catch (const std::exception& e) {
					logger::error("Failed to add Slider Set: {}. Error: {}", fileName, e.what());
				}
			}
			logger::info("Loaded {} Slider Sets ({})", profileMap[ProfileType::Sliders].size(), type);
		}
	}

	void Distribution::LoadConditions()
	{
		logger::info("Loading ConfigDatas");
		if (!fs::exists(CONFIGURATION_ROOT_PATH)) {
			logger::critical("Path to ConfigDatas does not exist");
			return;
		}
		for (auto& file : fs::directory_iterator{ CONFIGURATION_ROOT_PATH }) {
			if (!file.is_regular_file())
				continue;
			const auto fileName = file.path().filename().string();
			if (!fileName.ends_with(".yml") && !fileName.ends_with(".yaml")) {
				logger::warn("Skipping non-YML file: {}", fileName);
				continue;
			}
			try {
				[[maybe_unused]] YAML::Node config = YAML::LoadFile(file.path().string());
				// ConfigDatas.emplace_back(config, this);
			} catch (const YAML::Exception& e) {
				logger::error("Failed to parse ConfigData file '{}': {}", fileName, e.what());
				continue;
			}
		}
	}

	void Distribution::Save(SKSE::SerializationInterface* a_intfc, uint32_t)
	{
		std::size_t numRegs = cache.size();
		if (!a_intfc->WriteRecordData(numRegs)) {
			logger::error("Failed to save number of regs ({})", numRegs);
			return;
		}
		for (auto&& [formID, data] : cache) {
			if (!a_intfc->WriteRecordData(formID)) {
				logger::error("Failed to save reg ({:X})", formID);
				continue;
			}
			// COMEBACK: If version ever gets a value != 1, update index max here
			for (size_t i = 0; i < ProfileType::Total_V1; i++) {
				if (!stl::write_string(a_intfc, data[i] ? data[i]->GetName() : ""s)) {
					logger::error("Failed to save reg ({})", data[i] ? data[i]->GetName().data() : "null");
					continue;
				}
			}
		}

		numRegs = excludedForms.size();
		if (!a_intfc->WriteRecordData(numRegs)) {
			logger::error("Failed to save number of excluded forms ({})", numRegs);
			return;
		}
		for (const auto& formID : excludedForms) {
			if (!a_intfc->WriteRecordData(formID)) {
				logger::error("Failed to save excluded form ({:X})", formID);
				continue;
			}
		}
	}

	void Distribution::Load(SKSE::SerializationInterface* a_intfc, uint32_t)
	{
		cache.clear();
		excludedForms.clear();
		size_t numRegs;
		a_intfc->ReadRecordData(numRegs);

		RE::FormID formID;
		std::string cacheValue;
		for (size_t i = 0; i < numRegs; i++) {
			a_intfc->ReadRecordData(formID);
			if (!a_intfc->ResolveFormID(formID, formID)) {
				logger::warn("Error reading formID: {:X}", formID);
				continue;
			}
			auto& cacheEntry = cache[formID];
			// COMEBACK: If version ever gets a value != 1, update index max here
			for (size_t n = 0; n < ProfileType::Total_V1; n++) {
				if (!stl::read_string(a_intfc, cacheValue)) {
					logger::error("Failed to load reg: {}", cacheValue);
					continue;
				}
				auto it = profileMap[n].find(cacheValue);
				if (it != profileMap[n].end()) {
					cacheEntry[n] = it->second;
				} else if (!cacheValue.empty()) {
					logger::error("Failed to load profile: {}", cacheValue);
				}
			}
		}
		logger::info("Loaded {} cache entries", cache.size());

		a_intfc->ReadRecordData(numRegs);
		for (size_t i = 0; i < numRegs; i++) {
			a_intfc->ReadRecordData(formID);
			if (!a_intfc->ResolveFormID(formID, formID)) {
				logger::warn("Error reading formID: {:X}", formID);
				continue;
			}
			excludedForms.insert(formID);
		}
		logger::info("Loaded {} excluded forms", excludedForms.size());
	}

	void Distribution::Revert(SKSE::SerializationInterface*)
	{
		cache.clear();
	}

	void Distribution::OnAttach(
		[[maybe_unused]] RE::TESObjectREFR* refr,
		[[maybe_unused]] RE::TESObjectARMO* armor,
		[[maybe_unused]] RE::TESObjectARMA* addon,
		[[maybe_unused]] RE::NiAVObject* object,
		[[maybe_unused]] bool isFirstPerson,
		[[maybe_unused]] RE::NiNode* skeleton,
		[[maybe_unused]] RE::NiNode* root)
	{
		if (!refr || !armor || !addon || !object) {
			return;
		}
		const auto cacheEntry = cache.find(refr->GetFormID());
		if (cacheEntry == cache.end()) {
			return;
		}
		const auto& profiles = cacheEntry->second;
		const auto& profilePtr = profiles[ProfileType::Textures];
		if (!profilePtr) {
			return;
		}
		const auto& textureProfile = static_cast<const TextureProfile*>(profilePtr.get());
		textureProfile->OverrideObjectTextures(object);
	}

	Distribution::Configuration::Configuration(const YAML::Node& a_node, const Distribution* a_distribution)
	{
		const auto targetNode = a_node["Target"];
		const auto sliderNode = a_node["Sliders"];
		const auto textureNode = a_node["Textures"];
		if (!targetNode) {
			throw std::runtime_error("Target is not defined in configuration");
		} else if (!sliderNode && !textureNode) {
			throw std::runtime_error("At least one slider or texture must be defined in configuration");
		}
		auto parseFormList = [&]<class T>(const YAML::Node& node, std::vector<T>& out) {
			if (node && !isWildcardConfig) {
				for (const auto& val : node) {
					auto formStr = val.as<std::string>();
					if (formStr == "*") {
						out.clear();
						isWildcardConfig = true;
						return;
					}
					T form;
					if constexpr (std::is_same_v<T, RE::FormID>) {
						form = Util::FormFromString(formStr);
					} else {
						form = Util::FormFromString<T>(formStr);
					}
					if (form) {
						out.push_back(form);
					} else {
						logger::warn("Invalid form ID: {}", formStr);
					}
				}
			}
		};
		parseFormList(targetNode["Reference"], references);
		parseFormList(targetNode["ActorBase"], actorBases);
		parseFormList(targetNode["Keyword"], keywords);
		parseFormList(targetNode["Faction"], factions);
		parseFormList(targetNode["Race"], races);
		if (const auto conditionNode = targetNode["Condition"]) {
			const auto rawConditions = conditionNode.as<std::vector<std::string>>(std::vector<std::string>{});
			conditions = Conditions::Conditional{
				rawConditions,
				a_node["refMap"].as<std::map<std::string, std::string>>(std::map<std::string, std::string>{})
			};
		}
		for (size_t i = 0; i < ProfileType::Total; i++) {
			const auto profileIdx = static_cast<ProfileType>(i);
			const auto indexKey = magic_enum::enum_name(profileIdx);
			const auto profileNode = a_node[indexKey.data()];
			if (!profileNode.IsDefined() || !profileNode.IsSequence()) {
				continue;
			}
			auto& dest = profiles[i];
			for (const auto& val : profileNode) {
				const auto valStr = val.as<std::string>();
				if (valStr == "*") {
					dest.clear();
					a_distribution->ForEachProfile([&](std::shared_ptr<const ProfileBase> profil) {
						dest.push_back(profil);
					},
						profileIdx);
				} else {
					const auto& profile = a_distribution->GetProfile(valStr, profileIdx);
					if (profile) {
						dest.push_back(profile);
					} else {
						logger::warn("Profile '{}' not found in any profile", valStr);
					}
				}
			}
		}
	}

	std::shared_ptr<const ProfileBase> Distribution::Configuration::SelectProfile(RE::Actor* a_target, ProfileType a_type) const
	{
		const auto& profileList = profiles[a_type];
		std::vector<size_t> indices(profileList.size());
		std::iota(indices.begin(), indices.end(), 0);
		Random::shuffle(indices);
		for (size_t idx : indices) {
			if (profileList[idx]->IsApplicable(a_target)) {
				return profileList[idx];
			}
		}
		return nullptr;
	}

	Distribution::Configuration::MatchPriority Distribution::Configuration::GetMatchPriority(RE::Actor* a_target) const
	{
		if (conditions && !conditions.ConditionsMet(a_target, RE::PlayerCharacter::GetSingleton())) {
			return MatchPriority::None;
		} else if (isWildcardConfig) {
			return MatchPriority::Wildcard;
		} else if (std::ranges::contains(references, a_target->formID)) {
			return MatchPriority::Reference;
		}
		const auto npc = a_target->GetActorBase();
		const auto npcId = npc ? npc->GetFormID() : RE::FormID{ 0 };
		if (std::ranges::contains(actorBases, npcId)) {
			return MatchPriority::ActorBase;
		} else if (std::ranges::any_of(factions, [&](RE::TESFaction* faction) { return a_target->IsInFaction(faction); }) ||
				   a_target->HasKeywordInArray(keywords, false)) {
			return MatchPriority::Group;
		} else if (std::ranges::any_of(races, [&](RE::TESRace* race) { return race == npc->GetRace(); })) {
			return MatchPriority::Race;
		}
		return MatchPriority::None;
	}

}  // namespace DBD
