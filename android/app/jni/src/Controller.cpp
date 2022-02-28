#include "Controller.h"
#include "Network.h"

NesController::NesController(const InputSource<NesKey>& input) :
	input(input)
{
}

void NesController::Update() {
	history.push_back(state);
	if (replay.size()) {
		state |= replay.back();
		replay.pop_back();
	}
}

void NesController::SetState() {
	state |= input.GetKey(NesKey::Right) << 0;
	state |= input.GetKey(NesKey::Left) << 1;
	state |= input.GetKey(NesKey::Down) << 2;
	state |= input.GetKey(NesKey::Up) << 3;
	state |= input.GetKey(NesKey::Start) << 4;
	state |= input.GetKey(NesKey::Select) << 5;
	state |= input.GetKey(NesKey::B) << 6;
	state |= input.GetKey(NesKey::A) << 7;
}

uint8_t NesController::Read() {
	uint8_t res = state >> 7;
	state <<= 1;
	return res;
}

LocalNesInput::LocalNesInput(const InputSource<Key>& input) :
	input(input) {
}

bool LocalNesInput::GetKey(NesKey key) const {
	switch (key) {
	case NesKey::A:      return input.GetKey(keyA);
	case NesKey::B:      return input.GetKey(keyB);
	case NesKey::Start:  return input.GetKey(keyStart);
	case NesKey::Select: return input.GetKey(keySelect);
	case NesKey::Up:     return input.GetKey(keyUp);
	case NesKey::Down:   return input.GetKey(keyDown);
	case NesKey::Left:   return input.GetKey(keyLeft);
	case NesKey::Right:  return input.GetKey(keyRight);
	default: throw std::logic_error("Invalid NesKey enum value.");
	}
}

NetworkNesInput::NetworkNesInput(int slot) :
	slot(slot)
{
}

void NetworkNesInput::SetServer(const NetworkServer* server) {
	this->server = server;
}

bool NetworkNesInput::GetKey(NesKey key) const {
	return server ? server->GetKey(slot, key) : false;
}
