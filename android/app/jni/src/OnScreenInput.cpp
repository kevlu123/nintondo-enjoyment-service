#include "OnScreenInput.h"
#include "Font.h"
#include <queue>
#include <unordered_set>

constexpr size_t LAYOUT_WIDTH = 64;
constexpr size_t LAYOUT_HEIGHT = 43;

constexpr Colour OFF_COLOUR = Colour::Grey();
constexpr Colour ON_COLOUR = Colour::LightGrey();

static const std::string LEFT_LAYOUT =
"................xxxxxxxxxxx....................................."
"................xxxxxxxxxxx....................................."
"................xxxxxxxxxxx....................................."
"................xxxxxxxxxxx....................................."
"................xxxxxxxxxxx....................................."
"................xxxxxxxxxxx....................................."
"................xxxxxxxxxxx....................................."
"................xxxxxxxxxxx....................................."
"................xxxxxxxxxxx....................................."
"................xxxxxxxxxxx....................................."
"................xxxxxxxxxxx....................................."
"................xxxxxxxxxxx....................................."
"................xxxxxxxxxxx....................................."
"................xxxxxxxxxxx....................................."
"................xxxxxxxxxxx....................................."
".................xxxxxxxxx......................................"
"xxxxxxxxxxxxxxx...xxxxxxx...xxxxxxxxxxxxxxx....................."
"xxxxxxxxxxxxxxxx...xxxxx...xxxxxxxxxxxxxxxx....................."
"xxxxxxxxxxxxxxxxx...xxx...xxxxxxxxxxxxxxxxx....................."
"xxxxxxxxxxxxxxxxxx...x...xxxxxxxxxxxxxxxxxx....................."
"xxxxxxxxxxxxxxxxxxx.....xxxxxxxxxxxxxxxxxxx....................."
"xxxxxxxxxxxxxxxxxxxx...xxxxxxxxxxxxxxxxxxxx....................."
"xxxxxxxxxxxxxxxxxxx.....xxxxxxxxxxxxxxxxxxx....................."
"xxxxxxxxxxxxxxxxxx...x...xxxxxxxxxxxxxxxxxx....................."
"xxxxxxxxxxxxxxxxx...xxx...xxxxxxxxxxxxxxxxx....................."
"xxxxxxxxxxxxxxxx...xxxxx...xxxxxxxxxxxxxxxx....................."
"xxxxxxxxxxxxxxx...xxxxxxx...xxxxxxxxxxxxxxx....................."
".................xxxxxxxxx......................................"
"................xxxxxxxxxxx....................................."
"................xxxxxxxxxxx....................................."
"................xxxxxxxxxxx....................................."
"................xxxxxxxxxxx....................................."
"................xxxxxxxxxxx..................xxxxxxxxxxxxxxxxxxx"
"................xxxxxxxxxxx..................xxxxxxxxxxxxxxxxxxx"
"................xxxxxxxxxxx..................xxxxxxxxxxxxxxxxxxx"
"................xxxxxxxxxxx..................xxxxxxxxxxxxxxxxxxx"
"................xxxxxxxxxxx..................xxxxxxxxxxxxxxxxxxx"
"................xxxxxxxxxxx..................xxxxxxxxxxxxxxxxxxx"
"................xxxxxxxxxxx..................xxxxxxxxxxxxxxxxxxx"
"................xxxxxxxxxxx..................xxxxxxxxxxxxxxxxxxx"
"................xxxxxxxxxxx..................xxxxxxxxxxxxxxxxxxx"
"................xxxxxxxxxxx....................................."
"................xxxxxxxxxxx.....................................";

