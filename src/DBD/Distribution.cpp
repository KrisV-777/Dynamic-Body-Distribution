#include "Distribution.h"

#include "shared/KrisV/Util/FormLookup.h"
#include "shared/KrisV/Random.h"

namespace DBD
{
	const Distribution::ActorConfig& Distribution::SelectProfiles(RE::Actor* a_target) const
	{
		auto it = _actorConfigs.find(a_target->formID);
		if (it != _actorConfigs.end()) {
			auto& config = it->second;
			if (!config._updatedThisSession) {
				config._updatedThisSession = true;
				if (!config.texturePack)
					config.texturePack = _database.SelectTexturePack(a_target);
				if (!config.bodyslidePreset)
					config.bodyslidePreset = _database.SelectBodyslidePreset(a_target);
				if (!config.raceMenuPreset) {
					config.raceMenuPreset = _database.SelectRaceMenuPreset(a_target);
				}
			}
			return config;
		}

		ActorConfig retVal{
			.texturePack = _database.SelectTexturePack(a_target),
			.bodyslidePreset = _database.SelectBodyslidePreset(a_target),
			.raceMenuPreset = _database.SelectRaceMenuPreset(a_target)
		};

		const auto result = _actorConfigs.emplace(a_target->formID, std::move(retVal));
		return result.first->second;
	}

	std::array<std::string_view, 2> Distribution::GetProfileNames(RE::Actor* a_target) const
	{
		const auto& config = SelectProfiles(a_target);
		return {
			config.texturePack ? config.texturePack->GetName() : std::string_view{},
			config.bodyslidePreset ? config.bodyslidePreset->GetName() : std::string_view{}
		};
	}

	void Distribution::ApplyProfiles(RE::Actor* a_target, RE::NiAVObject* a_part) const
	{
		const auto& config = SelectProfiles(a_target);
		if (config.texturePack) {
			if (!a_part) {
				a_part = a_target->Get3D();
				if (!a_part) {
					logger::warn("Actor {} has no 3D model, skipping texture pack application", a_target->formID);
					return;
				}
			}
			config.texturePack->Apply(a_part);
		}
		if (config.bodyslidePreset) {
			config.bodyslidePreset->Apply(a_target);
		}
		if (config.raceMenuPreset) {
			config.raceMenuPreset->Apply(a_target);
		}
	}

	bool Distribution::ApplyTextureProfile(RE::Actor* a_target, std::string_view a_profileName) const
	{
		const auto texturePack = _database.FindTexturePackByName(a_profileName);
		if (texturePack) {
			_actorConfigs[a_target->formID].texturePack = texturePack;
			texturePack->Apply(a_target);
			return true;
		}
		return false;
	}

	bool Distribution::ApplySliderProfile(RE::Actor* a_target, std::string_view a_profileName) const
	{
		const auto bodyslidePreset = _database.FindBodyslidePresetByName(a_profileName);
		if (bodyslidePreset && bodyslidePreset->IsApplicable(a_target)) {
			_actorConfigs[a_target->formID].bodyslidePreset = bodyslidePreset;
			bodyslidePreset->Apply(a_target);
			return true;
		}
		return false;
	}

	bool Distribution::ApplyRaceMenuProfile(RE::Actor* a_target, std::string_view a_profileName) const
	{
		const auto raceMenuPreset = _database.FindRaceMenuPresetByName(a_profileName);
		if (raceMenuPreset) {
			_actorConfigs[a_target->formID].raceMenuPreset = raceMenuPreset;
			raceMenuPreset->Apply(a_target);
			return true;
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
		for (auto&& [formID, data] : _actorConfigs) {
			if (!a_intfc->WriteRecordData(formID)) {
				logger::error("Failed to save reg ({:X})", formID);
				continue;
			}
			::stl::write_string(a_intfc, data.texturePack ? data.texturePack->GetName() : "");
			::stl::write_string(a_intfc, data.bodyslidePreset ? data.bodyslidePreset->GetName() : "");
			::stl::write_string(a_intfc, data.raceMenuPreset ? data.raceMenuPreset->GetName() : "");
		}
	}

	void Distribution::Load(SKSE::SerializationInterface* a_intfc, uint32_t)
	{
		_actorConfigs.clear();
		size_t numRegs;
		a_intfc->ReadRecordData(numRegs);
		_actorConfigs.reserve(numRegs);

		RE::FormID formID;
		for (size_t i = 0; i < numRegs; i++) {
			a_intfc->ReadRecordData(formID);
			if (!a_intfc->ResolveFormID(formID, formID)) {
				logger::warn("Error reading formID: {:X}", formID);
				continue;
			}
			std::array<std::string, 3> profileNames;
			if (!std::ranges::all_of(profileNames, [&](auto& a_name) { return ::stl::read_string(a_intfc, a_name); })) {
				logger::error("Failed to read profiles for form {:X}", formID);
				return;
			}
			const auto& [textureProfile, bodyslideProfile, raceMenuProfile] = profileNames;
			ActorConfig config{
				.texturePack = textureProfile.empty() ? nullptr : _database.FindTexturePackByName(textureProfile),
				.bodyslidePreset = bodyslideProfile.empty() ? nullptr : _database.FindBodyslidePresetByName(bodyslideProfile),
				.raceMenuPreset = raceMenuProfile.empty() ? nullptr : _database.FindRaceMenuPresetByName(raceMenuProfile)
			};
			_actorConfigs.emplace(formID, std::move(config));
		}
		logger::info("Loaded {} cache entries", _actorConfigs.size());
	}

	void Distribution::Revert(SKSE::SerializationInterface*)
	{
		_actorConfigs.clear();
	}

}  // namespace DBD
