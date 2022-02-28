#include "Application.h"

bool Key::operator==(const Key& rhs) const {
	if (type != rhs.type)
		return false;

	switch (type) {
	case Key::Type::Null:
		return true;
	case Key::Type::Keyboard:
		return keyboard == rhs.keyboard;
	case Key::Type::Mouse:
		return mouse == rhs.mouse;
	case Key::Type::Controller:
		return joyID == rhs.joyID && controller == rhs.controller;
	case Key::Type::Joy:
		return joyID == rhs.joyID && joy == rhs.joy;
	default:
		throw std::logic_error("Invalid Key::Type enum value");
	}
}

std::string Key::Name() const {
	switch (type) {
	case Key::Type::Null:
		return "Null";
	case Key::Type::Keyboard:
		return SDL_GetScancodeName(keyboard);
	case Key::Type::Mouse:
		switch (mouse) {
		case SDL_BUTTON_LEFT: return "Left Click";
		case SDL_BUTTON_MIDDLE: return "Middle Click";
		case SDL_BUTTON_RIGHT: return "Right Click";
		case SDL_BUTTON_X1: return "Mouse X1";
		case SDL_BUTTON_X2: return "Mouse X2";
		default: return "Mouse X?";
		}
	case Key::Type::Controller:
		return std::string(SDL_GameControllerGetStringForButton(controller))
			+ ':'
			+ std::to_string(joyID);
	case Key::Type::Joy:
		return std::to_string(joy)
			+ ':'
			+ std::to_string(joyID);
	default:
		throw std::logic_error("Invalid Key::Type enum value");
	};
}

char Key::ToChar(bool shift) const {
	if (keycode >= SDLK_a && keycode <= SDLK_z) {
		if (shift)
			return 'A' + (keycode - SDLK_a);
		else
			return 'a' + (keycode - SDLK_a);
	} else if (keycode >= SDLK_0 && keycode <= SDLK_9) {
		if (shift)
			return ")!@#$%^&*("[keycode - SDLK_0];
		else
			return '0' + (keycode - SDLK_0);
	} else {
		switch (keycode) {
		case SDLK_SPACE: return ' ';
		case SDLK_COMMA: return shift ? '<' : ',';
		case SDLK_PERIOD: return shift ? '>' : '.';
		default: return '\0';
		}
	}
}

Key Key::Keyboard(SDL_Scancode k, SDL_Keycode c) {
	Key key{};
	key.type = Key::Type::Keyboard;
	key.keyboard = k;
	key.keycode = c;
	return key;
}

Key Key::Mouse(uint8_t k) {
	Key key{};
	key.type = Key::Type::Mouse;
	key.mouse = k;
	return key;
}

void Application::SetCommandLine(int argc, char** argv) {
	cmdArgs.clear();
	for (int i = 0; i < argc; i++)
		cmdArgs.push_back(argv[i]);
}

void Application::Run() {

	if (SDL_Init(SDL_INIT_EVERYTHING))
		goto cleanup;

	if (SDL_CreateWindowAndRenderer(
		1280,
		720,
		SDL_WINDOW_RESIZABLE,
		&win,
		&ren))
		goto cleanup;

	if (SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND))
		goto cleanup;

	{
		fullscreen = false;
		Stopwatch<> stopwatch;
		float curTime = stopwatch.Time();
		float t = 0.0f;
		int fps = 0;

		if (!OnStart())
			goto cleanup;

		SDL_SetWindowTitle(win, title.c_str());
		while (PollEvents()) {
			float dt = stopwatch.Time() - curTime;
			curTime += dt;
			t += dt;
			if ((int)(curTime - dt) != (int)curTime) {
#if !defined(NDEBUG) && !defined(__ANDROID__)
				std::string winTitle = title + " - " + std::to_string(fps) + "fps";
				SDL_SetWindowTitle(win, winTitle.c_str());
#endif
				this->fps = fps;
				fps = 0;
			}

			while (t > 1.0f / targetFps) {
				fps++;
				SDL_RenderClear(ren);
				if (!OnUpdate())
					goto cleanup;
				SDL_RenderPresent(ren);
				UpdateInput();

				if (targetFps) {
					do {
						t -= 1.0f / targetFps;
					} while (t > 3.0f / targetFps);
				}
			}

			SDL_Delay(1);
		}

	}

