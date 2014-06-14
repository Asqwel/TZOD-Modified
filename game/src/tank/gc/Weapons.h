// Weapons.h

#pragma once

#include "Pickup.h"

///////////////////////////////////////////////////////////////////////////////

class GC_Projectile;

struct AIWEAPSETTINGS
{
	float fMaxAttackAngleCos;
	float fProjectileSpeed;
	float fAttackRadius_min;
	float fAttackRadius_max;
	float fAttackRadius_crit;  // if you closer than critical distance you may damage your self
	float fDistanceMultipler;  // applies when traveling through brick walls
	bool  bNeedOutstrip;       // false if the projectile speed is unlimited
};


class GC_Weapon : public GC_Pickup
{
	string_t _scriptOnFire;   // on_fire()
	
protected:
	class MyPropertySet : public GC_Pickup::MyPropertySet
	{
		typedef GC_Pickup::MyPropertySet BASE;
		ObjectProperty _propTimeStay;
		ObjectProperty _propOnFire;
		ObjectProperty _propDir;

	public:
		MyPropertySet(GC_Object *object);
		virtual int GetCount() const;
		virtual ObjectProperty* GetProperty(int index);
		virtual void MyExchange(bool applyToObject);
	};
	virtual PropertySet* NewPropertySet();

	vec2d _directionReal;

protected:
	SafePtr<GC_2dSprite> _fireEffect;
	SafePtr<GC_Light>    _fireLight;
	vec2d _fePos;
	vec2d _feOrient;
	float _feTime;
	bool _advanced; // weapon has booster attached
	
protected:
	virtual void MapExchange(MapFile &f);

public:
	virtual void SetAdvanced(bool advanced) { _advanced = advanced; }
	inline  bool GetAdvanced()              { return _advanced;     }

	GC_RigidBodyStatic* GetCarrier() const { return reinterpret_cast<GC_RigidBodyStatic *>(GC_Pickup::GetCarrier()); }
	const vec2d& GetDirectionReal() const { return _directionReal; }


public:
	float _time;
	float _timeStay;
	float _timeReload;

	float    _angleReal;          // note that sprite rotation is predicted angle
	Rotator  _rotatorWeap;
	float	 _angleNormal;

	SafePtr<GC_Sound>     _rotateSound;
	SafePtr<GC_2dSprite>  _crosshair;
	bool _fixmeChAnimate;

public:
	GC_Weapon(float x, float y);
	GC_Weapon(FromFile);
	virtual ~GC_Weapon();
	virtual void Kill();

	virtual void Serialize(SaveFile &f);

	virtual float GetDefaultRespawnTime() const { return 6.0f; }
	virtual AIPRIORITY GetPriority(GC_Vehicle *veh);
	virtual void SetupAI(AIWEAPSETTINGS *pSettings) = 0;

	virtual void Attach(GC_Actor *actor);
	virtual void Detach();
	void ProcessRotate(float dt);

	virtual void SetCrosshair();

	virtual void Fire() = 0;
	
	virtual void OnFire(GC_Projectile *proj);
	virtual void OnFire(void);

	virtual void TimeStepFixed(float dt);
	virtual void TimeStepFloat(float dt);
	
	virtual float GetAngle() const;
	virtual float GetRealAngle() const;
	virtual void SetAngle(float _angle);

private:
	virtual void OnUpdateView() {};

#ifdef NETWORK_DEBUG
/*	virtual DWORD checksum(void) const
	{
		DWORD cs = reinterpret_cast<const DWORD&>(_angleReal)
			^ reinterpret_cast<const DWORD&>(_timeReload)
			^ reinterpret_cast<const DWORD&>(_timeStay);
		return GC_Pickup::checksum() ^ cs;
	}*/
#endif
};

///////////////////////////////////////////////////////////////////////////////

class GC_Weap_RocketLauncher : public GC_Weapon
{
	DECLARE_SELF_REGISTRATION(GC_Weap_RocketLauncher);

public:
	float _time_shot;
	int _nshots_total;
	int _nshots;
	bool _firing;
	bool _reloaded;

	virtual void Attach(GC_Actor *actor);
	virtual void Detach();

	GC_Weap_RocketLauncher(float x, float y);
	GC_Weap_RocketLauncher(FromFile);

