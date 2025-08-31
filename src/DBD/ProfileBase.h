#pragma once

namespace DBD
{
	enum class ConditionPriority
	{
		Reference,
		ActorBase,
		Group,
		Race,
		Wildcard,
		None
	};

	struct ConditionData
	{
		bool referenceWildcard{ false };
		std::vector<RE::FormID> references{};
		std::vector<RE::FormID> actorBases{};
		std::vector<RE::BGSKeyword*> keywords{};
		std::vector<RE::TESFaction*> factions{};
		std::vector<RE::TESRace*> races{};
	};

	class ProfileBase
	{
	public:
		ProfileBase(RE::BSFixedString a_name, std::string_view extension = ""sv) :
			name(std::move(a_name)), isPrivate(!name.empty() && name[0] == '.'), conditionData()
		{
			std::string nameTmp{ name.c_str() };
			name = nameTmp.substr(isPrivate, nameTmp.length() - extension.length() - isPrivate);
			if (name.empty()) {
				throw std::runtime_error("Profile name is empty");
			}
		}
		virtual ~ProfileBase() = default;

		RE::BSFixedString GetName() const { return name; }
		bool IsPrivate() const { return isPrivate; }

		void AppendConditions(const ConditionData& condition);
		ConditionPriority ValidatePriority(RE::Actor* a_target) const;

		virtual void Apply(RE::Actor* a_target) const = 0;
		virtual bool IsApplicable(RE::Actor* a_target) const = 0;

	protected:
		RE::BSFixedString name;
		bool isPrivate;

		ConditionData conditionData;
	};

	inline void ProfileBase::AppendConditions(const ConditionData& condition)
	{
		conditionData.referenceWildcard |= condition.referenceWildcard;
		const auto append_unique = [](auto& dest, const auto& src) {
			for (const auto& item : src) {
				if (!std::ranges::contains(dest, item)) {
					dest.push_back(item);
				}
			}
		};
		append_unique(conditionData.references, condition.references);
		append_unique(conditionData.actorBases, condition.actorBases);
		append_unique(conditionData.keywords, condition.keywords);
		append_unique(conditionData.factions, condition.factions);
		append_unique(conditionData.races, condition.races);
	}

	inline ConditionPriority ProfileBase::ValidatePriority(RE::Actor* a_target) const
	{
		const auto formId = a_target->GetFormID();
		if (std::ranges::contains(conditionData.references, formId)) {
			return ConditionPriority::Reference;
		}
		const auto npc = a_target->GetActorBase();
		const auto npcId = npc ? npc->GetFormID() : RE::FormID{ 0 };
		if (std::ranges::contains(conditionData.actorBases, npcId)) {
			return ConditionPriority::ActorBase;
		} else if (std::ranges::any_of(conditionData.factions, [&](RE::TESFaction* faction) { return a_target->IsInFaction(faction); }) ||
				   a_target->HasKeywordInArray(conditionData.keywords, false)) {
			return ConditionPriority::Group;
		} else if (std::ranges::any_of(conditionData.races, [&](RE::TESRace* race) { return race == npc->GetRace(); })) {
			return ConditionPriority::Race;
		}
		return conditionData.referenceWildcard ? ConditionPriority::Wildcard : ConditionPriority::None;
	}

}  // namespace DBD
