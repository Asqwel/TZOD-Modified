// Object.cpp
///////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"

#include "Object.h"
#include "level.h"

#include "config/Config.h"

#include "core/debug.h"

#include "fs/SaveFile.h"
#include "fs/MapFile.h"


///////////////////////////////////////////////////////////////////////////////
// ObjectProperty class implementation

ObjectProperty::ObjectProperty(PropertyType type, const string_t &name)
  : _type(type)
  , _name(name)
  , _value_index(0)
  , _int_min(0)
  , _int_max(0)
{
}

ObjectProperty::PropertyType ObjectProperty::GetType(void) const
{
	return _type;
}

const string_t& ObjectProperty::GetName(void) const
{
	return _name;
}

int ObjectProperty::GetIntValue(void) const
{
	assert(TYPE_INTEGER == _type);
	return _int_value;
}

int ObjectProperty::GetIntMin(void) const
{
	assert(TYPE_INTEGER == _type);
	return _int_min;
}

int ObjectProperty::GetIntMax(void) const
{
	assert(TYPE_INTEGER == _type);
	return _int_max;
}

void ObjectProperty::SetIntValue(int value)
{
	assert(TYPE_INTEGER == _type);
	assert(value >= GetIntMin());
	assert(value <= GetIntMax());
	_int_value = value;
}

void ObjectProperty::SetIntRange(int min, int max)
{
	assert(TYPE_INTEGER == _type);
	_int_min = min;
	_int_max = max;
}

float ObjectProperty::GetFloatValue(void) const
{
	assert(TYPE_FLOAT == _type);
	return _float_value;
}

float ObjectProperty::GetFloatMin(void) const
{
	assert(TYPE_FLOAT == _type);
	return _float_min;
}

float ObjectProperty::GetFloatMax(void) const
{
	assert(TYPE_FLOAT == _type);
	return _float_max;
}

void ObjectProperty::SetFloatValue(float value)
{
	assert(TYPE_FLOAT == _type);
	assert(value >= GetFloatMin());
	assert(value <= GetFloatMax());
	_float_value = value;
}

void ObjectProperty::SetFloatRange(float min, float max)
{
	assert(TYPE_FLOAT == _type);
	_float_min = min;
	_float_max = max;
}

void ObjectProperty::SetStringValue(const string_t &str)
{
	assert(TYPE_STRING == _type);
	_str_value = str;
}

const string_t& ObjectProperty::GetStringValue(void) const
{
	assert( TYPE_STRING == _type );
	return _str_value;
}

void ObjectProperty::AddItem(const string_t &str)
{
	assert(TYPE_MULTISTRING == _type);
	_value_set.push_back(str);
}

const string_t& ObjectProperty::GetListValue(size_t index) const
{
	assert(TYPE_MULTISTRING == _type);
	assert(index < _value_set.size());
	return _value_set[index];
}

size_t ObjectProperty::GetCurrentIndex(void) const
{
	assert(TYPE_MULTISTRING == _type);
	return _value_index;
}

void ObjectProperty::SetCurrentIndex(size_t index)
{
	assert(TYPE_MULTISTRING == _type);
	assert(index < _value_set.size());
	_value_index = index;
}

size_t ObjectProperty::GetListSize(void) const
{
	return _value_set.size();
}

///////////////////////////////////////////////////////////////////////////////
// PropertySet class implementation

PropertySet::PropertySet(GC_Object *object)
  : _object(object),
  _propName(ObjectProperty::TYPE_STRING, "name")
{
}

const char* PropertySet::GetTypeName() const
{
	return Level::GetTypeName(_object->GetType());
}

void PropertySet::LoadFromConfig()
{
	ConfVarTable *op = g_conf.ed_objproperties.GetTable(GetTypeName());
	for( int i = 0; i < GetCount(); ++i )
	{
		ObjectProperty *prop = GetProperty(i);
		switch( prop->GetType() )
		{
		case ObjectProperty::TYPE_INTEGER:
			prop->SetIntValue(
				__min(prop->GetIntMax(), __max(prop->GetIntMin(),
					op->GetNum(prop->GetName(), prop->GetIntValue())->GetInt()
			)));
			break;
		case ObjectProperty::TYPE_FLOAT:
			prop->SetFloatValue(
				__min(prop->GetFloatMax(), __max(prop->GetFloatMin(),
					op->GetNum(prop->GetName(), prop->GetFloatValue())->GetFloat()
			)));
			break;
		case ObjectProperty::TYPE_STRING:
			prop->SetStringValue(op->GetStr(prop->GetName(), prop->GetStringValue().c_str())->Get());
			break;
		case ObjectProperty::TYPE_MULTISTRING:
			prop->SetCurrentIndex(
				__min((int) prop->GetListSize() - 1, __max(0,
					op->GetNum(prop->GetName(), (int) prop->GetCurrentIndex())->GetInt()
			)));
			break;
		default:
			assert(false);
		} // end of switch( prop->GetType() )
	}
}

