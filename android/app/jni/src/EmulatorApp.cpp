#if defined(__ANDROID__)
#include "filesystem.h"
namespace fs = ghc::filesystem;
#else
#include <filesystem>
namespace fs = std::filesystem;
#endif

#include <fstream>
#include "EmulatorApp.h"
#include "Nes.h"
#include "json.h"
#include "Font.h"

constexpr size_t MAX_RECENT_ROMS = 10;
constexpr size_t MAX_CONTROLLERS = 8;

static bool ReadFromFile(const std::string& path, std::vector<uint8_t>& data) {
	data.clear();

	std::ifstream f(path, std::ios::binary | std::ios::ate);
	if (!f.is_open())
		return false;

	size_t len = f.tellg();
	f.seekg(0);
	data.resize(len);
	f.read((char*)data.data(), len);

	if (f.fail() || f.bad()) {
		data.clear();
		return false;
	} else {
		return true;
	}
}

static bool WriteToFile(const std::string& path, const std::vector<uint8_t>& data) {
	std::ofstream f(path, std::ios::binary);
	if (!f.is_open())
		return false;

	f.write((const char*)data.data(), data.size());
	return !f.fail() && !f.bad();
}

static void CreateFolder(const std::string& path) {
	try {
		fs::create_directory(path);
	} catch (std::exception&) { }
}

static bool FileExists(const std::string& path) {
	try {
		return fs::exists(path);
	} catch (std::exception&) {
		return false;
	}
}

template <class F>
static void IterateDirectory(const std::string& path, F f) {
	try {
		for (const auto& entry : fs::directory_iterator(path)) {
			if (entry.is_regular_file()) {
				f(entry.path().string());
			}
		}
	} catch (std::exception&) {}
}

static std::string PadLeft(int x, size_t len) {
	auto s = std::to_string(x);
	while (s.size() < len)
		s.insert(s.begin(), ' ');
	return s;
}

static std::string PadRight(int x, size_t len) {
	auto s = std::to_string(x);
	while (s.size() < len)
		s.push_back(' ');
	return s;
}

static std::string GetFilenameWithoutExtension(std::string name) {
	// Extract filename from path
	size_t slashIndex = name.find_last_of('/');
	if (slashIndex != std::string::npos)
		name = name.substr(slashIndex + 1);

	// Remove extension
	size_t dotIndex = name.find_last_of('.');
	name = name.substr(0, dotIndex);
	return name;
}

bool EmulatorApp::OnStart() {

	applicationName = "Nintondo Enjoyment Service";
	title = applicationName;
	targetFps = 60;

	systemInput = std::make_unique<SystemInput>(*this);
	onScreenNumpad = std::make_unique<OnScreenNumpad>(*this);

	menu = std::make_unique<Menu>(*this);
	overlay = std::make_unique<Overlay>(*this);

	screen = std::make_unique<Texture>(*this, 256, 240);
	float aspect = screen->GetAspectRatio();
	screen->pos.x = [=](int w, int h) {
		if ((float)w / h > aspect)
			return (w - int(h * aspect)) / 2;
		else
			return 0;
	};
	screen->pos.y = [=](int w, int h) {
		if ((float)w / h < aspect)
			return (h - int(w /  aspect)) / 2;
		else
			return 0;
	};
	screen->size.x = [=](int w, int h) {
		if ((float)w / h > aspect)
			return int(h * aspect);
		else
			return w;
	};
	screen->size.y = [=](int w, int h) {
		if ((float)w / h < aspect)
			return int(w / aspect);
		else
			return h;
	};

	{
		standbyScreen = std::make_unique<Texture>(*this, screen->GetWidth(), screen->GetHeight());
		standbyScreen->pos = screen->pos;
		standbyScreen->size = screen->size;
		std::vector<uint8_t> standbyScreenPx(Ppu::DRAWABLE_WIDTH * Ppu::DRAWABLE_HEIGHT);
		for (int x = 0; x < Ppu::DRAWABLE_WIDTH; x++) {
			for (int y = 0; y < Ppu::DRAWABLE_HEIGHT; y++) {
				standbyScreenPx[y * Ppu::DRAWABLE_WIDTH + x] = y / 22 + 0x31;
			}
		}
		std::vector<uint8_t> standbyScreenColours = NesInterface::ToRgba(standbyScreenPx);
#if defined(__ANDROID__)
		std::string t = "Press Back";
#else
		std::string t = "Press Escape";
#endif
		Character::DrawString(
				standbyScreenColours,
				screen->GetWidth(),
				(256 - t.size() * 8) / 2,
				113,
				t,
				Colour::White(),
				Colour{0xEC, 0xB4, 0xB0, 0xFF}
		);
		Character::DrawString(
				standbyScreenColours,
				screen->GetWidth(),
				64,
				121,
				"to Open the Menu",
				Colour::White(),
				Colour{0xEC, 0xB4, 0xB0, 0xFF}
		);
		standbyScreen->SetPixels(standbyScreenColours.data());
	}

	nes = std::make_unique<NesInterface>();
	nes->audioSampleRate = Application::AUDIO_SAMPLE_RATE;

	onScreenController = std::make_unique<OnScreenController>(*this);
	for (int i = 0; i < 2; i++) {
		auto netInput = std::make_unique<NetworkNesInput>(i);

		auto agg = std::make_unique<AggregateInputSource<NesKey>>();
		agg->AddSource(netInput.get());

		nes->SetController(i, std::make_unique<NesController>(*agg));

		networkNesInputs[i] = std::move(netInput);
		aggregatedNesInputs[i] = std::move(agg);
	}

#if defined(__ANDROID__)
	SetFullscreenState(true);
	SetHapticsEnabled(true);
#endif

	LoadIni();
	UpdateMenu();
	
	if (GetCommandLineArgs().size() >= 2) {
		LoadRom(GetCommandLineArgs()[1]);
	}

	return true;
}

