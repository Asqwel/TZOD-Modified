// Weapons.cpp

#include "stdafx.h"

#include "Weapons.h"

#include "Vehicle.h"
#include "RigidBodyDinamic.h"
#include "Sound.h"
#include "Light.h"
#include "Player.h"
#include "Indicators.h"
#include "Projectiles.h"
#include "Particles.h"

#include "Macros.h"
#include "Level.h"
#include "functions.h"
#include "script.h"

#include "fs/MapFile.h"
#include "fs/SaveFile.h"

///////////////////////////////////////////////////////////////////////////////


PropertySet* GC_Weapon::NewPropertySet()
{
	return new MyPropertySet(this);
}

GC_Weapon::MyPropertySet::MyPropertySet(GC_Object *object)
  : BASE(object)
  , _propTimeStay(ObjectProperty::TYPE_INTEGER, "stay_time")
  , _propOnFire(  ObjectProperty::TYPE_STRING,  "on_fire"  )
  , _propDir(	  ObjectProperty::TYPE_FLOAT, 	"dir"	   )
{
	_propTimeStay.SetIntRange(0, 1000000);
	_propDir.SetFloatRange(0, PI2);
}

int GC_Weapon::MyPropertySet::GetCount() const
{
	return BASE::GetCount() + 3;
}

ObjectProperty* GC_Weapon::MyPropertySet::GetProperty(int index)
{
	if( index < BASE::GetCount() )
		return BASE::GetProperty(index);

	switch( index - BASE::GetCount() )
	{
	case 0: return &_propTimeStay;
	case 1: return &_propOnFire;
	case 2: return &_propDir;
	}

	assert(false);
	return NULL;
}

void GC_Weapon::MapExchange(MapFile &f)
{
	GC_Pickup::MapExchange(f);

	MAP_EXCHANGE_STRING(on_fire, _scriptOnFire, "");
	MAP_EXCHANGE_FLOAT (dir,	 _angleNormal,  GetAngle());
}

void GC_Weapon::MyPropertySet::MyExchange(bool applyToObject)
{
	BASE::MyExchange(applyToObject);

	GC_Weapon *obj = static_cast<GC_Weapon*>(GetObject());
	if( applyToObject )
	{
		obj->_timeStay = (float) _propTimeStay.GetIntValue() / 1000.0f;
		obj->_scriptOnFire = _propOnFire.GetStringValue();
		obj->_angleNormal = _propDir.GetFloatValue();
		obj->SetAngle(obj->_angleNormal);
	}
	else
	{
		_propTimeStay.SetIntValue(int(obj->_timeStay * 1000.0f + 0.5f));
		_propOnFire.SetStringValue(obj->_scriptOnFire);
		obj->_angleNormal = obj->GetAngle();
		_propDir.SetFloatValue(obj->_angleNormal);
	}
}


GC_Weapon::GC_Weapon(float x, float y)
  : GC_Pickup(x, y)
  , _rotatorWeap(_angleReal)
  , _advanced(false)
  , _feTime(1.0f)
  , _feOrient(1,0)
  , _fePos(0,0)
  , _time(0)
  , _timeStay(15.0f)
  , _timeReload(0)
  , _fixmeChAnimate(true)
  , _angleNormal(GetAngle())
{
	SetRespawnTime(GetDefaultRespawnTime());
	SetAutoSwitch(false);
}

AIPRIORITY GC_Weapon::GetPriority(GC_Vehicle *veh)
{
	if( veh->GetWeapon() )
	{
		if( veh->GetWeapon()->_advanced )
			return AIP_NOTREQUIRED;

		if( _advanced )
			return AIP_WEAPON_ADVANCED;
		else
			return AIP_NOTREQUIRED;
	}

	return AIP_WEAPON_NORMAL + _advanced ? AIP_WEAPON_ADVANCED : AIP_NOTREQUIRED;
}

void GC_Weapon::Attach(GC_Actor *actor)
{
	assert(dynamic_cast<GC_Vehicle*>(actor));
	GC_Vehicle *veh = static_cast<GC_Vehicle*>(actor);

	GC_Pickup::Attach(actor);


	SetZ(Z_ATTACHED_ITEM);

	_rotateSound = WrapRawPtr(new GC_Sound(SND_TowerRotate, SMODE_STOP, GetPos()));
	_rotatorWeap.reset(0, 0, TOWER_ROT_SPEED, TOWER_ROT_ACCEL, TOWER_ROT_SLOWDOWN);

	SetVisible(true);
	SetBlinking(false);

	SetCrosshair();
	if( _crosshair )
	{
		if( GC_Vehicle *veh = dynamic_cast<GC_Vehicle*>(GetCarrier()) )
		{
			_crosshair->SetVisible(NULL != dynamic_cast<GC_PlayerLocal*>(veh->GetOwner()));
		}
		else
		{
			_crosshair->SetVisible(false);
		}
	}


	PLAY(SND_w_Pickup, GetPos());

	_fireEffect = WrapRawPtr(new GC_2dSprite());
	_fireEffect->SetZ(Z_EXPLODE);
	_fireEffect->SetVisible(false);

	_fireLight = WrapRawPtr(new GC_Light(GC_Light::LIGHT_POINT));
	_fireLight->SetActive(false);
}

void GC_Weapon::Detach()
{
	SAFE_KILL(_rotateSound);
	SAFE_KILL(_crosshair);
	SAFE_KILL(_fireEffect);
	SAFE_KILL(_fireLight);

	_time = 0;

	GC_Pickup::Detach();
}

void GC_Weapon::ProcessRotate(float dt)
{
	assert(GetCarrier());
	_rotatorWeap.process_dt(dt);
	const VehicleState &vs = static_cast<GC_Vehicle*>(GetCarrier())->_stateReal;
	if( vs._bExplicitTower )
	{
		_rotatorWeap.rotate_to( vs._fTowerAngle );
	}
	else
	{
		if( vs._bState_TowerCenter )
			_rotatorWeap.rotate_to( 0.0f );
		else if( vs._bState_TowerLeft )
			_rotatorWeap.rotate_left();
		else if( vs._bState_TowerRight )
			_rotatorWeap.rotate_right();
		else if( RS_GETTING_ANGLE != _rotatorWeap.GetState() )
			_rotatorWeap.stop();
	}
	_rotatorWeap.setup_sound(GetRawPtr(_rotateSound));

	vec2d a(_angleReal);
	_directionReal = Vec2dAddDirection(static_cast<GC_Vehicle*>(GetCarrier())->GetDirection(), a);
	vec2d directionVisual = Vec2dAddDirection(static_cast<GC_Vehicle*>(GetCarrier())->GetVisual()->GetDirection(), a);
	SetDirection(directionVisual);
	if( _fireEffect->GetVisible() )
	{
		int frame = int( _time / _feTime * (float) _fireEffect->GetFrameCount() );
		if( frame < _fireEffect->GetFrameCount() )
		{
			float op = 1.0f - pow(_time / _feTime, 2);

			_fireEffect->SetFrame(frame);
			_fireEffect->SetDirection(Vec2dAddDirection(directionVisual, _feOrient));
			_fireEffect->SetOpacity(op);

			_fireEffect->MoveTo(GetPosPredicted() + vec2d(_fePos * directionVisual, _fePos.x*directionVisual.y - _fePos.y*directionVisual.x));
			_fireLight->MoveTo(_fireEffect->GetPos());
			_fireLight->SetIntensity(op);
			_fireLight->SetActive(true);
		}
		else
		{
			_fireEffect->SetFrame(0);
			_fireEffect->SetVisible(false);
			_fireLight->SetActive(false);
		}
	}

	OnUpdateView();
}

void GC_Weapon::SetCrosshair()
{
	_crosshair = WrapRawPtr(new GC_2dSprite());
	_crosshair->SetTexture("indicator_crosshair1");
	_crosshair->SetZ(Z_VEHICLE_LABEL);
}

GC_Weapon::GC_Weapon(FromFile)
  : GC_Pickup(FromFile())
  , _rotatorWeap(_angleReal)
{
}

GC_Weapon::~GC_Weapon()
{
}