static const std::string RIGHT_LAYOUT =
"................................................................"
"................................................................"
"................................................................"
"................................................................"
"................................................................"
"................................................................"
"................................................................"
"................................................................"
"................................................................"
"................................................................"
"................................................................"
"................................................................"
"................................................................"
"................................................................"
"................................................................"
"................................................................"
"..................................xxxxx...............xxxxx....."
"................................xxxxxxxxx...........xxxxxxxxx..."
"...............................xxxxxxxxxxx.........xxxxxxxxxxx.."
"..............................xxxxxxxxxxxxx.......xxxxxxxxxxxxx."
"..............................xxxxxxxxxxxxx.......xxxxxxxxxxxxx."
".............................xxxxxxxxxxxxxxx.....xxxxxxxxxxxxxxx"
".............................xxxxxxxxxxxxxxx.....xxxxxxxxxxxxxxx"
".............................xxxxxxxxxxxxxxx.....xxxxxxxxxxxxxxx"
".............................xxxxxxxxxxxxxxx.....xxxxxxxxxxxxxxx"
".............................xxxxxxxxxxxxxxx.....xxxxxxxxxxxxxxx"
"..............................xxxxxxxxxxxxx.......xxxxxxxxxxxxx."
"..............................xxxxxxxxxxxxx.......xxxxxxxxxxxxx."
"...............................xxxxxxxxxxx.........xxxxxxxxxxx.."
"................................xxxxxxxxx...........xxxxxxxxx..."
"..................................xxxxx...............xxxxx....."
"................................................................"
"....xxxxxxxxxxxxxxxxxxx........................................."
"....xxxxxxxxxxxxxxxxxxx........................................."
"....xxxxxxxxxxxxxxxxxxx........................................."
"....xxxxxxxxxxxxxxxxxxx........................................."
"....xxxxxxxxxxxxxxxxxxx........................................."
"....xxxxxxxxxxxxxxxxxxx........................................."
"....xxxxxxxxxxxxxxxxxxx........................................."
"....xxxxxxxxxxxxxxxxxxx........................................."
"....xxxxxxxxxxxxxxxxxxx........................................."
"................................................................"
"................................................................";

struct Vecu {
	size_t x, y;
	bool operator==(const Vecu& rhs) const {
		return x == rhs.x && y == rhs.y;
	}
};

namespace std {
	template<> struct hash<Vecu> {
		size_t operator()(const Vecu& a) const {
			return std::hash<size_t>()(a.x) ^ (std::hash<size_t>()(a.y) << 1);
		}
	};
}

static std::unordered_set<Vecu> BUTTON_LEFT_FILL;
static std::unordered_set<Vecu> BUTTON_RIGHT_FILL;
static std::unordered_set<Vecu> BUTTON_UP_FILL;
static std::unordered_set<Vecu> BUTTON_DOWN_FILL;
static std::unordered_set<Vecu> BUTTON_A_FILL;
static std::unordered_set<Vecu> BUTTON_B_FILL;
static std::unordered_set<Vecu> BUTTON_START_FILL;
static std::unordered_set<Vecu> BUTTON_SELECT_FILL;
static bool init = false;

static std::unordered_set<Vecu> FloodFill(const std::string& px, size_t x, size_t y, size_t w) {

	size_t h = px.size() / w;

	std::unordered_set<Vecu> r;
	std::queue<Vecu> next;
	next.push({ x, y });

	auto check = [&](size_t x, size_t y) {
		if (x < w && y < h && px[y * w + x] != '.' && !r.contains({ x, y })) {
			r.insert({ x, y });
			next.push({ x, y });
		}
	};

	while (next.size()) {
		Vecu p = next.front();
		next.pop();
		check(p.x + 1, p.y);
		check(p.x - 1, p.y);
		check(p.x, p.y + 1);
		check(p.x, p.y - 1);
	}

	return r;
}

static void Init() {
	BUTTON_LEFT_FILL   = FloodFill(LEFT_LAYOUT, 19, 21, LAYOUT_WIDTH);
	BUTTON_RIGHT_FILL  = FloodFill(LEFT_LAYOUT, 23, 21, LAYOUT_WIDTH);
	BUTTON_UP_FILL     = FloodFill(LEFT_LAYOUT, 21, 19, LAYOUT_WIDTH);
	BUTTON_DOWN_FILL   = FloodFill(LEFT_LAYOUT, 21, 23, LAYOUT_WIDTH);
	BUTTON_SELECT_FILL = FloodFill(LEFT_LAYOUT, 63, 41, LAYOUT_WIDTH);
	BUTTON_A_FILL      = FloodFill(RIGHT_LAYOUT, 63, 22, LAYOUT_WIDTH);
	BUTTON_B_FILL      = FloodFill(RIGHT_LAYOUT, 29, 22, LAYOUT_WIDTH);
	BUTTON_START_FILL  = FloodFill(RIGHT_LAYOUT, 04, 41, LAYOUT_WIDTH);
}