bool EmulatorApp::OnUpdate() {

	// Check keyboard shortcuts
	SetInputEnabled(true);
	bool alt = GetKey(Key::Keyboard(SDL_SCANCODE_LALT)) || GetKey(Key::Keyboard(SDL_SCANCODE_RALT));
#if defined(__APPLE__)
	bool ctrl = GetKey(Key::Keyboard(SDL_SCANCODE_LGUI)) || GetKey(Key::Keyboard(SDL_SCANCODE_RGUI));
#else
	bool ctrl = GetKey(Key::Keyboard(SDL_SCANCODE_LCTRL)) || GetKey(Key::Keyboard(SDL_SCANCODE_RCTRL));
#endif

	if (alt) {
#if !defined(__ANDROID__)
		if (GetKeyDown(Key::Keyboard(SDL_SCANCODE_RETURN))) {
			ToggleFullscreen();
			SetInputEnabled(false);
		}
#endif

		if (nes->HasCartridgeLoaded())
			for (int i = 0; i < 10; i++)
				if (GetKeyDown(Key::Keyboard((SDL_Scancode)(SDL_SCANCODE_1 + i)))) {
					SaveState(i);
					SetInputEnabled(false);
				}
	}
	if (ctrl) {
		if (GetKeyDown(Key::Keyboard(SDL_SCANCODE_G))) {
			ToggleOnScreenInputEnabled();
			SetInputEnabled(false);
		}

		if (nes->HasCartridgeLoaded())
			if (GetKeyDown(Key::Keyboard(SDL_SCANCODE_W))) {
				CloseRom();
				SetInputEnabled(false);
			}

		if (nes->HasCartridgeLoaded())
			if (GetKeyDown(Key::Keyboard(SDL_SCANCODE_R))) {
				nes->Reset();
				SetInputEnabled(false);
			}

		if (GetKeyDown(Key::Keyboard(SDL_SCANCODE_M))) {
			ToggleMute();
			SetInputEnabled(false);
		}

		if (GetKeyDown(Key::Keyboard(SDL_SCANCODE_P))) {
			TogglePaused();
			SetInputEnabled(false);
		}

		if (!client)
			for (int i = 0; i < 10; i++)
				if (GetKeyDown(Key::Keyboard((SDL_Scancode)(SDL_SCANCODE_1 + i)))) {
					LoadState(i);
					SetInputEnabled(false);
				}
	}

	// Update menu
	auto prevMenuState = menuState;
	switch (menuState) {
	case MenuState::None:
	case MenuState::Menu:
		break;
	case MenuState::KeyBindEdit:
		OnUpdateEditKeyBind();
		break;
	case MenuState::ServerPortEdit:
		OnUpdateEditField(serverPort);
		break;
	case MenuState::ConnectIPEdit:
		OnUpdateEditField(connectIP, [&](bool backspace) {
			if (std::all_of(connectIP.begin(), connectIP.end(), [](char c) { return std::isdigit(c) || c == '.'; })) {
				static_assert(std::string::npos == (size_t)-1);
				size_t dotIndex = connectIP.rfind('.');
				if (connectIP.size() - dotIndex == 4) {
					if (backspace) {
						connectIP.pop_back();
					} else if (connectIP.size() <= 12) {
						connectIP.push_back('.');
					}
				}
			}
			});
		break;
	case MenuState::ConnectPortEdit:
		OnUpdateEditField(connectPort);
		break;
	case MenuState::ControllerNameEdit:
		OnUpdateEditField(controllerNameToBeChanged->name);
		break;
	case MenuState::VolumeEdit:
		OnUpdateEditSlider(volume, 0, 100, 10);
		break;
	case MenuState::OnScreenSizeEdit:
		OnUpdateEditSlider(onScreenController->size, 0, 100, 5);
		break;
	case MenuState::OnScreenPosEdit:
		OnUpdateEditSlider(onScreenController->pos, 0, 100, 5);
		break;
	default:
		throw std::logic_error("Invalid EmulatorApp::MenuState enum value.");
	}
	if (prevMenuState == MenuState::None || prevMenuState == MenuState::Menu) {
		if (systemInput->GetKeyDown(SystemKey::Back)) {
			if (menu->Toggle()) {
				menuState = MenuState::Menu;
				UpdateMenu();
			} else {
				menuState = MenuState::None;
			}
		} else if (systemInput->GetKeyDown(SystemKey::Up)) {
			menu->MoveUp();
		} else if (systemInput->GetKeyDown(SystemKey::Down)) {
			menu->MoveDown();
		} else if (systemInput->GetKeyDown(SystemKey::Left)) {
			menu->MoveLeft();
		} else if (systemInput->GetKeyDown(SystemKey::Right)) {
			menu->MoveRight();
		} else if (systemInput->GetKeyDown(SystemKey::Enter)) {
			if (!menu->Submit())
				menuState = MenuState::None;
		}
	}
	for (const auto& pos : GetCursorPositions()) {
		if (GetKeyDown(Key::Mouse())) {
			bool wasSlider = menu->GetEnabled() && menu->GetSelectedNode()->slider;
			if (!menu->Click(pos.x, pos.y, &sliderChange)) {
				menuState = MenuState::None;
			} else if (!sliderChange && wasSlider) {
				menuState = MenuState::Menu;
				UpdateMenu();
			}
		} else if (GetKey(Key::Mouse())) {
			menu->MouseHeld(pos.x, pos.y);
		}
	}
	menu->cursorPositions = GetCursorPositions();
	menu->Update();

	onScreenController->Update();
	onScreenNumpad->Update();

	// Update overlay
	overlay->fps = (size_t)GetFps();
	overlay->netReadSpeed = 0;
	overlay->netWriteSpeed = 0;
	if (server) {
		overlay->netReadSpeed += server->GetReadSpeed();
		overlay->netWriteSpeed += server->GetWriteSpeed();
	}
	if (client) {
		overlay->netReadSpeed += client->GetReadSpeed();
		overlay->netWriteSpeed += client->GetWriteSpeed();
	}
	overlay->Update();

	// Update emulation
	std::vector<int16_t> audioSamples;
	if (!client && nes->HasCartridgeLoaded()) {
		if (menuState != MenuState::None)
			SetInputEnabled(false);

		float step[] = {
			0.125f, 0.25f, 0.5f,
			1.0f, 2.0f, 4.0f, 8.0f,
		};

		emulationStep += step[emulationSpeed] * float(!paused);
		while (emulationStep >= 1.0f) {
			emulationStep -= 1.0f;
			nes->ClockFrame();
		}
		screenBuffer = &nes->GetScreenBuffer();
		audioSamples = nes->TakeAudioSamples();

		auto rgba = NesInterface::ToRgba(*screenBuffer);
		screen->SetPixels(rgba.data());

		SetInputEnabled(true);
	}

	// Update network
	if (server) {
		server->Update(*screenBuffer);
		if (audioSamples.size())
			server->SendAudio(audioSamples);
	}
	if (client) {
		client->Update();

		if (menuState != MenuState::None)
			SetInputEnabled(false);

		InputState is{};
		for (int i = 0; i < 2; i++) {
			is.port[i].keyA = aggregatedNesInputs[i]->GetKey(NesKey::A);
			is.port[i].keyB = aggregatedNesInputs[i]->GetKey(NesKey::B);
			is.port[i].keyStart = aggregatedNesInputs[i]->GetKey(NesKey::Start);
			is.port[i].keySelect = aggregatedNesInputs[i]->GetKey(NesKey::Select);
			is.port[i].keyUp = aggregatedNesInputs[i]->GetKey(NesKey::Up);
			is.port[i].keyDown = aggregatedNesInputs[i]->GetKey(NesKey::Down);
			is.port[i].keyLeft = aggregatedNesInputs[i]->GetKey(NesKey::Left);
			is.port[i].keyRight = aggregatedNesInputs[i]->GetKey(NesKey::Right);
		}
		client->SendInput(is);
		SetInputEnabled(true);

		if (client->GetFrame(screenBuffer)) {
			auto rgba = NesInterface::ToRgba(*screenBuffer);
			screen->SetPixels(rgba.data());
		}

		audioSamples = client->TakeAudioSamples();

		if (!client->IsConnected()) {
			client.reset();
			UpdateMenu();
			ErrorMessage("Connection to the server was closed.", this);
		}
	}

	// Draw graphics
	if (nes->HasCartridgeLoaded() || (bool)client) {
		screen->Draw();
	} else {
		standbyScreen->Draw();
	}
	overlay->Draw();
	menu->Draw();
	onScreenController->Draw();
	onScreenNumpad->Draw();

	// Queue audio
	if (!mute && volume != 0) {
		if (volume != 100) {
			std::transform(
				audioSamples.begin(),
				audioSamples.end(),
				audioSamples.begin(),
				[&](int16_t s) { return int16_t(s * volume / 100.0f); }
			);
		}
		EnqueueAudio(audioSamples);
	}

	return !quit;
}

