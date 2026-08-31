#pragma once

#include <shared/KrisV/Conditions/Conditional.h>
#include <shared/KrisV/Singleton.h>

#include "Data/BodyslidePreset.h"
#include "Data/RaceMenuPreset.h"
#include "Data/TexturePack.h"
#include "Database.h"

namespace DBD
{
	class Distribution :
		public Singleton<Distribution>
	{
		struct ActorConfig
		{
			std::shared_ptr<Data::TexturePack> texturePack;
			std::shared_ptr<Data::BodyslidePreset> bodyslidePreset;
			std::shared_ptr<Data::RaceMenuPreset> raceMenuPreset;

			bool _updatedThisSession{ false };
		};

	public:
		void Initialize() {};

		const Database& GetDatabase() const noexcept { return _database; }

		const ActorConfig& SelectProfiles(RE::Actor* a_target) const;
		std::array<std::string_view, 2> GetProfileNames(RE::Actor* a_target) const;
		void ApplyProfiles(RE::Actor* a_target, RE::NiAVObject* a_part = nullptr) const;

		bool ApplyTextureProfile(RE::Actor* a_target, std::string_view a_profileName) const;
		bool ApplySliderProfile(RE::Actor* a_target, std::string_view a_profileName) const;
		bool ApplyRaceMenuProfile(RE::Actor* a_target, std::string_view a_profileName) const;

		void ClearCache(RE::Actor* a_target) { _actorConfigs.erase(a_target->formID); }

	public:
		void Save(SKSE::SerializationInterface* a_intfc, uint32_t a_version);
		void Load(SKSE::SerializationInterface* a_intfc, uint32_t a_version);
		void Revert(SKSE::SerializationInterface* a_intfc);

	private:
		Database _database;
		mutable std::unordered_map<RE::FormID, ActorConfig> _actorConfigs;
	};
}  // namespace DBD
