#pragma once

#include "TexturePack.h"

namespace DBD::Data
{
	struct TexturePreset
	{
		/// @brief Retrieves the concatenated names of all texture packs in the preset.
		std::string GetName() const;

		/// @brief Applies the texture overrides to the given actor or object.
		/// @param a_actor The actor to apply the texture pack to.
		/// @param a_object The object to apply the texture pack to.
		/// @param a_textureOverrides A map of texture overrides to apply.
		/// @return True if any textures were overridden, false otherwise.
		bool Apply(RE::Actor* a_actor) const;
		bool Apply(RE::NiAVObject* a_object) const;

	public:
		std::set<std::shared_ptr<TexturePack>> packs{};
		TexturePack::TextureMap data{};
		bool _updatedThisSession = false;

	private:
		bool OverrideGeometry(RE::BSGeometry* a_geometry) const;
		RE::BSTextureSet* BuildReplacementTextureSet(RE::BSTextureSet* a_originalTextureSet) const;
		static void CopyFeatureSpecificTextures(RE::BSLightingShaderMaterialBase* a_source, RE::BSLightingShaderMaterialBase* a_target);

	public:
		operator bool() const { return !packs.empty() && !data.empty(); }
	};
}  // namespace DBD::Data
