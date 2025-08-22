#include "Serialization.h"

#include "DBD/Distribution.h"

namespace DBD
{

	void Serialize::SaveCallback(SKSE::SerializationInterface* a_intfc)
	{
		if (!a_intfc->OpenRecord(_Profiles, _Version))
			logger::error("Failed to open record <Defeated>");
		else
			Distribution::GetSingleton()->Save(a_intfc, _Version);

		logger::info("Finished writing data to cosave");
	}

	void Serialize::LoadCallback(SKSE::SerializationInterface* a_intfc)
	{
		uint32_t type;
		uint32_t version;
		uint32_t length;
		while (a_intfc->GetNextRecordInfo(type, version, length)) {
			const auto ty = GetTypeName(type);
			if (version != _Version) {
				logger::info("Invalid Version for loaded Data of Type = {}. Expected = {}; Got = {}", ty, static_cast<uint32_t>(_Version), version);
				continue;
			}
			logger::info("Loading record {}", ty);
			switch (type) {
			case _Profiles:
				Distribution::GetSingleton()->Load(a_intfc, _Version);
				break;
			default:
				logger::error("Unknown record type: {}", ty);
				break;
			}
		}

		logger::info("Finished loading data from cosave");
	}

	void Serialize::RevertCallback(SKSE::SerializationInterface* a_intfc)
	{
		Distribution::GetSingleton()->Revert(a_intfc);
	}

	void Serialize::FormDeleteCallback(RE::VMHandle)
	{
	}

	std::string GetTypeName(uint32_t a_type)
	{
		constexpr auto size = sizeof(uint32_t);
		std::string ret{};
		ret.resize(size);
		const char* it = reinterpret_cast<char*>(&a_type);
		for (size_t i = 0, j = size - 1; i < size; i++, j--)
			ret[j] = std::isprint(it[i]) ? it[i] : '_';

		return ret;
	}
}  // namespace DBD
