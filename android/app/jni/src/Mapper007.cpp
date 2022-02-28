#include "Mapper007.h"

Mapper007::Mapper007(int mapperNumber, int prgChunks, int chrChunks, std::vector<uint8_t>& prg, std::vector<uint8_t>& chr) :
	Mapper(mapperNumber, prgChunks, chrChunks, prg, chr) {
	Reset();
}

Mapper007::Mapper007(Savestate& state, std::vector<uint8_t>& prg, std::vector<uint8_t>& chr) :
	Mapper(state, prg, chr) {
	prgBank = state.Pop<int32_t>();
	prgBank &= 0b1111;
	mirrorMode = state.Pop<MirrorMode>();
}

Savestate Mapper007::SaveState() const {
	auto state = Mapper::SaveState();
	state.Push<int32_t>(prgBank);
	state.Push<MirrorMode>(mirrorMode);
	return state;
}

void Mapper007::Reset() {
	prgBank = prgChunks / 2 - 1;
	mirrorMode = MirrorMode::OneScreenLo;
}

bool Mapper007::MapCpuRead(uint16_t& addr, uint8_t& data, bool readonly) {
	uint32_t newAddr;
	if (addr >= 0x8000)
		newAddr = 0x8000 + prgBank * 0x8000 + (addr & 0x7FFF);
	else
		newAddr = addr;
	return Mapper::MapCpuRead(newAddr, data);
}

bool Mapper007::MapCpuWrite(uint16_t& addr, uint8_t data) {
	if (addr >= 0x8000) {
		prgBank = data & 0b1111;
		mirrorMode = (data & 0b0001'0000) ? MirrorMode::OneScreenHi : MirrorMode::OneScreenLo;
	}
	return false;
}

MirrorMode Mapper007::GetMirrorMode() const {
	return mirrorMode;
}
