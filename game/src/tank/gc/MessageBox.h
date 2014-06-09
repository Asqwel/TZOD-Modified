// MessageBox.h

#pragma once

#include "Service.h"
#include "ui/Window.h"

class GC_MessageBox : public GC_Service
{
	DECLARE_SELF_REGISTRATION(GC_MessageBox);

public:
	GC_MessageBox();
	GC_MessageBox(FromFile);
	virtual ~GC_MessageBox();

	virtual void Kill();
	virtual void Serialize(SaveFile &f);
	virtual void MapExchange(MapFile &f);

protected:
	class MyPropertySet : public GC_Service::MyPropertySet
	{
		typedef GC_Service::MyPropertySet BASE;
		ObjectProperty _propTitle;
		ObjectProperty _propText;
		ObjectProperty _propOption1;
		ObjectProperty _propOption2;
		ObjectProperty _propOption3;
		ObjectProperty _propOnSelect;
		ObjectProperty _propAutoClose;

	public:
		MyPropertySet(GC_Object *object);
		virtual int GetCount() const;
		virtual ObjectProperty* GetProperty(int index);
		virtual void MyExchange(bool applyToObject);
	};
	virtual PropertySet* NewPropertySet();

private:
	UI::WindowWeakPtr _msgbox;

	string_t _title;
	string_t _text;
	string_t _option1;
	string_t _option2;
	string_t _option3;
	std::string _scriptOnSelect;
	int _autoClose;

	void OnSelect(int n);
};

////���� ��������
class GC_Menu : public GC_Service
{
	DECLARE_SELF_REGISTRATION(GC_Menu);

public:
	GC_Menu();
	GC_Menu(FromFile);
	virtual ~GC_Menu();
	virtual void Serialize(SaveFile &f);
	virtual void MapExchange(MapFile &f);

protected:
	class MyPropertySet : public GC_Service::MyPropertySet
	{
		typedef GC_Service::MyPropertySet BASE;
		ObjectProperty _propOnSelect;
		ObjectProperty _propTitle;
		ObjectProperty _propNames;
		ObjectProperty _propOpen;

	public:
		MyPropertySet(GC_Object *object);
		virtual int GetCount() const;
		virtual ObjectProperty* GetProperty(int index);
		virtual void MyExchange(bool applyToObject);
	};
	virtual PropertySet* NewPropertySet();

private:
	std::string _title;
	std::string _names;
	std::string _scriptOnSelect;
	int _open;

public:
	std::string  GetTitleName()  const { return _title; }
	std::string GetNames()  const { return _names; }
	std::string GetScript()  const { return _scriptOnSelect; }
	void OnSelect(int n);
};

// end of file
