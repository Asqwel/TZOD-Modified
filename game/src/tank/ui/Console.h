// Console.h

#pragma once

#include "Base.h"
#include "Window.h"

#include <deque>

namespace UI
{
	// forward declarations
	class ConsoleBuffer;

///////////////////////////////////////////////////////////////////////////////

interface IConsoleHistory
{
	virtual void Enter(const string_t &str) = 0;
	virtual size_t GetItemCount() const = 0;
	virtual const string_t& GetItem(size_t index) const = 0;
};

///////////////////////////////////////////////////////////////////////////////

class ConsoleHistoryDefault : public IConsoleHistory
{
public:
	ConsoleHistoryDefault(size_t maxSize);

	virtual void Enter(const string_t &str);
	virtual size_t GetItemCount() const;
	virtual const string_t& GetItem(size_t index) const;

private:
	size_t _maxSize;
	std::deque<string_t> _buf;
};

///////////////////////////////////////////////////////////////////////////////

class Console : public Window
{
public:
	Console(Window *parent);
	static Console* Create(Window *parent, float x, float y, float w, float h, ConsoleBuffer *buf);

	float GetInputHeight() const;

	void SetColors(const SpriteColor *colors, size_t count);
	void SetHistory(IConsoleHistory *history);
	void SetBuffer(ConsoleBuffer *buf);
	void SetEcho(bool echo);
	std::tr1::function<void(const string_t &)> eventOnSendCommand;
	std::tr1::function<bool(const string_t &, int &, string_t &)> eventOnRequestCompleteCommand;

protected:
	virtual bool OnChar(int c);
	virtual bool OnRawChar(int c);
	virtual bool OnMouseWheel(float x, float y, float z);
	virtual bool OnMouseDown(float x, float y, int button);
	virtual bool OnMouseUp(float x, float y, int button);
	virtual bool OnMouseMove(float x, float y);

	virtual void OnTimeStep(float dt);
	virtual void DrawChildren(const DrawingContext *dc, float sx, float sy) const;
	virtual void OnSize(float width, float height);
	virtual bool OnFocus(bool focus);

private:
	void OnScroll(float pos);

private:
	ScrollBarVertical *_scroll;
	Edit  *_input;
	size_t _cmdIndex;
	size_t _font;

	ConsoleBuffer *_buf;
	IConsoleHistory *_history;
	std::vector<SpriteColor> _colors;

	bool _echo;
	bool _autoScroll;
};


///////////////////////////////////////////////////////////////////////////////
} // end of namespace UI

// end of file
