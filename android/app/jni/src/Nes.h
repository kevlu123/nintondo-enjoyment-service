#pragma once
#include "Cpu.h"
#include "Ppu.h"
#include "Apu.h"
#include "Cartridge.h"
#include "Controller.h"
#include "Savestate.h"

class Nes {
public:
	Nes();
	void Reset();
	void InsertCartridge(std::unique_ptr<Cartridge> cart);
	void SetController(int port, std::unique_ptr<NesController> controller);
	void UnplugController(int port);
	uint8_t CpuRead(uint16_t addr, bool readonly = false);
	void CpuWrite(uint16_t addr, uint8_t data);
	bool GetCurrentSprites(int scanline, uint8_t spriteSize, ObjectAttributeMemory out[8], int& spriteCount, bool& sprite0Loaded, std::array<bool, 64>& currentSpriteNumbers) const;
	uint8_t ReadOAM() const;
	void SetOAMAddr(uint8_t addr);
	void WriteOAM(uint8_t data);
	void DrawPixel(int x, int y, uint8_t c);
	Savestate SaveState() const;
	bool LoadState(Savestate& state);
	bool masterBg = true;
	bool masterFg = true;
	bool masterGreyscale = false;
	bool hideBorder = false;
	int audioSampleRate = 44100;

	void Clock();
	void ClockCpuInstruction();
	void ClockFrame();

	const std::vector<uint8_t>& GetScreenBuffer() const;
	std::vector<int16_t> TakeAudioSamples();
	const std::vector<uint8_t>& GetSram() const;
	const std::string GetCartridgeName() const;
	bool HasCartridgeLoaded() const;

	static std::vector<uint8_t> ToRgba(const std::vector<uint8_t>& indices);
private:
	int emuStep = 0;
	int clockNumber = 0;
	std::shared_ptr<Cartridge> cart;
	std::unique_ptr<Cpu> cpu;
	std::unique_ptr<Ppu> ppu;
	std::shared_ptr<Apu> apu;
	uint8_t ram[0x800];
	std::unique_ptr<NesController> controllers[2];
	uint8_t controllerLatch = 0;
	std::vector<uint8_t> screenBuffer;
	inline static NullInputSource<NesKey> NULL_INPUT_SOURCE;

	// Sprites
	void ClockDMA();
	ObjectAttributeMemory oam[64];
	uint8_t oamAddr = 0;
	uint16_t dmaAddr = 0;
	uint8_t dmaData = 0;
	bool dmaReady = false;
	bool dmaMode = false;
};

struct NesInterface : private Nes {
	using Nes::SaveState;
	using Nes::LoadState;

	using Nes::Reset;
	using Nes::InsertCartridge;
	using Nes::SetController;
	using Nes::ClockFrame;

	using Nes::GetScreenBuffer;
	using Nes::TakeAudioSamples;
	using Nes::GetSram;
	using Nes::GetCartridgeName;
	using Nes::HasCartridgeLoaded;

	using Nes::audioSampleRate;
	using Nes::masterBg;
	using Nes::masterFg;
	using Nes::masterGreyscale;
	using Nes::hideBorder;

	using Nes::ToRgba;
};
