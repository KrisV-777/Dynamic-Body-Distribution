#pragma once

#include "Util/Singleton.h"

namespace DBD
{
	class Serialize final :
		public Singleton<Serialize>
	{
	public:
		enum : std::uint32_t
		{
			_Version = 1,

			_Profiles = 'prf'
		};

		static void SaveCallback(SKSE::SerializationInterface* a_intfc);
		static void LoadCallback(SKSE::SerializationInterface* a_intfc);
		static void RevertCallback(SKSE::SerializationInterface* a_intfc);
		static void FormDeleteCallback(RE::VMHandle a_handle);
	};	// class Serialize

	std::string GetTypeName(uint32_t a_type);

}  // namespace Serialize