void GC_Weapon::Serialize(SaveFile &f)
{
	GC_Pickup::Serialize(f);

	_rotatorWeap.Serialize(f);

	f.Serialize(_directionReal);
	f.Serialize(_angleReal);
	f.Serialize(_angleNormal);
	f.Serialize(_advanced);
	f.Serialize(_feOrient);
	f.Serialize(_fePos);
	f.Serialize(_feTime);
	f.Serialize(_time);
	f.Serialize(_timeStay);
	f.Serialize(_timeReload);
	f.Serialize(_crosshair);
	f.Serialize(_fireEffect);
	f.Serialize(_fireLight);
	f.Serialize(_rotateSound);
	f.Serialize(_fixmeChAnimate);
	f.Serialize(_scriptOnFire);
}

void GC_Weapon::Kill()
{
	if( GetCarrier() )
	{
		Detach();
	}
	assert(!_crosshair);
	assert(!_rotateSound);
	assert(!_fireEffect);

	GC_Pickup::Kill();
}

void GC_Weapon::TimeStepFixed(float dt)
{
	GC_Pickup::TimeStepFixed(dt);

	_time += dt;

	if( GetCarrier() )
	{
		ProcessRotate(dt);
		if  (!GetCarrier()->GetOwner()) //���� ��� ���������, ����� ������
		{
			if( _crosshair->GetVisible())
				_crosshair->SetVisible(false);
			_rotatorWeap.rotate_to( 0.0f );
		} 
		if( _crosshair && _fixmeChAnimate )
		{
			_crosshair->MoveTo(GetPosPredicted() + GetDirection() * CH_DISTANCE_NORMAL);
			_crosshair->SetDirection(vec2d(_time * 5));
		}
	}
	else
	{
		if( GetRespawn() && GetVisible() )
		{
			SetBlinking(_time > _timeStay - 3.0f);
			if( _time > _timeStay )
			{
				SetBlinking(false);
				Disappear();
			}
		}
	}
}

void GC_Weapon::TimeStepFloat(float dt)
{
	GC_Pickup::TimeStepFloat(dt);
	if( !GetCarrier() && !GetRespawn() )
	{
		SetDirection(vec2d(GetTimeAnimation()));
	}
}

void GC_Weapon::OnFire(GC_Projectile *proj)
{
	if( !_scriptOnFire.empty() )
	{
		if (!proj)
		{
			std::stringstream buf;
			buf << "return function(self)";
			buf << _scriptOnFire;
			buf << "\nend";

			if( luaL_loadstring(g_env.L, buf.str().c_str()) )
			{
				GetConsole().Printf(1, "OnFire: %s", lua_tostring(g_env.L, -1));
				lua_pop(g_env.L, 1); // pop the error message from the stack
			}
			else
			{
				if( lua_pcall(g_env.L, 0, 1, 0) )
				{
					GetConsole().WriteLine(1, lua_tostring(g_env.L, -1));
					lua_pop(g_env.L, 1); // pop the error message from the stack
				}
				else
				{
					luaT_pushobject(g_env.L, this);
					if( lua_pcall(g_env.L, 1, 0, 0) )
					{
						GetConsole().WriteLine(1, lua_tostring(g_env.L, -1));
						lua_pop(g_env.L, 1); // pop the error message from the stack
					}
				}
			}
		}
		else
		{
			std::stringstream buf;
			buf << "return function(self,proj)";
			buf << _scriptOnFire;
			buf << "\nend";

			if( luaL_loadstring(g_env.L, buf.str().c_str()) )
			{
				GetConsole().Printf(1, "OnFire: %s", lua_tostring(g_env.L, -1));
				lua_pop(g_env.L, 1); // pop the error message from the stack
			}
			else
			{
				if( lua_pcall(g_env.L, 0, 1, 0) )
				{
					GetConsole().WriteLine(1, lua_tostring(g_env.L, -1));
					lua_pop(g_env.L, 1); // pop the error message from the stack
				}
				else
				{
					luaT_pushobject(g_env.L, this);
					luaT_pushobject(g_env.L, proj);
					if( lua_pcall(g_env.L, 2, 0, 0) )
					{
						GetConsole().WriteLine(1, lua_tostring(g_env.L, -1));
						lua_pop(g_env.L, 1); // pop the error message from the stack
					}
				}
			}
		}
	}
}

void GC_Weapon::OnFire(void)
{
	if( !_scriptOnFire.empty() )
	{
		std::stringstream buf;
		buf << "return function(self)";
		buf << _scriptOnFire;
		buf << "\nend";

		if( luaL_loadstring(g_env.L, buf.str().c_str()) )
		{
			GetConsole().Printf(1, "OnFire: %s", lua_tostring(g_env.L, -1));
			lua_pop(g_env.L, 1); // pop the error message from the stack
		}
		else
		{
			if( lua_pcall(g_env.L, 0, 1, 0) )
			{
				GetConsole().WriteLine(1, lua_tostring(g_env.L, -1));
				lua_pop(g_env.L, 1); // pop the error message from the stack
			}
			else
			{
				luaT_pushobject(g_env.L, this);
				if( lua_pcall(g_env.L, 1, 0, 0) )
				{
					GetConsole().WriteLine(1, lua_tostring(g_env.L, -1));
					lua_pop(g_env.L, 1); // pop the error message from the stack
				}
			}
		}
	}
}

float GC_Weapon::GetAngle() const
{
	return GetDirection().Angle();
}

