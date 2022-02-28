#pragma once
#include "InputSource.h"
#include "Application.h"

enum class SystemKey {
	Left, Right, Up, Down,
	Enter, Back,
};

struct SystemInput : public ExtendedInputSource<SystemKey> {
	SystemInput(const ExtendedInputSource<Key>& source);
	bool GetKey(SystemKey k) const;
	bool GetKeyDown(SystemKey key) const;
	bool GetKeyUp(SystemKey key) const;
	bool GetAnyKeyDown(SystemKey& key) const;
	std::vector<CursorPos> GetCursorPositions() const;
private:
	const ExtendedInputSource<Key>& source;
};