cleanup:
	OnQuit();
	SetHapticsEnabled(false);
	if (audioDevice) SDL_CloseAudioDevice(audioDevice.value());
	if (ren) SDL_DestroyRenderer(ren);
	if (win) SDL_DestroyWindow(win);
	SDL_Quit();
}

int Application::GetClientWidth() {
	int x;
	SDL_GetWindowSize(win, &x, nullptr);
	return x;
}

int Application::GetClientHeight() {
	int x;
	SDL_GetWindowSize(win, nullptr, &x);
	return x;
}

void Application::SetFullscreenState(bool enabled) {
	fullscreen = enabled;
	SDL_SetWindowFullscreen(win, enabled ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
}

bool Application::GetFullscreenState() const {
	return fullscreen;
}

int Application::GetFps() const {
	return fps;
}

const std::string& Application::GetAppdataPath() const {

	if (appdataPath.empty()) {
#if defined(__ANDROID__)
		appdataPath = "/storage/emulated/0/Documents/" + applicationName + "/";
#else
		if (char* p = SDL_GetBasePath()) {
			appdataPath = p;
			SDL_free(p);
			std::replace(appdataPath.begin(), appdataPath.end(), '\\', '/');
		}
#endif
	}

	return appdataPath;
}

void Application::InfoMessage(const std::string& caption, const std::string& msg, const Application* owner) {
	SDL_ShowSimpleMessageBox(
		SDL_MESSAGEBOX_INFORMATION,
		caption.c_str(),
		msg.c_str(),
		nullptr
	);
}

void Application::ErrorMessage(const std::string& msg, const Application* owner) {
	SDL_ShowSimpleMessageBox(
		SDL_MESSAGEBOX_ERROR,
		"Error",
		msg.c_str(),
		owner ? owner->win : nullptr
	);
}

bool Application::GetKey(Key key) const {
	if (!inputEnabled)
		return false;
	if (keys.find(key) == keys.end())
		return false;
	return keys.at(key) == KeyState::JustHeld
		|| keys.at(key) == KeyState::Held;
}

bool Application::GetKeyDown(Key key) const {
	if (!inputEnabled)
		return false;
	if (keys.find(key) == keys.end())
		return false;
	return keys.at(key) == KeyState::JustHeld;
}

bool Application::GetKeyUp(Key key) const {
	if (!inputEnabled)
		return false;
	if (keys.find(key) == keys.end())
		return false;
	return keys.at(key) == KeyState::JustReleased;
}

void Application::UpdateInput() {
	for (auto& k : keys) {
		if (k.second == KeyState::JustHeld)
			k.second = KeyState::Held;
		else if (k.second == KeyState::JustReleased)
			k.second = KeyState::NotHeld;
	}

	eventQueue = std::move(eventQueueBuffer);
}

bool Application::GetAnyKeyDown(Key& key) const {
	if (!inputEnabled)
		return false;

	for (const auto& p : keys)
		if (p.second == KeyState::JustHeld) {
			key = p.first;
			return true;
		}
	return false;
}

std::vector<CursorPos> Application::GetCursorPositions() const {
	if (!inputEnabled)
		return {};

	std::vector<CursorPos> p;
		
#if !defined(__ANDROID__)
	CursorPos pos;
	SDL_GetMouseState(&pos.x, &pos.y);
	p.push_back(pos);
#endif

	for (const auto& pair : touches)
		p.push_back(pair.second);

	return p;
}

void Application::SetInputEnabled(bool enabled) {
	inputEnabled = enabled;
}

bool Application::PollEvents() {

	auto pollEvent = [&](SDL_Event* ev) {
		if (eventQueue.size()) {
			*ev = eventQueue.front();
			eventQueue.pop();
			return 1;
		} else {
			return SDL_PollEvent(ev);
		}
	};

	SDL_Event ev;
	while (pollEvent(&ev)) {

		auto registerKey = [&](const Key& k, bool keydown) {
			if (keydown) {
				keys[k] = KeyState::JustHeld;
			} else {
				if (GetKeyDown(k)) {
					eventQueueBuffer.push(ev);
				} else {
					keys[k] = KeyState::JustReleased;
				}
			}
		};

		switch (ev.type) {

		case SDL_QUIT: {
			return false;
		}

		case SDL_APP_WILLENTERBACKGROUND:
			OnSuspend();
			break;

		case SDL_KEYDOWN:
		case SDL_KEYUP: {
			Key k{};
			k.type = Key::Type::Keyboard;
			k.keyboard = ev.key.keysym.scancode;
			k.keycode = ev.key.keysym.sym;
			registerKey(k, ev.type == SDL_KEYDOWN);
			break;
		}

		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP: {
			Key k{};
			k.type = Key::Type::Mouse;
			k.mouse = ev.button.button;
			registerKey(k, ev.type == SDL_MOUSEBUTTONDOWN);
			break;
		}

		case SDL_CONTROLLERBUTTONDOWN:
		case SDL_CONTROLLERBUTTONUP: {
			Key k{};
			k.type = Key::Type::Controller;
			k.joyID = ev.cbutton.which;
			k.controller = (SDL_GameControllerButton)ev.cbutton.button;
			registerKey(k, ev.type == SDL_CONTROLLERBUTTONDOWN);
			break;
		}

		case SDL_JOYBUTTONDOWN:
		case SDL_JOYBUTTONUP: {
			Key k{};
			k.type = Key::Type::Controller;
			k.joyID = ev.jbutton.which;
			k.joy = ev.jbutton.button;
			registerKey(k, ev.type == SDL_JOYBUTTONDOWN);
			break;
		}

		case SDL_FINGERDOWN: {
			int x = (int)(ev.tfinger.x * GetClientWidth());
			int y = (int)(ev.tfinger.y * GetClientHeight());
			touches[ev.tfinger.fingerId] = { x, y };
			break;
		}

		case SDL_FINGERUP: {
			if (GetKeyDown(Key::Mouse())) {
				eventQueueBuffer.push(ev);
			} else {
				touches.erase(ev.tfinger.fingerId);
			}
			break;
		}

		case SDL_FINGERMOTION: {
			int x = (int)(ev.tfinger.x * GetClientWidth());
			int y = (int)(ev.tfinger.y * GetClientHeight());
			touches.at(ev.tfinger.fingerId) = { x, y };
			break;
		}
		}
	}

	return true;
}

Texture::Texture(const Application& app, int width, int height) :
	win(app.win),
	ren(app.ren),
	width(width),
	height(height)
{
	tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, width, height);
	SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
}

