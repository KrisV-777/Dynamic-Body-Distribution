#include "Database.h"

#include <functional>
#include <unordered_set>

namespace DBD::Data
{
	namespace detail
	{
		template <class T>
		class CandidateResolver
		{
			using ObjectPtr = std::shared_ptr<T>;

		public:
			template <class GetCandidates>
			CandidateResolver(const std::vector<ObjectPtr>& a_objects, const std::vector<PreloadedRule>& a_rules, GetCandidates a_getCandidates)
			{
				std::unordered_set<std::string_view> exclusiveNames;
				for (const auto& rule : a_rules) {
					for (const auto& candidate : std::invoke(a_getCandidates, rule)) {
						if (candidate._exclusive) {
							exclusiveNames.emplace(candidate._name);
						}
					}
				}

				_byName.reserve(a_objects.size());
				_randomCandidates.reserve(a_objects.size());
				for (const auto& object : a_objects) {
					const auto name = object->GetName();
					_byName.try_emplace(name, object);
					if (!exclusiveNames.contains(name)) {
						_randomCandidates.push_back(object);
					}
				}
			}

			std::vector<ObjectPtr> Resolve(const std::vector<PreloadedRule::Candidate>& a_candidates, bool a_wildcard) const
			{
				if (a_wildcard) {
					return _randomCandidates;
				}

				std::vector<ObjectPtr> result;
				result.reserve(a_candidates.size());
				for (const auto& candidate : a_candidates) {
					if (const auto it = _byName.find(candidate._name); it != _byName.end()) {
						result.push_back(it->second);
					}
				}
				return result;
			}

		private:
			std::unordered_map<std::string_view, ObjectPtr> _byName;
			std::vector<ObjectPtr> _randomCandidates;
		};
	}  // namespace detail

}  // namespace DBD::Data

namespace DBD
{
	using Data::BodyslidePreset;
	using Data::PreloadedRule;
	using Data::RaceMenuPreset;
	using Data::Rule;
	using Data::SliderConfig;
	using Data::TexturePack;
	namespace detail = Data::detail;

	Database::Database()
	{
		logger::info("Initializing Database");

		// TODO: Preset Interface currently isnt available at all. Look for an alternative?
		SKEE::IBodyMorphInterface* morphInterface = nullptr;
		SKEE::IPresetInterface* presetInterface = nullptr;
		if (const auto intfc = SKEE::GetInterfaceMap()) {
			morphInterface = SKEE::GetBodyMorphInterface(intfc);
			presetInterface = SKEE::GetPresetInterface(intfc);
		} else {
			logger::error("Failed to get SKEE interface map");
		}


		std::array threads{
			std::thread([this] { LoadTexturePacks(); }),
			std::thread([this, morphInterface] { LoadBodyslidePresets(morphInterface); }),
			std::thread([this, presetInterface] { LoadRaceMenuPresets(presetInterface); })
		};
		const auto preloadedRules = PreloadRules();

		for (auto& thread : threads) {
			thread.join();
		}

		const detail::CandidateResolver<TexturePack> texturePacks{
			_texturePacks, preloadedRules, &PreloadedRule::_textureCandidates
		};
		const detail::CandidateResolver<BodyslidePreset> bodyslidePresets{
			_bodyslidePresets, preloadedRules, &PreloadedRule::_bodySlideCandidates
		};
		const detail::CandidateResolver<RaceMenuPreset> raceMenuPresets{
			_raceMenuPresets, preloadedRules, &PreloadedRule::_raceMenuCandidate
		};

		_rules.reserve(preloadedRules.size());
		for (const auto& rule : preloadedRules) {
			_rules.emplace_back(
				rule._name,
				rule._conditional,
				texturePacks.Resolve(rule._textureCandidates, rule._wildcardTexturePack),
				bodyslidePresets.Resolve(rule._bodySlideCandidates, rule._wildcardBodyslidePreset),
				raceMenuPresets.Resolve(rule._raceMenuCandidate, rule._wildcardRaceMenuPreset));
		}

		logger::info("Database initialized with {} texture packs, {} bodyslide presets, {} racemenu presets, and {} rules",
			_texturePacks.size(), _bodyslidePresets.size(), _raceMenuPresets.size(), _rules.size());
	}

	std::shared_ptr<TexturePack> Database::SelectTexturePack(RE::Actor* a_actor) const
	{
		std::shared_ptr<TexturePack> selectedPack = nullptr;
		uint16_t highestPriority = (std::numeric_limits<uint16_t>::max)();
		for (const auto& rule : _rules) {
			const auto priority = rule.GetPriority();
			if (priority >= highestPriority || !rule.ConditionsMet(a_actor)) {
				continue;
			}
			if (const auto pack = rule.SelectTexturePack(a_actor)) {
				selectedPack = pack;
				highestPriority = priority;
			}
		}
		return selectedPack;
	}