void EmulatorApp::OnQuit() {
	OnSuspend();
}

void EmulatorApp::OnSuspend() {
	SaveSram();
	SaveIni();
}

void EmulatorApp::SaveSram() const {
	if (nes->HasCartridgeLoaded()) {
		const auto& sram = nes->GetSram();
		if (sram.size()) {
			CreateFolder(GetAppdataPath() + "Sram");
			WriteToFile(
				GetAppdataPath() + "Sram/" + nes->GetCartridgeName() + ".sram",
				sram
			);
		}
	}
}

static std::string ChangeToEditMode(std::string s, size_t len) {
	s += '_';
	if (s.size() > len)
		return s.substr(s.size() - len);
	else
		return s;
}

void EmulatorApp::UpdateMenu() {

	menu->root.children.clear();
	MenuNode first{};
	MenuNode second{};
	MenuNode third{};
	auto pushFirst = [&] { menu->root.children.push_back(std::move(first)); };
	auto pushSecond = [&] { first.children.push_back(std::move(second)); };
	auto pushThird = [&] { second.children.push_back(std::move(third)); };

	first = MenuNode{};
	first.text = "Back";
	first.onclick = [&] { menu->Toggle(); menuState = MenuState::None; };
	pushFirst();

	first = MenuNode{};
	first.enabled = !client;
	first.text = "File"; {

		second = MenuNode{};
		second.text = "Open ROM"; {
			for (const auto& filename : GetRomList()) {
				third = MenuNode{};
				third.text = GetFilenameWithoutExtension(filename);
				third.onclick = [=] { LoadRom(filename); };
				third.closeAfterClick = true;
				pushThird();
			}
		}
		pushSecond();

		second = MenuNode{};
		second.text = "Open Recent"; {
			for (const auto& filename : recentRoms) {
				third = MenuNode{};
				third.text = GetFilenameWithoutExtension(filename);
				third.onclick = [=] { LoadRom(filename); };
				third.closeAfterClick = true;
				pushThird();
			}
		}
		pushSecond();

		second = MenuNode{};
		second.text = "Close ROM";
		second.onclick = [&] { CloseRom(); };
		second.closeAfterClick = true;
		second.enabled = nes->HasCartridgeLoaded();
		second.shortcut = "Ctrl+W";
		pushSecond();

		second = MenuNode{};
		second.text = "Save SRAM";
		second.onclick = [&] { SaveSram(); };
		second.closeAfterClick = true;
		second.enabled = nes->HasCartridgeLoaded();
		pushSecond();

		second = MenuNode{};
		second.enabled = nes->HasCartridgeLoaded();
		second.text = "Save State"; {

			for (int i = 0; i < 10; i++) {
				third = MenuNode{};
				third.text = "Slot " + std::to_string(i + 1);
				third.onclick = [=] { SaveState(i); };
				third.closeAfterClick = true;
				third.shortcut = std::string("Alt+") + "1234567890"[i];
				pushThird();
			}
		}
		pushSecond();

		second = MenuNode{};
		second.text = "Load State"; {

			for (int i = 0; i < 10; i++) {
				third = MenuNode{};
				third.text = "Slot " + std::to_string(i + 1);
				third.onclick = [=] { LoadState(i); };
				third.enabled = SaveStateExists(i);
				third.shortcut = std::string("Ctrl+") + "1234567890"[i];
				third.closeAfterClick = true;
				pushThird();
			}
		}
		pushSecond();

		second = MenuNode{};
		second.text = "About";
		second.onclick = [&] { DisplayAboutDialog(); };
		pushSecond();
	}
	pushFirst();

	first = MenuNode{};
	first.enabled = !client;
	first.text = "Emulation"; {

		second = MenuNode{};
		second.text = "Reset";
		second.onclick = [&] { nes->Reset(); };
		second.closeAfterClick = true;
		second.enabled = nes->HasCartridgeLoaded();
		second.shortcut = "Ctrl+R";
		pushSecond();

		second = MenuNode{};
		second.text = "Pause";
		second.onclick = [&] { paused = !paused; };
		second.checked = paused;
		second.checkMode = CheckMode::Checkable;
		second.shortcut = "Ctrl+P";
		pushSecond();

		second = MenuNode{};
		second.text = "Speed"; {

			const char* text[] = {
					"x1/8", "x1/4", "x1/2",
					"x1", "x2", "x4", "x8",
			};

			for (int i = 0; i < 7; i++) {
				third = MenuNode{};
				third.text = text[i];
				third.onclick = [=] { emulationSpeed = i; };
				third.checked = i == emulationSpeed;
				third.checkMode = CheckMode::Radio;
				pushThird();
			}
		}
		pushSecond();

		second = MenuNode{};
		second.text = "Disable BG";
		second.onclick = [&] { nes->masterBg = !nes->masterBg; };
		second.checked = !nes->masterBg;
		second.checkMode = CheckMode::Checkable;
		pushSecond();

		second = MenuNode{};
		second.text = "Disable FG";
		second.onclick = [&] { nes->masterFg = !nes->masterFg; };
		second.checked = !nes->masterFg;
		second.checkMode = CheckMode::Checkable;
		pushSecond();

		second = MenuNode{};
		second.text = "Greyscale";
		second.onclick = [&] { nes->masterGreyscale = !nes->masterGreyscale; };
		second.checked = nes->masterGreyscale;
		second.checkMode = CheckMode::Checkable;
		pushSecond();

		second = MenuNode{};
		second.text = "Hide Border";
		second.onclick = [&] { nes->hideBorder = !nes->hideBorder; };
		second.checked = nes->hideBorder;
		second.checkMode = CheckMode::Checkable;
		pushSecond();
	}
	pushFirst();

	first = MenuNode{};
	first.text = "Controllers"; {
		
		for (size_t i = 0; i < localNesInputs.size(); i++) {
			LocalNesInput* input = localNesInputs[i].get();

			second = MenuNode{};
			second.text = input->name; {

				third = MenuNode{};
				if (menuState == MenuState::ControllerNameEdit) {
					third.text = "Name      " + ChangeToEditMode(input->name, 12);
				} else {
					third.text = "Name      " + input->name;
				}
#if !defined(__ANDROID__)
				third.onclick = [=] { BeginControllerNameEdit(input); };
#endif
				pushThird();

				third = MenuNode{};
				third.text = "Enabled";
				third.onclick = [=] { input->enabled = !input->enabled; };
				third.checked = input->enabled;
				third.checkMode = CheckMode::Checkable;
				pushThird();
				
				for (int j = 0; j < 2; j++) {
					third = MenuNode{};
					third.text = "Player " + std::to_string(j + 1);
					third.onclick = [=] { ToggleLocalController(input, j); };
					third.checked = input->port[j];
					third.checkMode = CheckMode::Checkable;
					pushThird();
				}

				auto addEntry = [&](const std::string& text, Key& key) {
					std::string keyName = key.type == Key::Type::Null ? "Press Key" : key.Name();
					third = MenuNode{};
					third.text = text + keyName;
					third.onclick = std::bind(&EmulatorApp::BeginKeyBindEdit, this, &key);
					pushThird();
				};

				addEntry("Up        ", input->keyUp);
				addEntry("Down      ", input->keyDown);
				addEntry("Left      ", input->keyLeft);
				addEntry("Right     ", input->keyRight);
				addEntry("B         ", input->keyB);
				addEntry("A         ", input->keyA);
				addEntry("Start     ", input->keyStart);
				addEntry("Select    ", input->keySelect);

				third = MenuNode{};
				third.text = "Reset To Default";
				third.onclick = [=] { ResetNesInput(input); UpdateMenu(); };
				pushThird();

				third = MenuNode{};
				third.text = "Delete";
				third.onclick = [=] {
					localNesInputs.erase(localNesInputs.begin() + i);
					menu->MoveLeft();
					UpdateMenu();
				};
				third.enabled = localNesInputs.size() >= 2;
				pushThird();
			}
			pushSecond();
		}

		second = MenuNode{};
		second.text = "Add Controller";
		second.onclick = [&] { AddNesInput(); UpdateMenu(); };
		second.enabled = localNesInputs.size() < MAX_CONTROLLERS;
		pushSecond();

		second = MenuNode{};
		second.text = "Onscreen Input"; {

			third = MenuNode{};
			third.text = "Enabled";
			third.onclick = [&] { ToggleOnScreenInputEnabled(); };
			third.checked = onScreenController->enabled;
			third.checkMode = CheckMode::Checkable;
			third.shortcut = "Ctrl+G";
			pushThird();

			for (int i = 0; i < 2; i++) {
				third = MenuNode{};
				third.text = "Player " + std::to_string(i + 1);
				third.onclick = [=] { ToggleOnScreenInput(i); };
				third.checked = onScreenController->port[i];
				third.checkMode = CheckMode::Checkable;
				pushThird();
			}

#if defined(__ANDROID__)
			third = MenuNode{};
            third.text = "Vibrate";
            third.onclick = [&] { onScreenController->vibrate = !onScreenController->vibrate; };
			third.checked = onScreenController->vibrate;
			third.checkMode = CheckMode::Checkable;
			pushThird();
#endif
			third = MenuNode{};
			if (menuState == MenuState::OnScreenSizeEdit) {
				third.text = "Size     <" + PadLeft(onScreenController->size, 3) + ">";
			} else {
				third.text = "Size      " + PadLeft(onScreenController->size, 3);
			}
			third.onclick = [&] { BeginOnScreenSizeEdit(); };
			third.slider = true;
			pushThird();

			third = MenuNode();
			if (menuState == MenuState::OnScreenPosEdit) {
				third.text = "Position <" + PadLeft(onScreenController->pos, 3) + ">";
			} else {
				third.text = "Position  " + PadLeft(onScreenController->pos, 3);
			}
			third.onclick = [&] { BeginOnScreenPosEdit(); };
			third.slider = true;
			pushThird();
		}
		pushSecond();

	}
	pushFirst();

	first = MenuNode{};
	first.text = "Network"; {

		second = MenuNode{};
		second.text = "Host"; {

			third = MenuNode{};
			if (menuState == MenuState::ServerPortEdit) {
				third.text = "Port  " + ChangeToEditMode(serverPort, 16);
			} else {
				third.text = "Port  " + serverPort;
			}
			third.enabled = !server;
			third.onclick = [&] { BeginServerPortEdit(); };
			pushThird();

			third = MenuNode{};
			third.text = server ? "Stop" : "Start";
			third.onclick = [&] { ToggleServer(); };
			pushThird();

		}
		pushSecond();

		second = MenuNode{};
		second.text = "Join"; {

			third = MenuNode{};
			if (menuState == MenuState::ConnectIPEdit) {
				third.text = "IP    " + ChangeToEditMode(connectIP, 16);
			} else {
				third.text = "IP    " + connectIP;
			}
			third.enabled = !client;
			third.onclick = [&] { BeginConnectIPEdit(); };
			pushThird();

			third = MenuNode{};
			if (menuState == MenuState::ConnectPortEdit) {
				third.text = "Port  " + ChangeToEditMode(connectPort, 16);
			} else {
				third.text = "Port  " + connectPort;
			}
			third.enabled = !client;
			third.onclick = [&] { BeginConnectPortEdit(); };
			pushThird();

			third = MenuNode{};
			third.text = client ? "Disconnect" : "Connect";
			third.onclick = [&] { ToggleClient(); };
			pushThird();

		}
		pushSecond();
	}
	pushFirst();

	first = MenuNode{};
	first.text = "Options"; {

		second = MenuNode();
		if (menuState == MenuState::VolumeEdit) {
			second.text = "Volume<" + PadLeft(volume, 3) + ">";
		} else {
			second.text = "Volume " + PadLeft(volume, 3);
		}
		second.onclick = [&] { BeginVolumeEdit(); };
		second.slider = true;
		pushSecond();

		second = MenuNode{};
		second.text = "Mute";
		second.onclick = [&] { ToggleMute(); };
		second.checked = mute;
		second.checkMode = CheckMode::Checkable;
		second.shortcut = "Ctrl+M";
		pushSecond();

		second = MenuNode{};
		second.text = "Show FPS";
		second.onclick = [&] { overlay->showFps = !overlay->showFps; };
		second.checked = overlay->showFps;
		second.checkMode = CheckMode::Checkable;
		pushSecond();

		second = MenuNode{};
		second.text = "Show Network";
		second.onclick = [&] { overlay->showNetwork = !overlay->showNetwork; };
		second.checked = overlay->showNetwork;
		second.checkMode = CheckMode::Checkable;
		pushSecond();

#if !defined(__ANDROID__)
		second = MenuNode{};
		second.text = "Fullscreen";
		second.onclick = [&] { ToggleFullscreen(); };
		second.checked = GetFullscreenState();
		second.checkMode = CheckMode::Checkable;
		second.shortcut = "Alt+Enter";
		pushSecond();
#endif
		/*second = MenuNode{};
		second.text = "Reset Settings";
		second.onclick = [&] { LoadIni(true); };
		second.closeAfterClick = true;
		pushSecond();*/
	}
	pushFirst();

	first = MenuNode{};
	first.text = "Quit";
	first.onclick = [&] { quit = true; };
	pushFirst();
}

