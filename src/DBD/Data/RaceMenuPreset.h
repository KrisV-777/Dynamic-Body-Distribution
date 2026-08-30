#pragma once

#include <API/SKEE.h>

namespace DBD::Data
{
	class RaceMenuPreset
	{
	public:
		RaceMenuPreset(std::string a_presetName, SKEE::IPresetInterface* a_presetInterface) : _presetName(a_presetName), _presetInterface(a_presetInterface) {}
		~RaceMenuPreset() = default;

		std::string_view GetName() const noexcept { return _presetName; }

		void LoadPreset(RE::Actor* a_actor) const
		{
			assert(_presetInterface && a_actor);
			const auto tintTexture = _tintMask.empty() ? GetDefaultTintMask(a_actor) : _tintMask;
			_presetInterface->LoadPreset(_presetName.c_str(), tintTexture.c_str(), a_actor);
		}

	private:
		std::string GetDefaultTintMask(RE::Actor* a_actor) const
		{
			const auto face = a_actor->GetFaceNodeSkinned();
			const auto faceGen = static_cast<RE::BSLightingShaderMaterialFacegen*>(
				face->GetFirstGeometryOfShaderType(RE::BSShaderMaterial::BSShaderType::kFaceGen));
			return faceGen && faceGen->tintTexture ? faceGen->tintTexture->name : "";
		}

		std::string _presetName;
		std::string _tintMask;
		SKEE::IPresetInterface* _presetInterface;
	};
}  // namespace DBD::Data
