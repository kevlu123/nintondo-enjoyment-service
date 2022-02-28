#include "Network.h"
#include "Base64.h"
using jnet::Json;

#pragma warning(push, 0)
#include "lodepng.h"
#pragma warning(pop)

static_assert(std::endian::native == std::endian::little, "Big endian is not supported.");

template <class T>
std::vector<uint8_t> Compress(const std::vector<T>& data) {
	std::vector<uint8_t> out;
	lodepng::compress(out, (const unsigned char*)data.data(), data.size() * sizeof(T));
	return out;
}

template <class T>
bool Decompress(const std::vector<uint8_t>& data, std::vector<T>& out) {
	std::vector<uint8_t> _out;
	if (lodepng::decompress(_out, data))
		return false;

	if (_out.size() % sizeof(T))
		return false;

	out.resize(_out.size() / sizeof(T));
	std::memcpy(out.data(), _out.data(), _out.size());
	return true;
}

NetworkServer::NetworkServer(uint16_t port) :
	JServer(port) {
	maxClients = 8;
}

bool NetworkServer::OnClientConnect(jnet::ExternalJClient clientID) noexcept {
	Client c{};
	c.id = clientID;
	c.screenBuffer.resize(256 * 240, 0xF);
	clients[clientID] = std::move(c);
	if (onconnect)
		onconnect(clientID);
	return true;
}

void NetworkServer::OnClientDisconnect(jnet::ExternalJClient clientID) noexcept {
	clients.erase(clients.find(clientID));
	if (ondisconnect)
		ondisconnect(clientID);
}

void NetworkServer::OnReceive(jnet::ExternalJClient clientID, jnet::Json& j) noexcept {
	try {
		if (!j.is_object())
			return;
		if (!j.contains("msg") || !j["msg"].is_string())
			return;

		std::string msg = j["msg"].get<std::string>();
		if (msg == "input") {
			ReceivedInput(clientID, j);
		} else if (msg == "frame-request") {
			ReceivedFrameRequest(clientID);
		}

	} catch (std::exception&) {}
}

void NetworkServer::ReceivedInput(jnet::ExternalJClient clientID, const jnet::Json& j) {
	if (!j.contains("state") || !j["state"].is_string())
		return;

	Client& client = clients.at(clientID);
	auto bytes = Base64::Decode(j["state"]);
	if (bytes.size() == sizeof(InputState))
		std::memcpy(&client.input, bytes.data(), sizeof(InputState));
}

void NetworkServer::ReceivedFrameRequest(jnet::ExternalJClient clientID) {
	Client& client = clients.at(clientID);
	client.requestedFrames++;
}

bool NetworkServer::GetKey(int port, NesKey key) const {
	if (port != 0 && port != 1)
		throw std::logic_error("Attempted to get the key state of an invalid controller port.");

	auto checkKey = [&](const Client& client) {
		switch (key) {
		case NesKey::A:      return client.input.port[port].keyA;
		case NesKey::B:      return client.input.port[port].keyB;
		case NesKey::Start:  return client.input.port[port].keyStart;
		case NesKey::Select: return client.input.port[port].keySelect;
		case NesKey::Up:     return client.input.port[port].keyUp;
		case NesKey::Down:   return client.input.port[port].keyDown;
		case NesKey::Left:   return client.input.port[port].keyLeft;
		case NesKey::Right:  return client.input.port[port].keyRight;
		default: throw std::logic_error("Invalid NesKey enum value.");
		}
	};

	for (const auto& p : clients)
		if (checkKey(p.second))
			return true;
	return false;
}

void NetworkServer::Update(const std::vector<uint8_t>& screenBuffer)
{
	for (auto& p : clients) {
		auto& client = p.second;
		if (client.requestedFrames) {
			client.requestedFrames--;
			SendFrame(client, screenBuffer);
		}
	}

	JServer::Update();
}

void NetworkServer::SendFrame(Client& client, const std::vector<uint8_t>& screenBuffer)
{
	std::vector<uint8_t> difference(screenBuffer.size());
	for (size_t i = 0; i < screenBuffer.size(); i++)
		difference[i] = (screenBuffer[i] - client.screenBuffer[i]) & 0b0011'1111;
	std::memcpy(client.screenBuffer.data(), screenBuffer.data(), screenBuffer.size());

	difference = Compress(difference);

	auto j = Json::object();
	j["msg"] = "frame";
	j["pixels"] = Base64::Encode(difference.data(), difference.size());

	Send(client.id, j);
}

void NetworkServer::SendAudio(const std::vector<int16_t>& samples) {
	auto compressed = Compress(samples);
	auto encoded = Base64::Encode(compressed.data(), compressed.size());
	for (auto& p : clients) {
		auto j = Json::object();
		j["msg"] = "audio";
		j["samples"] = encoded;
		Send(p.first, j);
	}
}

NetworkClient::NetworkClient(const std::string& host, uint16_t port) :
	LocalJClient(host, port),
	screenBuffer(256 * 240, 0xF)
{
}

void NetworkClient::OnDisconnect() {
	connected = false;
}

void NetworkClient::OnReceive(Json& j) {
	try {
		if (!j.is_object())
			return;
		if (!j.contains("msg") || !j["msg"].is_string())
			return;

		std::string msg = j["msg"].get<std::string>();
		if (msg == "frame") {
			ReceivedFrame(j);
		} else if (msg == "audio") {
			ReceivedAudio(j);
		}

	} catch (std::exception&) {}
}

bool NetworkClient::GetFrame(std::vector<uint8_t> const*& screenBuffer) {
	if (newFrame) {
		screenBuffer = &this->screenBuffer;
		return true;
	} else {
		return false;
	}
}

std::vector<int16_t> NetworkClient::TakeAudioSamples() {
	auto ret = std::move(audioSamples);
	audioSamples.clear();
	return ret;
}

bool NetworkClient::IsConnected() const {
	return connected;
}

void NetworkClient::ReceivedFrame(const Json& j) {
	if (!j.contains("pixels") || !j["pixels"].is_string())
		return;

	std::vector<uint8_t> buffer;
	if (Decompress(Base64::Decode(j["pixels"]), buffer) && buffer.size() >= 256 * 240)
		for (size_t i = 0; i < screenBuffer.size(); i++)
			screenBuffer[i] += buffer[i];

	newFrame = true;

	SendFrameRequest();
}

void NetworkClient::ReceivedAudio(const jnet::Json& j) {
	if (!j.contains("samples") || !j["samples"].is_string())
		return;

	std::vector<int16_t> buffer;
	if (Decompress(Base64::Decode(j["samples"]), buffer)) {
		if (audioSamples.empty()) {
			audioSamples = std::move(buffer);
		} else {
			audioSamples.insert(
				audioSamples.end(),
				buffer.begin(),
				buffer.end()
			);

			if (audioSamples.size() > 44100) {
				audioSamples.erase(
					audioSamples.begin(),
					audioSamples.end() - 44100
				);
			}
		}
	}
}

void NetworkClient::SendFrameRequest() {
	auto j = Json::object();
	j["msg"] = "frame-request";
	Send(j);
}

void NetworkClient::SendInput(const InputState& state) {
	auto j = Json::object();
	j["msg"] = "input";
	j["state"] = Base64::Encode(&state, sizeof(state));
	Send(j);
}
