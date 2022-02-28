#pragma once
#include <array>
#include <vector>
#include "Application.h"

struct Character {
	Character(char c = ' ');
	bool GetPixel(int x, int y) const;
	static Character Checkmark();
	static Character UpArrow();
	static Character DownArrow();

	void DrawChar(
		std::vector<Colour>& buf,
		size_t imageWidth,
		size_t x,
		size_t y,
		const Colour& fg = Colour::White(),
		const Colour& bg = Colour::Transparent()) const;
	void DrawChar(
			std::vector<uint8_t>& buf,
			size_t imageWidth,
			size_t x,
			size_t y,
			const Colour& fg = Colour::White(),
			const Colour& bg = Colour::Transparent()) const;
	static void DrawString(
			std::vector<Colour>& buf,
			size_t imageWidth,
			size_t x,
			size_t y,
			const std::string& s,
			const Colour& fg = Colour::White(),
			const Colour& bg = Colour::Transparent());
	static void DrawString(
		std::vector<uint8_t>& buf,
		size_t imageWidth,
		size_t x,
		size_t y,
		const std::string& s,
		const Colour& fg = Colour::White(),
		const Colour& bg = Colour::Transparent());

private:
	const std::array<uint8_t, 8>* chr;
	static void Init();
	inline static bool initialized = false;
	inline static std::vector<std::array<uint8_t, 8>> font;
};
