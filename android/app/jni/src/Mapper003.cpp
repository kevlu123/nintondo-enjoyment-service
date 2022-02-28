#include "Mapper003.h"

Mapper003::Mapper003(int mapperNumber, int prgChunks, int chrChunks, std::vector<uint8_t>& prg, std::vector<uint8_t>& chr) :
	Mapper(mapperNumber, prgChunks, chrChunks, prg, chr),
	chrBank(0) {
}

Mapper003::Mapper003(Savestate& state, std::vector<uint8_t>& prg, std::vector<uint8_t>& chr) :
	Mapper(state, prg, chr) {
	chrBank = state.Pop<int32_t>();
	chrBank &= 0b11;
}

Savestate Mapper003::SaveState() const {
	Savestate state = Mapper::SaveState();
	state.Push<int32_t>(chrBank);
	return state;
}

void Mapper003::Reset() {
	chrBank = 0;
}

bool Mapper003::MapCpuRead(uint16_t& addr, uint8_t& data, bool readonly) {
	uint32_t newAddr;
	if (addr >= 0x8000)
		newAddr = 0x8000 + (prgChunks > 1 ? (addr & 0x7FFF) : (addr & 0x3FFF));
	else
		newAddr = addr;
	return Mapper::MapCpuRead(newAddr, data);
}

bool Mapper003::MapCpuWrite(uint16_t& addr, uint8_t data) {
	if (addr >= 0x8000)
		chrBank = data & 0b11;
	return false;
}

bool Mapper003::MapPpuRead(uint16_t& addr, uint8_t& data, bool readonly) {
	if (addr < 0x2000) {
		auto newAddr = (chrBank * 0x2000) + (addr & 0x1FFF);
		if (newAddr < chr.size()) {
			data = chr[newAddr];
			return true;
		}
	}
	return false;
}

bool Mapper003::MapPpuWrite(uint16_t& addr, uint8_t data) {
	return false;
}
