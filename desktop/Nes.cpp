#include "Nes.h"

struct NesColour {
	uint8_t r, g, b;
};

static const NesColour NTSC_PALETTE[64] = {
	NesColour{ 84,    84,     84 },
	NesColour{ 0,     30,     116 },
	NesColour{ 8,     16,     144 },
	NesColour{ 48,    0,      136 },
	NesColour{ 68,    0,      100 },
	NesColour{ 92,    0,      48 },
	NesColour{ 84,    4,      0 },
	NesColour{ 60,    24,     0 },
	NesColour{ 32,    42,     0 },
	NesColour{ 8,     58,     0 },
	NesColour{ 0,     64,     0 },
	NesColour{ 0,     60,     0 },
	NesColour{ 0,     50,     60 },
	NesColour{ 0,     0,      0 },
	NesColour{ 0,     0,      0 },
	NesColour{ 0,     0,      0 },
	NesColour{ 152,   150,    152 },
	NesColour{ 8,     76,     196 },
	NesColour{ 48,    50,     236 },
	NesColour{ 92,    30,     228 },
	NesColour{ 136,   20,     176 },
	NesColour{ 160,   20,     100 },
	NesColour{ 152,   34,     32 },
	NesColour{ 120,   60,     0 },
	NesColour{ 84,    90,     0 },
	NesColour{ 40,    114,    0 },
	NesColour{ 8,     124,    0 },
	NesColour{ 0,     118,    40 },
	NesColour{ 0,     102,    120 },
	NesColour{ 0,     0,      0 },
	NesColour{ 0,     0,      0 },
	NesColour{ 0,     0,      0 },
	NesColour{ 236,   238,    236 },
	NesColour{ 76,    154,    236 },
	NesColour{ 120,   124,    236 },
	NesColour{ 176,   98,     236 },
	NesColour{ 228,   84,     236 },
	NesColour{ 236,   88,     180 },
	NesColour{ 236,   106,    100 },
	NesColour{ 212,   136,    32 },
	NesColour{ 160,   170,    0 },
	NesColour{ 116,   196,    0 },
	NesColour{ 76,    208,    32 },
	NesColour{ 56,    204,    108 },
	NesColour{ 56,    180,    204 },
	NesColour{ 60,    60,     60 },
	NesColour{ 0,     0,      0 },
	NesColour{ 0,     0,      0 },
	NesColour{ 236,   238,    236 },
	NesColour{ 168,   204,    236 },
	NesColour{ 188,   188,    236 },
	NesColour{ 212,   178,    236 },
	NesColour{ 236,   174,    236 },
	NesColour{ 236,   174,    212 },
	NesColour{ 236,   180,    176 },
	NesColour{ 228,   196,    144 },
	NesColour{ 204,   210,    120 },
	NesColour{ 180,   222,    120 },
	NesColour{ 168,   226,    144 },
	NesColour{ 152,   226,    180 },
	NesColour{ 160,   214,    228 },
	NesColour{ 160,   162,    160 },
	NesColour{ 0,     0,      0 },
	NesColour{ 0,     0,      0 },
};

Nes::Nes() :
	ram{},
	oam{}
{
	screenBuffer.resize(Ppu::DRAWABLE_WIDTH * Ppu::DRAWABLE_HEIGHT, 0xF);

	controllers[0] = std::make_unique<NesController>(NULL_INPUT_SOURCE);
	controllers[1] = std::make_unique<NesController>(NULL_INPUT_SOURCE);
}

bool Nes::LoadState(Savestate& state) {

	auto GetFilenameFromPath = [](const std::string& path) {
		return path.substr(path.find_last_of("/\\") + 1);
	};

	Savestate backup;
	bool restore = (bool)cart;
	if (restore) {
		backup = SaveState();
	}

	try {
		cpu = std::make_unique<Cpu>(*this, state);
		cart = std::make_shared<Cartridge>(state);
		ppu = std::make_unique<Ppu>(*this, cart.get(), state);
		apu = std::make_shared<Apu>(*this, state);

		state.PopArray(ram, sizeof(ram));
		state.PopArray(oam, sizeof(oam));

		oamAddr = state.Pop<uint8_t>();
		dmaAddr = state.Pop<uint16_t>();
		dmaData = state.Pop<uint8_t>();
		dmaReady = state.Pop<uint8_t>();
		dmaMode = state.Pop<uint8_t>();
		controllerLatch = state.Pop<uint8_t>();
		clockNumber = state.Pop<int32_t>();

		if (state.size())
			throw InvalidFileException("Invalid savestate (trailing data).");

		return true;

	} catch (InvalidFileException&) {
		if (restore)
			LoadState(backup);
		return false;
	}
}

