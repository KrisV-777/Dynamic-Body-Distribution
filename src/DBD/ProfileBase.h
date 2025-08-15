#pragma once

namespace DBD
{
	class ProfileBase
	{
	public:
		ProfileBase(RE::BSFixedString a_name) :
			name(std::move(a_name)), isPrivate(!name.empty() && name[0] == '.') {}

		RE::BSFixedString GetName() const { return name; }
		bool IsPrivate() const { return isPrivate; }

		virtual void Apply(RE::Actor* a_target) const = 0;
		virtual bool IsApplicable(RE::Actor* a_target) const = 0;

	protected:
		RE::BSFixedString name;
		bool isPrivate;
	};
}  // namespace DBD
