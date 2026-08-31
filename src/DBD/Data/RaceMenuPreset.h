#pragma once

#include <API/SKEE.h>

namespace DBD::Data
{
	class RaceMenuPreset
	{
	public:
		RaceMenuPreset(fs::path a_presetJslot, SKEE::IPresetInterface* a_presetInterface) :
			_presetName(a_presetJslot.filename().string()), _presetPath(a_presetJslot), _presetInterface(a_presetInterface) {}
		~RaceMenuPreset() = default;

		std::string_view GetName() const noexcept { return _presetName; }

		void Apply(RE::Actor* a_actor) const
		{
			assert(_presetInterface && a_actor);
			const auto tintTexture = _tintMask.empty() ? GetDefaultTintMask(a_actor) : _tintMask;
			_presetInterface->LoadPreset(_presetPath.string().c_str(), tintTexture.c_str(), a_actor);
		}

	private:
		std::string GetDefaultTintMask(RE::Actor* a_actor) const
		{
			const auto face = a_actor->GetFaceNodeSkinned();
			const auto faceGeo = face->GetFirstGeometryOfShaderType(RE::BSShaderMaterial::Feature::kFaceGen);
			const auto shader = faceGeo ? faceGeo->lightingShaderProp_cast() : nullptr;
			const auto material = shader ? shader->material : nullptr;
			return material ? static_cast<RE::BSLightingShaderMaterialFacegen*>(material)->tintTexture->name.data() : "";
		}

		fs::path _presetPath;
		std::string _presetName;
		std::string _tintMask;
		SKEE::IPresetInterface* _presetInterface;
	};
}  // namespace DBD::Data