void GC_Weapon::SetAngle(float _angle)
{
	if (GetCarrier())
	{
		float vehAngle = reinterpret_cast<GC_Vehicle *>(GetCarrier())->GetAngle();
		float deltaAngle;
		if (vehAngle > _angle)
			deltaAngle = PI2 - vehAngle + _angle;
		else
			deltaAngle = _angle - vehAngle;

		_rotatorWeap.rotate_to(deltaAngle);
		SetDirection(vec2d(deltaAngle));
		_angleNormal = deltaAngle;		
	}
	else
	{
		_rotatorWeap.rotate_to(_angle);
		SetDirection(vec2d(_angle));
		_angleNormal = _angle;
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////

IMPLEMENT_SELF_REGISTRATION(GC_Weap_RocketLauncher)
{
	ED_ITEM("weap_rockets", "obj_weap_rockets", 4 );
	return true;
}

void GC_Weap_RocketLauncher::Attach(GC_Actor *actor)
{
	GC_Weapon::Attach(actor);

	_timeReload = 2.0f;
	_time       = _timeReload;

	_reloaded         = true;
	_firing           = false;
	_nshots           = 0;
	_nshots_total     = 6;
	_time_shot        = 0.13f;

	_fireEffect->SetTexture("particle_fire3");

//return;
//	veh->SetMaxHP(85);

//	veh->_ForvAccel = 300;
//	veh->_BackAccel = 200;
//	veh->_StopAccel = 500;

//	veh->_rotator.setl(3.5f, 10.0f, 30.0f);

//	veh->_MaxBackSpeed = 150;
//	veh->_MaxForvSpeed = 150;
}

void GC_Weap_RocketLauncher::Detach()
{
	_firing = false;
	GC_Weapon::Detach();
}

GC_Weap_RocketLauncher::GC_Weap_RocketLauncher(float x, float y)
  : GC_Weapon(x, y)
  , _firing(false)
{
	_feTime = 0.1f;
	SetTexture("weap_ak47");
}

GC_Weap_RocketLauncher::GC_Weap_RocketLauncher(FromFile)
  : GC_Weapon(FromFile())
{
}

void GC_Weap_RocketLauncher::Serialize(SaveFile &f)
{
	GC_Weapon::Serialize(f);
	f.Serialize(_firing);
	f.Serialize(_reloaded);
	f.Serialize(_nshots);
	f.Serialize(_nshots_total);
	f.Serialize(_time_shot);
}

void GC_Weap_RocketLauncher::Fire()
{
	assert(GetCarrier());
	const vec2d &dir = GetDirectionReal();
	if( _advanced )
	{
		if( _time >= _time_shot )
		{
			float dy = (((float)(g_level->net_rand()%(_nshots_total+1)) - 0.5f) / (float)_nshots_total - 0.5f) * 18.0f;
			_fePos.Set(13, dy);

			float ax = dir.x * 15.0f + dy * dir.y;
			float ay = dir.y * 15.0f - dy * dir.x;

			GC_Projectile *proj = new GC_Rocket(GetCarrier()->GetPos() + vec2d(ax, ay),
			              Vec2dAddDirection(dir, vec2d(g_level->net_frand(0.1f) - 0.05f)) * SPEED_ROCKET,
			              GetCarrier(), GetCarrier()->GetOwner(), _advanced);

			_time   = 0;
			_nshots = 0;
			_firing = false;
			
			GC_Weapon::OnFire(proj);

			_fireEffect->SetVisible(true);
		}
	}
	else
	{
		if( _firing )
		{
			if( _time >= _time_shot )
			{
				_nshots++;

				float dy = (((float)_nshots - 0.5f) / (float)_nshots_total - 0.5f) * 18.0f;
				_fePos.Set(13, dy);

				if( _nshots == _nshots_total )
				{
					_firing = false;
					_nshots = 0;
				}

				float ax = dir.x * 15.0f + dy * dir.y;
				float ay = dir.y * 15.0f - dy * dir.x;

				GC_Projectile *proj = new GC_Rocket( GetCarrier()->GetPos() + vec2d(ax, ay),
				               Vec2dAddDirection(dir, vec2d(g_level->net_frand(0.1f) - 0.05f)) * SPEED_ROCKET,
				               GetCarrier(), GetCarrier()->GetOwner(), _advanced );
							   
				GC_Weapon::OnFire(proj);

				_time = 0;
				_fireEffect->SetVisible(true);
			}
		}

		if( _time >= _timeReload )
		{
			_firing = true;
			_time   = 0;
		}
	}

	_reloaded = false;
}

void GC_Weap_RocketLauncher::SetupAI(AIWEAPSETTINGS *pSettings)
{
	pSettings->bNeedOutstrip      = TRUE;
	pSettings->fMaxAttackAngleCos = cos(0.2f);
	pSettings->fProjectileSpeed   = SPEED_ROCKET;
	pSettings->fAttackRadius_max  = 600.0f;
	pSettings->fAttackRadius_min  = 100.0f;
	pSettings->fAttackRadius_crit =  40.0f;
	pSettings->fDistanceMultipler = _advanced ? 1.2f : 3.5f;
}

void GC_Weap_RocketLauncher::TimeStepFixed(float dt)
{
	if( GetCarrier() )
	{
		if( _firing )
			Fire();
		else if( _time >= _timeReload && !_reloaded )
		{
			_reloaded = true;
			if( !_advanced) PLAY(SND_WeapReload, GetPos());
		}
	}

	GC_Weapon::TimeStepFixed(dt);
}

//////////////////////////////////////////////////////////////////////////////////////////////

IMPLEMENT_SELF_REGISTRATION(GC_Weap_AutoCannon)
{
	ED_ITEM( "weap_autocannon", "obj_weap_autocannon", 4 );
	return true;
}

GC_Weap_AutoCannon::GC_Weap_AutoCannon(float x, float y)
  : GC_Weapon(x, y)
{
	_feTime = 0.2f;
	SetTexture("weap_ac");
}

void GC_Weap_AutoCannon::SetAdvanced(bool advanced)
{
	GC_IndicatorBar *pIndicator = GC_IndicatorBar::FindIndicator(this, LOCATION_BOTTOM);
	if( pIndicator ) pIndicator->SetVisible(!advanced);
	if( _fireEffect ) _fireEffect->SetTexture(advanced ? "particle_fire4" : "particle_fire3");
	GC_Weapon::SetAdvanced(advanced);
}

void GC_Weap_AutoCannon::Attach(GC_Actor *actor)
{
	GC_Weapon::Attach(actor);

	_timeReload = 3.7f;
	_time       = _timeReload;

	_firing = false;
	_nshots = 0;
	_nshots_total = 30;
	_time_shot = 0.135f;

	GC_IndicatorBar *pIndicator = new GC_IndicatorBar("indicator_ammo", this,
		(float *) &_nshots, (float *) &_nshots_total, LOCATION_BOTTOM);
	pIndicator->SetInverse(true);

	_fireEffect->SetTexture("particle_fire3");

//return;
//	veh->SetMaxHP(80);

//	veh->_ForvAccel = 300;
//	veh->_BackAccel = 200;
//	veh->_StopAccel = 500;

//	veh->_rotator.setl(3.5f, 10.0f, 30.0f);

//	veh->_MaxForvSpeed = 240;
//	veh->_MaxBackSpeed = 160;
}

void GC_Weap_AutoCannon::Detach()
{
	GC_IndicatorBar *indicator = GC_IndicatorBar::FindIndicator(this, LOCATION_BOTTOM);
	if( indicator ) indicator->Kill();

	// kill the reload sound
	FOREACH( g_level->GetList(LIST_sounds), GC_Sound, object )
	{
		if( GC_Sound_link::GetTypeStatic() == object->GetType() )
		{
			if( ((GC_Sound_link *) object)->CheckObject(this) )
			{
				object->Kill();
				break;
			}
		}
	}

	GC_Weapon::Detach();
}

GC_Weap_AutoCannon::GC_Weap_AutoCannon(FromFile)
  : GC_Weapon(FromFile())
{
}

GC_Weap_AutoCannon::~GC_Weap_AutoCannon()
{
}

void GC_Weap_AutoCannon::Serialize(SaveFile &f)
{
	GC_Weapon::Serialize(f);
	f.Serialize(_firing);
	f.Serialize(_nshots);
	f.Serialize(_nshots_total);
	f.Serialize(_time_shot);
}

void GC_Weap_AutoCannon::Fire()
{
	if( _firing && GetCarrier() )
	{
		const vec2d &dir = GetDirectionReal();
		if( _advanced )
		{
			if( _time >= _time_shot )
			{
				for( int t = 0; t < 2; ++t )
				{
					float dy = t == 0 ? -9.0f : 9.0f;

					float ax = dir.x * 17.0f - dy * dir.y;
					float ay = dir.y * 17.0f + dy * dir.x;

					GC_Projectile *proj = new GC_ACBullet(GetCarrier()->GetPos() + vec2d(ax, ay),
									dir * SPEED_ACBULLET,
									GetCarrier(), GetCarrier()->GetOwner(), _advanced);
									
					GC_Weapon::OnFire(proj);
				}

				_time = 0;
				_fePos.Set(17.0f, 0);
				_fireEffect->SetVisible(true);

				PLAY(SND_ACShoot, GetPos());
			}
		}
		else
		{
			if( _time >= _time_shot )
			{
				_nshots++;

				float dy = (_nshots & 1) == 0 ? -9.0f : 9.0f;

				if( _nshots == _nshots_total )
				{
					_firing = false;
					new GC_Sound_link(SND_AC_Reload, SMODE_PLAY, this);
				}

				float ax = dir.x * 17.0f - dy * dir.y;
				float ay = dir.y * 17.0f + dy * dir.x;

				GC_Projectile *proj = new GC_ACBullet(GetCarrier()->GetPos() + vec2d(ax, ay),
								Vec2dAddDirection(dir, vec2d(g_level->net_frand(0.02f) - 0.01f)) * SPEED_ACBULLET,
								GetCarrier(), GetCarrier()->GetOwner(), _advanced );

				GC_Weapon::OnFire(proj);
								
				_time = 0;
				_fePos.Set(17.0f, -dy);
				_fireEffect->SetVisible(true);

				PLAY(SND_ACShoot, GetPos());
			}
		}
	}
}

void GC_Weap_AutoCannon::SetupAI(AIWEAPSETTINGS *pSettings)
{
	pSettings->bNeedOutstrip      = TRUE;
	pSettings->fMaxAttackAngleCos = cos(0.1f);
	pSettings->fProjectileSpeed   = SPEED_ACBULLET;
	pSettings->fAttackRadius_max  = 500;
	pSettings->fAttackRadius_min  = 100;
	pSettings->fAttackRadius_crit =   0;
	pSettings->fDistanceMultipler = _advanced ? 3.3f : 13.0f;
}

void GC_Weap_AutoCannon::TimeStepFixed(float dt)
{
	if( GetCarrier() )
	{
		if( _advanced )
			_nshots  = 0;

		if( _time >= _timeReload && !_firing )
		{
			_firing = true;
			_nshots  = 0;
			_time    = 0;
		}

		_firing |= _advanced;
	}

	GC_Weapon::TimeStepFixed(dt);
}

void GC_Weap_AutoCannon::Kill()
{
	GC_Weapon::Kill();
}

//////////////////////////////////////////////////////////////////////////////////////////////

IMPLEMENT_SELF_REGISTRATION(GC_Weap_Cannon)
{
	ED_ITEM( "weap_cannon", "obj_weap_cannon", 4 );
	return true;
}

GC_Weap_Cannon::GC_Weap_Cannon(float x, float y)
  : GC_Weapon(x, y)
{
	_fePos.Set(21, 0);
	_feTime = 0.2f;
	SetTexture("weap_cannon");
}

void GC_Weap_Cannon::Attach(GC_Actor *actor)
{
	GC_Weapon::Attach(actor);

	_timeReload    = 0.9f;
	_time_smoke_dt = 0;
	_time_smoke    = 0;

	_fireEffect->SetTexture("particle_fire3");

//return;
//	veh->SetMaxHP(125);

//	veh->_ForvAccel = 250;
//	veh->_BackAccel = 150;
//	veh->_StopAccel = 500;

//	veh->_rotator.setl(3.5f, 10.0f, 30.0f);

//	veh->_MaxForvSpeed = 160;
//	veh->_MaxBackSpeed = 120;
}

GC_Weap_Cannon::GC_Weap_Cannon(FromFile)
  : GC_Weapon(FromFile())
{
}

GC_Weap_Cannon::~GC_Weap_Cannon()
{
}

void GC_Weap_Cannon::Serialize(SaveFile &f)
{
	GC_Weapon::Serialize(f);
	f.Serialize(_time_smoke);
	f.Serialize(_time_smoke_dt);
}

void GC_Weap_Cannon::Fire()
{
	if( GetCarrier() && _time >= _timeReload )
	{
		GC_Vehicle * const veh = static_cast<GC_Vehicle*>(GetCarrier());
		const vec2d &dir = GetDirectionReal();

		GC_Projectile *proj = new GC_TankBullet(GetPos() + dir * 17.0f, 
			dir * SPEED_TANKBULLET + g_level->net_vrand(50), veh, veh->GetOwner(), _advanced);

		if( !_advanced )
		{
			veh->ApplyImpulse( dir * (-80.0f) );
		}

		_time       = 0;
		_time_smoke = 0.3f;

		_fireEffect->SetVisible(true);
		
		GC_Weapon::OnFire(proj);
	}
}

void GC_Weap_Cannon::SetupAI(AIWEAPSETTINGS *pSettings)
{
	pSettings->bNeedOutstrip      = TRUE;
	pSettings->fMaxAttackAngleCos = cos(0.1f);
	pSettings->fProjectileSpeed   = SPEED_TANKBULLET;
	pSettings->fAttackRadius_max  = 500;
	pSettings->fAttackRadius_min  = 100;
	pSettings->fAttackRadius_crit = _advanced ? 64.0f : 0;
	pSettings->fDistanceMultipler = _advanced ? 2.0f : 8.0f;
}

void GC_Weap_Cannon::TimeStepFixed(float dt)
{
	static const TextureCache tex("particle_smoke");

	GC_Weapon::TimeStepFixed( dt );

	if( GetCarrier() && _time_smoke > 0 )
	{
		_time_smoke -= dt;
		_time_smoke_dt += dt;

		for( ;_time_smoke_dt > 0; _time_smoke_dt -= 0.025f )
		{
			vec2d a = Vec2dAddDirection(static_cast<GC_Vehicle*>(GetCarrier())->GetVisual()->GetDirection(), vec2d(_angleReal));
			new GC_Particle(GetPosPredicted() + a * 26.0f, SPEED_SMOKE + a * 50.0f, tex, frand(0.3f) + 0.2f);
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////

IMPLEMENT_SELF_REGISTRATION(GC_Weap_Plazma)
{
	ED_ITEM( "weap_plazma", "obj_weap_plazma", 4 );
	return true;
}

GC_Weap_Plazma::GC_Weap_Plazma(float x, float y)
  : GC_Weapon(x, y)
{
	SetTexture("weap_plazma");
	_fePos.Set(0, 0);
	_feTime = 0.2f;
}

GC_Weap_Plazma::GC_Weap_Plazma(FromFile)
  : GC_Weapon(FromFile())
{
}

GC_Weap_Plazma::~GC_Weap_Plazma()
{
}

void GC_Weap_Plazma::Attach(GC_Actor *actor)
{
	GC_Weapon::Attach(actor);

	_timeReload = 0.3f;
	_fireEffect->SetTexture("particle_plazma_fire");

//return;
//	veh->SetMaxHP(100);

//	veh->_ForvAccel = 300;
//	veh->_BackAccel = 200;
//	veh->_StopAccel = 500;

//	veh->_rotator.setl(3.5f, 10.0f, 30.0f);

//	veh->_MaxForvSpeed = 200;
//	veh->_MaxBackSpeed = 160;
}

void GC_Weap_Plazma::Fire()
{
	if( GetCarrier() && _time >= _timeReload )
	{
		const vec2d &a = GetDirectionReal();
		GC_Projectile *proj = new GC_PlazmaClod(GetPos() + a * 15.0f,
			a * SPEED_PLAZMA + g_level->net_vrand(20),
			GetCarrier(), GetCarrier()->GetOwner(), _advanced);
		_time = 0;
		_fireEffect->SetVisible(true);
		GC_Weapon::OnFire(proj);
	}
}

void GC_Weap_Plazma::SetupAI(AIWEAPSETTINGS *pSettings)
{
	pSettings->bNeedOutstrip      = TRUE;
	pSettings->fMaxAttackAngleCos = cos(0.2f);
	pSettings->fProjectileSpeed   = SPEED_PLAZMA;
	pSettings->fAttackRadius_max  = 300;
	pSettings->fAttackRadius_min  = 100;
	pSettings->fAttackRadius_crit = 0;
	pSettings->fDistanceMultipler = _advanced ? 2.0f : 8.0f;  // fixme
}

//////////////////////////////////////////////////////////////////////////////////////////////

IMPLEMENT_SELF_REGISTRATION(GC_Weap_Gauss)
{
	ED_ITEM( "weap_gauss", "obj_weap_gauss", 4 );
	return true;
}

GC_Weap_Gauss::GC_Weap_Gauss(float x, float y)
  : GC_Weapon(x, y)
{
	SetTexture("weap_gauss");
	_feTime = 0.15f;
}

void GC_Weap_Gauss::Attach(GC_Actor *actor)
{
	GC_Weapon::Attach(actor);

	_timeReload = 1.3f;
	_fireEffect->SetTexture("particle_gaussfire");

//return;
//	veh->SetMaxHP(70);

//	veh->_ForvAccel = 350;
//	veh->_BackAccel = 250;
//	veh->_StopAccel = 700;

//	veh->_rotator.setl(3.5f, 15.0f, 30.0f);

//	veh->_MaxBackSpeed = 220;
//	veh->_MaxForvSpeed = 260;
}

GC_Weap_Gauss::GC_Weap_Gauss(FromFile)
  : GC_Weapon(FromFile())
{
}

GC_Weap_Gauss::~GC_Weap_Gauss()
{
}

void GC_Weap_Gauss::Fire()
{
	if( GetCarrier() && _time >= _timeReload )
	{
		const vec2d &dir = GetDirectionReal();
		GC_Projectile *proj = new GC_GaussRay(vec2d(GetPos().x + dir.x + 5 * dir.y, GetPos().y + dir.y - 5 * dir.x),
			dir * SPEED_GAUSS, GetCarrier(), GetCarrier()->GetOwner(), _advanced );

		_time = 0;
		_fireEffect->SetVisible(true);
		GC_Weapon::OnFire(proj);
	}
}

void GC_Weap_Gauss::SetupAI(AIWEAPSETTINGS *pSettings)
{
	pSettings->bNeedOutstrip      = FALSE;
	pSettings->fMaxAttackAngleCos = cos(0.01f);
	pSettings->fProjectileSpeed   = 0;
	pSettings->fAttackRadius_max  = 800;
	pSettings->fAttackRadius_min  = 400;
	pSettings->fAttackRadius_crit = 0;
	pSettings->fDistanceMultipler = _advanced ? 4.5f : 9.0f;
}

//////////////////////////////////////////////////////////////////////////////////////////////

IMPLEMENT_SELF_REGISTRATION(GC_Weap_Ram)
{
	ED_ITEM( "weap_ram", "obj_weap_ram", 4 );
	return true;
}

GC_Weap_Ram::GC_Weap_Ram(float x, float y)
  : GC_Weapon(x, y)
  , _firingCounter(0)
{
	SetTexture("weap_ram");
}

void GC_Weap_Ram::SetAdvanced(bool advanced)
{
	GC_IndicatorBar *pIndicator = GC_IndicatorBar::FindIndicator(this, LOCATION_BOTTOM);
	if( pIndicator ) pIndicator->SetVisible(!advanced);

	if( GetCarrier() )
	{
		static_cast<GC_Vehicle*>(GetCarrier())->_percussion =
			advanced ? WEAP_RAM_PERCUSSION * 2 : WEAP_RAM_PERCUSSION;
	}

	GC_Weapon::SetAdvanced(advanced);
}

void GC_Weap_Ram::Attach(GC_Actor *actor)
{
	GC_Weapon::Attach(actor);

	_engineSound = WrapRawPtr(new GC_Sound(SND_RamEngine, SMODE_STOP, GetPos()));

	_engineLight = WrapRawPtr(new GC_Light(GC_Light::LIGHT_POINT));
	_engineLight->SetIntensity(1.0f);
	_engineLight->SetRadius(120);
	_engineLight->SetActive(false);


	_fuel_max  = _fuel = 1.0f;
	_fuel_consumption_rate = 0.2f;
	_fuel_recuperation_rate  = 0.1f;

	_firingCounter = 0;
	_bReady = true;

	GC_IndicatorBar *pIndicator = new GC_IndicatorBar("indicator_fuel", this,
		(float *) &_fuel, (float *) &_fuel_max, LOCATION_BOTTOM);

//return;
//	veh->SetMaxHP(350);

//	veh->_ForvAccel = 250;
//	veh->_BackAccel = 250;
//	veh->_StopAccel = 500;

//	veh->_percussion = _advanced ? WEAP_RAM_PERCUSSION * 2 : WEAP_RAM_PERCUSSION;

//	veh->_rotator.setl(3.5f, 10.0f, 30.0f);

//	veh->_MaxBackSpeed = 160;
//	veh->_MaxForvSpeed = 160;
}

void GC_Weap_Ram::Detach()
{
	GC_IndicatorBar *pIndicator = GC_IndicatorBar::FindIndicator(this, LOCATION_BOTTOM);
	if( pIndicator ) pIndicator->Kill();

	SAFE_KILL(_engineSound);
	SAFE_KILL(_engineLight);

	GC_Weapon::Detach();
}

GC_Weap_Ram::GC_Weap_Ram(FromFile)
  : GC_Weapon(FromFile())
{
}

GC_Weap_Ram::~GC_Weap_Ram()
{
}

void GC_Weap_Ram::OnUpdateView()
{
	_engineLight->MoveTo(GetPosPredicted() - GetDirection() * 20);
}

void GC_Weap_Ram::Serialize(SaveFile &f)
{
	GC_Weapon::Serialize(f);
	/////////////////////////////////////
	f.Serialize(_firingCounter);
	f.Serialize(_bReady);
	f.Serialize(_fuel);
	f.Serialize(_fuel_max);
	f.Serialize(_fuel_consumption_rate);
	f.Serialize(_fuel_recuperation_rate);
	f.Serialize(_engineSound);
	f.Serialize(_engineLight);
}

void GC_Weap_Ram::Kill()
{
	SAFE_KILL(_engineSound);
	GC_Weapon::Kill();
}

void GC_Weap_Ram::Fire()
{
	assert(GetCarrier());
	if( _bReady )
	{
		_firingCounter = 2;
		if( GC_RigidBodyDynamic *owner = dynamic_cast<GC_RigidBodyDynamic *>(GetCarrier()) )
		{
			owner->ApplyForce(GetDirectionReal() * 2000);
		}
		GC_Weapon::OnFire(NULL);
	}
}

void GC_Weap_Ram::SetupAI(AIWEAPSETTINGS *pSettings)
{
	pSettings->bNeedOutstrip      = FALSE;
	pSettings->fMaxAttackAngleCos = cos(0.3f);
	pSettings->fProjectileSpeed   = 0;
	pSettings->fAttackRadius_max  = 100;
	pSettings->fAttackRadius_min  = 0;
	pSettings->fAttackRadius_crit = 0;
	pSettings->fDistanceMultipler = _advanced ? 2.5f : 6.0f;
}

void GC_Weap_Ram::TimeStepFloat(float dt)
{
	static const TextureCache tex1("particle_fire2");
	static const TextureCache tex2("particle_yellow");
	static const TextureCache tex3("particle_fire");


	if( GetCarrier() && _firingCounter )
	{
		GC_Vehicle *veh = static_cast<GC_Vehicle *>(GetCarrier());
		vec2d v = veh->GetVisual()->_lv;

		// primary
		{
			const vec2d &a = GetDirection();
			vec2d emitter = GetPosPredicted() - a * 20.0f;
			for( int i = 0; i < 29; ++i )
			{
				float time = frand(0.05f) + 0.02f;
				float t = frand(6.0f) - 3.0f;
				vec2d dx(-a.y * t, a.x * t);
				new GC_Particle(emitter + dx, v - a * frand(800.0f) - dx / time, fabs(t) > 1.5 ? tex1 : tex2, time);
			}
		}


		// secondary
		for( float l = -1; l < 2; l += 2 )
		{
			vec2d a = Vec2dAddDirection(GetDirection(), vec2d(l * 0.15f));
			vec2d emitter = GetPosPredicted() - a * 15.0f + vec2d( -a.y, a.x) * l * 17.0f;
			for( int i = 0; i < 10; i++ )
			{
				float time = frand(0.05f) + 0.02f;
				float t = frand(2.5f) - 1.25f;
				vec2d dx(-a.y * t, a.x * t);
				new GC_Particle(emitter + dx, v - a * frand(600.0f) - dx / time, tex3, time);
			}
		}
	}

	GC_Weapon::TimeStepFloat(dt);
}

void GC_Weap_Ram::TimeStepFixed(float dt)
{
	GC_Weapon::TimeStepFixed( dt );

	if( GetCarrier() )
	{
		assert(_engineSound);

		if( _advanced )
			_fuel = _fuel_max;

		if( _firingCounter )
		{
			_engineSound->Pause(false);
			_engineSound->MoveTo(GetPos());

			_fuel = __max(0, _fuel - _fuel_consumption_rate * dt);
			if( 0 == _fuel ) _bReady = false;

			GC_RigidBodyDynamic *veh = static_cast<GC_RigidBodyDynamic *>(GetCarrier());

			vec2d v = veh->_lv;

			// the primary jet
			{
				const float lenght = 50.0f;
				const vec2d &a = GetDirectionReal();
				vec2d emitter = GetPos() - a * 20.0f;
				vec2d hit;
				if( GC_RigidBodyStatic *object = g_level->TraceNearest(g_level->grid_rigid_s, GetCarrier(), emitter, -a * lenght, &hit) )
				{
					object->TakeDamage(dt * DAMAGE_RAM_ENGINE * (1.0f - (hit - emitter).len() / lenght), hit, GetCarrier()->GetOwner());
				}
			}

			// secondary jets
			for( float l = -1; l < 2; l += 2 )
			{
				const float lenght = 50.0f;
				vec2d a = Vec2dAddDirection(GetDirectionReal(), vec2d(l * 0.15f));
				vec2d emitter = GetPos() - a * 15.0f + vec2d( -a.y, a.x) * l * 17.0f;
				vec2d hit;
				GC_RigidBodyStatic *object = g_level->TraceNearest(g_level->grid_rigid_s,
					GetCarrier(), emitter + a * 2.0f, -a * lenght, &hit);
				if( object )
				{
					object->TakeDamage(
						dt * DAMAGE_RAM_ENGINE * (1.0f - (hit - emitter).len() / lenght), hit, GetCarrier()->GetOwner());
				}
			}
		}
		else
		{
			_engineSound->Pause(true);
			_fuel   = __min(_fuel_max, _fuel + _fuel_recuperation_rate * dt);
			_bReady = (_fuel_max < _fuel * 4.0f);
		}

		_engineLight->SetActive(_firingCounter > 0);
		if( _firingCounter ) --_firingCounter;
	}
	else
	{
		assert(!_engineSound);
	}

	return;
}

//////////////////////////////////////////////////////////////////////////////////////////////

IMPLEMENT_SELF_REGISTRATION(GC_Weap_BFG)
{
	ED_ITEM( "weap_bfg", "obj_weap_bfg", 4 );
	return true;
}

GC_Weap_BFG::GC_Weap_BFG(float x, float y)
  : GC_Weapon(x, y)
{
	SetTexture("weap_bfg");
}

void GC_Weap_BFG::Attach(GC_Actor *actor)
{
	GC_Weapon::Attach(actor);

	_time_ready  = 0;
	_timeReload = 1.1f;

//return;
//	veh->SetMaxHP(110);

//	veh->_ForvAccel = 250;
//	veh->_BackAccel = 200;
//	veh->_StopAccel = 1000;

//	veh->_rotator.setl(3.5f, 15.0f, 30.0f);

//	veh->_MaxForvSpeed = 200;
//	veh->_MaxBackSpeed = 180;
}

GC_Weap_BFG::GC_Weap_BFG(FromFile)
  : GC_Weapon(FromFile())
{
}

GC_Weap_BFG::~GC_Weap_BFG()
{
}

void GC_Weap_BFG::Serialize(SaveFile &f)
{
	GC_Weapon::Serialize(f);
	f.Serialize(_time_ready);
}

void GC_Weap_BFG::Fire()
{
	assert(GetCarrier());

	if( _time >= _timeReload )
	{
		if( !_advanced && 0 == _time_ready )
		{
			PLAY(SND_BfgInit, GetPos());
			_time_ready = FLT_EPSILON;
		}

		if( _time_ready >= 0.7f || _advanced )
		{
			const vec2d &a = GetDirectionReal();
			GC_Projectile *proj = new GC_BfgCore(GetPos() + a * 16.0f, a * SPEED_BFGCORE,
				GetCarrier(), GetCarrier()->GetOwner(), _advanced );
			_time_ready = 0;
			_time = 0;
			GC_Weapon::OnFire(proj);
		}
	}
}

void GC_Weap_BFG::SetupAI(AIWEAPSETTINGS *pSettings)
{
	pSettings->bNeedOutstrip      = TRUE;
	pSettings->fMaxAttackAngleCos = cos(0.01f);
	pSettings->fProjectileSpeed   = SPEED_BFGCORE;
	pSettings->fAttackRadius_max  = 600;
	pSettings->fAttackRadius_min  = 200;
	pSettings->fAttackRadius_crit =   0;
	pSettings->fDistanceMultipler = _advanced ? 13.0f : 20.0f;
}

void GC_Weap_BFG::TimeStepFixed(float dt)
{
	GC_Weapon::TimeStepFixed(dt);
	if( GetCarrier() && _time_ready != 0 )
	{
		_time_ready += dt;
		Fire();
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////

IMPLEMENT_SELF_REGISTRATION(GC_Weap_Ripper)
{
	ED_ITEM( "weap_ripper", "obj_weap_ripper", 4 );
	return true;
}

void GC_Weap_Ripper::UpdateDisk()
{
	_diskSprite->SetVisible(_time > _timeReload);
	_diskSprite->MoveTo(GetPosPredicted() - GetDirection() * 8);
	_diskSprite->SetDirection(vec2d(GetTimeAnimation() * 10));
}

void GC_Weap_Ripper::Attach(GC_Actor *actor)
{
	GC_Weapon::Attach(actor);

	_timeReload = 0.5f;
	_diskSprite = WrapRawPtr(new GC_2dSprite());
	_diskSprite->SetTexture("projectile_disk");
	_diskSprite->SetZ(Z_PROJECTILE);
	UpdateDisk();

//return;
//	veh->SetMaxHP(80);

//	veh->_ForvAccel = 300;
//	veh->_BackAccel = 200;
//	veh->_StopAccel = 500;

//	veh->_rotator.setl(3.5f, 10.0f, 30.0f);

//	veh->_MaxBackSpeed = 260;
//	veh->_MaxForvSpeed = 240;
}

void GC_Weap_Ripper::Detach()
{
	SAFE_KILL(_diskSprite);
	GC_Weapon::Detach();
}

GC_Weap_Ripper::GC_Weap_Ripper(float x, float y)
  : GC_Weapon(x, y)
{
	SetTexture("weap_ripper");
}

GC_Weap_Ripper::GC_Weap_Ripper(FromFile)
  : GC_Weapon(FromFile())
{
}

GC_Weap_Ripper::~GC_Weap_Ripper()
{
}

void GC_Weap_Ripper::Serialize(SaveFile &f)
{
	GC_Weapon::Serialize(f);
	f.Serialize(_diskSprite);
}

void GC_Weap_Ripper::Fire()
{
	if( GetCarrier() && _time >= _timeReload )
	{
		const vec2d &a = GetDirectionReal();
		GC_Projectile *proj = new GC_Disk(GetPos() - a * 9.0f, a * SPEED_DISK + g_level->net_vrand(10),
			GetCarrier(), GetCarrier()->GetOwner(), _advanced);
		PLAY(SND_DiskFire, GetPos());
		_time = 0;
		
		GC_Weapon::OnFire(proj);
	}
}

void GC_Weap_Ripper::SetupAI(AIWEAPSETTINGS *pSettings)
{
	pSettings->bNeedOutstrip      = TRUE;
	pSettings->fMaxAttackAngleCos = cos(0.2f);
	pSettings->fProjectileSpeed   = SPEED_DISK;
	pSettings->fAttackRadius_max  = 700;
	pSettings->fAttackRadius_min  = 500;
	pSettings->fAttackRadius_crit =  60;
	pSettings->fDistanceMultipler = _advanced ? 2.2f : 40.0f;
}

void GC_Weap_Ripper::TimeStepFloat(float dt)
{
	GC_Weapon::TimeStepFloat(dt);
	if( _diskSprite )
	{
		UpdateDisk();
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////

IMPLEMENT_SELF_REGISTRATION(GC_Weap_Minigun)
{
	ED_ITEM( "weap_minigun", "obj_weap_minigun", 4 );
	return true;
}

GC_Weap_Minigun::GC_Weap_Minigun(float x, float y)
  : GC_Weapon(x, y)
  , _bFire(false)
{
	SetTexture("weap_mg1");

	_fePos.Set(20, 0);
	_feTime   = 0.1f;
	_feOrient = vrand(1);
}

GC_Weap_Minigun::GC_Weap_Minigun(FromFile)
  : GC_Weapon(FromFile())
{
}

GC_Weap_Minigun::~GC_Weap_Minigun()
{
}

void GC_Weap_Minigun::Attach(GC_Actor *actor)
{
	GC_Weapon::Attach(actor);

	_timeReload = 0.03f;
	_timeRotate = 0;
	_timeFire   = 0;
	_timeShot   = 0;

	_sound = WrapRawPtr(new GC_Sound(SND_MinigunFire, SMODE_STOP, GetPos()));
	_bFire = false;

	_fireEffect->SetTexture("minigun_fire");


	if( _crosshairLeft )
	{
		if( GC_Vehicle *veh = dynamic_cast<GC_Vehicle*>(GetCarrier()) )
		{
			_crosshairLeft->SetVisible(NULL != dynamic_cast<GC_PlayerLocal*>(veh->GetOwner()));
		}
		else
		{
			_crosshairLeft->SetVisible(false);
		}
	}



//return;
//	veh->SetMaxHP(65);

//	veh->_ForvAccel = 700;
//	veh->_BackAccel = 600;
//	veh->_StopAccel = 2000;

//	veh->_rotator.setl(3.5f, 15.0f, 30.0f);

//	veh->_MaxBackSpeed = 300;
//	veh->_MaxForvSpeed = 350;

}

void GC_Weap_Minigun::Detach()
{
	SAFE_KILL(_crosshairLeft);
	SAFE_KILL(_sound);

	GC_Weapon::Detach();
}

void GC_Weap_Minigun::SetCrosshair()
{
	_crosshair = WrapRawPtr(new GC_2dSprite());
	_crosshair->SetTexture("indicator_crosshair2");
	_crosshair->SetZ(Z_VEHICLE_LABEL);

	_crosshairLeft = WrapRawPtr(new GC_2dSprite());
	_crosshairLeft->SetTexture("indicator_crosshair2");
	_crosshairLeft->SetZ(Z_VEHICLE_LABEL);

	_fixmeChAnimate = false;
}

void GC_Weap_Minigun::Serialize(SaveFile &f)
{
	GC_Weapon::Serialize(f);

	f.Serialize(_bFire);
	f.Serialize(_timeFire);
	f.Serialize(_timeRotate);
	f.Serialize(_timeShot);
	f.Serialize(_crosshairLeft);
	f.Serialize(_sound);
}

void GC_Weap_Minigun::Kill()
{
	SAFE_KILL(_crosshairLeft);
	SAFE_KILL(_sound);

	GC_Weapon::Kill();
}

void GC_Weap_Minigun::Fire()
{
	assert(GetCarrier());
	if( GetCarrier() )
		_bFire = true;
}

void GC_Weap_Minigun::SetupAI(AIWEAPSETTINGS *pSettings)
{
	pSettings->bNeedOutstrip      = TRUE;
	pSettings->fMaxAttackAngleCos = cos(0.3f);
	pSettings->fProjectileSpeed   = SPEED_BULLET;
	pSettings->fAttackRadius_max  = 200;
	pSettings->fAttackRadius_min  = 100;
	pSettings->fAttackRadius_crit =   0;
	pSettings->fDistanceMultipler = _advanced ? 5.0f : 10.0f;
}

void GC_Weap_Minigun::TimeStepFixed(float dt)
{
	static const TextureCache tex("particle_1");

	if( GetCarrier() )
	{
		GC_RigidBodyDynamic *veh = dynamic_cast<GC_RigidBodyDynamic *>(GetCarrier());
		if( _bFire )
		{
			_timeRotate += dt;
			_timeShot   += dt;

			SetTexture((fmod(_timeRotate, 0.08f) < 0.04f) ? "weap_mg1":"weap_mg2");

			_sound->MoveTo(GetPos());
			_sound->Pause(false);
			_bFire = false;

			for(; _timeShot > 0; _timeShot -= _advanced ? 0.02f : 0.04f)
			{
				_time = frand(_feTime);
				_feOrient = vrand(1);
				_fireEffect->SetVisible(true);

				float da = _timeFire * 0.07f / WEAP_MG_TIME_RELAX;

				vec2d a = Vec2dAddDirection(GetDirectionReal(), vec2d(g_level->net_frand(da * 2.0f) - da));
				a *= (1 - g_level->net_frand(0.2f));

				if( veh && !_advanced )
				{
					if( g_level->net_frand(WEAP_MG_TIME_RELAX * 5.0f) < _timeFire - WEAP_MG_TIME_RELAX * 0.2f )
					{
						float m = 3000;//veh->_inv_i; // FIXME
						veh->ApplyTorque(m * (g_level->net_frand(1.0f) - 0.5f));
					}
				}

				GC_Bullet *tmp = new GC_Bullet(GetPos() + a * 18.0f, a * SPEED_BULLET, GetCarrier(), GetCarrier()->GetOwner(), _advanced );
				GC_Weapon::OnFire(tmp);
				tmp->TimeStepFixed(_timeShot);
			}

			_timeFire = __min(_timeFire + dt * 2, WEAP_MG_TIME_RELAX);
		}
		else
		{
			_sound->Pause(true);
			_timeFire = __max(_timeFire - dt, 0);
		}

		vec2d delta(_timeFire * 0.1f / WEAP_MG_TIME_RELAX);
		if( _crosshair )
		{
			_crosshair->SetDirection(Vec2dAddDirection(GetDirection(), delta));
			_crosshair->MoveTo(GetPosPredicted() + _crosshair->GetDirection() * CH_DISTANCE_THIN);
		}
		if( _crosshairLeft )
		{
			_crosshairLeft->SetDirection(Vec2dSubDirection(GetDirection(), delta));
			_crosshairLeft->MoveTo(GetPosPredicted() + _crosshairLeft->GetDirection() * CH_DISTANCE_THIN);
		}
	}

	GC_Weapon::TimeStepFixed(dt);
}


//////////////////////////////////////////////////////////////////////////////////////////////

IMPLEMENT_SELF_REGISTRATION(GC_Weap_Zippo)
{
	ED_ITEM( "weap_zippo", "obj_weap_zippo", 4 );
	return true;
}

GC_Weap_Zippo::GC_Weap_Zippo(float x, float y)
  : GC_Weapon(x, y)
  , _bFire(false)
  , _timeBurn(0)
{
	SetTexture("weap_zippo");
}

GC_Weap_Zippo::GC_Weap_Zippo(FromFile)
  : GC_Weapon(FromFile())
{
}

GC_Weap_Zippo::~GC_Weap_Zippo()
{
}

void GC_Weap_Zippo::Attach(GC_Actor *actor)
{
	GC_Weapon::Attach(actor);

	_timeReload = 0.02f;
	_timeFire   = 0;
	_timeShot   = 0;

	_sound = WrapRawPtr(new GC_Sound(SND_RamEngine, SMODE_STOP, GetPos()));
	_bFire = false;
}

void GC_Weap_Zippo::Detach()
{
	SAFE_KILL(_sound);
	GC_Weapon::Detach();
}

void GC_Weap_Zippo::Serialize(SaveFile &f)
{
	GC_Weapon::Serialize(f);

	f.Serialize(_bFire);
	f.Serialize(_timeFire);
	f.Serialize(_timeShot);
	f.Serialize(_timeBurn);
	f.Serialize(_sound);
}

void GC_Weap_Zippo::Kill()
{
	SAFE_KILL(_sound);
	GC_Weapon::Kill();
}

void GC_Weap_Zippo::Fire()
{
	assert(GetCarrier());
	_bFire = true;
}

void GC_Weap_Zippo::SetupAI(AIWEAPSETTINGS *pSettings)
{
	pSettings->bNeedOutstrip      = TRUE;
	pSettings->fMaxAttackAngleCos = cos(0.5f);
	pSettings->fProjectileSpeed   = SPEED_FIRE;
	pSettings->fAttackRadius_max  = 300;
	pSettings->fAttackRadius_min  = 100;
	pSettings->fAttackRadius_crit =  10;
	pSettings->fDistanceMultipler = _advanced ? 5.0f : 10.0f;
}

void GC_Weap_Zippo::TimeStepFixed(float dt)
{
	GC_RigidBodyDynamic *veh = dynamic_cast<GC_RigidBodyDynamic *>(GetCarrier());

	if( GetCarrier() )
	{
		if( _bFire )
		{
			_timeShot += dt;
			_timeFire = __min(_timeFire + dt, WEAP_ZIPPO_TIME_RELAX);

			_sound->MoveTo(GetPos());
			_sound->Pause(false);
			_bFire = false;


			vec2d vvel = veh ? veh->_lv : vec2d(0,0);

			for(; _timeShot > 0; _timeShot -= _timeReload )
			{
				vec2d a(GetDirection());
				a *= (1 - g_level->net_frand(0.2f));

				GC_FireSpark *tmp = new GC_FireSpark(GetPos() + a * 18.0f, 
					vvel + a * SPEED_FIRE, GetCarrier(), GetCarrier()->GetOwner(), _advanced);
				tmp->TimeStepFixed(_timeShot);
				tmp->SetLifeTime(_timeFire);
				tmp->SetHealOwner(_advanced);
				tmp->SetSetFire(true);
				GC_Weapon::OnFire(tmp);
			}
		}
		else
		{
			_sound->Pause(true);
			_timeFire = __max(_timeFire - dt, 0);
		}
	}

	if( _advanced )
	{
		_timeBurn += dt;
		while( _timeBurn > 0 )
		{
			GC_FireSpark *tmp = new GC_FireSpark(GetPos() + g_level->net_vrand(33), 
				SPEED_SMOKE/2, GetCarrier(), GetCarrier() ? GetCarrier()->GetOwner() : NULL, true);
			tmp->SetLifeTime(0.3f);
			tmp->TimeStepFixed(_timeBurn);
			_timeBurn -= 0.01f;
		}
	}

	GC_Weapon::TimeStepFixed(dt);
}

//////////////////////////////////////////////////////////////////////////////////////////////

IMPLEMENT_SELF_REGISTRATION(GC_Weap_ScriptGun)
{
	ED_ITEM( "weap_scripted_gun", "obj_weap_scripted_gun", 4 );
	return true;
}

PropertySet* GC_Weap_ScriptGun::NewPropertySet()
{
	return new MyPropertySet(this);
}

GC_Weap_ScriptGun::MyPropertySet::MyPropertySet(GC_Object *object)
  : BASE(object)
  , _propTexture(		ObjectProperty::TYPE_STRING,  "texture" 		)
  , _propAmmo(			ObjectProperty::TYPE_INTEGER, "ammo"			)
  , _propOnLackOfAmmo(  ObjectProperty::TYPE_STRING,  "on_lack_of_ammo" )
  , _propRecoil(		ObjectProperty::TYPE_FLOAT,   "recoil"	  		)
  , _propRate(	  		ObjectProperty::TYPE_FLOAT,   "rate"	   		)
  , _propReload(		ObjectProperty::TYPE_FLOAT,   "reload"	   		)
  , _propPullPower(	  	ObjectProperty::TYPE_FLOAT,   "pull_power"	   	)
{
	_propAmmo.SetIntRange(0, 1000000);
	_propRecoil.SetFloatRange(0, 1000000.0f);
	_propRate.SetFloatRange(0, 1000000.0f);
	_propReload.SetFloatRange(0, 1000000.0f);
	_propPullPower.SetFloatRange(0, 1000000.0f);
}

int GC_Weap_ScriptGun::MyPropertySet::GetCount() const
{
	return BASE::GetCount() + 7;
}

ObjectProperty* GC_Weap_ScriptGun::MyPropertySet::GetProperty(int index)
{
	if( index < BASE::GetCount() )
		return BASE::GetProperty(index);

	switch( index - BASE::GetCount() )
	{
	case 0: return &_propTexture;
	case 1: return &_propAmmo;
	case 2: return &_propOnLackOfAmmo;
	case 3: return &_propRecoil;
	case 4: return &_propRate;
	case 5: return &_propReload;
	case 6: return &_propPullPower;
	}

	assert(false);
	return NULL;
}

void GC_Weap_ScriptGun::MapExchange(MapFile &f)
{
	GC_Weapon::MapExchange(f);

	MAP_EXCHANGE_STRING(texture, _texture, "weap_cannon");
	MAP_EXCHANGE_INT(ammo, _ammo, 0);
	MAP_EXCHANGE_STRING(on_lack_of_ammo, _scriptOnLackOfAmmo, "");
	MAP_EXCHANGE_FLOAT(recoil, _recoil, 0);
	MAP_EXCHANGE_FLOAT(rate, _rate, 0);
	MAP_EXCHANGE_FLOAT(reload, _reload, 0);
	MAP_EXCHANGE_FLOAT(pull_power, _pullPower, 0);
	
	if( f.loading() )
	{
		SetTexture(_texture.c_str());
	}
}

void GC_Weap_ScriptGun::MyPropertySet::MyExchange(bool applyToObject)
{
	BASE::MyExchange(applyToObject);

	GC_Weap_ScriptGun *obj = static_cast<GC_Weap_ScriptGun*>(GetObject());
	if( applyToObject )
	{
		obj->_texture = _propTexture.GetStringValue();
		obj->SetTexture(obj->_texture.c_str());
		obj->_ammo = _propAmmo.GetIntValue();
		obj->_scriptOnLackOfAmmo = _propOnLackOfAmmo.GetStringValue();
		obj->_recoil = _propRecoil.GetFloatValue();
		obj->_rate = _propRate.GetFloatValue();
		obj->_shotPeriod = (float) (1.0f) / obj->_rate;
		obj->_reload = _propReload.GetFloatValue();
		obj->_pullPower = _propPullPower.GetFloatValue();
	}
	else
	{
		_propTexture.SetStringValue(obj->_texture);
		_propAmmo.SetIntValue(obj->_ammo);
		_propOnLackOfAmmo.SetStringValue(obj->_scriptOnLackOfAmmo);
		_propRecoil.SetFloatValue(obj->_recoil);
		_propRate.SetFloatValue(obj->_rate);
		_propReload.SetFloatValue(obj->_reload);
		_propPullPower.SetFloatValue(obj->_pullPower);
	}
}

GC_Weap_ScriptGun::GC_Weap_ScriptGun(float x, float y)
  : GC_Weapon(x, y)
  , _texture("weap_cannon")
  , _ammo(0)
  , _ammo_fired(0)
  , _recoil(0)
  , _rate(2.0f)
  , _shotPeriod(0.5f)
  , _reload(2.0f)
  , _pullPower(0)
{
	_fePos.Set(21, 0);
	_feTime = 0.2f;
	SetTexture(_texture.c_str());
}

void GC_Weap_ScriptGun::SetAdvanced(bool advanced)
{
	GC_Weapon::SetAdvanced(advanced);
}

void GC_Weap_ScriptGun::Attach(GC_Actor *actor)
{
	GC_Weapon::Attach(actor);
	
	_time = _reload;
	
	if ( _ammo )
	{
		_firing = false;
		GC_IndicatorBar *pIndicator = new GC_IndicatorBar("indicator_ammo", this,
			(float *) &_ammo_fired, (float *) &_ammo, LOCATION_BOTTOM);
		pIndicator->SetInverse(true);
	}
}

void GC_Weap_ScriptGun::Detach()
{
	GC_Weapon::Detach();
	
	GC_IndicatorBar *indicator = GC_IndicatorBar::FindIndicator(this, LOCATION_BOTTOM);
	if( indicator ) indicator->Kill();
}

GC_Weap_ScriptGun::GC_Weap_ScriptGun(FromFile)
  : GC_Weapon(FromFile())
{
}

GC_Weap_ScriptGun::~GC_Weap_ScriptGun()
{
}

// TODO: Add variables
void GC_Weap_ScriptGun::Serialize(SaveFile &f)
{
	GC_Weapon::Serialize(f);
	
	f.Serialize(_texture);
	f.Serialize(_ammo_fired);
	f.Serialize(_ammo);
	f.Serialize(_recoil);
	f.Serialize(_rate);
	f.Serialize(_shotPeriod);
	f.Serialize(_reload);
	f.Serialize(_pullPower);
	f.Serialize(_firing);
	f.Serialize(_scriptOnLackOfAmmo);
}

void GC_Weap_ScriptGun::Fire()
{
	if ( GetCarrier() )
	{
		if ( _ammo )
		{
			if( _firing && _time >= _shotPeriod )
			{
				_ammo_fired++;

				if( _ammo == _ammo_fired )
				{
					_firing = false;
					if( !_scriptOnLackOfAmmo.empty() )
					{
						std::stringstream buf;
						buf << "return function(self)";
						buf << _scriptOnLackOfAmmo;
						buf << "\nend";

						if( luaL_loadstring(g_env.L, buf.str().c_str()) )
						{
							GetConsole().Printf(1, "OnLackOfAmmo: %s", lua_tostring(g_env.L, -1));
							lua_pop(g_env.L, 1); // pop the error message from the stack
						}
						else
						{
							if( lua_pcall(g_env.L, 0, 1, 0) )
							{
								GetConsole().WriteLine(1, lua_tostring(g_env.L, -1));
								lua_pop(g_env.L, 1); // pop the error message from the stack
							}
							else
							{
								luaT_pushobject(g_env.L, this);
								if( lua_pcall(g_env.L, 1, 0, 0) )
								{
									GetConsole().WriteLine(1, lua_tostring(g_env.L, -1));
									lua_pop(g_env.L, 1); // pop the error message from the stack
								}
							}
						}
					}
				}
				
				GC_Vehicle * const veh = static_cast<GC_Vehicle*>(GetCarrier());
				const vec2d &dir = GetDirectionReal();
				veh->ApplyImpulse( dir * (-_recoil) );
				veh->ApplyTorque(_pullPower * (g_level->net_frand(2.0f) - 1.0f));
				
				GC_Weapon::OnFire();
				
				_time = 0;
			}
		}
		else
		{
			if( _time >= _shotPeriod )
			{
				GC_Vehicle * const veh = static_cast<GC_Vehicle*>(GetCarrier());
				const vec2d &dir = GetDirectionReal();
				veh->ApplyImpulse( dir * (-_recoil) );
				veh->ApplyTorque(_pullPower * (g_level->net_frand(2.0f) - 1.0f));
				
				GC_Weapon::OnFire();

				_time = 0;
			}
		}
	}	
}

// TODO: Change this settings
void GC_Weap_ScriptGun::SetupAI(AIWEAPSETTINGS *pSettings)
{
	pSettings->bNeedOutstrip      = TRUE;
	pSettings->fMaxAttackAngleCos = cos(0.1f);
	pSettings->fProjectileSpeed   = SPEED_TANKBULLET;
	pSettings->fAttackRadius_max  = 500;
	pSettings->fAttackRadius_min  = 100;
	pSettings->fAttackRadius_crit = _advanced ? 64.0f : 0;
	pSettings->fDistanceMultipler = _advanced ? 2.0f : 8.0f;
}

//TODO: Add Code
void GC_Weap_ScriptGun::TimeStepFixed(float dt)
{
	GC_Weapon::TimeStepFixed( dt );
	
	if( _ammo )
	{
		if( _time >= _reload && !_firing )
		{
			_firing = true;
			_ammo_fired = 0;
			_time = 0;
		}
	}
}

void GC_Weap_ScriptGun::Kill()
{
	GC_Weapon::Kill();
}

///////////////////////////////////////////////////////////////////////////////
// end of file
