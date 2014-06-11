// RigidBodyDinamic.h

#pragma once

#include "RigidBody.h"


///////////////////////////////////////////////////////////////////////////////

#define GC_FLAG_RBDYMAMIC_ACTIVE    (GC_FLAG_RBSTATIC_ << 0)
#define GC_FLAG_RBDYMAMIC_PARITY    (GC_FLAG_RBSTATIC_ << 1)
#define GC_FLAG_RBDYMAMIC_          (GC_FLAG_RBSTATIC_ << 2)


class GC_RigidBodyDynamic : public GC_RigidBodyStatic
{
	struct Contact
	{
		GC_RigidBodyDynamic *obj1_d;
		GC_RigidBodyDynamic *obj2_d;
		GC_RigidBodyStatic  *obj2_s;
		vec2d o,n,t; // origin, normal, tangent
		float total_np, total_tp;
		float depth;
//		bool  inactive;
	};

	typedef std::vector<Contact> ContactList;
	static ContactList _contacts;
	static std::stack<ContactList> _contactsStack;
	static bool _glob_parity;

	float geta_s(const vec2d &n, const vec2d &c, const GC_RigidBodyStatic *obj) const;
	float geta_d(const vec2d &n, const vec2d &c, const GC_RigidBodyDynamic *obj) const;

	void impulse(const vec2d &origin, const vec2d &impulse);

	bool parity() { return CheckFlags(GC_FLAG_RBDYMAMIC_PARITY); }


	vec2d _external_force;
	float _external_momentum;
	vec2d _external_impulse;
	float _external_torque;

protected:
	class MyPropertySet : public GC_RigidBodyStatic::MyPropertySet
	{
		typedef GC_RigidBodyStatic::MyPropertySet BASE;
		ObjectProperty _propM;  // mass
		ObjectProperty _propI;  // scalar moment of inertia
		ObjectProperty _propPercussion;
		ObjectProperty _propFragility;
		ObjectProperty _propNx;
		ObjectProperty _propNy;
		ObjectProperty _propNw;
		ObjectProperty _propRotation;
	public:
		MyPropertySet(GC_Object *object);
		virtual int GetCount() const;
		virtual ObjectProperty* GetProperty(int index);
		virtual void MyExchange(bool applyToObject);
	};

public:
	float GetSpinup() const;
	vec2d GetBrakingLength() const;

public:
	GC_RigidBodyDynamic();
	GC_RigidBodyDynamic(FromFile);

	virtual PropertySet* NewPropertySet();
	virtual void MapExchange(MapFile &f);
	virtual void Serialize(SaveFile &f);
	virtual void TimeStepFixed(float dt);
	static void ProcessResponse(float dt);
	static void PushState();
	static void PopState();

	float Energy() const;
	void Sync(GC_RigidBodyDynamic *src);


	float _av;      // angular velocity
	vec2d _lv;      // linear velocity

	float _inv_m;   // 1/mass
	float _inv_i;   // 1/moment_of_inertia

//	float _Nb;      // braking friction factor X

	float _Nx;      // dry friction factor X
	float _Ny;      // dry friction factor Y
	float _Nw;      // angilar dry friction factor

	float _Mx;      // viscous friction factor X
	float _My;      // viscous friction factor Y
	float _Mw;      // angular viscous friction factor

	float _percussion;  // percussion factor
	float _fragility;   // fragility factor (0 - invulnerability)

	//--------------------------------

	void ApplyMomentum(float momentum);
	void ApplyForce(const vec2d &force);
	void ApplyForce(const vec2d &force, const vec2d &origin);

	void ApplyTorque(float torque);
	void ApplyImpulse(const vec2d &impulse);
	void ApplyImpulse(const vec2d &impulse, const vec2d &origin);
	
	vec2d GetForce() const {	return _lv;	}

	//--------------------------------

#ifdef NETWORK_DEBUG
public:
	virtual DWORD checksum(void) const
	{
		DWORD cs = reinterpret_cast<const DWORD&>(_av);
		cs ^= reinterpret_cast<const DWORD&>(_lv.x) ^ reinterpret_cast<const DWORD&>(_lv.y);
		cs ^= reinterpret_cast<const DWORD&>(_inv_m) ^ reinterpret_cast<const DWORD&>(_inv_i);

		cs ^= reinterpret_cast<const DWORD&>(_Nx);
		cs ^= reinterpret_cast<const DWORD&>(_Ny);
		cs ^= reinterpret_cast<const DWORD&>(_Nw);

		cs ^= reinterpret_cast<const DWORD&>(_Mx);
		cs ^= reinterpret_cast<const DWORD&>(_My);
		cs ^= reinterpret_cast<const DWORD&>(_Mw);

		cs ^= reinterpret_cast<const DWORD&>(_percussion) ^ reinterpret_cast<const DWORD&>(_fragility);

		cs ^= reinterpret_cast<const DWORD&>(_external_force.x);
		cs ^= reinterpret_cast<const DWORD&>(_external_force.y);
		cs ^= reinterpret_cast<const DWORD&>(_external_momentum);

		cs ^= reinterpret_cast<const DWORD&>(_external_impulse.x);
		cs ^= reinterpret_cast<const DWORD&>(_external_impulse.y);
		cs ^= reinterpret_cast<const DWORD&>(_external_torque);

		return GC_RigidBodyStatic::checksum() ^ cs;
	}
#endif


protected:
	virtual bool Ignore(GC_RigidBodyStatic *test) const { return false; }

};

///////////////////////////////////////////////////////////////////////////////
// end of file
