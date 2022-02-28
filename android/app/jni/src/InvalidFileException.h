#pragma once
#include <stdexcept>
#include <string>

struct InvalidFileException : public std::runtime_error {
	InvalidFileException(const char* msg) : runtime_error(msg) {}
	InvalidFileException(const std::string& msg) : runtime_error(msg) {}
};
