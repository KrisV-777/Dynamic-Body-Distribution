#pragma once

namespace DBD
{
	enum ProfileType
	{
		Textures,
		Sliders,

		Total_V1,
		Total = Total_V1
	};
	template <class T>
	using ProfileArray = std::array<T, ProfileType::Total>;

	class ProfileBase
	{
	public:
		ProfileBase(RE::BSFixedString a_name, std::string_view extension = ""sv) :
			name(std::move(a_name)), isPrivate(!name.empty() && name[0] == '.')
		{
			std::string nameTmp{ name.c_str() };
			name = nameTmp.substr(isPrivate, nameTmp.length() - extension.length() - isPrivate);
			if (name.empty()) {
				throw std::runtime_error("Profile name is empty");
			}
		}
		virtual ~ProfileBase() = default;

		RE::BSFixedString GetName() const { return name; }
		bool IsPrivate() const { return isPrivate; }

		virtual void Apply(RE::Actor* a_target) const = 0;
		virtual bool IsApplicable(RE::Actor* a_target) const = 0;

	protected:
		RE::BSFixedString name;
		bool isPrivate;
	};

}  // namespace DBD
