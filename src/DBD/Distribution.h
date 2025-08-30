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

	public:
		enum ProfileIndex
		{
			Textures,
			Sliders,

			Total_V1,
			Total = Total_V1
		};
		template <class T>
		using ProfileArray = std::array<T, ProfileIndex::Total>;

	public:
		void Initialize();

		ProfileArray<const ProfileBase*> SelectProfiles(RE::Actor* a_target);

		bool ApplyProfile(RE::Actor* a_target, const std::string& a_profileId, ProfileIndex a_type);
		bool ApplyTextureProfile(RE::Actor* a_target, const std::string& a_textureId);
		bool ApplySliderProfile(RE::Actor* a_target, const std::string& a_sliderId);

		void ForEachProfile(const std::function<void(const ProfileBase*)>& a_callback, ProfileIndex a_type) const;
		void ForEachTextureProfile(const std::function<void(const TextureProfile*)>& a_callback) const;
		void ForEachSliderProfile(const std::function<void(const SliderProfile*)>& a_callback) const;

		ProfileArray<const ProfileBase*> GetProfiles(RE::Actor* a_target) const;
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
		ProfileArray<std::map<std::string, std::unique_ptr<ProfileBase>, StringComparator>> profileMap;
		std::map<RE::FormID, ProfileArray<const ProfileBase*>> cache;

		RE::SEX playerSexPreChargen;
		std::set<RE::FormID> excludedForms;

		SKEE::IActorUpdateManager* actorUpdateManager;
		SKEE::IBodyMorphInterface* morphInterface;
	};

}  // namespace DBD