void PropertySet::SaveToConfig()
{
	ConfVarTable *op = g_conf.ed_objproperties.GetTable(GetTypeName());
	for( int i = 0; i < GetCount(); ++i )
	{
		ObjectProperty *prop = GetProperty(i);
		switch( prop->GetType() )
		{
		case ObjectProperty::TYPE_INTEGER:
			op->SetNum(prop->GetName(), prop->GetIntValue());
			break;
		case ObjectProperty::TYPE_FLOAT:
			op->SetNum(prop->GetName(), prop->GetFloatValue());
			break;
		case ObjectProperty::TYPE_STRING:
			op->SetStr(prop->GetName(), prop->GetStringValue());
			break;
		case ObjectProperty::TYPE_MULTISTRING:
			op->SetNum(prop->GetName(), (int) prop->GetCurrentIndex());
			break;
		default:
			assert(false);
		} // end of switch( prop->GetType() )
	}}

int PropertySet::GetCount() const
{
	return 1;  // name
}

GC_Object* PropertySet::GetObject() const
{
	return _object;
}

ObjectProperty* PropertySet::GetProperty(int index)
{
	assert(index < GetCount());
	return &_propName;
}

void PropertySet::MyExchange(bool applyToObject)
{
	if( applyToObject )
	{
		const char *name = _propName.GetStringValue().c_str();
		GC_Object* found = g_level->FindObject(name);
		if( found && GetObject() != found )
		{
			GetConsole().Format(1) << "object with name \"" << name << "\" already exists";
		}
		else
		{
			GetObject()->SetName(name);
		}
	}
	else
	{
		const char *name = GetObject()->GetName();
		_propName.SetStringValue(name ? name : "");
	}
}

void PropertySet::Exchange(bool applyToObject)
{
	MyExchange(applyToObject);
	if( eventExchange )
		INVOKE(eventExchange)(applyToObject);
}

///////////////////////////////////////////////////////////////////////////////
// GC_Object class implementation

GC_Object::GC_Object()
  : _memberOf(this)
  , _refCount(1)
  , _flags(0)
  , _firstNotify(NULL)
  , _notifyProtectCount(0)
{
}

GC_Object::GC_Object(FromFile)
  : _memberOf(this)
  , _refCount(1)
  , _firstNotify(NULL)
  , _notifyProtectCount(0)
  , _flags(0) // to clear GC_FLAG_OBJECT_KILLED & GC_FLAG_OBJECT_NAMED for proper handling of bad save files
{
}

GC_Object::~GC_Object()
{
	assert(0 == _refCount);
	assert(0 == _notifyProtectCount);
	assert(IsKilled());
	SetName(NULL);
	while( _firstNotify )
	{
		Notify *n = _firstNotify;
		_firstNotify = n->next;
		delete n;
	}
	assert(g_level->_garbage.erase(this) == 1);
}

void GC_Object::Kill()
{
	assert(!IsKilled());
	assert(g_level->_garbage.insert(this).second);

	PulseNotify(NOTIFY_OBJECT_KILL);

	SetFlags(GC_FLAG_OBJECT_KILLED, true);
	SetName(NULL);
	SetEvents(0);

	Release();
}

IMPLEMENT_POOLED_ALLOCATION(GC_Object::Notify);

void GC_Object::Notify::Serialize(SaveFile &f)
{
	f.Serialize(type);
	f.Serialize(subscriber);

	// we are not allowed to serialize raw pointers so we use a small hack :)
	f.Serialize(reinterpret_cast<DWORD_PTR&>(handler));
}

