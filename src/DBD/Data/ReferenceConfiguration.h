#pragma once

#include <functional>

#include "BodyslidePreset.h"
#include "DBD/Database.h"
#include "RaceMenuPreset.h"
#include "TexturePreset.h"

namespace DBD::Data
{
	struct ReferenceConfiguration
	{
		ReferenceConfiguration(RE::Actor* a_target, const Database& a_database);
		explicit ReferenceConfiguration(RE::FormID a_formID) : formID(a_formID) {}
		ReferenceConfiguration(ReferenceConfiguration&& a_rhs) noexcept;
		ReferenceConfiguration& operator=(ReferenceConfiguration&& a_rhs) noexcept;
		ReferenceConfiguration(const ReferenceConfiguration&) = delete;
		ReferenceConfiguration& operator=(const ReferenceConfiguration&) = delete;
		~ReferenceConfiguration() = default;

		bool AddTexturePack(std::shared_ptr<TexturePack> a_texturePack, bool a_update = true);
		bool SetBodyslidePreset(std::shared_ptr<BodyslidePreset> a_bodyslidePreset, bool a_update = true);
		bool SetRaceMenuPreset(std::shared_ptr<RaceMenuPreset> a_raceMenuPreset, bool a_update = true);
		std::array<std::string, 3> GetProfileNames() const;

		void ApplyProfiles(RE::NiAVObject* a_part = nullptr) const;
		[[nodiscard]] RE::FormID GetFormID() const noexcept { return formID; }
		[[nodiscard]] bool operator==(const ReferenceConfiguration& a_rhs) const noexcept { return formID == a_rhs.formID; }
		[[nodiscard]] bool operator<(const ReferenceConfiguration& a_rhs) const noexcept { return formID < a_rhs.formID; }

		void Save(SKSE::SerializationInterface* a_intfc, uint32_t) const;
		static std::optional<ReferenceConfiguration> Load(SKSE::SerializationInterface* a_intfc, const Database& a_database, uint32_t);

	private:
		RE::FormID formID{ 0 };
		RE::ActorHandle handle{};

		std::shared_ptr<Data::BodyslidePreset> bodyslidePreset{ nullptr };
		std::shared_ptr<Data::RaceMenuPreset> raceMenuPreset{ nullptr };
		std::shared_ptr<Data::TexturePreset> texturePreset{ nullptr };

		mutable std::mutex _mutex{};
	};
}  // namespace DBD::Data

namespace std
{
	template <>
	struct hash<DBD::Data::ReferenceConfiguration>
	{
		std::size_t operator()(const DBD::Data::ReferenceConfiguration& a_config) const noexcept
		{
			return std::hash<RE::FormID>{}(a_config.GetFormID());
		}
	};
}
