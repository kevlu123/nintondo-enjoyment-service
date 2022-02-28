#pragma once
#include "Application.h"
#include <Stopwatch.h>
#include <memory>
#include <string>
#include <functional>
#include <array>

enum class CheckMode {
	None,
	Checkable,
	Radio,
};

struct MenuNode {
	bool enabled = true;
	bool checked = false;
	CheckMode checkMode = CheckMode::None;
	bool closeAfterClick = false;
	bool slider = false;
	std::string text;
	std::string shortcut;
	std::string description;
	std::function<void()> onclick;
	std::vector<MenuNode> children;
};

struct Menu {
	Menu(const Application& app);
	void Update();
	void Draw() const;
	bool Toggle();
	bool GetEnabled() const;
	void MoveUp();
	void MoveDown();
	void MoveLeft();
	void MoveRight();
	bool Submit();
	bool Click(int x, int y, int* sliderChange = nullptr);
	void MouseHeld(int x, int y);
	const MenuNode* GetSelectedNode() const;
	MenuNode* GetSelectedNode();
	MenuNode root;
	bool highlight = false;
	std::vector<CursorPos> cursorPositions;
private:
	MenuNode* GetSelectedNodeParent();
	const MenuNode* GetSelectedNodeParent() const;
	const MenuNode* GetHoveredNode(int* scrollDir = nullptr, int* sliderDir = nullptr) const;
	void Scroll(int dir);

	bool active = false;
	std::vector<size_t> selection;
	std::vector<size_t> scroll;
	std::array<bool, 10> canScrollUp{};
	std::array<bool, 10> canScrollDown{};
	Stopwatch<> scrollTimer;

	std::unique_ptr<Texture> tex;
	std::unique_ptr<Texture> background;
	mutable std::vector<Colour> pixels;
	size_t lastSelection = 0;
	size_t lastScroll = 0;
};