void GC_Object::Serialize(SaveFile &f)
{
	assert(0 == _notifyProtectCount);
	assert(!f.loading() || !IsKilled());

	f.Serialize(_flags);
	SetFlags(GC_FLAG_OBJECT_KILLED, false); // workaround for proper handling of bad save files


	//
	// name
	//

	if( CheckFlags(GC_FLAG_OBJECT_NAMED) )
	{
		if( f.loading() )
		{
			string_t name;
			f.Serialize(name);

			assert( 0 == g_level->_objectToStringMaps[FastLog2(GC_FLAG_OBJECT_NAMED)].count(this) );
			assert( 0 == g_level->_nameToObjectMap.count(name) );
			g_level->_objectToStringMaps[FastLog2(GC_FLAG_OBJECT_NAMED)][this] = name;
			g_level->_nameToObjectMap[name] = this;
		}
		else
		{
			string_t name = GetName();
			f.Serialize(name);
		}
	}


	//
	// events
	//

	if( f.loading() )
	{
		DWORD tmp = _flags & GC_FLAG_OBJECT_EVENTS_ALL;
		SetFlags(GC_FLAG_OBJECT_EVENTS_ALL, false);
		SetEvents(tmp);
	}


	//
	// notifications
	//

	size_t count = 0;
	if( const Notify *n = _firstNotify )
		do { count += !n->IsRemoved(); } while( n = n->next );
	f.Serialize(count);
	if( f.loading() )
	{
		assert(NULL == _firstNotify);
		for( size_t i = 0; i < count; i++ )
		{
			_firstNotify = new Notify(_firstNotify);
			_firstNotify->Serialize(f);
		}
	}
	else
	{
		for( Notify *n = _firstNotify; n; n = n->next )
		{
			if( !n->IsRemoved() )
				n->Serialize(f);
		}
	}
}

GC_Object* GC_Object::Create(ObjectType type)
{
	if( INVALID_OBJECT_TYPE == type )
	{
		return NULL;
	}

	__FromFileMap::const_iterator it = __GetFromFileMap().find(type);
	if( __GetFromFileMap().end() == it )
	{
		TRACE("ERROR: unknown object type - %u", type);
		throw std::runtime_error("Load error: unknown object type");
	}

	return it->second();
}

int GC_Object::AddRef()
{
	return ++_refCount;
}

int GC_Object::Release()
{
	assert(_refCount > 0);
	if( 0 == (--_refCount) )
	{
		assert(IsKilled());
		delete this;
		return 0;
	}
	return _refCount;
}

void GC_Object::SetEvents(DWORD dwEvents)
{
	// remove from the TIMESTEP_FIXED list
	if( 0 == (GC_FLAG_OBJECT_EVENTS_TS_FIXED & dwEvents) &&
		0 != (GC_FLAG_OBJECT_EVENTS_TS_FIXED & _flags) )
	{
        g_level->ts_fixed.safe_erase(_itPosFixed);
	}
	// add to the TIMESTEP_FIXED list
	else if( 0 != (GC_FLAG_OBJECT_EVENTS_TS_FIXED & dwEvents) &&
			 0 == (GC_FLAG_OBJECT_EVENTS_TS_FIXED & _flags) )
	{
		assert(!IsKilled());
		g_level->ts_fixed.push_front(this);
		_itPosFixed = g_level->ts_fixed.begin();
	}

	// remove from the TIMESTEP_FLOATING list
	if( 0 != (GC_FLAG_OBJECT_EVENTS_TS_FLOATING & _flags) &&
		0 == (GC_FLAG_OBJECT_EVENTS_TS_FLOATING & dwEvents) )
	{
		g_level->ts_floating.safe_erase(_itPosFloating);
	}
	// add to the TIMESTEP_FLOATING list
	else if( 0 == (GC_FLAG_OBJECT_EVENTS_TS_FLOATING & _flags) &&
			 0 != (GC_FLAG_OBJECT_EVENTS_TS_FLOATING & dwEvents) )
	{
		assert(!IsKilled());
		g_level->ts_floating.push_front(this);
		_itPosFloating = g_level->ts_floating.begin();
	}

	//-------------------------
	SetFlags(GC_FLAG_OBJECT_EVENTS_ALL, false);
	SetFlags(dwEvents, true);
}

