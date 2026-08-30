#pragma once

#include <API/SKEE.h>
#include <rapidxml/rapidxml.hpp>

namespace DBD::Data
{
    class SliderConfig
	{
		constexpr static const char* CONFIG_PATH{ "Data\\SKSE\\DBD\\SliderConfig" };

	public:
		SliderConfig();
		~SliderConfig() = default;

		/// @brief Checks if the preset represented by the XML node is excluded.
		/// @param a_node The XML node representing the slider preset.
		/// @return True if the preset is excluded, false otherwise.
		bool IsExcluded(const rapidxml::xml_node<char>* a_node) const;

		/// @brief Attempts to infer the sex associated with the given XML node.
		/// @param a_node The XML node representing the slider preset.
		/// @return The sex (RE::SEX) associated with the preset, or RE::SEX::kNone if not specified
		RE::SEX GetSex(const rapidxml::xml_node<char>* a_node) const;

	private:
		std::vector<std::string> excludedPresets{};
		std::map<std::string, RE::SEX> sexMapping{};
	};

	class BodyslidePreset
	{
		constexpr static const char* MORPH_KEY = "DBD_Morph";
		using SliderRange = std::pair<int32_t, int32_t>;

	public:
		BodyslidePreset(const rapidxml::xml_node<char>* a_node, RE::SEX a_sex, SKEE::IBodyMorphInterface* a_interface);
		~BodyslidePreset() = default;

		/// @brief Applies the slider preset to the specified actor.
		/// @param a_target The actor to apply the preset to.
		void ApplyPreset(RE::Actor* a_target) const;

		/// @return The name of the preset.
		std::string_view GetName() const noexcept { return _name; }

		/// @brief Checks if the slider preset is applicable to the specified actor.
		/// @param a_target The actor to check applicability for.
		/// @return True if the preset is applicable to the actor, false otherwise.
		bool IsApplicable(RE::Actor* a_target) const;

		/// @brief Deletes the morphs associated with this preset from the specified actor.
		/// @param a_target The actor from which to delete the morphs.
		/// @param a_interface The body morph interface used to manipulate the actor's morphs.
		static void DeleteMorphs(RE::Actor* a_target, SKEE::IBodyMorphInterface* a_interface);

	private:
		RE::SEX _sex;
        std::string _name;
		std::map<std::string, SliderRange, StringComparator> _sliders;
		SKEE::IBodyMorphInterface* _transformInterface;
	};

	std::vector<std::shared_ptr<BodyslidePreset>> LoadBodyslidePresets(
        const std::filesystem::path& a_xmlfilePath, SKEE::IBodyMorphInterface* a_interface, SliderConfig* a_config);

} // namespace DBD::Data
