#include "Distribution.h"

#include "shared/KrisV/Util/FormLookup.h"
#include "shared/KrisV/Random.h"

namespace DBD
{
	const Data::ReferenceConfiguration& Distribution::SelectProfiles(RE::Actor* a_target) const
	{
		std::unique_lock lock{ _mutex };
		auto it = _actorConfigs.find(Data::ReferenceConfiguration{ a_target->formID });
		if (it != _actorConfigs.end()) {
			return *it;
		}
		const auto result = _actorConfigs.emplace(a_target, _database);
		return *result.first;
	}

	std::unordered_set<Data::ReferenceConfiguration>::node_type Distribution::SelectProfilesMutable(RE::Actor* a_target) const
	{
		auto node = _actorConfigs.extract(Data::ReferenceConfiguration{ a_target->formID });
		if (!node.empty()) {
			return node;
		}

		auto [it, _] = _actorConfigs.emplace(a_target, _database);
		return _actorConfigs.extract(it);
	}

	void Distribution::ApplyProfiles(RE::Actor* a_target, RE::NiAVObject* a_part) const
	{
		const auto& config = SelectProfiles(a_target);
		config.ApplyProfiles(a_part);
	}

	std::array<std::string, 3> Distribution::GetProfileNames(RE::Actor* a_target) const
	{
		const auto& config = SelectProfiles(a_target);
		return config.GetProfileNames();
	}

	bool Distribution::ApplyTextureProfile(RE::Actor* a_target, std::string_view a_profileName) const
	{
		const auto texturePack = _database.FindTexturePackByName(a_profileName);
		if (!texturePack) {
			return false;
		}
		std::unique_lock lock{ _mutex };
		auto node = SelectProfilesMutable(a_target);
		const auto ret = node.value().AddTexturePack(texturePack);
		_actorConfigs.insert(std::move(node));
		return ret;
	}

	bool Distribution::ApplySliderProfile(RE::Actor* a_target, std::string_view a_profileName) const
	{
		const auto bodyslidePreset = _database.FindBodyslidePresetByName(a_profileName);
		if (bodyslidePreset && bodyslidePreset->IsApplicable(a_target)) {
			std::unique_lock lock{ _mutex };
			auto node = SelectProfilesMutable(a_target);
			const auto ret = node.value().SetBodyslidePreset(bodyslidePreset);
			_actorConfigs.insert(std::move(node));
			return ret;
		}
		return false;
	}

	bool Distribution::ApplyRaceMenuProfile(RE::Actor* a_target, std::string_view a_profileName) const
	{
		const auto raceMenuPreset = _database.FindRaceMenuPresetByName(a_profileName);
		if (raceMenuPreset) {
			std::unique_lock lock{ _mutex };
			auto node = SelectProfilesMutable(a_target);
			const auto ret = node.value().SetRaceMenuPreset(raceMenuPreset);
			_actorConfigs.insert(std::move(node));
			return ret;
		}
		return false;
	}

	void Distribution::Save(SKSE::SerializationInterface* a_intfc, uint32_t)
	{
		std::size_t numRegs = _actorConfigs.size();
		if (!a_intfc->WriteRecordData(numRegs)) {
			logger::error("Failed to save number of regs ({})", numRegs);
			return;
		}
		for (auto&& data : _actorConfigs) {
			data.Save(a_intfc, 0);
		}
	}

	void Distribution::Load(SKSE::SerializationInterface* a_intfc, uint32_t)
	{
		_actorConfigs.clear();
		size_t numRegs;
		a_intfc->ReadRecordData(numRegs);
		_actorConfigs.reserve(numRegs);

		for (size_t i = 0; i < numRegs; i++) {
			if (auto config = Data::ReferenceConfiguration::Load(a_intfc, _database, 0)) {
				_actorConfigs.emplace(std::move(*config));
			}
		}
		logger::info("Loaded {} cache entries", _actorConfigs.size());
	}

	void Distribution::Revert(SKSE::SerializationInterface*)
	{
		_actorConfigs.clear();
	}

}  // namespace DBD