Texture::~Texture() {
	if (tex)
		SDL_DestroyTexture(tex);
}

int Texture::GetWidth() const {
	return width;
}

int Texture::GetHeight() const {
	return height;
}

float Texture::GetAspectRatio() const {
	return (float)width / (float)height;
}

void Texture::ScreenToTexPos(int x, int y, int& outX, int& outY) const {
	int winW, winH;
	SDL_GetWindowSize(win, &winW, &winH);

	SDL_Rect rc{};
	rc.x = pos.x ? pos.x(winW, winH) : 0;
	rc.y = pos.y ? pos.y(winW, winH) : 0;
	rc.w = size.x ? size.x(winW, winH) : winW;
	rc.h = size.y ? size.y(winW, winH) : winH;

	x -= rc.x;
	y -= rc.y;
	outX = x * width / rc.w;
	outY = y * height / rc.h;
}

void Texture::SetPixels(const void* data) {
	if (!tex)
		return;

	void* pixels = nullptr;
	int pitch = 0;
	SDL_LockTexture(tex, nullptr, &pixels, &pitch);

	for (int row = 0; row < height; row++) {
		std::memcpy(
			(uint8_t*)pixels + row * pitch,
			(const uint8_t*)data + row * GetWidth() * 4,
			pitch
		);
	}

	SDL_UnlockTexture(tex);
}