static nlohmann::json KeyToJson(const Key& k) {
	auto j = nlohmann::json::object();
	j["type"] = (int)k.type;
	j["joy-id"] = k.joyID;
	switch (k.type) {
	case Key::Type::Null:
		break;
	case Key::Type::Keyboard:
		j["keyboard"] = (int)k.keyboard;
		j["keycode"] = (int)k.keycode;
		break;
	case Key::Type::Mouse:
		j["mouse"] = (int)k.mouse;
		break;
	case Key::Type::Joy:
		j["joy-id"] = (int)k.joyID;
		j["joy"] = (int)k.joy;
		break;
	case Key::Type::Controller:
		j["joy-id"] = (int)k.joyID;
		j["controller"] = (int)k.controller;
		break;
	default:
		throw std::logic_error("Invalid Key::Type enum value");
	}
	return j;
}

static bool JsonToKey(const nlohmann::json& j, Key& k) {
	try {

		auto getField = [&](const char* field) {
			if (!j.contains(field) || !j[field].is_number_integer())
				throw std::exception();
			return j[field].get<int>();
		};

		k.type = (Key::Type)getField("type");
		
		switch (k.type) {
		case Key::Type::Null:
			break;
		case Key::Type::Keyboard:
			k.keyboard = (SDL_Scancode)getField("keyboard");
			break;
		case Key::Type::Mouse:
			k.mouse = (uint8_t)getField("mouse");
			break;
		case Key::Type::Joy:
			k.joyID = (SDL_JoystickID)getField("joy-id");
			k.joy = (uint8_t)getField("joy");
			break;
		case Key::Type::Controller:
			k.joyID = (SDL_JoystickID)getField("joy-id");
			k.controller = (SDL_GameControllerButton)getField("joy");
			break;
		default:
			throw std::exception();
		}

		return true;
	} catch (std::exception&) {
		return false;
	}
}

