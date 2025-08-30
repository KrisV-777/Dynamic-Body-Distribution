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

		const auto player = RE::PlayerCharacter::GetSingleton();
		const auto playerNPC = player->GetActorBase();
		playerSexPreChargen = playerNPC ? playerNPC->GetSex() : RE::SEX::kMale;

		// RE::UI::GetSingleton()->AddEventSink<RE::MenuOpenCloseEvent>(this);
	}

	Distribution::ProfileArray<const ProfileBase*> Distribution::SelectProfiles(RE::Actor* a_target)
	{
		ProfileArray<const ProfileBase*> selectedProfiles{};
		if (excludedForms.contains(a_target->formID)) {
			return selectedProfiles;
		}

		const auto cacheIt = cache.find(a_target->formID);
		if (cacheIt != cache.end()) {
			if (a_target->IsPlayerRef()) {
				const auto npc = a_target->GetActorBase();
				if (npc && npc->GetSex() != playerSexPreChargen) {
					ClearProfiles(a_target, false);
					playerSexPreChargen = npc->GetSex();
					logger::info("Player sex changed from {} to {}", std::to_underlying(playerSexPreChargen), std::to_underlying(npc->GetSex()));
					goto SkipCaching;
				}
			}
			selectedProfiles = cacheIt->second;
		}

SkipCaching:
		for (size_t i = 0; i < ProfileIndex::Total; i++) {
			if (selectedProfiles[i] != nullptr)
				continue;
			const auto& mapping = profileMap[i];
			std::vector<const ProfileBase*> candidates;
			auto priority = ConditionPriority::None;
			for (auto&& [id, profile] : mapping) {
				if (!profile->IsApplicable(a_target)) {
					continue;
				}
				const auto tmpPriority = profile->ValidatePriority(a_target);
				if (tmpPriority == ConditionPriority::None) {
					continue;
				}
				const auto pVal = std::to_underlying(priority), tmpVal = std::to_underlying(tmpPriority);
				if (tmpVal < pVal) {
					priority = tmpPriority;
					candidates = { profile.get() };
				} else if (tmpVal == pVal) {
					candidates.push_back(profile.get());
				}
			}
			selectedProfiles[i] = candidates.empty() ? nullptr : Random::draw(candidates);
		}

		cache[a_target->formID] = selectedProfiles;
		return selectedProfiles;
	}

	bool Distribution::ApplyProfile(RE::Actor* a_target, const std::string& a_profileId, ProfileIndex a_type)
	{
		auto it = profileMap[a_type].find(a_profileId);
		if (it != profileMap[a_type].end() && it->second.get()->IsApplicable(a_target)) {
			cache[a_target->formID][a_type] = it->second.get();
			excludedForms.erase(a_target->formID);
			a_target->DoReset3D(false);
			return true;
		}
		return false;
	}

	bool Distribution::ApplyTextureProfile(RE::Actor* a_target, const std::string& a_textureId)
	{
		return ApplyProfile(a_target, a_textureId, ProfileIndex::Textures);
	}

	bool Distribution::ApplySliderProfile(RE::Actor* a_target, const std::string& a_sliderId)
	{
		return ApplyProfile(a_target, a_sliderId, ProfileIndex::Sliders);
	}

	void Distribution::ForEachProfile(const std::function<void(const ProfileBase*)>& a_callback, ProfileIndex a_type) const
	{
		const auto& source = profileMap[a_type];
		for (const auto& [id, profile] : source) {
			a_callback(profile.get());
		}
	}

	void Distribution::ForEachTextureProfile(const std::function<void(const TextureProfile*)>& a_callback) const
	{
		ForEachProfile([&](const ProfileBase* profile) {
			a_callback(static_cast<const TextureProfile*>(profile));
		},
			ProfileIndex::Textures);
	}

	void Distribution::ForEachSliderProfile(const std::function<void(const SliderProfile*)>& a_callback) const
	{
		ForEachProfile([&](const ProfileBase* profile) {
			a_callback(static_cast<const SliderProfile*>(profile));
		},
			ProfileIndex::Sliders);
	}

	Distribution::ProfileArray<const ProfileBase*> Distribution::GetProfiles(RE::Actor* a_target) const
	{
		const auto it = cache.find(a_target->formID);
		if (it != cache.end()) {
			ProfileArray<const ProfileBase*> result{};
			for (size_t i = 0; i < it->second.size(); ++i) {
				result[i] = it->second[i];
			}
			return result;
		}
		return ProfileArray<const ProfileBase*>{};
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
					profileMap[ProfileIndex::Textures][name] = std::move(profile);
					logger::info("Added Texture Set: {}", name);
				} catch (const std::exception& e) {
					logger::error("Failed to add Texture Set: {}. Error: {}", folder.path().filename().string(), e.what());
				}
			}
			logger::info("Loaded {} Texture Sets", profileMap[ProfileIndex::Textures].size());
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
					profileMap[ProfileIndex::Sliders][name] = std::move(profile);
					logger::info("Added Slider Set: {}", name);
				} catch (const std::exception& e) {
					logger::error("Failed to add Slider Set: {}. Error: {}", fileName, e.what());
				}
			}
			logger::info("Loaded {} Slider Sets ({})", profileMap[ProfileIndex::Sliders].size(), type);
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
				auto target = config["Target"];
				if (!target.IsDefined()) {
					logger::warn("Target is not defined in configuration file '{}'", fileName);
					continue;
				}
				ConditionData conditions;
				auto parseFormList = [&]<class T>(const YAML::Node& node, std::vector<T>& out) {
					if (node && node.IsSequence()) {
						for (const auto& val : node) {
							auto formStr = val.as<std::string>();
							if (formStr == "*") {
								conditions.referenceWildcard = true;
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
				parseFormList(target["Reference"], conditions.references);
				parseFormList(target["ActorBase"], conditions.actorBases);
				parseFormList(target["Keyword"], conditions.keywords);
				parseFormList(target["Faction"], conditions.factions);
				parseFormList(target["Race"], conditions.races);

				bool isEmpty = true;
				for (size_t i = 0; i < ProfileIndex::Total; i++) {
					const auto indexKey = magic_enum::enum_name(static_cast<ProfileIndex>(i));
					const auto profileNode = config[indexKey.data()];
					if (!profileNode.IsDefined() || !profileNode.IsSequence()) {
						continue;
					}
					const auto& source = profileMap[i];
					std::vector<ProfileBase*> vec{};
					for (const auto& val : profileNode) {
						const auto valStr = val.as<std::string>();
						if (valStr == "*") {
							vec.clear();
							vec.reserve(source.size());
							for (auto&& [id, profile] : source) {
								if (profile->IsPrivate())
									continue;
								vec.push_back(profile.get());
							}
						} else {
							const auto& profileObj = source.find(valStr);
							if (profileObj != source.end()) {
								vec.push_back(profileObj->second.get());
							} else {
								logger::warn("Profile '{}' not found in any profile", valStr);
							}
						}
					}
					isEmpty &= vec.empty();
					std::for_each(vec.begin(), vec.end(), [&](ProfileBase* profile) {
						profile->AppendConditions(conditions);
					});
				}
				if (isEmpty) {
					logger::error("Configuration '{}' has no valid sliders or textures. Configuration will not be loaded", fileName);
				} else {
					logger::info("Loaded configuration: {}", fileName);
				}
			} catch (const YAML::Exception& e) {
				logger::error("Failed to parse configuration file '{}': {}", fileName, e.what());
				continue;
			}
		}
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
				auto it = profileMap[n].find(cacheValue);
				if (it != profileMap[n].end()) {
					cacheEntry[n] = it->second.get();
				} else if (!cacheValue.empty()) {
					logger::error("Failed to load profile: {}", cacheValue);
				}
			}
		}
		logger::info("Loaded {} cache entries", cache.size());
	}

	void Distribution::Revert(SKSE::SerializationInterface*)
	{
		cache.clear();
	}

	RE::BSEventNotifyControl Distribution::ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*)
	{
		constexpr auto charGenMenu = RE::RaceSexMenu::MENU_NAME;
		if (!a_event || a_event->menuName != charGenMenu) {
			return RE::BSEventNotifyControl::kContinue;
		}
		const auto player = RE::PlayerCharacter::GetSingleton();
		const auto playerNPC = player->GetActorBase();
		if (!playerNPC) {
			return RE::BSEventNotifyControl::kContinue;
		}
		const auto playerSex = playerNPC->GetSex();
		if (a_event->opening) {
			playerSexPreChargen = playerSex;
		} else if (playerSex != playerSexPreChargen) {
			ClearProfiles(player, false);
			player->DoReset3D(false);
		}
		return RE::BSEventNotifyControl::kContinue;
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
		const auto& textureProfile = static_cast<const TextureProfile*>(profiles[ProfileIndex::Textures]);
		if (!textureProfile) {
			return;
		}
		textureProfile->HandleOnAttach(object);
	}

}  // namespace DBD
