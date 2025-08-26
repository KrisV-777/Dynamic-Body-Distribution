#include "Distribution.h"

#include <yaml-cpp/yaml.h>

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
	}

	Distribution::ProfileArray Distribution::SelectProfiles(RE::Actor* a_target)
	{
		ProfileArray selectedProfiles{};
		if (excludedForms.contains(a_target->formID)) {
			return selectedProfiles;
		}

		const auto cacheIt = cache.find(a_target->formID);
		if (cacheIt != cache.end()) {
			selectedProfiles = cacheIt->second;
		}

		auto trySelect = [&](auto* cfg, auto idx) mutable {
			if (selectedProfiles[idx] != nullptr)
				return true;
			auto& profiles = cfg->profiles[idx];
			Random::shuffle(profiles);
			for (auto* p : profiles)
				if (p->IsApplicable(a_target))
					return selectedProfiles[idx] = p, true;
			return false;
		};

		std::vector<DistributionConfig*> wildcards;
		for (auto& cfg : configurations) {
			switch (cfg.GetApplicationLevel(a_target)) {
			case DistributionConfig::MatchLevel::None:
				break;
			case DistributionConfig::MatchLevel::Wildcard:
				wildcards.push_back(&cfg);
				break;
			default:
				trySelect(&cfg, ProfileIndex::TextureId);
				trySelect(&cfg, ProfileIndex::SliderId);
			}
		}

		const auto draw = [&]<class T>(T& map) -> typename T::mapped_type* {
			if (map.empty())
				return nullptr;
			std::vector<typename T::mapped_type*> result;
			result.reserve(map.size());
			for (auto& [key, value] : map) {
				if (!value.IsPrivate() && value.IsApplicable(a_target))
					result.push_back(&value);
			}
			return result.empty() ? nullptr : Random::draw(result);
		};

		using Wildcard = DistributionConfig::Wildcard;
		Random::shuffle(wildcards);
		while (!wildcards.empty() && std::ranges::find(selectedProfiles, nullptr) != selectedProfiles.end()) {
			auto* cfg = wildcards.back();
			wildcards.pop_back();
			for (size_t i = 0; i < ProfileIndex::Total; i++) {
				if (selectedProfiles[i] != nullptr)
					continue;
				if (cfg->wildcards.all(Wildcard(1 << (i + 1)))) {
					if (i == ProfileIndex::TextureId) {
						selectedProfiles[i] = draw(textures);
					} else {
						selectedProfiles[i] = draw(sliders);
					}
				} else {
					trySelect(cfg, i);
				}
			}
		}

		cache[a_target->formID] = selectedProfiles;
		return selectedProfiles;
	}

	bool Distribution::ApplyTextureProfile(RE::Actor* a_target, const std::string& a_textureId)
	{
		auto it = textures.find(a_textureId);
		if (it != textures.end() && it->second.IsApplicable(a_target)) {
			cache[a_target->formID][ProfileIndex::TextureId] = &it->second;
			excludedForms.erase(a_target->formID);
			a_target->DoReset3D(false);
			return true;
		}
		return false;
	}

	bool Distribution::ApplySliderProfile(RE::Actor* a_target, const std::string& a_sliderId)
	{
		auto it = sliders.find(a_sliderId);
		if (it != sliders.end() && it->second.IsApplicable(a_target)) {
			cache[a_target->formID][ProfileIndex::SliderId] = &it->second;
			excludedForms.erase(a_target->formID);
			a_target->DoReset3D(true);
			return true;
		}
		return false;
	}

	void Distribution::ForEachTextureProfile(const std::function<void(const TextureProfile*)>& a_callback) const
	{
		if (!a_callback) {
			return;
		}
		for (const auto& [id, profile] : textures) {
			a_callback(&profile);
		}
	}

	void Distribution::ForEachSliderProfile(const std::function<void(const SliderProfile*)>& a_callback) const
	{
		if (!a_callback) {
			return;
		}
		for (const auto& [id, profile] : sliders) {
			a_callback(&profile);
		}
	}

	const Distribution::ProfileArray& Distribution::GetProfiles(RE::Actor* a_target)
	{
		const auto it = cache.find(a_target->formID);
		return it != cache.end() ? it->second : (cache[a_target->formID] = ProfileArray{});
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
					TextureProfile profile{ folder };
					auto name = profile.GetName();
					textures.emplace(std::make_pair(name, profile));
					logger::info("Added Texture Set: {}", name);
				} catch (const std::exception& e) {
					logger::error("Failed to add Texture Set: {}. Error: {}", folder.path().filename().string(), e.what());
				}
			}
			logger::info("Loaded {} Texture Sets", textures.size());
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
					SliderProfile profile{ file.path().string(), (type == "male"s), morphInterface };
					auto name = profile.GetName();
					sliders.emplace(std::make_pair(name, profile));
					logger::info("Added Slider Set: {}", name);
				} catch (const std::exception& e) {
					logger::error("Failed to add Slider Set: {}. Error: {}", fileName, e.what());
				}
			}
			logger::info("Loaded {} Slider Sets ({})", sliders.size(), type);
		}
	}

	void Distribution::LoadConditions()
	{
		logger::info("Loading Configurations");
		if (!fs::exists(CONFIGURATION_ROOT_PATH)) {
			logger::critical("Path to configurations does not exist");
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
				YAML::Node config = YAML::LoadFile(file.path().string());
				DistributionConfig configuration;

				if (auto target = config["Target"]) {
					auto parseFormList = [&]<class T>(const YAML::Node& node, std::vector<T>& out) {
						if (node && node.IsSequence()) {
							for (const auto& val : node) {
								auto formStr = val.as<std::string>();
								if (formStr == "*") {
									configuration.wildcards.set(DistributionConfig::Wildcard::Condition);
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
					parseFormList(target["Reference"], configuration.references);
					parseFormList(target["ActorBase"], configuration.actorBases);
					parseFormList(target["Keyword"], configuration.keywords);
					parseFormList(target["Faction"], configuration.factions);
					parseFormList(target["Race"], configuration.races);
				}

				auto parseProfileList = [&]<class T>(const YAML::Node& node, std::vector<ProfileBase*>& out, const std::map<std::string, T, StringComparator>& source) -> bool {
					if (!node || !node.IsSequence()) {
						return false;
					}
					for (const auto& val : node) {
						const auto valStr = val.as<std::string>();
						if (valStr == "*") {
							return true;
						}
						const auto& profileObj = source.find(valStr);
						if (profileObj != source.end()) {
							out.push_back(const_cast<T*>(&profileObj->second));
						} else {
							logger::warn("Profile '{}' not found in any profile", valStr);
						}
					}
					return false;
				};

				if (auto sliderNode = config["Sliders"]) {
					auto& vec = configuration.profiles[ProfileIndex::SliderId];
					if (parseProfileList(sliderNode, vec, sliders)) {
						configuration.wildcards.set(DistributionConfig::Wildcard::Sliders);
						vec.clear();
					}
				}

				if (auto textureNode = config["Textures"]) {
					auto& vec = configuration.profiles[ProfileIndex::TextureId];
					if (parseProfileList(textureNode, vec, textures)) {
						configuration.wildcards.set(DistributionConfig::Wildcard::Textures);
						vec.clear();
					}
				}

				if (configuration.profiles[ProfileIndex::SliderId].empty() && configuration.profiles[ProfileIndex::TextureId].empty() &&
					configuration.wildcards.none(DistributionConfig::Wildcard::Sliders, DistributionConfig::Wildcard::Textures)) {
					logger::error("Configuration '{}' has no valid sliders or textures", fileName);
					continue;
				}

				configurations.emplace_back(std::move(configuration));
				logger::info("Loaded configuration: {}", fileName);
			} catch (const YAML::Exception& e) {
				logger::error("Failed to parse configuration file '{}': {}", fileName, e.what());
				continue;
			}
		}
		logger::info("Loaded {} configurations", configurations.size());
	}

	Distribution::DistributionConfig::MatchLevel Distribution::DistributionConfig::GetApplicationLevel(RE::Actor* a_target) const
	{
		if (wildcards.any(Wildcard::Condition)) {
			return MatchLevel::Wildcard;  // Wildcard condition matches all actors
		}
		if (std::ranges::contains(references, a_target->GetFormID()))
			return MatchLevel::Explicit;
		const auto base = a_target->GetActorBase();
		if (base && std::ranges::contains(actorBases, base->GetFormID()))
			return MatchLevel::Explicit;
		const auto race = a_target->GetRace();
		if (race && std::ranges::contains(races, race->GetFormID()))
			return MatchLevel::Explicit;
		if (a_target->HasKeywordInArray(keywords, false))
			return MatchLevel::Explicit;
		if (std::ranges::any_of(factions, [&](const RE::TESFaction* faction) {
				return a_target->IsInFaction(faction);
			})) {
			return MatchLevel::Explicit;
		}
		return MatchLevel::None;
	}

	void Distribution::Save(SKSE::SerializationInterface* a_intfc, uint32_t)
	{
		const std::size_t numRegs = cache.size();
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
			for (size_t i = 0; i < ProfileIndex::Total_V1; i++) {
				if (!stl::write_string(a_intfc, data[i] ? data[i]->GetName() : ""s)) {
					logger::error("Failed to save reg ({})", data[i] ? data[i]->GetName().data() : "null");
					continue;
				}
			}
		}
	}

	void Distribution::Load(SKSE::SerializationInterface* a_intfc, uint32_t)
	{
		cache.clear();
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
			for (size_t n = 0; n < ProfileIndex::Total_V1; n++) {
				if (!stl::read_string(a_intfc, cacheValue)) {
					logger::error("Failed to load reg: {}", cacheValue);
					continue;
				}
				auto assignProfile = [&]<class T>(T& profiles) {
					auto it = profiles.find(cacheValue);
					if (it != profiles.end()) {
						cacheEntry[n] = &it->second;
					} else if (!cacheValue.empty()) {
						logger::error("Failed to load profile: {}", cacheValue);
					}
				};
				switch (n) {
				case ProfileIndex::SliderId:
					assignProfile(sliders);
					break;
				case ProfileIndex::TextureId:
					assignProfile(textures);
					break;
				default:
					logger::warn("Unknown ProfileIndex: {}", n);
					break;
				}
			}
		}
		logger::info("Loaded {} cache entries", cache.size());
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
		if (!refr || !armor || !addon || !object || !refr->IsPlayerRef()) {
			return;
		}
		const auto cacheEntry = cache.find(refr->GetFormID());
		if (cacheEntry == cache.end()) {
			return;
		}
		// Apply cached profiles to the actor
		const auto& profiles = cacheEntry->second;
		const auto& textureProfile = static_cast<TextureProfile*>(profiles[ProfileIndex::TextureId]);
		if (!textureProfile) {
			return;
		}
		textureProfile->HandleOnAttach(object);
	}

}  // namespace DBD
