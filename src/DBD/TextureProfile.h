#pragma once

#include "ProfileBase.h"

namespace DBD
{
	class TextureProfile : public ProfileBase
	{
		template <typename T = std::string>
		using TextureMap = std::map<std::string, T, StringComparator>;

		using VisitControl = RE::BSVisit::BSVisitControl;
		using Feature = RE::BSLightingShaderMaterialBase::Feature;
		using MaterialBase = RE::BSLightingShaderMaterialBase;
		using MaterialFacegen = RE::BSLightingShaderMaterialFacegen;
		using Texture = RE::BSTextureSet::Texture;
		using TextureData = std::array<std::string, Texture::kTotal>;

		static constexpr std::string_view PREFIX_PATH{ "data/textures"sv };

	public:
		TextureProfile(const fs::directory_entry& a_textureFolder);
		~TextureProfile() = default;

		void Apply(RE::Actor* a_target) const override;
		bool IsApplicable(RE::Actor* a_target) const override;

		void ApplyHeadTexture(RE::Actor* a_target) const;
		void ApplySkinTexture(RE::Actor* a_target) const;

		void HandleOnAttach(RE::NiAVObject* a_object);

	private:
		void FillTextureSet(RE::BSTextureSet* a_sourceSet, RE::BSTextureSet* a_targetSet) const;
		static std::string GetSubfolderKey(std::string a_path);

	private:
		TextureMap<> textures;
	};

}  // namespace DBD
