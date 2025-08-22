#include "Distribution.h"

#include <yaml-cpp/yaml.h>

#include "Util/FormLookup.h"
#include "Util/Random.h"

namespace DBD
{
	void Distribution::Initialize()
	{
		LoadTextureProfiles();
		LoadSliderProfiles();
		LoadConditions();
	}

	int32_t Distribution::ApplyProfiles(RE::Actor* a_target)
	{
		bool textureApplied = false, sliderApplied = false;
		const auto cacheIt = cache.find(a_target->formID);
		if (cacheIt != cache.end()) {
			const auto& cached = cacheIt->second;
			textureApplied = cached[CacheIndex::TextureId] != "" && ApplyTextureProfile(a_target, cached[CacheIndex::TextureId]);
			sliderApplied = cached[CacheIndex::SliderId] != "" && ApplySliderProfile(a_target, cached[CacheIndex::SliderId]);
		}

		auto tryApply = [&](auto& profiles, auto fn) {
			Random::shuffle(profiles);
			for (auto* p : profiles)
				if (p->IsApplicable(a_target) && fn(a_target, p->GetName()))
					return true;
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
				textureApplied |= tryApply(cfg.textures, [this](auto* t, auto n) { return ApplyTextureProfile(t, n); });
				sliderApplied |= tryApply(cfg.sliders, [this](auto* t, auto n) { return ApplySliderProfile(t, n); });
			}
		}
		if (textureApplied && sliderApplied)
			return 2;
		Random::shuffle(wildcards);
		for (auto* cfg : wildcards) {
			textureApplied |= tryApply(cfg->textures, [this](auto* t, auto n) { return ApplyTextureProfile(t, n); });
			sliderApplied |= tryApply(cfg->sliders, [this](auto* t, auto n) { return ApplySliderProfile(t, n); });
		}
		return textureApplied + sliderApplied;
	}

	bool Distribution::ApplyTextureProfile(RE::Actor* a_target, const RE::BSFixedString& a_textureId)
	{
		auto it = textures.find(a_textureId);
		if (it != textures.end() && ApplyProfile(a_target, it->second)) {
			cache[a_target->formID][CacheIndex::TextureId] = a_textureId;
		}
		return false;
	}

	bool Distribution::ApplySliderProfile(RE::Actor* a_target, const RE::BSFixedString& a_sliderId)
	{
		auto it = sliders.find(a_sliderId);
		if (it != sliders.end() && ApplyProfile(a_target, it->second)) {
			cache[a_target->formID][CacheIndex::SliderId] = a_sliderId;
		}
		return false;
	}

	bool Distribution::ApplyProfile(RE::Actor* a_target, const ProfileBase& a_profile)
	{
		if (a_profile.IsApplicable(a_target)) {
			return a_profile.Apply(a_target), true;
		}
		return false;
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
					SliderProfile profile{ file.path().string(), (type == "male"s) };
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

				auto parseProfileList = [&]<class T>(const YAML::Node& node, std::vector<ProfileBase*>& out, const std::map<RE::BSFixedString, T, FixedStringComparator>& source) -> bool {
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
					if (parseProfileList(sliderNode, configuration.sliders, sliders)) {
						configuration.sliders.clear();
						configuration.wildcards.set(DistributionConfig::Wildcard::Sliders);
					}
				}

				if (auto textureNode = config["Textures"]) {
					if (parseProfileList(textureNode, configuration.textures, textures)) {
						configuration.textures.clear();
						configuration.wildcards.set(DistributionConfig::Wildcard::Textures);
					}
				}

				if (configuration.sliders.empty() && configuration.textures.empty() &&
					!configuration.wildcards.any(DistributionConfig::Wildcard::Sliders, DistributionConfig::Wildcard::Textures)) {
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
			for (size_t i = 0; i < CacheIndex::Total_V1; i++) {
				if (!stl::write_string(a_intfc, data[i])) {
					logger::error("Failed to save reg ({})", data[i].data());
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
				logger::warn("Error reading formID ({:X})", formID);
				continue;
			}
			auto& cacheEntry = cache[formID];
			// COMEBACK: If version ever gets a value != 1, update index max here
			for (size_t n = 0; n < CacheIndex::Total_V1; n++) {
				if (!stl::read_string(a_intfc, cacheValue)) {
					logger::error("Failed to load reg ({})", cacheValue);
					continue;
				}
				cacheEntry[n] = cacheValue;
			}
		}
		logger::info("Loaded {} cache entries", cache.size());
	}

	void Distribution::Revert(SKSE::SerializationInterface*)
	{
		cache.clear();
	}

}  // namespace DBD
