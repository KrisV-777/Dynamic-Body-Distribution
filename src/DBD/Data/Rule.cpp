#include "Rule.h"

#include <glaze/glaze.hpp>
#include <shared/KrisV/Random.h>

namespace DBD::Data
{
	namespace detail
	{
		struct Candidate
		{
			std::string Name;
			bool IsExclusive;
		};
		static_assert(glz::reflectable<Candidate>);

		struct RuleData
		{
			std::string Id;
			std::string Name;
			std::chrono::system_clock::time_point LastUpdated;

			std::vector<std::string> Conditions;

			std::vector<Candidate> TextureCandidates;
			std::vector<Candidate> BodySlideCandidates;
			std::string RaceMenuCandidate;
		};
		static_assert(glz::reflectable<RuleData>);
	}

	PreloadedRule::PreloadedRule(std::string_view a_jsonFilePath)
	{
		detail::RuleData data;
		if (auto ec = glz::read_file_json(data, a_jsonFilePath.data(), std::string{})) {
			throw std::runtime_error(
				std::format("Failed to load Rule from {} with Error Code {}: {}",
					a_jsonFilePath, std::to_underlying(ec.ec), ec.custom_error_message));
		}

		_name = data.Name;
		_conditional = Conditions::Conditional(
			data.Conditions, Conditions::RefMap(std::map<std::string, std::string>{}));

		for (const auto& candidate : data.TextureCandidates) {
			_wildcardTexturePack = _wildcardTexturePack || candidate.Name == "Any";
			_textureCandidates.push_back({ candidate.Name, candidate.IsExclusive });
		}
		for (const auto& candidate : data.BodySlideCandidates) {
			_wildcardBodyslidePreset = _wildcardBodyslidePreset || candidate.Name == "Any";
			_bodySlideCandidates.push_back({ candidate.Name, candidate.IsExclusive });
		}
		if (!data.RaceMenuCandidate.empty()) {
			_wildcardRaceMenuPreset = _wildcardRaceMenuPreset || data.RaceMenuCandidate == "Any";
			_raceMenuCandidate.push_back({ data.RaceMenuCandidate, true });
		}
	}

	Rule::Rule(std::string a_name, Conditions::Conditional a_conditional,
		std::vector<std::shared_ptr<TexturePack>> a_texturePacks,
		std::vector<std::shared_ptr<BodyslidePreset>> a_bodyslidePresets,
		std::vector<std::shared_ptr<RaceMenuPreset>> a_raceMenuPresets) :
		_name(std::move(a_name)),
		_priority(std::numeric_limits<uint16_t>::max() - 1),
		_conditional(std::move(a_conditional)),
		_texturePacks(std::move(a_texturePacks)),
		_bodyslidePresets(std::move(a_bodyslidePresets)),
		_raceMenuPresets(std::move(a_raceMenuPresets))
	{
		if (!_conditional) {
			return;
		}
		for (auto item = _conditional.GetUnderlying()->head; item; item = item->next) {
			const auto& data = item->data;
			const auto functionID = data.functionData.function.get();
			const auto priority = MapFunctionToPriority(functionID);
			_priority = (std::min)(_priority, priority);
		}
	}

	std::shared_ptr<TexturePack> Rule::SelectTexturePack(RE::Actor*) const
	{
		return _texturePacks.empty() ? nullptr : Random::draw(_texturePacks);
	}

	std::shared_ptr<BodyslidePreset> Rule::SelectBodyslidePreset(RE::Actor*) const
	{
		return _bodyslidePresets.empty() ? nullptr : Random::draw(_bodyslidePresets);
	}

	std::shared_ptr<RaceMenuPreset> Rule::SelectRaceMenuPreset(RE::Actor*) const
	{
		return _raceMenuPresets.empty() ? nullptr : Random::draw(_raceMenuPresets);
	}

	bool Rule::ConditionsMet(RE::TESObjectREFR* a_subject) const
	{
		return _conditional && _conditional.ConditionsMet(a_subject, a_subject);
	}

	uint16_t Rule::MapFunctionToPriority(RE::FUNCTION_DATA::FunctionID a_functionID)
	{
		using FunctionID = RE::FUNCTION_DATA::FunctionID;
		switch (a_functionID) {
		case FunctionID::kGetIsReference:
			return 100;
		case FunctionID::kGetIsID:
			return 200;
		case FunctionID::kGetIsRace:
			return 300;
		case FunctionID::kIsUndead:
			return 400;
		case FunctionID::kIsCommandedActor:
			return 500;
		case FunctionID::kGetFactionRank:
			return 600;
		case FunctionID::kGetInFaction:
			return 700;
		case FunctionID::kGetIsCrimeFaction:
			return 800;
		case FunctionID::kIsInList:
			return 900;
		case FunctionID::kHasPerk:
			return 1000;
		case FunctionID::kHasKeyword:
			return 1100;
		case FunctionID::kWornHasKeyword:
			return 1200;
		case FunctionID::kGetIsVoiceType:
			return 1300;
		case FunctionID::kGetIsClass:
			return 1400;
		case FunctionID::kGetTimeDead:
			return 1500;
		case FunctionID::kGetDead:
			return 1600;
		case FunctionID::kGetDisease:
			return 1700;
		case FunctionID::kGetIsGhost:
			return 1800;
		case FunctionID::kIsGuard:
			return 1900;
		case FunctionID::kIsUnique:
			return 2000;
		case FunctionID::kIsEssential:
			return 2100;
		case FunctionID::kGetGlobalValue:
			return 2200;
		case FunctionID::kGetIsEditorLocation:
			return 2300;
		default:
			// Leave a small upward buffer for condition-less and random rules
			return std::numeric_limits<uint16_t>::max() - 100;
		}

	}

}  // namespace DBD::Data
