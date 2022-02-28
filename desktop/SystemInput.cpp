#include "SystemInput.h"
#include <unordered_map>
#include <vector>

static const std::unordered_map<SystemKey, std::vector<Key>> KEY_MAPPINGS = {
	{ SystemKey::Up, { Key::Keyboard(SDL_SCANCODE_UP) }},
	{ SystemKey::Down, { Key::Keyboard(SDL_SCANCODE_DOWN) }},
	{ SystemKey::Left, { Key::Keyboard(SDL_SCANCODE_LEFT) }},
	{ SystemKey::Right, { Key::Keyboard(SDL_SCANCODE_RIGHT) }},
	{ SystemKey::Enter, { Key::Keyboard(SDL_SCANCODE_RETURN) }},
	{ SystemKey::Back, { Key::Keyboard(SDL_SCANCODE_ESCAPE), Key::Keyboard(SDL_SCANCODE_AC_BACK) }},
};

SystemInput::SystemInput(const ExtendedInputSource<Key>& source) :
	source(source) {
}

bool SystemInput::GetKey(SystemKey key) const {
	for (auto k : KEY_MAPPINGS.at(key))
		if (source.GetKey(k))
			return true;
	return false;
}

bool SystemInput::GetKeyDown(SystemKey key) const {
	for (auto k : KEY_MAPPINGS.at(key))
		if (source.GetKeyDown(k))
			return true;
	return false;
}

bool SystemInput::GetKeyUp(SystemKey key) const {
	for (auto k : KEY_MAPPINGS.at(key))
		if (source.GetKeyUp(k))
			return true;
	return false;
}

bool SystemInput::GetAnyKeyDown(SystemKey& key) const {
	for (auto p : KEY_MAPPINGS)
		for (auto k : p.second)
			if (source.GetKeyDown(k)) {
				key = p.first;
				return true;
			}
	return false;
}

std::vector<CursorPos> SystemInput::GetCursorPositions() const {
	return source.GetCursorPositions();
}
