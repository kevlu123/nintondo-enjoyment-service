#pragma once
#include <cstdint>
#include <vector>
#include "Savestate.h"

enum class MirrorMode : int32_t
{
	Hardwired,
	Horizontal,
	Vertical,
	OneScreenLo,
	OneScreenHi,
};

class Mapper
{
public:
	virtual ~Mapper() = default;
	virtual Savestate SaveState() const;
	int MapperNumber() const;

	virtual bool MapCpuRead(uint16_t& addr, uint8_t& data, bool readonly = false);
	virtual bool MapCpuWrite(uint16_t& addr, uint8_t data);
	virtual bool MapPpuRead(uint16_t& addr, uint8_t& data, bool readonly = false);
	virtual bool MapPpuWrite(uint16_t& addr, uint8_t data);
	virtual void Reset();
	virtual MirrorMode GetMirrorMode() const;
	virtual bool GetIrq() const;
	virtual void ClearIrq();
	virtual void CountScanline();
	const std::vector<uint8_t>& GetSram() const;
	void SetSram(std::vector<uint8_t> data);
protected:
	Mapper(int mapperNumber, int prgChunks, int chrChunks, std::vector<uint8_t>& prg, std::vector<uint8_t>& chr);
	Mapper(Savestate& state, std::vector<uint8_t>& prg, std::vector<uint8_t>& chr);
	bool MapCpuRead(uint32_t addr, uint8_t& data);
	bool MapCpuWrite(uint32_t addr, uint8_t data);
	std::vector<uint8_t>& prg;
	std::vector<uint8_t>& chr;
	int mapperNumber;
	int prgChunks;
	int chrChunks;
	std::vector<uint8_t> sram;
	size_t sramSize = 0;
};
