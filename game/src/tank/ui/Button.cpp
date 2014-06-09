// Button.cpp

#include "stdafx.h"

#include "Button.h"
#include "GuiManager.h"
#include "video/TextureManager.h"

namespace UI
{

///////////////////////////////////////////////////////////////////////////////
// Button class implementation

ButtonBase::ButtonBase(Window *parent)
  : Window(parent)
  , _state(stateNormal)
{
}

void ButtonBase::SetState(State s)
{
	if( _state != s )
	{
		_state = s;
		OnChangeState(s);
	}
}

bool ButtonBase::OnMouseMove(float x, float y)
{
	if( GetManager()->GetCapture() == this )
	{
		bool push = x < GetWidth() && y < GetHeight() && x > 0 && y > 0;
		SetState(push ? statePushed : stateNormal);
	}
	else
	{
		SetState(stateHottrack);
	}
	if( eventMouseMove )
		INVOKE(eventMouseMove) (x, y);
	return true;
}

bool ButtonBase::OnMouseDown(float x, float y, int button)
{
	if( 1 == button ) // left button only
	{
		GetManager()->SetCapture(this);
		SetState(statePushed);
		if( eventMouseDown )
			INVOKE(eventMouseDown) (x, y);
		return true;
	}
	return false;
}

bool ButtonBase::OnMouseUp(float x, float y, int button)
{
	if( GetManager()->GetCapture() == this )
	{
		GetManager()->SetCapture(NULL);
		bool click = (GetState() == statePushed);
		WindowWeakPtr wwp(this);
		if( eventMouseUp )
			INVOKE(eventMouseUp) (x, y); // handler may destroy this object
		if( click && wwp.Get() )
		{
			OnClick();                   // handler may destroy this object
			if( eventClick && wwp.Get() )
				INVOKE(eventClick) ();   // handler may destroy this object
		}
		if( wwp.Get() && GetEnabled() )  // handler may disable this button
			SetState(stateHottrack);
		return true;
	}
	return false;
}

bool ButtonBase::OnMouseLeave()
{
	SetState(stateNormal);
	return true;
}

void ButtonBase::OnClick()
{
}

void ButtonBase::OnChangeState(State state)
{
}

void ButtonBase::OnEnabledChange(bool enable, bool inherited)
{
	if( enable )
	{
		SetState(stateNormal);
	}
	else
	{
		SetState(stateDisabled);
	}
}


///////////////////////////////////////////////////////////////////////////////
// button class implementation

Button* Button::Create(Window *parent, const string_t &text, float x, float y, float w, float h)
{
	Button *res = new Button(parent);
	res->Move(x, y);
	res->SetText(text);
	if( w >= 0 && h >= 0 )
	{
		res->Resize(w, h);
	}

	return res;
}

Button::Button(Window *parent)
  : ButtonBase(parent)
  , _font(GetManager()->GetTextureManager()->FindSprite("font_small"))
{
	SetTexture("ui/button", true);
	SetDrawBorder(false);
	OnChangeState(stateNormal);
}

void Button::OnChangeState(State state)
{
	SetFrame(state);
}

void Button::DrawChildren(const DrawingContext *dc, float sx, float sy) const
{
	float x = GetWidth() / 2;
	float y = GetHeight() / 2;
	SpriteColor c = 0;

	switch( GetState() )
	{
	case statePushed:
		c = 0xffffffff;
		break;
	case stateHottrack:
		c = 0xffffffff;
		break;
	case stateNormal:
		c = 0xffffffff;
		break;
	case stateDisabled:
		c = 0xbbbbbbbb;
		break;
	default:
		assert(false);
	}

	dc->DrawBitmapText(sx + x, sy + y, _font, c, GetText(), alignTextCC);

	__super::DrawChildren(dc, sx, sy);
}


///////////////////////////////////////////////////////////////////////////////
// text button class implementation

TextButton* TextButton::Create(Window *parent, float x, float y, const string_t &text, const char *font)
{
	TextButton *res = new TextButton(parent);
	res->Move(x, y);
	res->SetText(text);
	res->SetFont(font);
	return res;
}

TextButton::TextButton(Window *parent)
  : ButtonBase(parent)
  , _fontTexture((size_t) -1)
  , _drawShadow(true)
{
	SetTexture(NULL, false);
	OnChangeState(stateNormal);
}

void TextButton::AlignSizeToContent()
{
	if( -1 != _fontTexture )
	{
		float w = GetManager()->GetTextureManager()->GetFrameWidth(_fontTexture, 0);
		float h = GetManager()->GetTextureManager()->GetFrameHeight(_fontTexture, 0);
		Resize((w - 1) * (float) GetText().length(), h + 1);
	}
}

void TextButton::SetDrawShadow(bool drawShadow)
{
	_drawShadow = drawShadow;
}

bool TextButton::GetDrawShadow() const
{
	return _drawShadow;
}

void TextButton::SetFont(const char *fontName)
{
	_fontTexture = GetManager()->GetTextureManager()->FindSprite(fontName);
	AlignSizeToContent();
}

void TextButton::OnTextChange()
{
	AlignSizeToContent();
}

void TextButton::DrawChildren(const DrawingContext *dc, float sx, float sy) const
{
	// grep 'enum State'
	SpriteColor colors[] = 
	{
		SpriteColor(0xffffffff), // normal
		SpriteColor(0xffccccff), // hottrack
		SpriteColor(0xffccccff), // pushed
		SpriteColor(0xAAAAAAAA), // disabled
	};
	if( _drawShadow && stateDisabled != GetState() )
	{
		dc->DrawBitmapText(sx + 1, sy + 1, _fontTexture, 0xff000000, GetText());
	}
	dc->DrawBitmapText(sx, sy, _fontTexture, colors[GetState()], GetText());
	__super::DrawChildren(dc, sx, sy);
}

///////////////////////////////////////////////////////////////////////////////
// ImageButton class implementation

ImageButton* ImageButton::Create(Window *parent, float x, float y, const char *texture)
{
	ImageButton *res = new ImageButton(parent);
	res->Move(x, y);
	res->SetTexture(texture, true);
	return res;
}

ImageButton::ImageButton(Window *parent)
  : ButtonBase(parent)
{
}

void ImageButton::OnChangeState(State state)
{
	SetFrame(state);
}

///////////////////////////////////////////////////////////////////////////////
// CheckBox class implementation

CheckBox* CheckBox::Create(Window *parent, float x, float y, const string_t &text)
{
	CheckBox *res = new CheckBox(parent);
	res->Move(x, y);
	res->SetText(text);
	return res;
}

CheckBox::CheckBox(Window *parent)
  : ButtonBase(parent)
  , _boxTexture(GetManager()->GetTextureManager()->FindSprite("ui/checkbox"))
  , _fontTexture(GetManager()->GetTextureManager()->FindSprite("font_small"))
  , _isChecked(false)
  , _drawShadow(true)
{
	SetTexture(NULL, false);
	AlignSizeToContent();
}

void CheckBox::AlignSizeToContent()
{
	TextureManager *dc = GetManager()->GetTextureManager();
	float th = dc->GetFrameHeight(_fontTexture, 0);
	float tw = dc->GetFrameWidth(_fontTexture, 0);
	float bh = dc->GetFrameHeight(_boxTexture, GetFrame());
	float bw = dc->GetFrameWidth(_boxTexture, GetFrame());
	Resize(bw + (tw - 1) * (float) GetText().length(), std::max(th + 1, bh));
}

void CheckBox::SetCheck(bool checked)
{
	_isChecked = checked;
	SetFrame(_isChecked ? GetState()+4 : GetState());
}

void CheckBox::OnClick()
{
	SetCheck(!GetCheck());
}

void CheckBox::OnTextChange()
{
	AlignSizeToContent();
}

void CheckBox::OnChangeState(State state)
{
	SetFrame(_isChecked ? state+4 : state);
}

void CheckBox::DrawChildren(const DrawingContext *dc, float sx, float sy) const
{
	float bh = dc->GetFrameHeight(_boxTexture, GetFrame());
	float bw = dc->GetFrameWidth(_boxTexture, GetFrame());
	float th = dc->GetFrameHeight(_fontTexture, 0);

	FRECT box = {sx, sy + (GetHeight() - bh) / 2, sx + bw, sy + (GetHeight() - bh) / 2 + bh};
	dc->DrawSprite(&box, _boxTexture, GetBackColor(), GetFrame());

	// grep 'enum State'
	SpriteColor colors[] = 
	{
		SpriteColor(0xffffffff), // Normal
		SpriteColor(0xffffffff), // Hottrack
		SpriteColor(0xffffffff), // Pushed
		SpriteColor(0xffffffff), // Disabled
	};
	if( _drawShadow && stateDisabled != GetState() )
	{
		dc->DrawBitmapText(sx + bw + 1, sy + (GetHeight() - th) / 2 + 1, _fontTexture, 0xff000000, GetText());
	}
	dc->DrawBitmapText(sx + bw, sy + (GetHeight() - th) / 2, _fontTexture, colors[GetState()], GetText());

	__super::DrawChildren(dc, sx, sy);
}


///////////////////////////////////////////////////////////////////////////////
} // end of namespace UI

// end of file

