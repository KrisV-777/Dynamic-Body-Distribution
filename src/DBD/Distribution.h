#pragma once

#include "SliderProfile.h"
#include "TextureProfile.h"
#include "Util/Singleton.h"

namespace DBD
{
	class Distribution :
		public Singleton<Distribution>
	// public RE::BSTEventSink<RE::TESCellAttachDetachEvent>
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
		void Initialize();

		int32_t ApplyProfiles(RE::Actor* a_target);
		bool ApplyTextureProfile(RE::Actor* a_target, const RE::BSFixedString& a_textureId) const;
		bool ApplySliderProfile(RE::Actor* a_target, const RE::BSFixedString& a_sliderId) const;

		// protected:
		// RE::BSEventNotifyControl ProcessEvent(const RE::TESCellAttachDetachEvent* a_event, RE::BSTEventSource<RE::TESCellAttachDetachEvent>*) override;

	private:
		void LoadTextureProfiles();
		void LoadSliderProfiles();
		void LoadConditions();

	private:
		std::vector<DistributionConfig> configurations;
		std::map<RE::BSFixedString, TextureProfile, FixedStringComparator> skins;
		std::map<RE::BSFixedString, SliderProfile, FixedStringComparator> sliders;

		std::map<RE::FormID, std::pair<RE::BSFixedString, RE::BSFixedString>> cache;  // <textureId, sliderId>
	};

}  // namespace DBD
