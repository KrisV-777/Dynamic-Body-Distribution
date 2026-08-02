#pragma once

#include <yaml-cpp/yaml.h>

#include "API/SKEE.h"
#include "DBD/SliderProfile.h"
#include "DBD/TextureProfile.h"
#include "ProfileBase.h"
#include "shared/KrisV/Conditions/Conditional.h"
#include "shared/KrisV/Singleton.h"

namespace DBD
{
	class Distribution :
		public Singleton<Distribution>
	{
		static constexpr const char* TEXTURE_ROOT_PATH{ "Data\\Textures\\DBD" };
		static constexpr const char* SLIDER_ROOT_PATH{ "Data\\SKSE\\DBD\\Sliders" };
		static constexpr const char* SLIDER_DEFAULT_PATH{ "Data\\CalienteTools\\BodySlide\\SliderPresets" };
		static constexpr const char* CONFIGURATION_ROOT_PATH{ "Data\\SKSE\\DBD\\Configurations" };

		struct Configuration
		{
			enum MatchPriority
			{
				Reference,
				ActorBase,
				Group,
				Race,
				Wildcard,
				None
			};

			Configuration(const YAML::Node& a_node, const Distribution* a_distribution);
			~Configuration() = default;

			std::shared_ptr<const ProfileBase> SelectProfile(RE::Actor* a_target, ProfileType a_type) const;
			MatchPriority GetMatchPriority(RE::Actor* a_target) const;

			ProfileArray<std::vector<std::shared_ptr<const ProfileBase>>> profiles;
			bool isWildcardConfig{ false };
			std::vector<RE::FormID> references{};
			std::vector<RE::FormID> actorBases{};
			std::vector<RE::BGSKeyword*> keywords{};
			std::vector<RE::TESFaction*> factions{};
			std::vector<RE::TESRace*> races{};
			Conditions::Conditional conditions{};
		};

	public:
		void Initialize();

		ProfileArray<std::shared_ptr<const ProfileBase>> SelectProfiles(RE::Actor* a_target);

		bool ApplyProfile(RE::Actor* a_target, const std::string& a_profileId, ProfileType a_type);
		bool ApplyTextureProfile(RE::Actor* a_target, const std::string& a_textureId);
		bool ApplySliderProfile(RE::Actor* a_target, const std::string& a_sliderId);

		std::shared_ptr<const ProfileBase> GetProfile(std::string a_profileId, ProfileType a_type) const;
		void ForEachProfile(const std::function<void(std::shared_ptr<const ProfileBase>)>& a_callback, ProfileType a_type) const;
		void ForEachTextureProfile(const std::function<void(const TextureProfile*)>& a_callback) const;
		void ForEachSliderProfile(const std::function<void(const SliderProfile*)>& a_callback) const;

		ProfileArray<std::shared_ptr<const ProfileBase>> GetProfiles(RE::Actor* a_target) const;
		void ClearProfiles(RE::Actor* a_target, bool a_exclude);

	public:
		void Save(SKSE::SerializationInterface* a_intfc, uint32_t a_version);
		void Load(SKSE::SerializationInterface* a_intfc, uint32_t a_version);
		void Revert(SKSE::SerializationInterface* a_intfc);

	private:
		void LoadTextureProfiles();
		void LoadSliderProfiles();
		void LoadConditions();

	private:
		std::vector<Configuration> configurations;
		ProfileArray<std::map<std::string, std::shared_ptr<ProfileBase>, StringComparator>> profileMap;
		std::map<RE::FormID, ProfileArray<std::shared_ptr<const ProfileBase>>> cache;
		std::set<RE::FormID> excludedForms;
		RE::SEX playerSexPreChargen;

		SKEE::IActorUpdateManager* actorUpdateManager;
		SKEE::IBodyMorphInterface* morphInterface;
	};

	inline void ApplyProfile(RE::Actor* a_actor, RE::NiAVObject* a_part = nullptr)
	{
		if (!a_part) {
			a_part = a_actor->Get3D();
			if (!a_part) {
				logger::warn("Actor {} has no 3D model, skipping profile application", a_actor->formID);
				return;
			}
		}

		logger::info("Trying to apply profile to Actor: {}", a_actor->formID);
		const auto dist = DBD::Distribution::GetSingleton();
		const auto profiles = dist->SelectProfiles(a_actor);


		for (auto&& profile : profiles) {
			if (!profile) {
				continue;
			}
			if (profile->IsTextureProfile()) {
				const auto texSet = static_cast<const TextureProfile*>(profile.get());
				texSet->OverrideObjectTextures(a_part);
			} else {
				profile->Apply(a_actor);
			}
		}
	}

}  // namespace DBD