OnScreenController::OnScreenController(const Application& app) :
	app(app),
	leftTex(std::make_unique<Texture>(app, (int)LAYOUT_WIDTH, (int)LAYOUT_HEIGHT)),
	rightTex(std::make_unique<Texture>(app, (int)LAYOUT_WIDTH, (int)LAYOUT_HEIGHT))
{
	if (!init) {
		Init();
		init = true;
	}

	leftPx.resize(LAYOUT_WIDTH * LAYOUT_HEIGHT, Colour::Transparent());
	rightPx = leftPx;

	constexpr float aspect = (float)LAYOUT_WIDTH / LAYOUT_HEIGHT;
	leftTex->pos.y = rightTex->pos.y = [&](int w, int h) { return int(h - (h * Pos()) - (h * Size())); };
	leftTex->size.x = rightTex->size.x = [&](int w, int h) { return int(h * Size() * aspect); };
	leftTex->size.y = rightTex->size.y = [&](int w, int h) { return int(h * Size()); };

	leftTex->pos.x = [&](int w, int h) { return int(h * Pos()); };
	rightTex->pos.x = [&](int w, int h) { return int(w - (h * Size() * aspect) - (h * Pos())); };
}

bool OnScreenController::GetKey(NesKey key) const {

	if (!enabled)
		return false;

	auto get = [&](bool left, Vecu pos) {
		const auto& px = left ? leftPx : rightPx;
		return px[pos.y * LAYOUT_WIDTH + pos.x] == ON_COLOUR;
	};

	switch (key) {
	case NesKey::A:      return get(false, *BUTTON_A_FILL.begin());
	case NesKey::B:      return get(false, *BUTTON_B_FILL.begin());
	case NesKey::Start:  return get(false, *BUTTON_START_FILL.begin());
	case NesKey::Select: return get(true, *BUTTON_SELECT_FILL.begin());
	case NesKey::Up:     return get(true, *BUTTON_UP_FILL.begin());
	case NesKey::Down:   return get(true, *BUTTON_DOWN_FILL.begin());
	case NesKey::Left:   return get(true, *BUTTON_LEFT_FILL.begin());
	case NesKey::Right:  return get(true, *BUTTON_RIGHT_FILL.begin());
	default: throw std::logic_error("Invalid NesKey enum value.");
	}
}

void OnScreenController::Update() {
	if (!enabled)
		return;

	bool leftPressed = false;
	bool rightPressed = false;
	bool upPressed = false;
	bool downPressed = false;
	bool aPressed = false;
	bool bPressed = false;
	bool startPressed = false;
	bool selectPressed = false;

	if (app.GetKey(Key::Mouse())) {
		for (const auto& pos : app.GetCursorPositions()) {
			int x, y;

			leftTex->ScreenToTexPos(pos.x, pos.y, x, y);
			Vecu p = { (size_t)x, (size_t)y };
			if (x >= 0 && x < LAYOUT_WIDTH && y >= 0 && y < LAYOUT_HEIGHT) {
				if (BUTTON_LEFT_FILL.contains(p)) leftPressed = true;
				else if (BUTTON_RIGHT_FILL.contains(p)) rightPressed = true;
				else if (BUTTON_UP_FILL.contains(p)) upPressed = true;
				else if (BUTTON_DOWN_FILL.contains(p)) downPressed = true;
				else if (BUTTON_SELECT_FILL.contains(p)) selectPressed = true;
			}

			rightTex->ScreenToTexPos(pos.x, pos.y, x, y);
			p = { (size_t)x, (size_t)y };
			if (x >= 0 && x < LAYOUT_WIDTH && y >= 0 && y < LAYOUT_HEIGHT) {
				if (BUTTON_A_FILL.contains(p)) aPressed = true;
				else if (BUTTON_B_FILL.contains(p)) bPressed = true;
				else if (BUTTON_START_FILL.contains(p)) startPressed = true;
			}
		}
	}

	auto fillTex = [&](bool left, const std::unordered_set<Vecu>& area, bool on) {
		auto& tex = left ? leftTex : rightTex;
		auto& px = left ? leftPx : rightPx;

		for (const auto& p : area)
			px[p.y * LAYOUT_WIDTH + p.x] = on ? ON_COLOUR : OFF_COLOUR;

		tex->SetPixels(px.data());

		if (on && vibrate) {
			app.HapticRumble();
		}
	};

	if (!texInit || leftPressed != GetKey(NesKey::Left))     fillTex(true, BUTTON_LEFT_FILL, leftPressed);
	if (!texInit || rightPressed != GetKey(NesKey::Right))   fillTex(true, BUTTON_RIGHT_FILL, rightPressed);
	if (!texInit || upPressed != GetKey(NesKey::Up))         fillTex(true, BUTTON_UP_FILL, upPressed);
	if (!texInit || downPressed != GetKey(NesKey::Down))     fillTex(true, BUTTON_DOWN_FILL, downPressed);
	if (!texInit || selectPressed != GetKey(NesKey::Select)) fillTex(true, BUTTON_SELECT_FILL, selectPressed);
	if (!texInit || aPressed != GetKey(NesKey::A))           fillTex(false, BUTTON_A_FILL, aPressed);
	if (!texInit || bPressed != GetKey(NesKey::B))           fillTex(false, BUTTON_B_FILL, bPressed);
	if (!texInit || startPressed != GetKey(NesKey::Start))   fillTex(false, BUTTON_START_FILL, startPressed);
	texInit = true;
}