void EmulatorApp::SaveIni() const {
	using nlohmann::json;
	auto j = json::object();

	j["mute"] = mute;
	j["volume"] = volume;
	j["fullscreen"] = GetFullscreenState();
	j["show-fps"] = overlay->showFps;
	j["show-network"] = overlay->showNetwork;
	j["paused"] = paused;
	j["speed"] = emulationSpeed;
	j["disable-bg"] = !nes->masterBg;
	j["disable-fg"] = !nes->masterFg;
	j["greyscale"] = nes->masterGreyscale;
	j["hide-border"] = nes->hideBorder;

	auto jrecentRoms = json::array();
	for (const auto& filename : recentRoms)
		jrecentRoms.push_back(filename);
	j["recent-roms"] = jrecentRoms;

	j["inputs"] = json::array();
	for (const auto& localInputs : localNesInputs) {
		auto k = json::object();
		k["name"] = localInputs->name;
		k["enabled"] = localInputs->enabled;
		k["player1"] = localInputs->port[0];
		k["player2"] = localInputs->port[1];
		k["key-a"] = KeyToJson(localInputs->keyA);
		k["key-b"] = KeyToJson(localInputs->keyB);
		k["key-start"] = KeyToJson(localInputs->keyStart);
		k["key-select"] = KeyToJson(localInputs->keySelect);
		k["key-up"] = KeyToJson(localInputs->keyUp);
		k["key-down"] = KeyToJson(localInputs->keyDown);
		k["key-left"] = KeyToJson(localInputs->keyLeft);
		k["key-right"] = KeyToJson(localInputs->keyRight);
		j["inputs"].push_back(std::move(k));
	}

	{
		auto k = json::object();
		k["enabled"] = onScreenController->enabled;
		k["player1"] = onScreenController->port[0];
		k["player2"] = onScreenController->port[1];
		k["vibrate"] = onScreenController->vibrate;
		k["pos"] = onScreenController->pos;
		k["size"] = onScreenController->size;
		j["onscreen-input"] = k;
	}

	std::string dumps = j.dump(1, '\t');
	WriteToFile(
		GetAppdataPath() + "ini.json",
		std::vector<uint8_t>(dumps.begin(), dumps.end())
	);
}