	virtual void Serialize(SaveFile &f);
	virtual void Fire();
	virtual void SetupAI(AIWEAPSETTINGS *pSettings);
	virtual void TimeStepFixed(float dt);
};

///////////////////////////////////////////////////////////////////////////////

class GC_Weap_AutoCannon : public GC_Weapon
{
	DECLARE_SELF_REGISTRATION(GC_Weap_AutoCannon);

public:
	virtual void SetAdvanced(bool advanced);

public:
	float _time_shot;
	int _nshots_total;
	int _nshots;
	bool _firing;

	virtual void Attach(GC_Actor *actor);
	virtual void Detach();

	GC_Weap_AutoCannon(float x, float y);
	GC_Weap_AutoCannon(FromFile);
	virtual ~GC_Weap_AutoCannon();
	virtual void Kill();

	virtual void Serialize(SaveFile &f);
	virtual void Fire();
	virtual void SetupAI(AIWEAPSETTINGS *pSettings);
	virtual void TimeStepFixed(float dt);
};

///////////////////////////////////////////////////////////////////////////////

class GC_Weap_Cannon : public GC_Weapon
{
	DECLARE_SELF_REGISTRATION(GC_Weap_Cannon);

private:
	float _time_smoke;
	float _time_smoke_dt;

public:
	virtual void Attach(GC_Actor *actor);

	GC_Weap_Cannon(float x, float y);
	GC_Weap_Cannon(FromFile);
	virtual ~GC_Weap_Cannon();

	virtual void Serialize(SaveFile &f);
	virtual void Fire();
	virtual void SetupAI(AIWEAPSETTINGS *pSettings);
	virtual void TimeStepFixed(float dt);
};

///////////////////////////////////////////////////////////////////////////////

class GC_Weap_Plazma : public GC_Weapon
{
	DECLARE_SELF_REGISTRATION(GC_Weap_Plazma);

public:
	virtual void Attach(GC_Actor *actor);

	GC_Weap_Plazma(float x, float y);
	GC_Weap_Plazma(FromFile);
	virtual ~GC_Weap_Plazma();

	virtual void Fire();
	virtual void SetupAI(AIWEAPSETTINGS *pSettings);
};

///////////////////////////////////////////////////////////////////////////////

class GC_Weap_Gauss : public GC_Weapon
{
	DECLARE_SELF_REGISTRATION(GC_Weap_Gauss);

public:
	virtual void Attach(GC_Actor *actor);

	GC_Weap_Gauss(float x, float y);
	GC_Weap_Gauss(FromFile);
	virtual ~GC_Weap_Gauss();

	virtual void Fire();
	virtual void SetupAI(AIWEAPSETTINGS *pSettings);
};

///////////////////////////////////////////////////////////////////////////////

class GC_Weap_Ram : public GC_Weapon
{
	DECLARE_SELF_REGISTRATION(GC_Weap_Ram);

private:
	SafePtr<GC_Sound> _engineSound;
	SafePtr<GC_Light> _engineLight;

protected:
	virtual void OnUpdateView();

public:
	float _fuel;
	float _fuel_max;
	float _fuel_consumption_rate;
	float _fuel_recuperation_rate;
	int _firingCounter;
	bool _bReady;

public:
	virtual void SetAdvanced(bool advanced);

public:
	virtual void Attach(GC_Actor *actor);
	virtual void Detach();

	GC_Weap_Ram(float x, float y);
	GC_Weap_Ram(FromFile);
	virtual ~GC_Weap_Ram();
	virtual void Kill();

	virtual void Serialize(SaveFile &f);
	virtual void Fire();
	virtual void SetupAI(AIWEAPSETTINGS *pSettings);
	virtual void TimeStepFixed(float dt);
	virtual void TimeStepFloat(float dt);
};

///////////////////////////////////////////////////////////////////////////////

class GC_Weap_BFG : public GC_Weapon
{
	DECLARE_SELF_REGISTRATION(GC_Weap_BFG);

private:
	float _time_ready;

public:
	virtual void Attach(GC_Actor *actor);

	GC_Weap_BFG(float x, float y);
	GC_Weap_BFG(FromFile);
	virtual ~GC_Weap_BFG();

	virtual void Serialize(SaveFile &f);
	virtual void Fire();
	virtual void SetupAI(AIWEAPSETTINGS *pSettings);
	virtual void TimeStepFixed(float dt);
};

