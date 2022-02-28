#include "Mapper.h"

Mapper::Mapper(int mapperNumber, int prgChunks, int chrChunks, std::vector<uint8_t>& prg, std::vector<uint8_t>& chr) :
	prgChunks(prgChunks),
	chrChunks(chrChunks),
	mapperNumber(mapperNumber),
	prg(prg),
	chr(chr) {
}

Mapper::Mapper(Savestate& state, std::vector<uint8_t>& prg, std::vector<uint8_t>& chr) :
	prg(prg),
	chr(chr) {
	mapperNumber = state.Pop<int32_t>();
	prgChunks = state.Pop<int32_t>();
	chrChunks = state.Pop<int32_t>();
}

Savestate Mapper::SaveState() const {
	Savestate state;
	state.Push<int32_t>(mapperNumber);
	state.Push<int32_t>(prgChunks);
	state.Push<int32_t>(chrChunks);
	return state;
}

void Mapper::Reset() {
}

int Mapper::MapperNumber() const {
	return mapperNumber;
}

bool Mapper::MapCpuRead(uint16_t& addr, uint8_t& data, bool readonly) {
	return MapCpuRead(addr, data);
}

bool Mapper::MapCpuWrite(uint16_t& addr, uint8_t data) {
	return MapCpuWrite(addr, data);
}

bool Mapper::MapCpuRead(uint32_t addr, uint8_t& data) {
	if (addr >= 0x8000) {
		addr -= 0x8000;
		if (addr < prg.size())
			data = prg[addr];
		return true;
	}
	return false;
}

bool Mapper::MapCpuWrite(uint32_t addr, uint8_t data) {
	if (addr >= 0x8000) {
		addr -= 0x8000;
		if (addr < prg.size())
			prg[addr] = data;
		return true;
	}
	return false;
}

bool Mapper::MapPpuRead(uint16_t& addr, uint8_t& data, bool readonly) {
	if (addr < 0x2000) {
		if (addr < chr.size())
			data = chr[addr];
		return true;
	}
	return false;
}

bool Mapper::MapPpuWrite(uint16_t& addr, uint8_t data) {
	if (addr < 0x2000) {
		if (addr < chr.size())
			chr[addr] = data;
		return true;
	}
	return false;
}

MirrorMode Mapper::GetMirrorMode() const {
	return MirrorMode::Hardwired;
}

bool Mapper::GetIrq() const {
	return false;
}

void Mapper::ClearIrq() {
}

void Mapper::CountScanline() {
}

const std::vector<uint8_t>& Mapper::GetSram() const {
	return sram;
}

void Mapper::SetSram(std::vector<uint8_t> data) {
	sram = std::move(data);
	sram.resize(sramSize);
}
