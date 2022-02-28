#include "EmulatorApp.h"

#if defined(_WIN32)
#pragma comment(lib, "SDL2.lib")
#pragma comment(lib, "SDL2main.lib")

	#if defined(NDEBUG)
	#pragma comment(linker, "/subsystem:windows")
	#endif

#endif

int main(int argc, char** argv) {

#if defined(NDEBUG)
	try {
#endif
		EmulatorApp app;
		app.SetCommandLine(argc, argv);
		app.Run();
		return 0;

#if defined(NDEBUG)
	} catch (std::exception& ex) {
		EmulatorApp::ErrorMessage(ex.what());
		return -1;
	}
#endif
}
