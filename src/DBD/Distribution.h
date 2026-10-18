#pragma once

#include <shared/KrisV/Conditions/Conditional.h>
#include <shared/KrisV/Singleton.h>

#include <unordered_set>

#include "Data/BodyslidePreset.h"
#include "Data/RaceMenuPreset.h"
#include "Data/ReferenceConfiguration.h"
#include "Data/TexturePack.h"
#include "Database.h"

namespace DBD
{
	class Distribution :
		public Singleton<Distribution>
	{
	public:
		void Initialize() {};

		const Database& GetDatabase() const noexcept { return _database; }

		const Data::ReferenceConfiguration& SelectProfiles(RE::Actor* a_target) const;
		std::array<std::string, 3> GetProfileNames(RE::Actor* a_target) const;
		void ApplyProfiles(RE::Actor* a_target, RE::NiAVObject* a_part = nullptr) const;

		bool ApplyTextureProfile(RE::Actor* a_target, std::string_view a_profileName) const;
		bool ApplySliderProfile(RE::Actor* a_target, std::string_view a_profileName) const;
		bool ApplyRaceMenuProfile(RE::Actor* a_target, std::string_view a_profileName) const;

		void ClearCache(RE::Actor* a_target) { _actorConfigs.erase(Data::ReferenceConfiguration{ a_target->formID }); }

	public:
		void Save(SKSE::SerializationInterface* a_intfc, uint32_t a_version);
		void Load(SKSE::SerializationInterface* a_intfc, uint32_t a_version);
		void Revert(SKSE::SerializationInterface* a_intfc);

	private:
		std::unordered_set<Data::ReferenceConfiguration>::node_type SelectProfilesMutable(RE::Actor* a_target) const;

		Database _database;
		mutable std::mutex _mutex;
		mutable std::unordered_set<Data::ReferenceConfiguration> _actorConfigs;
	};
}  // namespace DBD
