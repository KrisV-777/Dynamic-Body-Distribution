#include "BodyslidePreset.h"

#include <shared/KrisV/Util/String.h>
#include <yaml-cpp/yaml.h>

namespace DBD::Data
{
	SliderConfig::SliderConfig()
	{
		if (!fs::exists(CONFIG_PATH)) {
			logger::error("Slider config path does not exist");
			return;
		}
		for (auto& file : fs::recursive_directory_iterator{ CONFIG_PATH }) {
			if (file.path().extension() != ".yml" && file.path().extension() != ".yaml")
				continue;
			try {
				const auto root = YAML::LoadFile(file.path().string());
				if (const auto assignments = root["Assignments"])
					for (auto&& node : root["Assignments"]) {
						const auto nameStr = Util::CastLower(node.first.as<std::string>());
						const auto sexStr = Util::CastLower(node.second.as<std::string>());
						sexMapping[nameStr] = sexStr.starts_with("f") ? RE::SEX::kFemale : RE::SEX::kMale;
					}
				if (const auto exclusions = root["Excluded"])
					for (auto&& node : exclusions) {
						const auto nameStr = Util::CastLower(node.as<std::string>());
						excludedPresets.push_back(nameStr);
					}
			} catch (const std::exception& e) {
				logger::error("Failed to load slider config '{}': {}", file.path().filename().string(), e.what());
			}
		}
	}

	bool SliderConfig::IsExcluded(const rapidxml::xml_node<char>* a_node) const
	{
		const auto nameAttr = a_node->first_attribute("name");
		const auto nameStr = nameAttr ? nameAttr->value() : "unknown";
		return std::ranges::contains(excludedPresets, Util::CastLower(nameStr));
	}

	RE::SEX SliderConfig::GetSex(const rapidxml::xml_node<char>* a_node) const
	{
		for (auto* group = a_node ? a_node->first_node("Group") : nullptr; group; group = group->next_sibling("Group")) {
			const auto* nameAttr = group->first_attribute("name");
			if (!nameAttr) {
				continue;
			}
			const auto nameStr = Util::CastLower(nameAttr->value());
			const auto it = std::ranges::find_if(sexMapping, [&](const auto& pair) {
				return pair.first.contains(nameStr);
			});
			if (it != sexMapping.end()) {
				return it->second;
			}
		}
		return RE::SEX::kNone;
	}

	BodyslidePreset::BodyslidePreset(const rapidxml::xml_node<char>* a_node, RE::SEX a_sex, SKEE::IBodyMorphInterface* a_interface) :
		_sex(a_sex), _transformInterface(a_interface), _name(a_node->first_attribute("name") ? a_node->first_attribute("name")->value() : "unknown")
	{
		for (auto* slider = a_node ? a_node->first_node("SetSlider") : nullptr; slider; slider = slider->next_sibling("SetSlider")) {
			auto* nameAttr = slider->first_attribute("name");
			auto* sizeAttr = slider->first_attribute("size");
			auto* valueAttr = slider->first_attribute("value");
			if (nameAttr && sizeAttr && valueAttr) {
				std::string sliderName = nameAttr->value();
				std::string size = sizeAttr->value();
				int value = std::stoi(valueAttr->value());
				auto& pair = _sliders[sliderName];
				(size == "small" ? pair.first : pair.second) = value;
			} else {
				throw std::runtime_error(std::format("Invalid slider attributes in {}", _name));
			}
		}
	}

	void BodyslidePreset::ApplyPreset(RE::Actor* a_target) const
	{
		logger::info("Applying slider profile {} to {}", _name.data(), a_target->formID);
		_transformInterface->ClearBodyMorphKeys(a_target, MORPH_KEY);
		const auto base = a_target->GetActorBase();
		const auto weight = base ? base->weight / 100.0f : 0.5f;
		for (const auto& [sliderName, sliderValues] : _sliders) {
			const auto& [minVal, maxVal] = sliderValues;
			const float val{ ((maxVal - minVal) * weight) + minVal };
			_transformInterface->SetMorph(a_target, sliderName.data(), MORPH_KEY, val / 100.0f);
		}
		_transformInterface->ApplyBodyMorphs(a_target, false);
		_transformInterface->UpdateModelWeight(a_target, true);
	}

	bool BodyslidePreset::IsApplicable(RE::Actor* a_target) const
	{
		const auto race = a_target->GetRace();
		if (!race || !race->HasKeywordString("ActorTypeNPC")) {
			return false;
		}
		const auto base = a_target->GetActorBase();
		return base && base->GetSex() == _sex;
	}

	void BodyslidePreset::DeleteMorphs(RE::Actor* a_target, SKEE::IBodyMorphInterface* a_interface)
	{
		a_interface->ClearBodyMorphKeys(a_target, MORPH_KEY);
		a_interface->ApplyBodyMorphs(a_target, false);
		a_interface->UpdateModelWeight(a_target, true);
	}

	std::vector<std::shared_ptr<BodyslidePreset>> LoadBodyslidePresets(
		const std::filesystem::path& a_xmlfilePath, SKEE::IBodyMorphInterface* a_interface, SliderConfig* a_config)
	{
		if (a_xmlfilePath.empty() || a_xmlfilePath.extension() != ".xml") {
			throw std::invalid_argument("XML file path cannot be empty");
		} else if (!std::filesystem::exists(a_xmlfilePath)) {
			throw std::invalid_argument(std::format("XML file does not exist: {}", a_xmlfilePath.string()));
		} else if (!a_interface) {
			throw std::runtime_error(std::format("Missing transform interface: {}", a_xmlfilePath.string()));
		}

		std::ifstream file(a_xmlfilePath);
		if (!file) {
			throw std::runtime_error(std::format("Failed to open XML file: {}", a_xmlfilePath.string()));
		}

		std::stringstream buffer;
		buffer << file.rdbuf();
		std::string contents = buffer.str();
        if (contents.empty()) {
            throw std::runtime_error(std::format("XML file is empty: {}", a_xmlfilePath.string()));
        }

		rapidxml::xml_document<> doc;
		doc.parse<0>(contents.data());
		auto* root = doc.first_node("SliderPresets");
		if (!root) {
			throw std::runtime_error(std::format("Invalid <SliderPresets> element in {}", a_xmlfilePath.string()));
		}

		std::vector<std::shared_ptr<BodyslidePreset>> profiles{};
		for (auto* preset = root->first_node("Preset"); preset; preset = preset->next_sibling("Preset")) {
			if (a_config->IsExcluded(preset)) {
				continue;
			}
			const auto sex = a_config->GetSex(preset);
			if (sex == RE::SEX::kNone) {
				const auto nameAttr = preset->first_attribute("name");
				const auto nameStr = nameAttr ? nameAttr->value() : "unknown";
				logger::warn("Skipping slider preset due to unknown sex: {}; File: {}", nameStr, a_xmlfilePath.string());
				continue;
			}
			profiles.emplace_back(std::make_shared<BodyslidePreset>(preset, sex, a_interface));
		}
		return profiles;
	}

}  // namespace DBD::Data