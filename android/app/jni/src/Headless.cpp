#include "Headless.h"
#include "Application.h"
#include "Network.h"
#include "Nes.h"
#include "Stopwatch.h"
#include "Controller.h"
#include "File.h"
#include <string>
#include <iostream>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <queue>

#if defined(__ANDROID__)
#include "SDL.h"
#else
#include "SDL2/SDL.h"
#endif

#if defined(_WIN32)
#include <Windows.h>

static bool InitConsole() {

    if (!AllocConsole()) {
        return false;
    }

    // std::cout, std::clog, std::cerr, std::cin
    FILE* fDummy;
    freopen_s(&fDummy, "CONOUT$", "w", stdout);
    freopen_s(&fDummy, "CONOUT$", "w", stderr);
    freopen_s(&fDummy, "CONIN$", "r", stdin);
    std::cout.clear();
    std::clog.clear();
    std::cerr.clear();
    std::cin.clear();

    // std::wcout, std::wclog, std::wcerr, std::wcin
    HANDLE hConOut = CreateFileW(L"CONOUT$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    HANDLE hConIn = CreateFileW(L"CONIN$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    SetStdHandle(STD_OUTPUT_HANDLE, hConOut);
    SetStdHandle(STD_ERROR_HANDLE, hConOut);
    SetStdHandle(STD_INPUT_HANDLE, hConIn);
    std::wcout.clear();
    std::wclog.clear();
    std::wcerr.clear();
    std::wcin.clear();
    return true;
}

static void CleanupConsole() {
    FreeConsole();
}

#else

static bool InitConsole() { return true; }
static void CleanupConsole() {}

#endif

struct AsyncInput {

    AsyncInput() {
        terminate = false;
        thrd = std::thread(&AsyncInput::Run, this);
    }

    ~AsyncInput() {
        terminate = true;
        if (thrd.joinable())
            thrd.join();
    }

    bool GetCommand(std::string& cmd) {
        std::scoped_lock<std::mutex> lock(mtx);
        if (cmds.empty()) {
            return false;
        } else {
            cmd = std::move(cmds.front());
            cmds.pop();
            return true;
        }
    }

    std::atomic_bool terminate;

private:

    void Run() {
        while (!terminate) {
            SDL_Delay(50);
            std::string cmd;
            std::cout << ">" << std::flush;
            std::getline(std::cin, cmd);

            if (cmd == "exit") {
                terminate = true;
            } else {
                std::scoped_lock<std::mutex> lock(mtx);
                cmds.push(std::move(cmd));
            }
        }
    }

    std::thread thrd;
    std::mutex mtx;
    std::queue<std::string> cmds;
};

struct HeadlessProperties {
    // Settings
    float emulationSpeed = 1.0f;

    // State
    float emulationStep = 0.0f;
    const std::vector<uint8_t>* screenBuffer = nullptr;
    bool pause = false;
};

static const std::string& GetAppdataPath() {

    static std::string appdataPath;
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

static std::string GetSramPath(const std::string& romPath) {
    std::string name = File::GetFilenameWithoutExtension(romPath);
    return GetAppdataPath() + "Sram/" + name + ".sram";
}

static std::string GetRomPath(const std::string& rom) {
    return GetAppdataPath() + "Roms/" + rom + ".nes";
}

static std::string GetSaveStateFilename(int slot) {
    return GetAppdataPath()
        + "States/"
        + std::to_string(slot + 1)
        + ".sav";
}

static void SaveSram(Nes& nes) {
    if (!nes.HasCartridgeLoaded()) {
        std::cout << "No ROM open" << std::endl;
        return;
    }

    const auto& sram = nes.GetSram();
    if (sram.size()) {
        File::CreateFolder(GetAppdataPath() + "Sram");
        File::WriteToFile(
            GetAppdataPath() + "Sram/" + nes.GetCartridgeName() + ".sram",
            sram
        );
    }
}

static void DeleteSram(const std::string& romPath) {
    fs::remove(GetSramPath(romPath));
}

static void LoadRom(Nes& nes, const std::string& romName) {
    if (nes.HasCartridgeLoaded()) {
        SaveSram(nes);
    }

    std::string path = GetRomPath(romName);

    // Read ROM
    std::vector<uint8_t> rom;
    if (!File::ReadFromFile(path, rom)) {
        std::cout << "Cannot open file " << path << std::endl;
        return;
    }

    // Read SRAM
    std::vector<uint8_t> sram;
    File::ReadFromFile(GetSramPath(path), sram);

    nes.InsertCartridge(std::make_unique<Cartridge>(File::GetFilenameWithoutExtension(path), rom, sram));
}

static void CloseRom(Nes& nes, bool validate) {
    if (!nes.HasCartridgeLoaded()) {
        if (validate) {
            std::cout << "No ROM open" << std::endl;
        }
        return;
    }

    SaveSram(nes);
    nes.InsertCartridge(nullptr);
}

static void SaveState(Nes& nes, const std::string& slotName) {
    if (!nes.HasCartridgeLoaded()) {
        std::cout << "No ROM open" << std::endl;
        return;
    }

    int slot = -1;
    try { slot = std::stoi(slotName); }
    catch (std::exception&) { }
    if (slot < 0) {
        std::cout << "Invalid argument" << std::endl;
        return;
    }

    File::CreateFolder(GetAppdataPath() + "States");
    if (!File::WriteToFile(GetSaveStateFilename(slot), nes.SaveState().GetBuffer())) {
        std::cout << "Failed to write savestate." << std::endl;
    }
}

static void LoadState(Nes& nes, const std::string& slotName) {
    int slot = -1;
    try { slot = std::stoi(slotName); } catch (std::exception&) {}
    if (slot < 0) {
        std::cout << "Invalid argument" << std::endl;
        return;
    }

    std::vector<uint8_t> data;
    if (!File::ReadFromFile(GetSaveStateFilename(slot), data)) {
        std::cout << "Failed to read savestate." << std::endl;
    } else {
        Savestate state = data;
        if (!nes.LoadState(state)) {
            std::cout << "Savestate is invalid." << std::endl;
        }
    }
}

static void Reset(Nes& nes) {
    if (!nes.HasCartridgeLoaded()) {
        std::cout << "No ROM open" << std::endl;
        return;
    }

    nes.Reset();
}

static bool GetToggleArg(const std::string& arg, bool& b) {
    if (arg == "on") {
        b = true;
        return true;
    } else if (arg == "off") {
        b = false;
        return true;
    } else {
        std::cout << "Invalid argument" << std::endl;
        return false;
    }
}

static void SetSpeed(HeadlessProperties& prop, const std::string& arg) {
    try { prop.emulationSpeed = std::stof(arg); }
    catch (std::exception&) {
        std::cout << "Invalid argument" << std::endl;
    }
}

static void ToggleBackground(Nes& nes, const std::string& arg) {
    bool enable{};
    if (!GetToggleArg(arg, enable))
        return;
    nes.masterBg = enable;
}

static void ToggleForeground(Nes& nes, const std::string& arg) {
    bool enable{};
    if (!GetToggleArg(arg, enable))
        return;
    nes.masterFg = enable;
}

static void ToggleGreyscale(Nes& nes, const std::string& arg) {
    bool enable{};
    if (!GetToggleArg(arg, enable))
        return;
    nes.masterGreyscale = enable;
}

static void ToggleBorder(Nes& nes, const std::string& arg) {
    bool enable{};
    if (!GetToggleArg(arg, enable))
        return;
    nes.hideBorder = !enable;
}

static void PrintHelp() {
    std::cout << "\nHelp:\n";
    std::cout << "\thelp                    Displays this message.\n";
    std::cout << "\texit                    Quits out of the server.\n";
    std::cout << "\topen <rom>              Opens a ROM.\n";
    std::cout << "\tclose                   Closes the currently open ROM.\n";
    std::cout << "\tsavesram                Saves the SRAM to disk.\n";
    std::cout << "\tclearsram <rom>         Deletes the SRAM of a rom from disk.\n";
    std::cout << "\tsavestate <slot>        Save current emulation state to disk.\n";
    std::cout << "\tloadstate <slot>        Load emulation state from disk.\n";
    std::cout << "\treset                   Resets the NES system.\n";
    std::cout << "\tpause                   Pauses emulation.\n";
    std::cout << "\tunpause                 Unpauses emulation.\n";
    std::cout << "\tspeed <multiplier>      Sets emulation speed.\n";
    std::cout << "\tbackground <on|off>     Toggles background graphics.\n";
    std::cout << "\tforeground <on|off>     Toggles foreground graphics.\n";
    std::cout << "\tgreyscale  <on|off>     Toggles greyscale mode.\n";
    std::cout << "\tborder     <on|off>     Toggles rendering of border tiles.\n";
    std::cout << std::endl;
}

static void HandleCommand(Nes& nes, HeadlessProperties& prop, const std::string& cmd) {
    std::string arg;
    auto isCmd = [&](const std::string& cmdName) {
        if (!cmd.starts_with(cmdName))
            return false;
        if (cmd.size() > cmdName.size() + 1)
            arg = cmd.substr(cmdName.size() + 1);
        return true;
    };

    if (cmd == "help") {
        PrintHelp();
    } else if (isCmd("open")) {
        CloseRom(nes, false);
        LoadRom(nes, arg);
    } else if (cmd == "close") {
        CloseRom(nes, true);
    } else if (cmd == "savesram") {
        SaveSram(nes);
    } else if (isCmd("clearsram")) {
        DeleteSram(arg);
    } else if (isCmd("savestate")) {
        SaveState(nes, arg);
    } else if (isCmd("loadstate")) {
        LoadState(nes, arg);
    } else if (cmd == "reset") {
        Reset(nes);
    } else if (cmd == "pause") {
        prop.pause = true;
    } else if (cmd == "unpause") {
        prop.pause = false;
    } else if (isCmd("speed")) {
        SetSpeed(prop, arg);
    } else if (isCmd("background")) {
        ToggleBackground(nes, arg);
    } else if (isCmd("foreground")) {
        ToggleForeground(nes, arg);
    } else if (isCmd("greyscale")) {
        ToggleGreyscale(nes, arg);
    } else if (isCmd("border")) {
        ToggleBorder(nes, arg);
    } else {
        std::cout << "Unrecognised command" << std::endl;
    }
}

static void Cleanup() {
    SDL_Quit();
    CleanupConsole();
}

void RunHeadless(int argc, char** argv) {
    if (!InitConsole()) {
        Application::ErrorMessage("Failed to initialize console");
        return;
    }
    if (SDL_Init(SDL_INIT_TIMER)) {
        Application::ErrorMessage("Failed to initialize console");
        Cleanup();
        return;
    }

    // Parse port
    int port = 42942;
    if (argc >= 3) {
        try {
            port = std::stoi(argv[2]);
        } catch (std::exception&) {}

        if (port < 0 || port > 65535) {
            std::cout << "Port out of range" << std::endl;
        }
    }

    // Initialize server
    std::unique_ptr<NetworkServer> server;
    size_t connectionCount = 0;
    try {
        server = std::make_unique<NetworkServer>(port);
    } catch (jnet::Exception& ex) {
        std::cout << "Failed to create server: " << ex.what() << std::endl;
        Cleanup();
        return;
    }
    server->onconnect = [&](auto c) {
        connectionCount++;
        std::cout << "\nClient " << std::to_string(c) << " connected.\n>" << std::flush;
    };
    server->ondisconnect = [&](auto c) {
        connectionCount--;
        std::cout << "\nClient " << std::to_string(c) << " disconnected.\n>" << std::flush;
    };
    std::cout << "Started server on port " << server->GetPort() << std::endl;

    // Initialize emulation
    Nes nes;
    nes.audioSampleRate = Application::AUDIO_SAMPLE_RATE;
    std::vector<uint8_t> standbyScreen(Ppu::DRAWABLE_WIDTH * Ppu::DRAWABLE_HEIGHT);
    for (int x = 0; x < Ppu::DRAWABLE_WIDTH; x++)
        for (int y = 0; y < Ppu::DRAWABLE_HEIGHT; y++)
            standbyScreen[(size_t)y * Ppu::DRAWABLE_WIDTH + x] = y / 22 + 0x31;

    // Initialize input
    std::unique_ptr<NetworkNesInput> netInputs[2];
    for (int i = 0; i < 2; i++) {
        netInputs[i] = std::make_unique<NetworkNesInput>(i);
        netInputs[i]->SetServer(server.get());
        std::unique_ptr<NesController> controller = std::make_unique<NesController>(*netInputs[i]);
        nes.SetController(i, std::move(controller));
    }

    // Update loop
    AsyncInput input;
    Stopwatch sw;
    HeadlessProperties prop;
    while (!input.terminate) {

        SDL_Delay(1);

        // Process server commands
        std::string cmd;
        while (input.GetCommand(cmd)) {
            HandleCommand(nes, prop, cmd);
        }

        // Update emulation
        std::vector<int16_t> audioSamples;
        constexpr float FRAME_DURATION = 1.0f / 60.0f;
        prop.emulationStep += prop.emulationSpeed * sw.Time();
        sw.Restart();
        while (prop.emulationStep >= FRAME_DURATION) {
            prop.emulationStep -= FRAME_DURATION;
            if (connectionCount && !prop.pause) {
                if (nes.HasCartridgeLoaded()) {
                    nes.ClockFrame();
                }
            }

            if (nes.HasCartridgeLoaded()) {
                prop.screenBuffer = &nes.GetScreenBuffer();
                auto samples = nes.TakeAudioSamples();

                auto scaleAudio = [](const std::vector<int16_t>& samples, size_t targetSamples) {
                    std::vector<int16_t> out(targetSamples);
                    if (!samples.empty()) {
                        if (samples.size() <= targetSamples) {
                            // Scale up
                            for (size_t i = 0; i < out.size(); i++) {
                                float m = (float)i / out.size();
                                out.at(i) = samples.at(size_t(m * samples.size()));
                            }
                        } else if (samples.size() > targetSamples) {
                            // Scale down
                            for (size_t i = 0; i < samples.size(); i++) {
                                float m = (float)i / samples.size();
                                out.at(size_t(m * out.size())) = samples.at(i);
                            }
                        }
                    }
                    return out;
                };
                samples = scaleAudio(samples, size_t(nes.audioSampleRate / 60 / prop.emulationSpeed));
                audioSamples.insert(audioSamples.end(), samples.begin(), samples.end());
            }
        }

        if (!nes.HasCartridgeLoaded() || prop.screenBuffer == nullptr) {
            prop.screenBuffer = &standbyScreen;
        }

        // Update network
        server->Update(*prop.screenBuffer);
        if (audioSamples.size())
            server->SendAudio(audioSamples);
    }

    Cleanup();
}
