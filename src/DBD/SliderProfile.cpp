#include "SliderProfile.h"

namespace DBD
{
	std::vector<std::shared_ptr<SliderProfile>> SliderProfile::LoadProfiles(const std::filesystem::path& a_xmlfilePath, bool a_isMale, SKEE::IBodyMorphInterface* a_interface)
	{
		std::vector<std::shared_ptr<SliderProfile>> profiles{};
		if (a_xmlfilePath.empty() || a_xmlfilePath.extension() != ".xml") {
			throw std::invalid_argument("XML file path cannot be empty");
		} else if (!std::filesystem::exists(a_xmlfilePath)) {
			throw std::invalid_argument(std::format("XML file does not exist: {}", a_xmlfilePath.string()));
		} else if (!a_interface) {
			throw std::runtime_error("Missing transform interface");
		}
		const bool isPrivate = a_xmlfilePath.c_str()[0] == '.';
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
			const auto msg = std::format("Invalid <SliderPresets> element in {}", a_xmlfilePath.string());
			throw std::runtime_error(msg);
		}
		for (auto* preset = root->first_node("Preset"); preset; preset = preset->next_sibling("Preset")) {
			profiles.emplace_back(std::make_shared<SliderProfile>(preset, a_isMale, isPrivate, a_interface));
		}
		if (!profiles.empty()) {
			// Copy the first profile with the name of the .xml file into library, to allow referencing it by filename in configs
			const auto copyFileName = std::make_shared<SliderProfile>(*profiles.front(), a_xmlfilePath.filename().string());
			if (copyFileName->GetName() != profiles.front()->GetName()) {
				profiles.push_back(copyFileName);
			}
		}
		return profiles;
	}

	SliderProfile::SliderProfile(const rapidxml::xml_node<char>* a_node, bool a_isMale, bool a_isPrivate, SKEE::IBodyMorphInterface* a_interface) :
		ProfileBase([&]() -> const char* {
			if (auto* attr = a_node->first_attribute("name"))
				return attr->value();
			throw std::runtime_error("Missing 'name' attribute in SliderProfile node");
		}()),
		isMale(a_isMale), transformInterface(a_interface)
	{
		this->isPrivate = a_isPrivate;
		for (auto* slider = a_node ? a_node->first_node("SetSlider") : nullptr; slider; slider = slider->next_sibling("SetSlider")) {
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
				throw std::runtime_error(std::format("Invalid slider attributes in {}", name.data()));
			}
		}
	}

	void SliderProfile::Apply(RE::Actor* a_target) const
	{
		logger::info("Applying slider profile {} to {}", name.data(), a_target->formID);
		transformInterface->ClearBodyMorphKeys(a_target, MORPH_KEY);
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

	void SliderProfile::DeleteMorphs(RE::Actor* a_target, SKEE::IBodyMorphInterface* a_interface)
	{
		a_interface->ClearBodyMorphKeys(a_target, MORPH_KEY);
		a_interface->ApplyBodyMorphs(a_target, false);
		a_interface->UpdateModelWeight(a_target, true);
	}

}  // namespace DBD
