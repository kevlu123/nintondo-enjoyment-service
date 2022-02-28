#pragma once
#include <memory>
#include "Application.h"
#include "Controller.h"

struct OnScreenController : InputSource<NesKey> {
	OnScreenController(const Application& app);
	void Update();
	void Draw() const;
	bool GetKey(NesKey key) const;

	bool enabled = false;
	bool vibrate = true;
	bool port[2]{};

	int size = 70;
	int pos = 40;
	float Size() const { return size / 100.0f * 0.5f + 0.05f; }
	float Pos()  const { return pos  / 100.0f * 0.2f; }
private:
	const Application& app;
	std::vector<Colour> leftPx;
	std::vector<Colour> rightPx;
	std::unique_ptr<Texture> leftTex;
	std::unique_ptr<Texture> rightTex;
	bool texInit = false;
};

struct OnScreenNumpad {
	OnScreenNumpad(const Application& app);
	void Update();
	void Draw() const;
	bool GetKeyAnyDown(Key& key) const;
	bool enabled = false;
private:
	const ExtendedInputSource<Key>& input;
	std::unique_ptr<Texture> tex;
};