	std::shared_ptr<BodyslidePreset> Database::SelectBodyslidePreset(RE::Actor* a_actor) const
	{
		std::shared_ptr<BodyslidePreset> selectedPreset = nullptr;
		uint16_t highestPriority = (std::numeric_limits<uint16_t>::max)();
		for (const auto& rule : _rules) {
			const auto priority = rule.GetPriority();
			if (priority >= highestPriority || !rule.ConditionsMet(a_actor)) {
				continue;
			}
			if (const auto preset = rule.SelectBodyslidePreset(a_actor)) {
				if (preset->IsApplicable(a_actor)) {
					selectedPreset = preset;
					highestPriority = priority;
				}
			}
		}
		return selectedPreset;
	}

	std::shared_ptr<RaceMenuPreset> Database::SelectRaceMenuPreset(RE::Actor* a_actor) const
	{
		std::shared_ptr<RaceMenuPreset> selectedPreset = nullptr;
		uint16_t highestPriority = (std::numeric_limits<uint16_t>::max)();
		for (const auto& rule : _rules) {
			const auto priority = rule.GetPriority();
			if (priority >= highestPriority || !rule.ConditionsMet(a_actor)) {
				continue;
			}
			if (const auto preset = rule.SelectRaceMenuPreset(a_actor)) {
				selectedPreset = preset;
				highestPriority = priority;
			}
		}
		return selectedPreset;
	}

	template <typename T>
	std::shared_ptr<T> FindByName(const std::vector<std::shared_ptr<T>>& a_objects, std::string_view name)
	{
		const auto it = std::ranges::find_if(a_objects, [name](const auto& obj) { return obj->GetName() == name; });
		return it != a_objects.end() ? *it : nullptr;
	}
	std::shared_ptr<TexturePack> Database::FindTexturePackByName(std::string_view name) const { return FindByName(_texturePacks, name); }
	std::shared_ptr<BodyslidePreset> Database::FindBodyslidePresetByName(std::string_view name) const { return FindByName(_bodyslidePresets, name); }
	std::shared_ptr<RaceMenuPreset> Database::FindRaceMenuPresetByName(std::string_view name) const { return FindByName(_raceMenuPresets, name); }

	void Database::LoadTexturePacks()
	{
		if (!fs::exists(TEXTURE_ROOT_PATH)) {
			logger::critical("Path to textures does not exist");
			return;
		}
		for (const auto& file : fs::recursive_directory_iterator{ TEXTURE_ROOT_PATH }) {
			if (!file.is_regular_file() || file.path().extension() != ".json") {
				continue;
			}
			try {
				_texturePacks.emplace_back(std::make_shared<TexturePack>(file.path().string()));
				logger::info("Added Texture Set: {}", file.path().filename().string());
			} catch (const std::exception& e) {
				logger::error("Failed to add Texture Set: {}. Error: {}", file.path().filename().string(), e.what());
			}
		}
	}

	void Database::LoadBodyslidePresets(SKEE::IBodyMorphInterface* morphInterface)
	{
		if (!morphInterface) {
			logger::critical("Missing morph interface. Skipping slider profile initialization");
			return;
		}
		SliderConfig sliderConfig{};
		for (auto& xmlFile : fs::directory_iterator{ SLIDER_ROOT_PATH }) {
			if (!xmlFile.is_regular_file() || xmlFile.path().extension() != ".xml") {
				continue;
			}
			try {
				const auto sliderProfiles = Data::LoadBodyslidePresets(xmlFile.path(), morphInterface, &sliderConfig);
				for (const auto& profile : sliderProfiles) {
					_bodyslidePresets.push_back(profile);
					logger::info("Added Slider Set: {}", profile->GetName());
				}
			} catch (const std::exception& e) {
				logger::error("Failed to add Slider Set: {}. Error: {}", xmlFile.path().filename().string(), e.what());
			}
		}
	}

	void Database::LoadRaceMenuPresets(SKEE::IPresetInterface* presetInterface)
	{
		if (!presetInterface) {
			logger::critical("Missing preset interface. Skipping RaceMenu preset initialization");
			return;
		}
		for (auto& jslotFile : fs::directory_iterator{ RACEMENU_ROOT_PATH }) {
			if (!jslotFile.is_regular_file() || jslotFile.path().extension() != ".jslot") {
				continue;
			}
			try {
				_raceMenuPresets.emplace_back(std::make_shared<RaceMenuPreset>(jslotFile.path(), presetInterface));
				logger::info("Added RaceMenu Preset: {}", jslotFile.path().filename().string());
			} catch (const std::exception& e) {
				logger::error("Failed to add RaceMenu Preset: {}. Error: {}", jslotFile.path().filename().string(), e.what());
			}
		}
	}

	std::vector<PreloadedRule> Database::PreloadRules()
	{
		std::vector<PreloadedRule> preloadedRules;
		if (!fs::exists(RULES_ROOT_PATH)) {
			logger::critical("Path to rules does not exist");
			return preloadedRules;
		}
		for (auto& file : fs::directory_iterator{ RULES_ROOT_PATH }) {
			if (!file.is_regular_file() || file.path().extension() != ".json") {
				continue;
			}
			try {
				preloadedRules.emplace_back(file.path().string());
				logger::info("Added Rule: {}", file.path().filename().string());
			} catch (const std::exception& e) {
				logger::error("Failed to add Rule: {}. Error: {}", file.path().filename().string(), e.what());
			}
		}
		return preloadedRules;
	}

}  // namespace DBD
