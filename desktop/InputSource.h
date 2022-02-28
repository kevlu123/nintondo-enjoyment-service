#pragma once
#include <vector>
#include <stdexcept>

template <class Key>
struct InputSource {
	virtual ~InputSource() = default;
	virtual bool GetKey(Key key) const = 0;
};

template <class Key>
struct NullInputSource : InputSource<Key> {
	bool GetKey(Key key) const { return false; }
};

template <class Key>
struct AggregateInputSource : public InputSource<Key> {

	virtual ~AggregateInputSource() = default;

	bool GetKey(Key key) const {
		for (auto src : sources)
			if (src->GetKey(key))
				return true;
		return false;
	}

	void AddSource(const InputSource<Key>* source) {
		sources.push_back(source);
	}

	void RemoveSource(const InputSource<Key>* source) {
		auto it = std::find(sources.begin(), sources.end(), source);
		if (it == sources.end())
			throw std::logic_error("Attempted to remove an input source which does not exist.");
		sources.erase(it);
	}

private:
	std::vector<const InputSource<Key>*> sources;
};

struct CursorPos {
	int x;
	int y;
};

template <class Key>
struct ExtendedInputSource : public InputSource<Key> {
	virtual ~ExtendedInputSource() = default;

	virtual bool GetKeyDown(Key key) const = 0;
	virtual bool GetKeyUp(Key key) const = 0;
	virtual bool GetAnyKeyDown(Key& key) const = 0;
	virtual std::vector<CursorPos> GetCursorPositions() const = 0;
};
