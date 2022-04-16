LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := main

SDL_PATH := ../SDL

LOCAL_C_INCLUDES := $(LOCAL_PATH)/$(SDL_PATH)/include $(LOCAL_PATH)/jnet

# Add your application source files here...
LOCAL_SRC_FILES := \
	Application.cpp \
	Apu.cpp \
	Cartridge.cpp \
	Controller.cpp \
	Cpu.cpp \
	EmulatorApp.cpp \
	Font.cpp \
	Headless.cpp \
	lodepng.cpp \
	Main.cpp \
	Mapper.cpp \
	Mapper000.cpp \
	Mapper001.cpp \
	Mapper002.cpp \
	Mapper003.cpp \
	Mapper004.cpp \
	Mapper007.cpp \
	Mapper066.cpp \
	Mapper140.cpp \
	Menu.cpp \
	Nes.cpp \
	Network.cpp \
	OnScreenInput.cpp \
	Overlay.cpp \
	Ppu.cpp \
	SystemInput.cpp

LOCAL_CPPFLAGS := -fexceptions -frtti
LOCAL_CFLAGS := -std=c++2a

LOCAL_SHARED_LIBRARIES := SDL2

LOCAL_LDLIBS := -lGLESv1_CM -lGLESv2 -llog

include $(BUILD_SHARED_LIBRARY)
