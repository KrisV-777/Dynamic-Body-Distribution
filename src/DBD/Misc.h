#pragma once

inline std::string GetModName(const RE::FormID a_id)
{
	auto modidx = a_id >> 24;
	if (modidx == 0xFF)
		return "";

	const RE::TESFile* file;
	if (modidx == 0xFE) {
		file = RE::TESDataHandler::GetSingleton()->LookupLoadedLightModByIndex((a_id & 0x00FFF000) >> 12);
	} else {
		file = RE::TESDataHandler::GetSingleton()->LookupLoadedModByIndex(modidx);
	}
	return file ? std::string(file->GetFilename()) : "";
}

inline std::string GetIDHex(RE::FormID a_id)
{
	std::stringstream s;
	s << std::setfill('0') << std::setw(6) << std::hex << (a_id & 0xFFFFFF);
	return s.str();
}
