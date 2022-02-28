#include "Overlay.h"
#include "Font.h"
#include <iomanip>
#include <sstream>

static std::string RoundString(float x, std::streamsize dp) {
	std::stringstream stream;
	stream << std::fixed << std::setprecision(dp) << x;
	return stream.str();
}

static std::string ToDataString(size_t n) {

	std::string suffix;
	std::string s;

	constexpr size_t X = 1024;
    if (n < X) {
		suffix = "B";
		s = RoundString((float)n, 2);
	} else if (n < X * X) {
		suffix = "KB";
		s = RoundString((float)n / X, 2);
	} else if (n < X * X * X) {
		suffix = "MB";
		s = RoundString((float)n / X / X, 2);
	} else {
		suffix = "GB";
		s = RoundString((float)n / X / X / X, 2);
	}

	if (s.find('.') != s.npos) {
		while (s.ends_with('0'))
			s.pop_back();
		if (s.ends_with('.'))
			s.pop_back();
	}

	return s + suffix;
}

Overlay::Overlay(const Application& app) :
	topTex(std::make_unique<Texture>(app, 600, 16)),
	notifyTex(std::make_unique<Texture>(app, 600, 8))
{
	topTex->size.y = [&](int w, int h) { return int(w / topTex->GetAspectRatio()); };
	std::vector<Colour> pixels(topTex->GetWidth() * topTex->GetHeight(), Colour::Transparent());
	topTex->SetPixels(pixels.data());

	notifyTex->pos.y = [&](int w, int h) { return h - int(w / notifyTex->GetAspectRatio()); };
	notifyTex->size.y = [&](int w, int h) { return int(w / notifyTex->GetAspectRatio()); };
	pixels = std::vector<Colour>(topTex->GetWidth() * topTex->GetHeight(), Colour::Transparent());
	notifyTex->SetPixels(pixels.data());
}

void Overlay::Update() {

	if (notifyText.size()) {
		std::vector<Colour> pixels(notifyTex->GetWidth() * notifyTex->GetHeight(), Colour::Transparent());
		Colour c = Colour::White();
		float alpha = std::min(2.0f - notifyTimer.Time(), 1.0f);
		c.a = (uint8_t)(255 * alpha);
		Character::DrawString(
			pixels,
			notifyTex->GetWidth(),
			0,
			0,
			notifyText,
			c
		);
		notifyTex->SetPixels(pixels.data());

		if (notifyTimer.Time() > 2.0f)
			notifyText.clear();
	};

	if (showFps || showNetwork) {
		std::vector<Colour> pixels(topTex->GetWidth() * topTex->GetHeight(), Colour::Transparent());
		if (showFps) {
			std::string text = std::to_string(fps);

			Character::DrawString(
				pixels,
				topTex->GetWidth(),
				topTex->GetWidth() - 8 * text.size() - 8,
				0,
				text
			);
		}
		if (showNetwork) {
			std::string text = "Down " + ToDataString(netReadSpeed) + "ps/"
				+ "Up " + ToDataString(netWriteSpeed) + "ps";

			Character::DrawString(
				pixels,
				topTex->GetWidth(),
				topTex->GetWidth() - 8 * text.size() - 8,
				showFps ? 8 : 0,
				text
			);
		}
		topTex->SetPixels(pixels.data());
	}
}

void Overlay::Draw() const {
	if (notifyText.size())
		notifyTex->Draw();

	if (showFps || showNetwork)
		topTex->Draw();
}

void Overlay::Notify(std::string text) {
	notifyText = std::move(text);
	notifyTimer.Restart();
}
