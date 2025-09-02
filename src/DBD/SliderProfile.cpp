#include "SliderProfile.h"

#include <yaml-cpp/yaml.h>

#include "Util/StringUtil.h"

namespace DBD
{
	std::vector<std::shared_ptr<SliderProfile>> SliderProfile::LoadProfiles(
		const std::filesystem::path& a_xmlfilePath,
		RE::SEX a_sex,
		SKEE::IBodyMorphInterface* a_interface,
		SliderConfig* a_config)
	{
		std::vector<std::shared_ptr<SliderProfile>> profiles{};
		if (a_xmlfilePath.empty() || a_xmlfilePath.extension() != ".xml") {
			throw std::invalid_argument("XML file path cannot be empty");
		} else if (!std::filesystem::exists(a_xmlfilePath)) {
			throw std::invalid_argument(std::format("XML file does not exist: {}", a_xmlfilePath.string()));
		} else if (!a_interface) {
			throw std::runtime_error("Missing transform interface");
		}
		assert(a_sex != RE::SEX::kNone || a_config);
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
			const auto nameAttr = preset->first_attribute("name");
			const auto nameStr = nameAttr ? nameAttr->value() : "unknown";
			if (Util::CastLower(nameStr) == ZERO_SLIDER_PRESET) {
				continue;
			}
			const auto sex = a_sex == RE::SEX::kNone ? a_config->GetSex(preset) : a_sex;
			if (sex == RE::SEX::kNone) {
				logger::warn("Skipping preset '{}' due to unknown sex", nameStr);
				continue;
			}
			profiles.emplace_back(std::make_shared<SliderProfile>(preset, sex, isPrivate, a_interface));
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

	SliderProfile::SliderProfile(const rapidxml::xml_node<char>* a_node, RE::SEX a_sex, bool a_isPrivate, SKEE::IBodyMorphInterface* a_interface) :
		ProfileBase([&]() -> const char* {
			if (auto* attr = a_node->first_attribute("name"))
				return attr->value();
			throw std::runtime_error("Missing 'name' attribute in SliderProfile node");
		}()),
		sex(a_sex), transformInterface(a_interface)
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
		return base && base->GetSex() == sex;
	}

	void SliderProfile::DeleteMorphs(RE::Actor* a_target, SKEE::IBodyMorphInterface* a_interface)
	{
		a_interface->ClearBodyMorphKeys(a_target, MORPH_KEY);
		a_interface->ApplyBodyMorphs(a_target, false);
		a_interface->UpdateModelWeight(a_target, true);
	}

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
				for (auto&& node : root) {
					const auto nameStr = Util::CastLower(node.first.as<std::string>());
					const auto sexStr = Util::CastLower(node.second.as<std::string>());
					sexMapping[nameStr] = sexStr.starts_with("f") ? RE::SEX::kFemale : RE::SEX::kMale;
				}
			} catch (const std::exception& e) {
				logger::error("Failed to load slider config '{}': {}", file.path().filename().string(), e.what());
			}
		}
	}

	RE::SEX SliderConfig::GetSex(const rapidxml::xml_node<char>* a_node) const
	{
		for (auto* group = a_node ? a_node->first_node("Group") : nullptr; group; group = group->next_sibling("Group")) {
			const auto* nameAttr = group->first_attribute("name");
			if (!nameAttr) {
				continue;
			}
			const auto nameStr = Util::CastLower(nameAttr->value());
			if (const auto it = sexMapping.find(nameStr); it != sexMapping.end()) {
				return it->second;
			}
		}
		return RE::SEX::kNone;
	}

}  // namespace DBD