///////////////////////////////////////////////////////////////////////////////

class GC_Weap_Ripper : public GC_Weapon
{
	DECLARE_SELF_REGISTRATION(GC_Weap_Ripper);

	SafePtr<GC_2dSprite> _diskSprite;
	void UpdateDisk();

public:
	virtual void Attach(GC_Actor *actor);
	virtual void Detach();

	GC_Weap_Ripper(float x, float y);
	GC_Weap_Ripper(FromFile);
	virtual ~GC_Weap_Ripper();
	virtual void Serialize(SaveFile &f);

	virtual void Fire();
	virtual void SetupAI(AIWEAPSETTINGS *pSettings);
	virtual void TimeStepFloat(float dt);
};

///////////////////////////////////////////////////////////////////////////////

class GC_Weap_Minigun : public GC_Weapon
{
	DECLARE_SELF_REGISTRATION(GC_Weap_Minigun);

private:
	SafePtr<GC_Sound> _sound;
	float _timeRotate; // for firing animation
	float _timeFire;
	float _timeShot;
	bool _bFire;

	SafePtr<GC_2dSprite> _crosshairLeft;

public:
	virtual void Attach(GC_Actor *actor);
	virtual void Detach();

	GC_Weap_Minigun(float x, float y);
	GC_Weap_Minigun(FromFile);
	virtual ~GC_Weap_Minigun();
	virtual void Kill();

	virtual void SetCrosshair();

	virtual void Serialize(SaveFile &f);
	virtual void Fire();
	virtual void SetupAI(AIWEAPSETTINGS *pSettings);
	virtual void TimeStepFixed(float dt);
};

///////////////////////////////////////////////////////////////////////////////

class GC_Weap_Zippo : public GC_Weapon
{
	DECLARE_SELF_REGISTRATION(GC_Weap_Zippo);

private:
	SafePtr<GC_Sound> _sound;
	float _timeFire;
	float _timeShot;
	float _timeBurn;
	bool _bFire;

public:
	virtual void Attach(GC_Actor *actor);
	virtual void Detach();

	GC_Weap_Zippo(float x, float y);
	GC_Weap_Zippo(FromFile);
	virtual ~GC_Weap_Zippo();
	virtual void Kill();

	virtual void Serialize(SaveFile &f);
	virtual void Fire();
	virtual void SetupAI(AIWEAPSETTINGS *pSettings);
	virtual void TimeStepFixed(float dt);
};

///////////////////////////////////////////////////////////////////////////////

class GC_Weap_ScriptGun : public GC_Weapon
{

	DECLARE_SELF_REGISTRATION(GC_Weap_ScriptGun);

	string_t _scriptOnLackOfAmmo;   // on_lack_of_ammo()
	
protected:
	class MyPropertySet : public GC_Weapon::MyPropertySet
	{
		typedef GC_Weapon::MyPropertySet BASE;
		ObjectProperty _propTexture;
		ObjectProperty _propAmmo;
		ObjectProperty _propOnLackOfAmmo;
		ObjectProperty _propRecoil;
		ObjectProperty _propRate;
		ObjectProperty _propReload;
		ObjectProperty _propPull;

	public:
		MyPropertySet(GC_Object *object);
		virtual int GetCount() const;
		virtual ObjectProperty* GetProperty(int index);
		virtual void MyExchange(bool applyToObject);
	};
	virtual PropertySet* NewPropertySet();

protected:
	virtual void MapExchange(MapFile &f);
	
public:
	virtual void SetAdvanced(bool advanced);

public:
	string_t _texture;
	int _ammo_fired;
	int _ammo;
	float _recoil;
	float _rate;
	float _shotPeriod;
	float _reload;
	int _pull;
	bool _firing;
	

	virtual void Attach(GC_Actor *actor);
	virtual void Detach();

	GC_Weap_ScriptGun(float x, float y);
	GC_Weap_ScriptGun(FromFile);
	virtual ~GC_Weap_ScriptGun();
	virtual void Kill();

	virtual void Serialize(SaveFile &f);
	virtual void Fire();
	virtual void SetupAI(AIWEAPSETTINGS *pSettings);
	virtual void TimeStepFixed(float dt);
};

///////////////////////////////////////////////////////////////////////////////
// end of file
