#pragma once
#include <memory>
#include <vector>
#include "Savestate.h"

class FrameCounter;
class PulseChannel;
class TriangleChannel;
class NoiseChannel;
class Nes;

class Apu {
public:
	Apu(Nes& nes);
	Apu(Nes& nes, Savestate& bytes);
	Savestate SaveState() const;
	void Reset();
	void Clock();
	uint8_t ReadFromCpu(uint16_t cpuAddress, bool readonly = false);
	void WriteFromCpu(uint16_t cpuAddress, uint8_t value);
	bool GetIrq() const;
	std::vector<int16_t> TakeSamples();
private:
	friend class FrameCounter;
	int16_t Sample() const;
	Nes& nes;
	bool enabled = false;
	bool evenFrame = true;
	float channelVolumes[4];
	int clockNumber = 0;
	float time = 0.0f;
	std::vector<int16_t> samples;
	std::shared_ptr<FrameCounter> frameCounter;
	std::shared_ptr<PulseChannel> pulseChannel1;
	std::shared_ptr<PulseChannel> pulseChannel2;
	std::shared_ptr<TriangleChannel> triangleChannel;
	std::shared_ptr<NoiseChannel> noiseChannel;
};
