#pragma once

#include <shared/KrisV/Conditions/Conditional.h>

#include "BodyslidePreset.h"
#include "RaceMenuPreset.h"
#include "TexturePack.h"

namespace DBD::Data
{
	class Rule
	{
	public:
		Rule(std::string a_name, Conditions::Conditional a_conditional,
			std::vector<std::shared_ptr<TexturePack>> a_texturePacks,
			std::vector<std::shared_ptr<BodyslidePreset>> a_bodyslidePresets,
			std::vector<std::shared_ptr<RaceMenuPreset>> a_raceMenuPresets);
		~Rule() = default;

		_NODISCARD std::shared_ptr<TexturePack> SelectTexturePack(RE::Actor* a_actor) const;
		_NODISCARD std::shared_ptr<BodyslidePreset> SelectBodyslidePreset(RE::Actor* a_actor) const;
		_NODISCARD std::shared_ptr<RaceMenuPreset> SelectRaceMenuPreset(RE::Actor* a_actor) const;

		_NODISCARD std::string_view GetName() const { return _name; }
		_NODISCARD uint16_t GetPriority() const { return _priority; }

		_NODISCARD bool ConditionsMet(RE::TESObjectREFR* a_subject) const;

	private:
		static uint16_t MapFunctionToPriority(RE::FUNCTION_DATA::FunctionID a_functionID);

		std::string _name;
		uint16_t _priority;
		Conditions::Conditional _conditional;
		std::vector<std::shared_ptr<TexturePack>> _texturePacks;
		std::vector<std::shared_ptr<BodyslidePreset>> _bodyslidePresets;
		std::vector<std::shared_ptr<RaceMenuPreset>> _raceMenuPresets;
	};

	struct PreloadedRule
	{
		struct Candidate
		{
			std::string _name;
			bool _exclusive;
		};

	public:
		PreloadedRule(std::string_view a_jsonFilePath);
		~PreloadedRule() = default;

	public:
		std::string _name{};
		Conditions::Conditional _conditional{};

		bool _wildcardTexturePack{ false };
		bool _wildcardBodyslidePreset{ false };
		bool _wildcardRaceMenuPreset{ false };

		std::vector<Candidate> _textureCandidates{};
		std::vector<Candidate> _bodySlideCandidates{};
		std::vector<Candidate> _raceMenuCandidate{};
	};
}  // namespace DBD::Data
