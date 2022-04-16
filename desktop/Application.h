#pragma once
#include <vector>
#include <deque>
#include <string>
#include <queue>
#include <unordered_map>
#include <mutex>
#include <functional>
#include <optional>
#include <cstring>
#include "Stopwatch.h"
#include "InputSource.h"

#if defined(__ANDROID__)
#include "SDL.h"
#else
#include "SDL2/SDL.h"
#endif

struct Key {
	enum class Type { Null, Keyboard, Mouse, Controller, Joy } type;
	SDL_JoystickID joyID;
	SDL_Keycode keycode;
	union {
		SDL_Scancode keyboard;
		uint8_t mouse;
		SDL_GameControllerButton controller;
		uint8_t joy;
	};

	bool operator==(const Key& rhs) const;
	std::string Name() const;
	char ToChar(bool shift = false) const;
	static Key Keyboard(SDL_Scancode k, SDL_Keycode = 0);
	static Key Mouse(uint8_t k = SDL_BUTTON_LEFT);
};

struct Colour {
	uint8_t r, g, b, a;
	bool operator==(const Colour& rhs) const { return std::memcmp(this, &rhs, sizeof(Colour)) == 0; }
	static constexpr Colour Transparent() { return Colour{ 0, 0, 0, 0 }; }
	static constexpr Colour Black() { return Colour{ 0, 0, 0, 255 }; }
	static constexpr Colour DarkGrey() { return Colour{ 64, 64, 64, 255 }; }
	static constexpr Colour Grey()  { return Colour{ 128, 128, 128, 255 }; }
	static constexpr Colour LightGrey() { return Colour{ 192, 192, 192, 255 }; }
	static constexpr Colour White() { return Colour{ 255, 255, 255, 255 }; }
};

namespace std {
	template <>
	struct hash<Key> {
		std::size_t operator()(const Key& k) const {
			switch (k.type) {
			case Key::Type::Null:       return Hash(k.type);
			case Key::Type::Keyboard:   return Hash(k.type) ^ Hash(k.keyboard);
			case Key::Type::Mouse:      return Hash(k.type) ^ Hash(k.mouse);
			case Key::Type::Controller: return Hash(k.type) ^ Hash(k.joyID) ^ Hash(k.controller);
			case Key::Type::Joy:        return Hash(k.type) ^ Hash(k.joyID) ^ Hash(k.joy);
			default: throw std::logic_error("Invalid Key::Type enum value.");
			}
		}

		template <class T>
		static std::size_t Hash(T v) {
			return std::hash<std::remove_reference_t<T>>()(v);
		}
	};
}

struct Application : public ExtendedInputSource<Key> {

	void SetCommandLine(int argc, char** argv);
	const std::vector<std::string>& GetCommandLineArgs() const { return cmdArgs; }

	virtual bool OnStart() { return true; }
	virtual bool OnUpdate() { return true; }
	virtual void OnQuit() {}
	virtual void OnSuspend() {}
	
	void Run();
	int GetClientWidth();
	int GetClientHeight();
	void SetFullscreenState(bool enabled);
	bool GetFullscreenState() const;
	int GetFps() const;
	const std::string& GetAppdataPath() const;
	static void InfoMessage(const std::string& caption, const std::string& msg, const Application* owner = nullptr);
	static void ErrorMessage(const std::string& msg, const Application* owner = nullptr);

	static constexpr int AUDIO_SAMPLE_RATE = 44100;

	std::string title;
	int targetFps = 60;
	std::string applicationName = "Application";
private:
	SDL_Window* win = nullptr;
	SDL_Renderer* ren = nullptr;
	bool fullscreen;
	std::vector<std::string> cmdArgs;
	mutable std::string appdataPath;
	int fps = 0;

	// Keyboard
public:
	bool GetKey(Key key) const;
	bool GetKeyDown(Key key) const;
	bool GetKeyUp(Key key) const;
	bool GetAnyKeyDown(Key& key) const;
	std::vector<CursorPos> GetCursorPositions() const;
	void SetInputEnabled(bool enabled);
private:
	void UpdateInput();
	bool PollEvents();
	enum class KeyState { NotHeld, JustHeld, Held, JustReleased };
	std::unordered_map<Key, KeyState> keys;
	std::unordered_map<SDL_FingerID, CursorPos> touches;
	bool inputEnabled = true;
	std::queue<SDL_Event> eventQueue;
	std::queue<SDL_Event> eventQueueBuffer;

	// Graphics
public:
	void DrawBackground(float r, float g, float b) const;
	friend struct Texture;

	// Audio
public:
	void EnqueueAudio(const std::vector<int16_t>& samples);
private:
	static void AudioCallback(void* userdata, uint8_t* stream, int len);
	std::deque<int16_t> audioBuffer;
	bool audioStarved = false;
	int16_t lastAudioSample = 0;
	std::mutex audioMtx;
	std::optional<SDL_AudioDeviceID> audioDevice;

	// Haptics
public:
	void SetHapticsEnabled(bool enabled);
	void HapticRumble(uint32_t milliseconds = 20, float strength = 0.5f) const;
private:
	SDL_Haptic* haptic = nullptr;
};

struct Texture {
	Texture(const Application& app, int width, int height);
	~Texture();

	int GetWidth() const;
	int GetHeight() const;
	float GetAspectRatio() const;
	void ScreenToTexPos(int x, int y, int& outX, int& outY) const;
	void SetPixels(const void* data);
	void Draw() const;

	struct {
		std::function<int(int, int)> x, y;
	} pos, size;

private:
	int width;
	int height;
	SDL_Texture* tex = nullptr;
	SDL_Window* win = nullptr;
	SDL_Renderer* ren = nullptr;
};
