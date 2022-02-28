#include "Cartridge.h"
#include <fstream>
#include <filesystem>
#include "InvalidFileException.h"
#include "Mapper000.h"
#include "Mapper001.h"
#include "Mapper002.h"
#include "Mapper003.h"
#include "Mapper004.h"
#include "Mapper007.h"
#include "Mapper066.h"
#include "Mapper140.h"

Cartridge::Cartridge(std::string cartName, std::vector<uint8_t> bytes, std::vector<uint8_t> sram) :
	cartName(cartName)
{
	// Extract header
	if (bytes.size() < sizeof(header))
		throw InvalidFileException("Invalid ROM (header too small).");
	std::memcpy(&header, bytes.data(), sizeof(header));
	if (std::string(reinterpret_cast<char*>(header.signature), 4) != "NES\x1A")
		throw InvalidFileException("Invalid ROM (invalid signature).");
	bytes.erase(bytes.begin(), bytes.begin() + sizeof(header));

	// Extract prg
	if (bytes.size() < static_cast<size_t>(header.prgChunks * 0x4000))
		throw InvalidFileException("Invalid ROM (PRG too small).");
	prg.resize(header.prgChunks * 16384);
	std::memcpy(prg.data(), bytes.data(), header.prgChunks * 0x4000);
	bytes.erase(bytes.begin(), bytes.begin() + header.prgChunks * 0x4000);

	// Extract chr
	if (header.chrChunks == 0) {
		header.chrChunks = 4;
		chr.resize(header.chrChunks * 8192);
	} else {
		if (bytes.size() < static_cast<size_t>(header.chrChunks * 0x2000))
			throw InvalidFileException("Invalid ROM (CHR too small).");
		chr.resize(header.chrChunks * 8192);
		std::memcpy(chr.data(), bytes.data(), header.chrChunks * 0x2000);
		bytes.erase(bytes.begin(), bytes.begin() + header.chrChunks * 0x2000);
	}

	// Select mapper
	if (!header.padding[3] && !header.padding[4] && !header.padding[5] && !header.padding[6])
		LoadMapper((header.flag7.upperMapperNumber << 4) | header.flag6.lowerMapperNumber);
	else
		LoadMapper(header.flag6.lowerMapperNumber);

	// Load sram
	mapper->SetSram(std::move(sram));
}

Cartridge::Cartridge(Savestate& state) {

	cartName = state.PopString();
	header = state.Pop<Header>();
	prg = state.PopVec();
	chr = state.PopVec();

	int mapperNumber = state.Pop<int32_t>();
	LoadMapper(mapperNumber, &state);
	mapper->SetSram(state.PopVec());
}

Savestate Cartridge::SaveState() const {
	Savestate state;
	state.PushString(cartName);
	state.Push<Header>(header);
	state.PushVec(prg);
	state.PushVec(chr);

	state.Push<int32_t>(mapper->MapperNumber());
	state.PushState(mapper->SaveState());
	state.PushVec(mapper->GetSram());
	return state;
}

void Cartridge::Reset() {
	mapper->Reset();
}

void Cartridge::LoadMapper(int mapperNumber, Savestate* state) {

	auto createMapper = [&]<class M>() {
		if (state)
			return std::make_unique<M>(*state, prg, chr);
		else
			return std::make_unique<M>(mapperNumber, header.prgChunks, header.chrChunks, prg, chr);
	};

	switch (mapperNumber) {
	case   0: mapper = createMapper.operator()<Mapper000>(); break;
	case   1: mapper = createMapper.operator()<Mapper001>(); break;
	case   2: mapper = createMapper.operator()<Mapper002>(); break;
	case   3: mapper = createMapper.operator()<Mapper003>(); break;
	case   4: mapper = createMapper.operator()<Mapper004>(); break;
	case   7: mapper = createMapper.operator()<Mapper007>(); break;
	case  66: mapper = createMapper.operator()<Mapper066>(); break;
	case 140: mapper = createMapper.operator()<Mapper140>(); break;
	default: throw InvalidFileException("Unsupported mapper " + std::to_string(mapperNumber) + '.');
	}
#undef Load
}

bool Cartridge::CpuWrite(uint16_t& addr, uint8_t data) {
	return mapper->MapCpuWrite(addr, data);
}

bool Cartridge::CpuRead(uint16_t& addr, uint8_t& data, bool readonly) {
	return mapper->MapCpuRead(addr, data, readonly);
}

bool Cartridge::PpuWrite(uint16_t& addr, uint8_t data) {
	return mapper->MapPpuWrite(addr, data);
}

bool Cartridge::PpuRead(uint16_t& addr, uint8_t& data, bool readonly) {
	return mapper->MapPpuRead(addr, data, readonly);
}

MirrorMode Cartridge::GetMirrorMode() const {
	auto mode = mapper->GetMirrorMode();
	if (mode == MirrorMode::Hardwired)
		return header.flag6.mirroring ? MirrorMode::Vertical : MirrorMode::Horizontal;
	else
		return mode;
}

Mapper& Cartridge::GetMapper() {
	return *mapper;
}

const Mapper& Cartridge::GetMapper() const {
	return *mapper;
}

const std::vector<uint8_t>& Cartridge::GetSram() const {
	return mapper->GetSram();
}

const std::string& Cartridge::GetCartridgeName() const {
	return cartName;
}
