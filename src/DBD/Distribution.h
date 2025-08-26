#pragma once

#include "API/SKEE.h"
#include "SliderProfile.h"
#include "TextureProfile.h"
#include "Util/Singleton.h"

namespace DBD
{
	class Distribution :
		public Singleton<Distribution>,
		public SKEE::IAddonAttachmentInterface
	{
		static constexpr const char* TEXTURE_ROOT_PATH{ "Data\\Textures\\DBD" };
		static constexpr const char* SLIDER_ROOT_PATH{ "Data\\SKSE\\DBD\\Sliders" };
		static constexpr const char* CONFIGURATION_ROOT_PATH{ "Data\\SKSE\\DBD\\Configurations" };

		struct DistributionConfig
		{
			enum class MatchLevel
			{
				None,
				Wildcard,
				Explicit
			};

			enum class Wildcard
			{
				Condition = 1 << 0,
				Sliders = 1 << 1,
				Textures = 1 << 2
			};
			REX::EnumSet<Wildcard> wildcards;

			std::vector<RE::FormID> references;
			std::vector<RE::FormID> actorBases;
			std::vector<RE::FormID> races;
			std::vector<RE::BGSKeyword*> keywords;
			std::vector<RE::TESFaction*> factions;

			std::vector<ProfileBase*> sliders;
			std::vector<ProfileBase*> textures;

			MatchLevel GetApplicationLevel(RE::Actor* a_target) const;
		};

	public:
		enum ProfileIndex
		{
			TextureId,
			SliderId,

			Total_V1,
			Total = Total_V1
		};
		using ProfileArray = std::array<ProfileBase*, ProfileIndex::Total>;

	public:
		void Initialize();

		ProfileArray SelectProfiles(RE::Actor* a_target);

		bool ApplyTextureProfile(RE::Actor* a_target, const std::string& a_textureId);
		bool ApplySliderProfile(RE::Actor* a_target, const std::string& a_sliderId);

		void ForEachTextureProfile(const std::function<void(const TextureProfile*)>& a_callback) const;
		void ForEachSliderProfile(const std::function<void(const SliderProfile*)>& a_callback) const;

		const ProfileArray& GetProfiles(RE::Actor* a_target);
		void ClearProfiles(RE::Actor* a_target, bool a_exclude);

	public:
		void Save(SKSE::SerializationInterface* a_intfc, uint32_t a_version);
		void Load(SKSE::SerializationInterface* a_intfc, uint32_t a_version);
		void Revert(SKSE::SerializationInterface* a_intfc);

	private:
		void OnAttach(RE::TESObjectREFR* refr, RE::TESObjectARMO* armor, RE::TESObjectARMA* addon, RE::NiAVObject* object, bool isFirstPerson, RE::NiNode* skeleton, RE::NiNode* root) override;

		void LoadTextureProfiles();
		void LoadSliderProfiles();
		void LoadConditions();

	private:
		std::vector<DistributionConfig> configurations;
		std::map<std::string, TextureProfile, StringComparator> textures;
		std::map<std::string, SliderProfile, StringComparator> sliders;

		std::set<RE::FormID> excludedForms;
		std::map<RE::FormID, ProfileArray> cache;

		SKEE::IActorUpdateManager* actorUpdateManager;
		SKEE::IBodyMorphInterface* morphInterface;
	};

}  // namespace DBD