Savestate Nes::SaveState() const {
	if (!cart)
		throw std::logic_error("Attempted to save without a cartridge loaded.");

	Savestate state;
	state.PushState(cpu->SaveState());
	state.PushState(cart->SaveState());
	state.PushState(ppu->SaveState());
	state.PushState(apu->SaveState());

	state.PushArray(ram, sizeof(ram));
	state.PushArray(oam, sizeof(oam));

	state.Push<uint8_t>(oamAddr);
	state.Push<uint16_t>(dmaAddr);
	state.Push<uint8_t>(dmaData);
	state.Push<uint8_t>(dmaReady);
	state.Push<uint8_t>(dmaMode);
	state.Push<uint8_t>(controllerLatch);
	state.Push<int32_t>(clockNumber);
	return state;
}

void Nes::Reset() {
	cart->Reset();
	cpu->Reset();
	ppu->Reset();
	apu->Reset();
}

void Nes::InsertCartridge(std::unique_ptr<Cartridge> cart) {
	if (cart) {
		this->cart = std::move(cart);

		cpu = std::make_unique<Cpu>(*this);
		ppu = std::make_unique<Ppu>(*this, this->cart.get());
		apu = std::make_unique<Apu>(*this);

		clockNumber = 0;
		std::memset(ram, 0, std::size(ram));

		this->cart->Reset();
		ppu->Reset();
		apu->Reset();
	} else {
		cpu.reset();
		ppu.reset();
		apu.reset();
		this->cart.reset();
	}
}

void Nes::SetController(int port, std::unique_ptr<NesController> controller) {
	if (port != 0 && port != 1)
		throw std::logic_error("Invalid controller port.");
	controllers[port] = std::move(controller);
}

void Nes::UnplugController(int port) {
	SetController(port, nullptr);
}

void Nes::Clock() {
	ppu->Clock();
	apu->Clock();
	if (clockNumber == 3 || clockNumber == 6) {
		if (clockNumber == 6)
			clockNumber = 0;
		if (dmaMode)
			ClockDMA();
		else
			cpu->Clock();
	}
	clockNumber++;

	if (ppu->CheckNmi())
		cpu->Nmi();

	bool irq = false;
	if (cart->GetMapper().GetIrq()) {
		cart->GetMapper().ClearIrq();
		irq = true;
	}
	if (apu->GetIrq())
		irq = true;

	if (irq)
		cpu->Irq();

	if (controllerLatch & 1)
		for (auto& controller : controllers)
			controller->SetState();
}

void Nes::ClockCpuInstruction() {
	do {
		Clock();
	} while (!cpu->InstructionComplete());
	do {
		Clock();
	} while (cpu->InstructionComplete());
}

void Nes::ClockFrame() {
	do {
		Clock();
	} while (!ppu->IsBeginningFrame());
}

void Nes::CpuWrite(uint16_t addr, uint8_t data) {
	if (cart->CpuWrite(addr, data)) {
	} else if (addr < 0x2000) {
		addr &= 0x7FF;
		ram[addr] = data;
	} else if (addr >= 0x2000 && addr < 0x4000) {
		ppu->WriteFromCpu(addr, data);
	} else if ((addr >= 0x4000 && addr < 0x4014) || addr == 0x4015 || addr == 0x4017) {
		apu->WriteFromCpu(addr, data);
	} else if (addr == 0x4014) {
		dmaAddr = data << 8;
		dmaMode = true;
	} else if (addr == 0x4016) {
		controllerLatch = data & 1;
	}
}