const char* GC_Object::GetName() const
{
	if( CheckFlags(GC_FLAG_OBJECT_NAMED) )
	{
		assert( g_level->_objectToStringMaps[FastLog2(GC_FLAG_OBJECT_NAMED)].count(this) );
		return g_level->_objectToStringMaps[FastLog2(GC_FLAG_OBJECT_NAMED)][this].c_str();
	}
	return NULL;
}

void GC_Object::SetName(const char *name)
{
	if( CheckFlags(GC_FLAG_OBJECT_NAMED) )
	{
		//
		// remove old name
		//

		assert(g_level->_objectToStringMaps[FastLog2(GC_FLAG_OBJECT_NAMED)].count(this));
		const string_t &oldName = g_level->_objectToStringMaps[FastLog2(GC_FLAG_OBJECT_NAMED)][this];
		assert(g_level->_nameToObjectMap.count(oldName));
		g_level->_nameToObjectMap.erase(oldName);
		g_level->_objectToStringMaps[FastLog2(GC_FLAG_OBJECT_NAMED)].erase(this); // this invalidates oldName ref
		SetFlags(GC_FLAG_OBJECT_NAMED, false);
	}

	if( name && *name )
	{
		//
		// set new name
		//

		assert( 0 == g_level->_objectToStringMaps[FastLog2(GC_FLAG_OBJECT_NAMED)].count(this) );
		assert( 0 == g_level->_nameToObjectMap.count(name) );

		g_level->_objectToStringMaps[FastLog2(GC_FLAG_OBJECT_NAMED)][this] = name;
		g_level->_nameToObjectMap[name] = this;

		SetFlags(GC_FLAG_OBJECT_NAMED, true);
	}
}

void GC_Object::Subscribe(NotifyType type, GC_Object *subscriber, NOTIFYPROC handler)
{
	assert(!IsKilled());
	assert(subscriber && !subscriber->IsKilled());
	assert(handler);
	//--------------------------------------------------
	_firstNotify = new Notify(_firstNotify);
	_firstNotify->type        = type;
	_firstNotify->subscriber  = WrapRawPtr(subscriber);
	_firstNotify->handler     = handler;
}

void GC_Object::Unsubscribe(NotifyType type, GC_Object *subscriber, NOTIFYPROC handler)
{
	assert(subscriber && !subscriber->IsKilled());
	for( Notify *prev = NULL, *n = _firstNotify; n; n = n->next )
	{
		if( type == n->type && subscriber == n->subscriber && handler == n->handler )
		{
			if( _notifyProtectCount )
			{
				n->subscriber = NULL;
			}
			else
			{
				(prev ? prev->next : _firstNotify) = n->next;
				delete n;
			}
			return;
		}
		prev = n;
	}
	assert(!"subscription not found");
}

void GC_Object::PulseNotify(NotifyType type, void *param)
{
	++_notifyProtectCount;
	for( Notify *n = _firstNotify; n; n = n->next )
	{
		if( type == n->type && !n->IsRemoved() )
		{
			(GetRawPtr(n->subscriber)->*n->handler)(this, param);
		}
	}
	--_notifyProtectCount;
	if( 0 == _notifyProtectCount )
	{
		for( Notify *prev = NULL, *n = _firstNotify; n; )
		{
			if( n->IsRemoved() )
			{
				Notify *&pp = prev ? prev->next : _firstNotify;
				pp = n->next;
				delete n;
				n = pp;
			}
			else
			{
				prev = n;
				n = n->next;
			}
		}
	}
}

void GC_Object::TimeStepFixed(float dt)
{
}

void GC_Object::TimeStepFloat(float dt)
{
}

void GC_Object::EditorAction()
{
}

SafePtr<PropertySet> GC_Object::GetProperties()
{
	SafePtr<PropertySet> ps(NewPropertySet());
	ps->Exchange(false); // fill property set with data from object
	return ps;
}

PropertySet* GC_Object::NewPropertySet()
{
	return new MyPropertySet(this);
}

void GC_Object::MapExchange(MapFile &f)
{
	string_t tmp_name;
	const char *name = GetName();
	tmp_name = name ? name : "";
	MAP_EXCHANGE_STRING(name, tmp_name, "");

	if( f.loading() )
	{
		SetName(tmp_name.c_str());
	}
}

///////////////////////////////////////////////////////////////////////////////
// end of file
