#include "Menu.h"
#include "Font.h"

constexpr size_t CELL_WIDTH = 24; // In character units

constexpr size_t MAX_COLS = 3;
constexpr size_t MAX_ROWS = 16;

Menu::Menu(const Application& app) :
	tex(std::make_unique<Texture>(app, 8 * int(MAX_COLS * CELL_WIDTH), 8 * int(MAX_ROWS + 2))),
	background(std::make_unique<Texture>(app, 1, 1))
{
#if defined(__ANDROID__)
	// Stretch vertically for smaller touch screens
	tex->size.y = [&](int w, int h) { return int(1.7f * w / tex->GetAspectRatio()); };
#else
	tex->size.y = [&](int w, int h) { return int(w / tex->GetAspectRatio()); };
#endif

	// Set background to a single semi-transparent pixel
	Colour bgCol = { 0, 0, 0, 100 };
	background->SetPixels((const uint8_t*)&bgCol);
}

bool Menu::Toggle() {
	active = !active;
	selection = { 0 };
	scroll = { 0 };
	lastSelection = lastScroll = 0;
	return active;
}

bool Menu::GetEnabled() const {
	return active;
}

void Menu::MoveUp() {
	if (active && selection.back() > 0) {
		selection.back()--;
		lastSelection = lastScroll = 0;
		if (selection.back() < scroll.back()) {
			scroll.back()--;
		}
	}
}

void Menu::MoveDown() {
	if (active && selection.back() < GetSelectedNodeParent()->children.size() - 1) {
		selection.back()++;
		lastSelection = lastScroll = 0;
		if (selection.back() >= scroll.back() + MAX_ROWS)
			scroll.back()++;
	}
}

void Menu::MoveLeft() {
	if (active && selection.size() > 1) {
		lastSelection = selection.back();
		lastScroll = scroll.back();
		selection.pop_back();
		scroll.pop_back();
	}
}

void Menu::MoveRight() {
	if (active && GetSelectedNode()->children.size()) {
		selection.push_back(lastSelection);
		scroll.push_back(lastScroll);
		lastSelection = lastScroll = 0;
	}
}

bool Menu::Submit() {
	if (!active)
		return false;

	bool hide = false;
	auto node = GetSelectedNode();
	if (node->enabled) {
		if (node->checkMode == CheckMode::Checkable) {
			node->checked = !node->checked;
		} else if (node->checkMode == CheckMode::Radio) {
			for (auto& child : GetSelectedNodeParent()->children)
				child.checked = false;
			node->checked = true;
		}

		if (node->closeAfterClick) {
			hide = true;
		}

		if (node->onclick) {
			node->onclick();
		}
	}

	if (hide)
		Toggle();
	return active;
}

bool Menu::Click(int x, int y, int* sliderChange) {
	*sliderChange = 0;

	if (!active)
		return false;

	cursorPositions = { {x, y} };
	tex->ScreenToTexPos(x, y, x, y);
	int scrollDir = 0;
	if (auto node = GetHoveredNode(&scrollDir, sliderChange)) {

		// Menu item pressed
		int cx = x / (8 * CELL_WIDTH);
		int cy = y / 8 - 1;

		if (GetSelectedNode() != node)
			*sliderChange = 0;

		selection.resize(cx);
		selection.push_back(cy + scroll.back());
		scroll.resize(cx);
		scroll.push_back(0);
		lastScroll = lastSelection = 0;

		return Submit();

	} else if (scrollDir) {
		// Scroll button clicked
		Scroll(scrollDir);
		return true;
	} else {
		return true;
	}
}

void Menu::MouseHeld(int x, int y) {
	if (!active)
		return;

	cursorPositions = { {x, y} };
	int scrollButton = 0;
	GetHoveredNode(&scrollButton);
	Scroll(scrollButton);
}

void Menu::Scroll(int dir) {
	if (dir && scrollTimer.Time() > 0.05f) {
		if (selection.size() < MAX_COLS) {
			selection.resize(MAX_COLS);
			scroll.resize(MAX_COLS);
		}
		selection.back() += dir;
		scroll.back() += dir;
		scrollTimer.Restart();
	}
}

