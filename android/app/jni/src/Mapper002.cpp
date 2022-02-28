#include "Mapper002.h"

Mapper002::Mapper002(int mapperNumber, int prgChunks, int chrChunks, std::vector<uint8_t>& prg, std::vector<uint8_t>& chr) :
	Mapper(mapperNumber, prgChunks, chrChunks, prg, chr),
	loPrgBank(0),
	hiPrgBank(prgChunks - 1) {
}

Mapper002::Mapper002(Savestate& state, std::vector<uint8_t>& prg, std::vector<uint8_t>& chr) :
	Mapper(state, prg, chr) {
	loPrgBank = state.Pop<uint8_t>();
	hiPrgBank = prgChunks - 1;
}

void Mapper002::Reset() {
	loPrgBank = 0;
}

Savestate Mapper002::SaveState() const {
	Savestate state = Mapper::SaveState();
	state.Push<uint8_t>(loPrgBank);
	return state;
}

bool Mapper002::MapCpuRead(uint16_t& addr, uint8_t& data, bool readonly) {
	uint32_t newAddr;
	if (addr >= 0x8000) {
		if (addr < 0x8000 + 0x4000)
			newAddr = 0x8000 + (0x4000 * loPrgBank) + (addr & 0x3FFF);
		else
			newAddr = 0x8000 + (0x4000 * hiPrgBank) + (addr & 0x3FFF);
	} else
		newAddr = addr;
	return Mapper::MapCpuRead(newAddr, data);
}

bool Mapper002::MapCpuWrite(uint16_t& addr, uint8_t data) {
	if (addr >= 0x8000)
		loPrgBank = data;
	return false;
}
