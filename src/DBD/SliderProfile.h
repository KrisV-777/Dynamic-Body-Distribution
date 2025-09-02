#pragma once

#include <rapidxml/rapidxml.hpp>

#include "API/SKEE.h"
#include "ProfileBase.h"

namespace DBD
{
	class SliderConfig
	{
		constexpr static const char* CONFIG_PATH{ "Data\\SKSE\\DBD\\SliderConfig" };

	public:
		SliderConfig();
		~SliderConfig() = default;

		bool IsExcluded(const rapidxml::xml_node<char>* a_node) const;
		RE::SEX GetSex(const rapidxml::xml_node<char>* a_node) const;

	private:
		std::vector<std::string> excludedPresets{};
		std::map<std::string, RE::SEX, StringComparator> sexMapping{};
	};

	class SliderProfile : public ProfileBase
	{
		using SliderRange = std::pair<int32_t, int32_t>;

		constexpr static const char* MORPH_KEY = "DBD_Morph";

	public:
		static std::vector<std::shared_ptr<SliderProfile>> LoadProfiles(const std::filesystem::path& a_xmlfilePath, RE::SEX a_sex, SKEE::IBodyMorphInterface* a_interface, SliderConfig* a_config);
		SliderProfile(const rapidxml::xml_node<char>* a_node, RE::SEX a_sex, bool isPrivate, SKEE::IBodyMorphInterface* a_interface);
		SliderProfile(const SliderProfile& a_other, RE::BSFixedString a_name) :
			ProfileBase(a_name, ".xml"), sex(a_other.sex), transformInterface(a_other.transformInterface), sliders(a_other.sliders) {}
		~SliderProfile() = default;

		void Apply(RE::Actor* a_target) const override;
		bool IsApplicable(RE::Actor* a_target) const override;

		static void DeleteMorphs(RE::Actor* a_target, SKEE::IBodyMorphInterface* a_interface);

	private:
		RE::SEX sex;

		std::map<std::string, SliderRange, StringComparator> sliders;
		SKEE::IBodyMorphInterface* transformInterface;
	};

}  // namespace DBD
