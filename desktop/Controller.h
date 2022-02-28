#pragma once
#include <string>
#include <vector>
#include "InputSource.h"
#include "Application.h"

enum class NesKey {
	A,
	B,
	Start,
	Select,
	Up,
	Down,
	Left,
	Right,
};

struct NesController { 
	NesController(const InputSource<NesKey>& input);
	void SetState();
	uint8_t Read();
	void Update();
	std::vector<uint8_t> replay;
	std::vector<uint8_t> history;
private:
	const InputSource<NesKey>& input;
	uint8_t state = 0;
};

struct LocalNesInput : public InputSource<NesKey> {
	LocalNesInput(const InputSource<Key>& input);
	bool GetKey(NesKey key) const;

	bool enabled = false;
	std::string name = "Controller";
	bool port[2]{};
	Key keyA = Key{};
	Key keyB = Key{};
	Key keyStart = Key{};
	Key keySelect = Key{};
	Key keyUp = Key{};
	Key keyDown = Key{};
	Key keyLeft = Key{};
	Key keyRight = Key{};

private:
	const InputSource<Key>& input;
};

struct NetworkServer;

struct NetworkNesInput : public InputSource<NesKey> {
	NetworkNesInput(int slot);
	void SetServer(const NetworkServer* server);
	bool GetKey(NesKey key) const;
private:
	const int slot;
	const NetworkServer* server = nullptr;
};
