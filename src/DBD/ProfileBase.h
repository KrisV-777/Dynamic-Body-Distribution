#pragma once

namespace DBD
{
	class ProfileBase
	{
	public:
		ProfileBase(RE::BSFixedString a_name, std::string_view extension = ""sv) :
			name(std::move(a_name)), isPrivate(!name.empty() && name[0] == '.')
		{
			std::string nameTmp{ name.c_str() };
			name = nameTmp.substr(isPrivate, nameTmp.length() - extension.length());
			if (name.empty()) {
				throw std::runtime_error("Profile name is empty");
			}
		}

		RE::BSFixedString GetName() const { return name; }
		bool IsPrivate() const { return isPrivate; }

		virtual void Apply(RE::Actor* a_target) const = 0;
		virtual bool IsApplicable(RE::Actor* a_target) const = 0;

	protected:
		RE::BSFixedString name;
		bool isPrivate;
	};
}  // namespace DBD