void Texture::Draw() const {
	int cw, ch;
	SDL_GetWindowSize(win, &cw, &ch);

	SDL_Rect rc{};
	rc.w = cw;
	rc.h = ch;

	if (pos.x) rc.x = pos.x(cw, ch);
	if (pos.y) rc.y = pos.y(cw, ch);
	if (size.x) rc.w = size.x(cw, ch);
	if (size.y) rc.h = size.y(cw, ch);

	SDL_RenderCopy(ren, tex, nullptr, &rc);
}

void Application::DrawBackground(float r, float g, float b) const {
	SDL_SetRenderDrawColor(
		ren,
		(uint8_t)(r * 0xFF),
		(uint8_t)(g * 0xFF),
		(uint8_t)(b * 0xFF),
		0xFF
	);
}

void Application::EnqueueAudio(const std::vector<int16_t>& samples) {
	std::unique_lock<std::mutex> lock(audioMtx);

	if (!audioDevice) {
		// Initialize audio for the first time
		SDL_AudioSpec spec{};
		spec.size = 512 * sizeof(int16_t);
		spec.freq = AUDIO_SAMPLE_RATE;
		spec.format = AUDIO_S16LSB;
		spec.silence = 0;
		spec.channels = 1;
		spec.padding = 0;
		spec.callback = AudioCallback;
		spec.userdata = this;

		SDL_AudioSpec obtainedSpec{};
		audioDevice = SDL_OpenAudioDevice(
			nullptr,
			false,
			&spec,
			&obtainedSpec,
			0
		);

		if (!audioDevice)
			return;

		// Unpause audio
		SDL_PauseAudioDevice(audioDevice.value(), 0);
	}

	audioBuffer.insert(
		audioBuffer.end(),
		samples.begin(),
		samples.end()
	);
}

void Application::AudioCallback(void* userdata, uint8_t* stream, int len) {
	Application* self = (Application*)userdata;
	std::unique_lock<std::mutex> lock(self->audioMtx);

	auto& buffer = self->audioBuffer;
	size_t consumeLen = std::min((size_t)len / 2, buffer.size());

	if (buffer.size()) {
		for (int i = 0; i < len; i += 2) {
			float m = (float)i / len;
			int16_t sample = buffer[(size_t)(m * consumeLen)];
			stream[i + 0] = sample & 0xFF;
			stream[i + 1] = sample >> 8;
		}
		self->lastAudioSample = buffer.back();
	} else {
		for (int i = 0; i < len; i += 2) {
			stream[i + 0] = self->lastAudioSample & 0xFF;
			stream[i + 1] = self->lastAudioSample >> 8;
		}
	}

	buffer.erase(
		buffer.begin(),
		buffer.begin() + consumeLen
	);
}

void Application::SetHapticsEnabled(bool enabled) {
	if (enabled == (bool)haptic)
		return;

	if (enabled) {

		if (!(haptic = SDL_HapticOpen(0)))
			return;

		if (SDL_HapticRumbleInit(haptic) != 0) {
			SDL_HapticClose(haptic);
			haptic = nullptr;
			return;
		}

	} else {
		SDL_HapticClose(haptic);
		haptic = nullptr;
	}
}

void Application::HapticRumble(uint32_t milliseconds, float strength) const {
	if (haptic) {
		SDL_HapticRumblePlay(haptic, strength, milliseconds);
	}
}
