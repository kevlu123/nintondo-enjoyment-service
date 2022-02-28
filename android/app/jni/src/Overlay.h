#pragma once
#include <string>
#include <memory>
#include "Application.h"
#include "Stopwatch.h"

struct Overlay {

	Overlay(const Application& app);
	void Update();
	void Draw() const;
	void Notify(std::string text);

	bool showFps = false;
	size_t fps = 0;

	bool showNetwork = false;
	size_t netReadSpeed = 0;
	size_t netWriteSpeed = 0;

private:
	std::unique_ptr<Texture> topTex;

	std::unique_ptr<Texture> notifyTex;
	std::string notifyText;
	Stopwatch<> notifyTimer;
};