uint8_t Nes::CpuRead(uint16_t addr, bool readonly) {
	uint8_t data;
	if (cart->CpuRead(addr, data, readonly)) {
	} else if (addr < 0x2000) {
		addr &= 0x7FF;
		data = ram[addr];
	} else if (addr >= 0x2000 && addr < 0x4000) {
		data = ppu->ReadFromCpu(addr, readonly);
	} else if (addr == 0x4015) {
		data = apu->ReadFromCpu(addr, readonly);
	} else if (addr == 0x4016 || addr == 0x4017) {
		data = controllers[addr & 1]->Read() + 0x40;
	}
	return data;
}

void Nes::ClockDMA() {
	if (!dmaReady) {
		if (clockNumber % 2 == 1)
			dmaReady = true;
	} else {
		if (clockNumber % 2 == 0) {
			dmaData = CpuRead(dmaAddr);
		} else {
			reinterpret_cast<uint8_t*>(oam)[dmaAddr & 0xFF] = dmaData;
			dmaAddr++;
			if ((dmaAddr & 0xFF) == 0) {
				dmaMode = false;
				dmaReady = false;
			}
		}
	}
}

bool Nes::GetCurrentSprites(int scanline, uint8_t spriteSize, ObjectAttributeMemory out[8], int& spriteCount, bool& sprite0Loaded, std::array<bool, 64>& currentSpriteNumbers) const {
	spriteCount = 0;
	sprite0Loaded = false;
	std::memset(out, 0xFF, 32);
	if (scanline >= Ppu::DRAWABLE_HEIGHT)
		return false;

	for (int i = 0; i < 64; i++) {
		int difference = scanline - oam[i].y;
		if (difference >= 0 && difference < 8 * (spriteSize + 1)) {
			if (spriteCount < 8) {
				if (i == 0)
					sprite0Loaded = true;
				out[spriteCount] = oam[i];
				spriteCount++;

				currentSpriteNumbers[i] = true;
			} else {
				return true;
			}
		}
	}
	return false;
}

uint8_t Nes::ReadOAM() const {
	return reinterpret_cast<const uint8_t*>(oam)[oamAddr];
}

void Nes::SetOAMAddr(uint8_t addr) {
	oamAddr = addr;
}

void Nes::WriteOAM(uint8_t data) {
	reinterpret_cast<uint8_t*>(oam)[oamAddr] = data;
	oamAddr++;
}

void Nes::DrawPixel(int x, int y, uint8_t c) {
	if (hideBorder && (x < 8 || y < 8 || x >= Ppu::DRAWABLE_WIDTH - 8 || y >= Ppu::DRAWABLE_HEIGHT - 8)) {
		c = 0xF;
	} if (masterGreyscale) {
		c &= 0x30;
	}
	screenBuffer[y * Ppu::DRAWABLE_WIDTH + x] = c;
}

std::vector<uint8_t> Nes::ToRgba(const std::vector<uint8_t>& indices) {
	std::vector<uint8_t> rgba(4 * indices.size(), 0xFF);
	for (size_t i = 0; i < indices.size(); i++) {
		auto c = NTSC_PALETTE[indices[i] % 64];
		rgba[4 * i + 0] = c.r;
		rgba[4 * i + 1] = c.g;
		rgba[4 * i + 2] = c.b;
	}
	return rgba;
}

const std::vector<uint8_t>& Nes::GetScreenBuffer() const {
	return screenBuffer;
}

std::vector<int16_t> Nes::TakeAudioSamples() {
	if (apu) {
		return apu->TakeSamples();
	} else {
		return {};
	}
}

const std::vector<uint8_t>& Nes::GetSram() const {
	if (!cart)
		throw std::logic_error("Attempted to access SRAM without a cartridge loaded.");
	return cart->GetSram();
}

const std::string Nes::GetCartridgeName() const {
	if (!cart)
		throw std::logic_error("Attempted to access cartridge name without a cartridge loaded.");
	return cart->GetCartridgeName();
}

bool Nes::HasCartridgeLoaded() const {
	return (bool)cart;
}