void EmulatorApp::LoadIni(bool reset) {
	using nlohmann::json;

	std::vector<uint8_t> data;
	if (!reset) {
		ReadFromFile(GetAppdataPath() + "ini.json", data);
	}

	auto j = json::parse(data, nullptr, false, true);
	if (!j.is_object())
		j = json::object();

	auto getField = [&]<class T>(const json & j, const char* field, T defaultVal, bool(json:: * typeCheck)() const) {
		try {
			if (!j.contains(field) || !(j[field].*typeCheck)())
				throw std::exception();
			return j[field].get<T>();
		} catch (std::exception&) {
			return defaultVal;
		}
	};
	auto getArray = [&](const json& j, const char* field) {
		try {
			if (!j.contains(field) || !j[field].is_array())
				throw std::exception();
			return j[field];
		} catch (std::exception&) {
			return json::array();
		}
	};
	auto getObject = [&](const json& j, const char* field) {
		try {
			if (!j.contains(field) || !j[field].is_object())
				throw std::exception();
			return j[field];
		} catch (std::exception&) {
			return json::object();
		}
	};
	auto getInt = [&](const json& j, const char* field, int defaultVal) {
		return getField.operator() < int > (j, field, defaultVal, &json::is_number_integer);
	};
	auto getBool = [&](const json& j, const char* field, bool defaultVal) {
		return getField.operator() < bool > (j, field, defaultVal, &json::is_boolean);
	};
	auto getString = [&](const json& j, const char* field, std::string defaultVal) {
		return getField.operator() < std::string > (j, field, defaultVal, &json::is_string);
	};
	auto clamp = [](int x, int min, int max) {
		if (x < min) return min;
		else if (x > max) return max;
		else return x;
	};

#if !defined(__ANDROID__)
	SetFullscreenState(getBool(j, "fullscreen", true));
#endif
	volume = getInt(j, "volume", 100);
	mute = getBool(j, "mute", false);
	overlay->showFps = getBool(j, "show-fps", false);
	overlay->showNetwork = getBool(j, "show-network", false);
	paused = getBool(j, "paused", false);
	emulationSpeed = clamp(getInt(j, "speed", 3), 0, 6);
	nes->masterBg = !getBool(j, "disable-bg", false);
	nes->masterFg = !getBool(j, "disable-fg", false);
	nes->masterGreyscale = getBool(j, "greyscale", false);
	nes->hideBorder = getBool(j, "hide-border", false);

	recentRoms.clear();
	for (const auto& filename : getArray(j, "recent-roms")) {
		if (filename.is_string()) {
			recentRoms.push_back(filename.get<std::string>());
		} else if (recentRoms.size() >= MAX_RECENT_ROMS) {
			break;
		}
	}

	localNesInputs.clear();
	for (const auto& k : getArray(j, "inputs")) {
		auto inp = std::make_unique<LocalNesInput>(*this);
		ResetNesInput(inp.get());
		inp->name = getString(k, "name", "Default Input");
		inp->enabled = getBool(k, "enabled", false);
		JsonToKey(getObject(k, "key-a"), inp->keyA);
		JsonToKey(getObject(k, "key-b"), inp->keyB);
		JsonToKey(getObject(k, "key-start"), inp->keyStart);
		JsonToKey(getObject(k, "key-select"), inp->keySelect);
		JsonToKey(getObject(k, "key-up"), inp->keyUp);
		JsonToKey(getObject(k, "key-down"), inp->keyDown);
		JsonToKey(getObject(k, "key-left"), inp->keyLeft);
		JsonToKey(getObject(k, "key-right"), inp->keyRight);

		if (getBool(k, "player1", false))
			ToggleLocalController(inp.get(), 0);
		if (getBool(k, "player2", false))
			ToggleLocalController(inp.get(), 1);

		localNesInputs.push_back(std::move(inp));

		if (localNesInputs.size() >= MAX_CONTROLLERS)
			break;
	}

	if (localNesInputs.empty()) {
		AddNesInput();
		ToggleLocalController(localNesInputs[0].get(), 0);
	}

	{
		json k = getObject(j, "onscreen-input");
#if defined(__ANDROID__)
		onScreenController->enabled = getBool(k, "enabled", !SDL_IsAndroidTV());
        onScreenController->vibrate = getBool(k, "vibrate", !SDL_IsAndroidTV());
#else
		onScreenController->enabled = getBool(k, "enabled", false);
		onScreenController->vibrate = getBool(k, "vibrate", false);
#endif
		onScreenController->pos = getInt(k, "pos", 40);
		onScreenController->size = getInt(k, "size", 70);

		if (getBool(k, "player1", true))
			ToggleOnScreenInput(0);
		if (getBool(k, "player2", false))
			ToggleOnScreenInput(1);
	}
}

