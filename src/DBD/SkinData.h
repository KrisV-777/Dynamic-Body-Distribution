#pragma once

namespace DBD
{
	using TextureType = RE::BSTextureSet::Texture;

	// IDEA: Enable creature support? Randomizing WW skins or so without esp soundin kinda nice idk
	enum class RaceType
	{
		Default,
		Argonian,
		Khajiit,
		Unknown
	};

	enum BodyPart
	{
		Face,
		FaceVampire,
		Body,
		Hands,

		Total
	};

	class SkinData
	{
	public:
		struct ExtraTexture
		{
			ExtraTexture(const std::string& a_texturepath) :
				TexturePath(a_texturepath) {}
			~ExtraTexture() = default;
			// Full relative path to texture = TexturePath + TextureFile
			std::string TexturePath;									// Path to relative root folder
			std::vector<std::pair<std::string, TextureType>> Textures;	// [ TextureFile | TextureType ]

			RE::BGSTextureSet* ExtraSet[2] = { nullptr, nullptr };	// [Non Vampire, Vampire]
		};
	
	public:
		static SkinData* InitializeTexture(const fs::directory_entry& a_texturefolder);

	public:
		SkinData(const std::string& a_name) :
			_Name(a_name) { assert(!_Name.empty()); }
		~SkinData() = default;
		bool InitializeTextureSets(RE::SEX a_sex);
		std::string GetID() { return _Name; }

		RaceType Type = decltype(Type)::Unknown;
		std::array<RE::BGSTextureSet*[RE::SEX::kTotal], BodyPart::Total> Textures;
		std::vector<ExtraTexture> AdditionalTextures;  // [ OriginFolder | Texture[] ]

		RE::TESObjectARMO* NakedSkin = nullptr;

	private:
		static constexpr const char* GetVanillaTexture(TextureType a_type, RE::SEX a_sex, BodyPart a_part, RaceType a_set);
		static std::optional<TextureType> GetTextureType(const std::string& a_file);
		static void SetTexture(RE::BGSTextureSet* a_set, const std::string& a_texture, bool a_force);

	private:
		std::string _Name;	// id
	};

}  // namespace DBD
