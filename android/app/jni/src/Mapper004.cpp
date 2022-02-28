#include "Mapper004.h"

Mapper004::Mapper004(int mapperNumber, int prgChunks, int chrChunks, std::vector<uint8_t>& prg, std::vector<uint8_t>& chr) :
	Mapper(mapperNumber, prgChunks, chrChunks, prg, chr),
	lastPrgBankNumber(prgChunks * 2 - 1) {
	sramSize = 0x2000;
	sram.resize(sramSize);
	Reset();
}

Mapper004::Mapper004(Savestate& state, std::vector<uint8_t>& prg, std::vector<uint8_t>& chr) :
	Mapper(state, prg, chr),
	lastPrgBankNumber(prgChunks * 2 - 1) {
	sramSize = 0x2000;
	sram.resize(sramSize);

	state.PopArray(regs, sizeof(regs));
	bankSelect = state.Pop<decltype(bankSelect)>();
	mirrorMode = state.Pop<MirrorMode>();
	irqCounter = state.Pop<int32_t>();
	irqReload = state.Pop<int32_t>();
	irqEnabled = state.Pop<uint8_t>();
	irqState = state.Pop<uint8_t>();
	reloadPending = state.Pop<uint8_t>();
}

Savestate Mapper004::SaveState() const {
	Savestate state = Mapper::SaveState();

	state.PushArray(regs, sizeof(regs));
	state.Push<decltype(bankSelect)>(bankSelect);
	state.Push<MirrorMode>(mirrorMode);
	state.Push<int32_t>(irqCounter);
	state.Push<int32_t>(irqReload);
	state.Push<uint8_t>(irqEnabled);
	state.Push<uint8_t>(irqState);
	state.Push<uint8_t>(reloadPending);

	return state;
}

void Mapper004::Reset() {
	std::memset(regs, 0, std::size(regs));
	bankSelect.reg = 0;
	irqCounter = 0;
	irqReload = 0;
	irqEnabled = false;
	irqState = false;
	reloadPending = false;
	mirrorMode = MirrorMode::Hardwired;
}

MirrorMode Mapper004::GetMirrorMode() const {
	return mirrorMode;
}

bool Mapper004::MapCpuRead(uint16_t& addr, uint8_t& data, bool readonly) {
	uint32_t newAddr;
	if (addr >= 0x6000 && addr < 0x8000) {
		data = sram[addr - 0x6000];
		return true;
	} else if (addr >= 0x8000 && addr < 0xA000) {
		if (bankSelect.prgMode == 0)
			newAddr = 0x8000 + (regs[6] * 0x2000) + (addr & 0x1FFF);
		else
			newAddr = 0x8000 + ((lastPrgBankNumber - 1) * 0x2000) + (addr & 0x1FFF);
	} else if (addr >= 0xA000 && addr < 0xC000) {
		newAddr = 0x8000 + (regs[7] * 0x2000) + (addr & 0x1FFF);
	} else if (addr >= 0xC000 && addr < 0xE000) {
		if (bankSelect.prgMode)
			newAddr = 0x8000 + (regs[6] * 0x2000) + (addr & 0x1FFF);
		else
			newAddr = 0x8000 + ((lastPrgBankNumber - 1) * 0x2000) + (addr & 0x1FFF);
	} else if (addr >= 0xE000) {
		newAddr = 0x8000 + (lastPrgBankNumber * 0x2000) + (addr & 0x1FFF);
	} else
		newAddr = addr;
	return Mapper::MapCpuRead(newAddr, data);
}

bool Mapper004::MapCpuWrite(uint16_t& addr, uint8_t data) {
	bool evenAddr = addr % 2 == 0;
	if (addr >= 0x6000 && addr < 0x8000) {
		sram[addr - 0x6000] = data;
		return true;
	} else if (addr >= 0x8000 && addr < 0xA000) {
		if (evenAddr)
			bankSelect.reg = data;
		else {
			uint8_t n = bankSelect.regNumber;
			if (n == 0 || n == 1)
				data &= 0b1111'1110;
			else if (n == 6 || n == 7)
				data &= 0b0011'1111;
			regs[n] = data;
		}
	} else if (addr >= 0xA000 && addr < 0xC000) {
		if (evenAddr)
			mirrorMode = (data & 1) ? MirrorMode::Horizontal : MirrorMode::Vertical;
		else {
			// RAM protect
		}
	} else if (addr >= 0xC000 && addr < 0xE000) {
		if (evenAddr)
			irqReload = data;
		else
			reloadPending = true;
	} else if (addr >= 0xE000) {
		if (evenAddr) {
			irqEnabled = false;
			irqState = false;
		} else
			irqEnabled = true;
	}
	return false;
}

bool Mapper004::MapPpuAddr(uint16_t& addr, uint32_t& newAddr) const {
	newAddr = addr;
	if (addr < 0x2000) {
		if (bankSelect.chrMode == 0) {
			if (addr < 0x1000)
				newAddr = (regs[addr >= 0x800] * 0x400) + (addr & 0x7FF);
			else
				newAddr = (regs[(addr - 0x1000) / 0x400 + 2] * 0x400) + (addr & 0x3FF);
		} else {
			if (addr >= 0x1000)
				newAddr = (regs[(addr - 0x1000) >= 0x800] * 0x400) + (addr & 0x7FF);
			else
				newAddr = (regs[addr / 0x400 + 2] * 0x400) + (addr & 0x3FF);
		}
		return true;
	}
	return false;
}

bool Mapper004::MapPpuRead(uint16_t& addr, uint8_t& data, bool readonly) {
	uint32_t newAddr;
	if (MapPpuAddr(addr, newAddr)) {
		if (newAddr < chr.size())
			data = chr[newAddr];
		return true;
	}
	return false;
}

bool Mapper004::MapPpuWrite(uint16_t& addr, uint8_t data) {
	uint32_t newAddr;
	if (MapPpuAddr(addr, newAddr)) {
		if (newAddr < chr.size())
			chr[newAddr] = data;
		return true;
	}
	return false;
}


bool Mapper004::GetIrq() const {
	return irqState;
}

void Mapper004::ClearIrq() {
	irqState = false;
}

void Mapper004::CountScanline() {
	if (reloadPending || irqCounter <= 0) {
		reloadPending = false;
		irqCounter = irqReload;
		if (irqCounter == 0)
			irqState = true;
	} else {
		irqCounter--;
		if (irqCounter <= 0 && irqEnabled)
			irqState = true;
	}
}