void EmulatorApp::AddNesInput() {
	auto inp = std::make_unique<LocalNesInput>(*this);
	inp->name = "Controller " + std::to_string(localNesInputs.size() + 1);
	ResetNesInput(inp.get());
	localNesInputs.push_back(std::move(inp));
}

void EmulatorApp::ResetNesInput(LocalNesInput* inp) const {
	inp->keyA = Key::Keyboard(SDL_SCANCODE_APOSTROPHE);
	inp->keyB = Key::Keyboard(SDL_SCANCODE_SEMICOLON);
	inp->keyStart = Key::Keyboard(SDL_SCANCODE_RETURN);
	inp->keySelect = Key::Keyboard(SDL_SCANCODE_RSHIFT);
	inp->keyUp = Key::Keyboard(SDL_SCANCODE_W);
	inp->keyDown = Key::Keyboard(SDL_SCANCODE_S);
	inp->keyLeft = Key::Keyboard(SDL_SCANCODE_A);
	inp->keyRight = Key::Keyboard(SDL_SCANCODE_D);
}

void EmulatorApp::LoadRom(const std::string& path) {

	SaveSram();

	std::string name = GetFilenameWithoutExtension(path);

	// Read ROM
	std::vector<uint8_t> rom;
	if (!ReadFromFile(path, rom)) {
		ErrorMessage("Cannot open file " + path, this);
		return;
	}

	// Read SRAM
	std::vector<uint8_t> sram;
	ReadFromFile(GetAppdataPath() + "Sram/" + name + ".sram", sram);

	nes->InsertCartridge(std::make_unique<Cartridge>(name, rom, sram));

	// Update recents list
	auto it = std::find(recentRoms.begin(), recentRoms.end(), path);
	if (it != recentRoms.end())
		recentRoms.erase(it);
	recentRoms.push_front(path);
	if (recentRoms.size() >= MAX_RECENT_ROMS)
		recentRoms.pop_back();
	UpdateMenu();

	SaveIni();
}

void EmulatorApp::CloseRom() {
	SaveSram();
	nes->InsertCartridge(nullptr);
	UpdateMenu();

	SaveIni();
}

std::vector<std::string> EmulatorApp::GetRomList() const {
	CreateFolder(GetAppdataPath() + "Roms");
	std::string folder = GetAppdataPath() + "Roms/";

	std::vector<std::string> files;
	IterateDirectory(folder, [&](const std::string& filename) {
		if (filename.ends_with(".nes"))
			files.push_back(filename);
	});
	return files;
}

void EmulatorApp::DisplayAboutDialog() const {
	InfoMessage(
		"About",

		"Nintondo Enjoyment Service\n"
		"Copyright (c) 2022 Kevin Lu\n"
		"\n"
		"JSON library\n"
		"Copyright (c) 2013-2022 Niels Lohmann\n"
		"\n"
		"NES APU\n"
		"Copyright (c) 2014 Antonio Maiorano\n"
		,

		this
	);
}

void EmulatorApp::BeginKeyBindEdit(Key* k) {
	previousKeyBind = *k;
	*k = Key{};
	keyToBeBound = k;
	keyBindTimer.Restart();

	menuState = MenuState::KeyBindEdit;
	menu->highlight = true;
	UpdateMenu();
}

void EmulatorApp::BeginServerPortEdit() {
	menuState = MenuState::ServerPortEdit;
	menu->highlight = true;
	lastFieldValue = serverPort;
	UpdateMenu();

#if defined(__ANDROID__)
	if (GetKeyDown(Key::Mouse())) {
		onScreenNumpad->enabled = true;
	}
#endif
}

void EmulatorApp::BeginConnectIPEdit() {
	menuState = MenuState::ConnectIPEdit;
	menu->highlight = true;
	lastFieldValue = connectIP;
#if defined(__ANDROID__)
	connectIP.clear();
#endif
	UpdateMenu();

	if (GetKeyDown(Key::Mouse())) {
		onScreenNumpad->enabled = true;
	}
}

void EmulatorApp::BeginConnectPortEdit() {
	menuState = MenuState::ConnectPortEdit;
	menu->highlight = true;
	lastFieldValue = connectPort;
#if defined(__ANDROID__)
	connectPort.clear();
#endif
	UpdateMenu();

	if (GetKeyDown(Key::Mouse())) {
		onScreenNumpad->enabled = true;
	}
}

void EmulatorApp::BeginControllerNameEdit(LocalNesInput* controller) {
	menuState = MenuState::ControllerNameEdit;
	controllerNameToBeChanged = controller;
	menu->highlight = true;
	lastFieldValue = controller->name;
#if defined(__ANDROID__)
	controller->name.clear();
#endif
	UpdateMenu();
}

void EmulatorApp::BeginVolumeEdit() {
	menuState = MenuState::VolumeEdit;
	menu->highlight = true;
	UpdateMenu();
}

void EmulatorApp::BeginOnScreenSizeEdit() {
	menuState = MenuState::OnScreenSizeEdit;
	menu->highlight = true;
	UpdateMenu();
}

void EmulatorApp::BeginOnScreenPosEdit() {
	menuState = MenuState::OnScreenPosEdit;
	menu->highlight = true;
	UpdateMenu();
}

void EmulatorApp::OnUpdateEditKeyBind() {

	bool endKeyBind = false;

	// Timed out
	if (keyBindTimer.Time() > 3.0f) {
		endKeyBind = true;
		*keyToBeBound = previousKeyBind;
	}

	// Cancelled
	else if (systemInput->GetKeyDown(SystemKey::Back)) {
		endKeyBind = true;
		*keyToBeBound = previousKeyBind;
	}

	// Get new key pressed
	else if (GetAnyKeyDown(*keyToBeBound)) {
		endKeyBind = true;
	}

	if (endKeyBind) {
		keyToBeBound = nullptr;
		menuState = MenuState::Menu;
		menu->highlight = false;
		UpdateMenu();
	}
}

