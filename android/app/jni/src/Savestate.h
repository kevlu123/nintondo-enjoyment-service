#pragma once
#include <vector>
#include <cstdint>
#include <deque>
#include <limits>
#include <cstring>
#include "InvalidFileException.h"

static_assert(std::numeric_limits<float>::is_iec559, "float must be IEC 559 (IEEE 754).");
static_assert(sizeof(float) == 4, "float must be 4 bytes.");

struct Savestate {

	Savestate() {}
	Savestate(const std::vector<uint8_t>& val) : buf(val.begin(), val.end()) {}

	size_t size() const {
		return buf.size();
	}

	std::vector<uint8_t> GetBuffer() const {
		return std::vector<uint8_t>(buf.begin(), buf.end());
	}

	template <class T>
	void Push(std::common_type_t<T> val) {
		buf.insert(
			buf.end(),
			(const uint8_t*)&val,
			(const uint8_t*)&val + sizeof(T)
		);
	}

	void PushSize(size_t val) {
		Push<uint32_t>((uint32_t)val);
	}

	void PushFloat(float val) {
		uint32_t i;
		std::memcpy(&i, &val, sizeof(float));
		Push<uint32_t>(i);
	}

	void PushString(const std::string& val) {
		PushSize(val.size());
		PushArray((const uint8_t*)val.data(), val.size());
	}

	void PushArray(const void* p, size_t len) {
		buf.insert(
			buf.end(),
			(const uint8_t*)p,
			(const uint8_t*)p + len
		);
	}

	void PushVec(const std::vector<uint8_t>& val) {
		PushSize(val.size());
		PushArray(val.data(), val.size());
	}

	void PushState(const Savestate& val) {
		buf.insert(
			buf.end(),
			val.buf.begin(),
			val.buf.end()
		);
	}

	template <class T>
	T Pop() {
		if (buf.size() < sizeof(T))
			throw InvalidFileException("Invalid savestate.");

		T out{};
		std::copy(
			buf.begin(),
			buf.begin() + sizeof(T),
			(uint8_t*)&out
		);
		buf.erase(buf.begin(), buf.begin() + sizeof(T));
		return out;
	}

	size_t PopSize() {
		return (size_t)Pop<uint32_t>();
	}

	float PopFloat() {
		uint32_t i = Pop<uint32_t>();
		float f;
		std::memcpy(&f, &i, sizeof(float));
		return f;
	}

	std::string PopString() {
		return PopContainer<std::string>();
	}

	void PopArray(void* p, size_t len) {
		if (buf.size() < len)
			throw InvalidFileException("Invalid savestate.");

		std::copy(
			buf.begin(),
			buf.begin() + len,
			(uint8_t*)p
		);
		buf.erase(buf.begin(), buf.begin() + len);
	}

	std::vector<uint8_t> PopVec() {
		return PopContainer<std::vector<uint8_t>>();
	}

private:

	template <class C>
	C PopContainer(size_t maxLen = 512 * 1024 * 1024) {
		size_t len = PopSize();
		if (len > maxLen)
			throw InvalidFileException("Invalid savestate.");
		C out;
		out.resize(len);
		PopArray(out.data(), len);
		return out;
	}

	std::deque<uint8_t> buf;
};
