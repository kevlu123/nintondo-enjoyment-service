#pragma once
#include <memory>
#include <deque>
#include "Application.h"
#include "Nes.h"
#include "Menu.h"
#include "Network.h"
#include "Stopwatch.h"
#include "Overlay.h"
#include "OnScreenInput.h"
#include "SystemInput.h"

struct EmulatorApp : public Application {

	bool OnStart() override;
	bool OnUpdate() override;
	void OnQuit() override;
	void OnSuspend() override;

private:

	void LoadIni(bool reset = false);
	void SaveIni() const;
	void DisplayAboutDialog() const;
	void OnUpdateEditKeyBind();
	void OnUpdateEditField(std::string& field, std::function<void(bool)> callback = nullptr);
	void OnUpdateEditSlider(int& field, int min, int max, int step);
	void BeginVolumeEdit();
	void LoadRom(const std::string& path);
	void CloseRom();
	void SaveSram() const;
	void ToggleFullscreen();
	void ToggleMute();
	void TogglePaused();
	std::vector<std::string> GetRomList() const;
	std::unique_ptr<NesInterface> nes;
	std::unique_ptr<Texture> screen;
	std::vector<uint8_t> standbyScreenPx;
	std::unique_ptr<Texture> standbyScreen;
	std::unique_ptr<Overlay> overlay;
	const std::vector<uint8_t>* screenBuffer = nullptr;
	float emulationStep = 0.0f;
	int emulationSpeed = 3;
	bool mute = false;
	int volume = 100;
	bool paused = false;
	bool quit = false;
	std::deque<std::string> recentRoms;
	std::string lastFieldValue;
	Stopwatch<> acbackButtonPressTimer;

	void UpdateMenu();
	std::unique_ptr<Menu> menu;
	int sliderChange = 0;
	enum class MenuState {
		None,
		Menu,
		VolumeEdit,
		KeyBindEdit,
		ServerPortEdit,
		ConnectIPEdit,
		ConnectPortEdit,
		ControllerNameEdit,
		OnScreenSizeEdit,
		OnScreenPosEdit,
	} menuState = MenuState::None;

	// Keybinds
	void BeginKeyBindEdit(Key* k);
	Key* keyToBeBound = nullptr;
	Key previousKeyBind = {};
	Stopwatch<> keyBindTimer;

	// Input
	void BeginControllerNameEdit(LocalNesInput* controller);
	void BeginOnScreenSizeEdit();
	void BeginOnScreenPosEdit();
	void ToggleLocalController(LocalNesInput* controller, int port);
	void ToggleOnScreenInputEnabled();
	void ToggleOnScreenInput(int port);
	void AddNesInput();
	void ResetNesInput(LocalNesInput* input) const;
	std::unique_ptr<AggregateInputSource<NesKey>> aggregatedNesInputs[2];
	std::vector<std::unique_ptr<LocalNesInput>> localNesInputs;
	std::unique_ptr<NetworkNesInput> networkNesInputs[2];
	std::unique_ptr<OnScreenController> onScreenController;
	LocalNesInput* controllerNameToBeChanged;
	std::unique_ptr<SystemInput> systemInput;
	std::unique_ptr<OnScreenNumpad> onScreenNumpad;

	// Server
	void BeginServerPortEdit();
	void ToggleServer();
	std::string serverPort = "42942";
	std::unique_ptr<NetworkServer> server;

	// Client
	void BeginConnectIPEdit();
	void BeginConnectPortEdit();
	void ToggleClient();
	std::string connectIP = "127.0.0.1";
	std::string connectPort = "42942";
	std::unique_ptr<NetworkClient> client;

	// Savestates
	std::string GetSaveStateFilename(int slot) const;
	void SaveState(int slot);
	void LoadState(int slot);
	bool SaveStateExists(int slot) const;
};