void EmulatorApp::OnUpdateEditField(std::string& field, std::function<void(bool)> callback) {
	Key key{};
	if (onScreenNumpad->GetKeyAnyDown(key) || GetAnyKeyDown(key)) {
		if (key.keycode == SDLK_BACKSPACE) {
			if (field.size()) {
				field.pop_back();
				if (callback)
					callback(true);
				UpdateMenu();
			}
		} else if (key.ToChar()) {
			bool shift = GetKey(Key::Keyboard(SDL_SCANCODE_LSHIFT))
				|| GetKey(Key::Keyboard(SDL_SCANCODE_RSHIFT));
			field += key.ToChar(shift);
			if (callback)
				callback(false);
			UpdateMenu();
		} else if (systemInput->GetKeyDown(SystemKey::Enter)
				|| systemInput->GetKeyDown(SystemKey::Back)
				|| GetKey(Key::Mouse()))
		{
			menuState = MenuState::Menu;
			menu->highlight = false;
			onScreenNumpad->enabled = false;
			if (field.empty())
				field = lastFieldValue;
			UpdateMenu();
		}
	}
}

void EmulatorApp::OnUpdateEditSlider(int& field, int min, int max, int step) {

	int incr = 0;
	if (sliderChange) {
		incr = sliderChange;
		sliderChange = 0;
	} else {
		if (systemInput->GetKeyDown(SystemKey::Right)) {
			incr = 1;
		} else if (systemInput->GetKeyDown(SystemKey::Left)) {
			incr = -1;
		} else if (systemInput->GetKeyDown(SystemKey::Enter) || systemInput->GetKeyDown(SystemKey::Back)) {
			menuState = MenuState::Menu;
			menu->highlight = false;
			UpdateMenu();
			return;
		}
	}
	incr *= step;

	if (incr) {
		field += incr;
		if (field < min)
			field = min;
		if (field > max)
			field = max;
		UpdateMenu();
	}
}

static bool ParseUint16(const std::string& s, uint16_t& out) {
	out = 0;
	try {
		size_t processed = 0;
		int i = std::stoi(s, &processed);
		if (i <= 0 || i > UINT16_MAX || s.size() != processed)
			return false;
		out = i;
		return true;
	} catch (std::exception&) {
		return false;
	}
}

void EmulatorApp::ToggleServer() {

	if (!server) {

		// Start server

		uint16_t port = 0;
		if (!ParseUint16(serverPort, port)) {
			ErrorMessage("Port is invalid.", this);
			return;
		}

		try {
			server = std::make_unique<NetworkServer>(port);
		} catch (jnet::Exception& ex) {
			ErrorMessage(std::string("Failed to create server: ") + ex.what(), this);
			return;
		}

		for (auto& input : networkNesInputs)
			input->SetServer(server.get());

		server->onconnect = [&](auto c) {
			overlay->Notify("Client " + std::to_string(c) + " connected.");
		};
		server->ondisconnect = [&](auto c) {
			overlay->Notify("Client " + std::to_string(c) + " disconnected.");
		};

	} else {
		// Quit server
		for (auto& input : networkNesInputs)
			input->SetServer(nullptr);
		server.reset();
	}

	UpdateMenu();
}

void EmulatorApp::ToggleClient() {

	if (!client) {
		// Connect

		uint16_t port = 0;
		if (!ParseUint16(connectPort, port)) {
			ErrorMessage("Port is invalid.", this);
			return;
		}

		try {
			client = std::make_unique<NetworkClient>(connectIP, port);
		} catch (jnet::Exception& ex) {
			ErrorMessage(std::string("Cannot to connect to server: ") + ex.what(), this);
			return;
		}

	} else {
		// Disconnect
		client.reset();
	}

	UpdateMenu();
}

std::string EmulatorApp::GetSaveStateFilename(int slot) const {
	return GetAppdataPath()
		+ "States/"
		+ std::to_string(slot + 1)
		+ ".sav";
}

void EmulatorApp::SaveState(int slot) {
	CreateFolder(GetAppdataPath() + "State");
	if (!WriteToFile(GetSaveStateFilename(slot), nes->SaveState().GetBuffer())) {
		ErrorMessage("Failed to save state.", this);
	} else {
		UpdateMenu();
	}
}

void EmulatorApp::LoadState(int slot) {
	std::vector<uint8_t> data;
	if (!ReadFromFile(GetSaveStateFilename(slot), data)) {
		ErrorMessage("Failed to read savestate.", this);
	} else {
		Savestate state = data;
		if (!nes->LoadState(state)) {
			ErrorMessage("Savestate is invalid.", this);
		} else {
			UpdateMenu();
		}
	}
}

bool EmulatorApp::SaveStateExists(int slot) const {
	return FileExists(GetSaveStateFilename(slot));
}

void EmulatorApp::ToggleOnScreenInputEnabled() {
	onScreenController->enabled = !onScreenController->enabled;
	UpdateMenu();
}

void EmulatorApp::ToggleOnScreenInput(int port) {
	bool& enabled = onScreenController->port[port];
	enabled = !enabled;

	if (!enabled) {
		// Disable
		aggregatedNesInputs[port]->RemoveSource(onScreenController.get());
	} else {
		// Enable
		aggregatedNesInputs[port]->AddSource(onScreenController.get());
	}
	UpdateMenu();
}

void EmulatorApp::ToggleLocalController(LocalNesInput* controller, int port) {
	bool& enabled = controller->port[port];
	enabled = !enabled;

	if (!enabled) {
		// Disable
		aggregatedNesInputs[port]->RemoveSource(controller);
	} else {
		// Enable
		aggregatedNesInputs[port]->AddSource(controller);
	}
}

void EmulatorApp::ToggleFullscreen() {
#if !defined(__ANDROID__)
	SetFullscreenState(!GetFullscreenState());
	UpdateMenu();
#endif
}

void EmulatorApp::ToggleMute() {
	mute = !mute;
	UpdateMenu();
}

void EmulatorApp::TogglePaused() {
	paused = !paused;
	UpdateMenu();
}
