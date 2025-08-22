#include "SliderProfile.h"

#include <rapidxml/rapidxml.hpp>

namespace DBD
{
	SliderProfile::SliderProfile(const std::filesystem::path& a_xmlfilePath, bool a_isMale) :
		ProfileBase(a_xmlfilePath.filename().string(), ".xml"), isMale(a_isMale), transformInterface([]() {
			const auto intfc = SKEE::GetInterfaceMap();
			return intfc ? SKEE::GetBodyMorphInterface(intfc) : nullptr;
		}())
	{
		if (a_xmlfilePath.empty() || a_xmlfilePath.extension() != ".xml") {
			throw std::invalid_argument("XML file path cannot be empty");
		} else if (!std::filesystem::exists(a_xmlfilePath)) {
			throw std::invalid_argument(std::format("XML file does not exist: {}", a_xmlfilePath.string()));
		} else if (!transformInterface) {
			throw std::runtime_error("Failed to get transform interface");
		}

		std::ifstream file(a_xmlfilePath);
		if (!file) {
			throw std::runtime_error("Failed to open XML file");
		}
		std::stringstream buffer;
		buffer << file.rdbuf();
		std::string content = buffer.str();

		rapidxml::xml_document<> doc;
		doc.parse<0>(&content[0]);
		auto* root = doc.first_node("SliderPresets");
		if (!root) {
			const auto msg = std::format("Invalid <SliderPresets> element in {}", name.data());
			throw std::runtime_error(msg);
		}
		for (auto* preset = root->first_node("Preset"); preset; preset = preset->next_sibling("Preset")) {
			for (auto* slider = preset->first_node("SetSlider"); slider; slider = slider->next_sibling("SetSlider")) {
				auto* nameAttr = slider->first_attribute("name");
				auto* sizeAttr = slider->first_attribute("size");
				auto* valueAttr = slider->first_attribute("value");
				if (nameAttr && sizeAttr && valueAttr) {
					std::string sliderName = nameAttr->value();
					std::string size = sizeAttr->value();
					int value = std::stoi(valueAttr->value());
					auto& pair = sliders[sliderName];
					(size == "small" ? pair.first : pair.second) = value;
				} else {
					const auto msg = std::format("Invalid slider attributes in {}", name.data());
					throw std::runtime_error(msg);
				}
			}
		}
	}

	void SliderProfile::Apply(RE::Actor* a_target) const
	{
		const auto base = a_target->GetActorBase();
		const auto weight = base ? base->weight / 100.0f : 0.5f;
		for (const auto& [sliderName, sliderValues] : sliders) {
			const auto& [minVal, maxVal] = sliderValues;
			const float val{ ((maxVal - minVal) * weight) + minVal };
			transformInterface->SetMorph(a_target, sliderName.data(), MORPH_KEY, val / 100.0f);
		}
		transformInterface->ApplyBodyMorphs(a_target, false);
		transformInterface->UpdateModelWeight(a_target, true);
	}

	bool SliderProfile::IsApplicable(RE::Actor* a_target) const
	{
		const auto race = a_target->GetRace();
		if (!race || !race->HasKeywordString("ActorTypeNPC")) {
			return false;
		}
		const auto base = a_target->GetActorBase();
		return base && base->GetSex() == (isMale ? RE::SEX::kMale : RE::SEX::kFemale);
	}

}  // namespace DBD
