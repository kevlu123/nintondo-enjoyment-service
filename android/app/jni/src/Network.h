#pragma once
#include "jnet/jnet.h"
#include "Controller.h"
#include <unordered_map>
#include <functional>

struct InputState {
	struct {
		bool keyA : 1;
		bool keyB : 1;
		bool keyStart : 1;
		bool keySelect : 1;
		bool keyUp : 1;
		bool keyDown : 1;
		bool keyLeft : 1;
		bool keyRight : 1;
	} port[2]{};
};
static_assert(sizeof(InputState) == 2, "sizeof(InputState) must be 2");

struct NetworkServer : public jnet::JServer {
	NetworkServer(uint16_t port);
	bool OnClientConnect(jnet::ExternalJClient clientID) noexcept override;
	void OnClientDisconnect(jnet::ExternalJClient clientID) noexcept override;
	void OnReceive(jnet::ExternalJClient clientID, jnet::Json& j) noexcept override;

	bool GetKey(int port, NesKey key) const;
	void Update(const std::vector<uint8_t>& screenBuffer);
	void SendAudio(const std::vector<int16_t>& samples);

	std::function<void(jnet::ExternalJClient)> onconnect = nullptr;
	std::function<void(jnet::ExternalJClient)> ondisconnect = nullptr;
private:

	struct Client {
		jnet::ExternalJClient id = 0;
		std::vector<uint8_t> screenBuffer;
		int scrollX = 0;
		int scrollY = 0;
		size_t requestedFrames = 5;
		InputState input{};
	};
	std::unordered_map<jnet::ExternalJClient, Client> clients;

	void ReceivedInput(jnet::ExternalJClient clientID, const jnet::Json& j);
	void ReceivedFrameRequest(jnet::ExternalJClient clientID);
	void SendFrame(Client& client, const std::vector<uint8_t>& screenBuffer);
};

struct NetworkClient : public jnet::LocalJClient {
	NetworkClient(const std::string& host, uint16_t port);
	void OnDisconnect() override;
	void OnReceive(jnet::Json& j) override;

	bool GetFrame(std::vector<uint8_t> const*& screenBuffer);
	std::vector<int16_t> TakeAudioSamples();
	bool IsConnected() const;
	void SendInput(const InputState& state);
private:
	bool connected = true;
	bool newFrame = false;
	std::vector<uint8_t> screenBuffer;
	std::vector<int16_t> audioSamples;

	void ReceivedFrame(const jnet::Json& j);
	void ReceivedAudio(const jnet::Json& j);
	void SendFrameRequest();
};
