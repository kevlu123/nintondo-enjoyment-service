#include "Mapper001.h"

Mapper001::Mapper001(int mapperNumber, int prgChunks, int chrChunks, std::vector<uint8_t>& prg, std::vector<uint8_t>& chr) :
	Mapper(mapperNumber, prgChunks, chrChunks, prg, chr)
{
	sramSize = 0x2000;
	sram.resize(sramSize);
	Reset();
}

Mapper001::Mapper001(Savestate& state, std::vector<uint8_t>& prg, std::vector<uint8_t>& chr) :
	Mapper(state, prg, chr)
{
	sramSize = 0x2000;
	sram.resize(sramSize);

	shift = state.Pop<uint8_t>();
	chrLo = state.Pop<uint8_t>();
	chrHi = state.Pop<uint8_t>();
	prgLo = state.Pop<uint8_t>();
	ctrl = state.Pop<decltype(ctrl)>();
}

Savestate Mapper001::SaveState() const {
	Savestate state = Mapper::SaveState();
	state.Push<uint8_t>(shift);
	state.Push<uint8_t>(chrLo);
	state.Push<uint8_t>(chrHi);
	state.Push<uint8_t>(prgLo);
	state.Push<decltype(ctrl)>(ctrl);
	return state;
}

void Mapper001::Reset() {
	shift = 0b10000;
	ctrl.prgBankMode = 3;
}

MirrorMode Mapper001::GetMirrorMode() const {
	switch (ctrl.mirror) {
	case 0:
		return MirrorMode::OneScreenLo;
	case 1:
		return MirrorMode::OneScreenHi;
	case 2:
		return MirrorMode::Vertical;
	case 3:
		return MirrorMode::Horizontal;
	default:
		return MirrorMode::Hardwired;
	}
}

bool Mapper001::MapCpuRead(uint16_t& addr, uint8_t& data, bool readonly) {
	uint32_t newAddr;
	if (addr >= 0x6000 && addr < 0x8000) {
		data = sram[addr - 0x6000];
		return true;
	} else if (addr >= 0x8000) {
		uint8_t bank = prgLo & 0x0F;
		switch (ctrl.prgBankMode) {
		case 0:
		case 1:
			newAddr = 0x8000 + ((bank & 0b1111'1110) * 0x4000) + (addr & 0x7FFF);
			break;
		case 2:
			if (addr < 0xC000)
				newAddr = 0x8000 + (addr & 0x3FFF);
			else
				newAddr = 0x8000 + (bank * 0x4000) + (addr & 0x3FFF);
			break;
		case 3:
			if (addr >= 0xC000)
				newAddr = 0x8000 + ((prgChunks - 1) * 0x4000) + (addr & 0x3FFF);
			else
				newAddr = 0x8000 + (bank * 0x4000) + (addr & 0x3FFF);
			break;
		}
	} else
		newAddr = addr;
	return Mapper::MapCpuRead(newAddr, data);
}

bool Mapper001::MapCpuWrite(uint16_t& addr, uint8_t data) {
	if (addr >= 0x6000 && addr < 0x8000) {
		sram[addr - 0x6000] = data;
		return true;
	} else if (addr >= 0x8000) {
		if (data & 0x80) {
			Reset();
		} else {
			bool filled = shift & 1;
			shift = (shift >> 1) | ((data & 1) << 4);
			shift &= 0b11111;
			if (filled) {
				switch ((addr - 0x8000) / 0x2000) {
				case 0:
					ctrl.reg = shift;
					break;
				case 1:
					chrLo = shift;
					break;
				case 2:
					chrHi = shift;
					break;
				case 3:
					prgLo = shift;
					break;
				}
				shift = 0b10000;
			}
		}
	}
	return false;
}

bool Mapper001::MapPpuAddr(uint16_t& addr, uint32_t& newAddr) const {
	newAddr = addr;
	if (addr < 0x2000) {
		if (ctrl.chrBankMode == 0) {
			newAddr = ((chrLo & 0b1111'1110) * 0x1000) + (addr & 0x1FFF);
		} else {
			if (addr < 0x1000)
				newAddr = (chrLo * 0x1000) + (addr & 0x0FFF);
			else
				newAddr = (chrHi * 0x1000) + (addr & 0x0FFF);
		}
		return true;
	}
	return false;
}

bool Mapper001::MapPpuRead(uint16_t& addr, uint8_t& data, bool readonly) {
	uint32_t newAddr;
	if (MapPpuAddr(addr, newAddr)) {
		if (newAddr < chr.size())
			data = chr[newAddr];
		return true;
	}
	return false;
}

bool Mapper001::MapPpuWrite(uint16_t& addr, uint8_t data) {
	uint32_t newAddr;
	if (MapPpuAddr(addr, newAddr)) {
		if (newAddr < chr.size())
			chr[newAddr] = data;
		return true;
	}
	return false;
}