void OnScreenController::Draw() const {
	if (enabled) {
		leftTex->Draw();
		rightTex->Draw();
	}
}

constexpr int NUMPAD_BORDER = 3;
constexpr int NUMPAD_KEY_SIZE = 2 * NUMPAD_BORDER + 8;

OnScreenNumpad::OnScreenNumpad(const Application& app) :
	input(app)
{
	tex = std::make_unique<Texture>(
		app, 3 * NUMPAD_KEY_SIZE, 4 * NUMPAD_KEY_SIZE);

	float aspect = (float)tex->GetWidth() / tex->GetHeight();
	tex->pos.x = [=](int w, int h) { return int(w - aspect * h / 3) / 2; };
	tex->pos.y = [=](int w, int h) { return h - h / 3; };
	tex->size.x = [=](int w, int h) { return int(aspect * h / 3); };
	tex->size.y = [=](int w, int h) { return h / 3; };
}

void OnScreenNumpad::Update() {
	if (!enabled)
		return;

	std::vector<Colour> px(tex->GetWidth() * tex->GetHeight());
	
	for (int y = 0; y < 4; y++)
		for (int x = 0; x < 3; x++) {

			int ax = x * NUMPAD_KEY_SIZE;
			int ay = y * NUMPAD_KEY_SIZE;

			bool pressed = false;
			if (input.GetKey(Key::Mouse())) {
				for (auto pos : input.GetCursorPositions()) {
					int cx, cy;
					tex->ScreenToTexPos(pos.x, pos.y, cx, cy);
					if (cx >= ax && cx < ax + NUMPAD_KEY_SIZE && cy >= ay && cy < ay + NUMPAD_KEY_SIZE) {
						pressed = true;
						break;
					}
				}
			}

			Colour c = pressed ? Colour::LightGrey() : Colour::Grey();
			for (int a = 0; a < NUMPAD_KEY_SIZE; a++)
				for (int b = 0; b < NUMPAD_KEY_SIZE; b++)
					px[(ay + b) * NUMPAD_KEY_SIZE * 3 + (ax + a)] = c;

			Character("123456789 0<"[y * 3 + x]).DrawChar(
				px,
				tex->GetWidth(),
				ax + NUMPAD_BORDER + 1,
				ay + NUMPAD_BORDER + 1,
				Colour::White(),
				c
			);
		}

	tex->SetPixels(px.data());
}

void OnScreenNumpad::Draw() const {
	if (enabled) {
		tex->Draw();
	}
}

bool OnScreenNumpad::GetKeyAnyDown(Key& key) const {
	if (!enabled)
		return false;

	for (int i = 0; i < 12; i++) {
		int x = i % 3;
		int y = i / 3;
		int ax = x * NUMPAD_KEY_SIZE;
		int ay = y * NUMPAD_KEY_SIZE;

		if (input.GetKeyDown(Key::Mouse())) {
			for (auto pos : input.GetCursorPositions()) {
				int cx, cy;
				tex->ScreenToTexPos(pos.x, pos.y, cx, cy);

				if (cx >= ax && cx < ax + NUMPAD_KEY_SIZE && cy >= ay && cy < ay + NUMPAD_KEY_SIZE) {
					constexpr SDL_Scancode scancodes[12] = {
						SDL_SCANCODE_1, SDL_SCANCODE_2, SDL_SCANCODE_3,
						SDL_SCANCODE_4, SDL_SCANCODE_5, SDL_SCANCODE_6,
						SDL_SCANCODE_7, SDL_SCANCODE_8, SDL_SCANCODE_9,
						(SDL_Scancode)0, SDL_SCANCODE_0, SDL_SCANCODE_BACKSPACE,
					};
					constexpr SDL_Keycode keycodes[12] = {
						SDLK_1, SDLK_2, SDLK_3,
						SDLK_4, SDLK_5, SDLK_6,
						SDLK_7, SDLK_8, SDLK_9,
						0, SDLK_0, SDLK_BACKSPACE,
					};
					key = Key::Keyboard(scancodes[i], keycodes[i]);
					return true;
				}
			}
		}
	}
	return false;
}