void Menu::Update() {

	if (!active)
		return;

	pixels.clear();
	pixels.resize((size_t)tex->GetWidth() * tex->GetHeight(), Colour::Transparent());
	Character::DrawString(pixels, (size_t)tex->GetWidth(), 8, 0, "--Menu--", Colour::White(), Colour::Transparent());

	for (size_t i = 0; i < canScrollUp.size(); i++) {
		canScrollDown[i] = false;
		canScrollUp[i] = false;
	}

	auto selectedNode = GetSelectedNode();
	auto hoveredNode = GetHoveredNode();

	std::function<void(const MenuNode&, size_t, size_t)> updateRow = [&](const MenuNode& node, size_t col, size_t row) {

		bool selected = col == (size_t)-1 || (col < selection.size() && selection[col] == row);
	
		if (col != (size_t)-1) {

			// If row is negative after scrolling it will wrap to a large number anyway and return
			if (col < scroll.size())
				row -= scroll[col];

			if (row >= MAX_ROWS)
				return;
			row++;

			// Choose background colour
			Colour bg = Colour::Black();
			if (&node == selectedNode) {
				bg = highlight ? Colour::LightGrey() : Colour::Grey();
			} else if (selected || &node == hoveredNode) {
				bg = Colour::DarkGrey();
			}

			bg.a = bg == Colour::Black() ? 0 : 200;

			// Choose foreground colour
			Colour fg = Colour::White();
			if (!node.enabled) {
				fg = Colour::DarkGrey();
			}

			// Draw characters
			std::string text = node.text;
#if defined(__ANDROID__)
			std::string shortcut;
#else
			std::string shortcut = node.shortcut;
#endif
			text.resize(CELL_WIDTH - shortcut.size() - 2, ' ');
			text += shortcut;
			Character::DrawString(
				pixels,
				(size_t)tex->GetWidth(),
				8 * (col * CELL_WIDTH + 1),
				8 * row,
				text,
				fg,
				bg
			);

			// Draw checkmark
			Character c = node.checked ? Character::Checkmark() : Character();
			c.DrawChar(
				pixels,
				tex->GetWidth(),
				8 * col * CELL_WIDTH,
				8 * row,
				fg,
				bg
			);
		}
	
		if (selected && node.enabled) {
			size_t nextCol = col + 1;

			// Draw children
			for (size_t i = 0; i < node.children.size(); i++)
				updateRow(node.children[i], nextCol, i);

			// Draw scroll arrows
			if (node.children.size() > MAX_ROWS) {
				size_t scr = nextCol < scroll.size() ? scroll[nextCol] : 0;

				canScrollUp[col + 1] = scr > 0;
				if (canScrollUp[col + 1]) {
					Character::UpArrow().DrawChar(
						pixels,
						tex->GetWidth(),
						8 * (nextCol * CELL_WIDTH + CELL_WIDTH / 2 - 1),
						0
					);
				}

				canScrollDown[col + 1] = scr < node.children.size() - MAX_ROWS;
				if (canScrollDown[col + 1]) {
					Character::DownArrow().DrawChar(
						pixels,
						tex->GetWidth(),
						8 * (nextCol * CELL_WIDTH + CELL_WIDTH / 2 - 1),
						8 * (MAX_ROWS + 1)
					);
				}
			}
		}
	};
	
	updateRow(root, (size_t)-1, 0);

	tex->SetPixels(pixels.data());
}

void Menu::Draw() const {
	if (active) {
		background->Draw();
		tex->Draw();
	}
}

MenuNode* Menu::GetSelectedNodeParent() {
	auto node = &root;
	for (size_t i = 0; i < selection.size() - 1; i++) {
		node = &node->children[selection[i]];
	}
	return node;
}

const MenuNode* Menu::GetSelectedNodeParent() const {
	auto node = &root;
	for (size_t i = 0; i < selection.size() - 1; i++) {
		node = &node->children[selection[i]];
	}
	return node;
}

MenuNode* Menu::GetSelectedNode() {
	if (!active)
		return nullptr;

	return &GetSelectedNodeParent()->children[selection.back()];
}

const MenuNode* Menu::GetSelectedNode() const {
	if (!active)
		return nullptr;

	return &GetSelectedNodeParent()->children[selection.back()];
}
#include <iostream>
const MenuNode* Menu::GetHoveredNode(int* scrollDir, int* sliderDir) const {

	if (!active)
		return nullptr;

	if (cursorPositions.empty())
		return nullptr;

	int cursorX = cursorPositions[0].x;
	int cursorY = cursorPositions[0].y;
	
	int x, y;
	tex->ScreenToTexPos(cursorX, cursorY, x, y);

	int cx = x / (8 * CELL_WIDTH);
	int cy = y / 8 - 1;

	if (cx < 0 || cx > selection.size())
		return nullptr;

	if (scrollDir) {
		if (cy == -1 && canScrollUp[cx]) {
			*scrollDir = -1;
			return nullptr;
		} else if (cy == MAX_ROWS && canScrollDown[cx]) {
			*scrollDir = 1;
			return nullptr;
		}
	}

	int scr = 0;
	if (cx < scroll.size())
		scr = (int)scroll[cx];

	auto* parent = &root;
	for (size_t i = 0; i < cx; i++) {
		parent = &parent->children[selection[i]];
	}
	if (cy < 0 || cy >= MAX_ROWS || cy >= parent->children.size() - scr)
		return nullptr;

	if (sliderDir) {
		*sliderDir = (x - cx * (8 * CELL_WIDTH) < 8 * CELL_WIDTH / 2) ? -1 : 1;
	}

	return &parent->children[cy + scr];
}
