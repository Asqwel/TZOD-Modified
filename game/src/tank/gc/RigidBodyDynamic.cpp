// RigidBodyDynamic.cpp

#include "stdafx.h"
#include "RigidBodyDinamic.h"
#include "Projectiles.h"
#include "Sound.h"

#include "fs/SaveFile.h"
#include "fs/MapFile.h"

#include "level.h"
#include "crate.h"
#include "UserObjects.h"

///////////////////////////////////////////////////////////////////////////////

GC_RigidBodyDynamic::MyPropertySet::MyPropertySet(GC_Object *object)
  : BASE(object)
  , _propM(ObjectProperty::TYPE_FLOAT, "M")
  , _propI(ObjectProperty::TYPE_FLOAT, "I")
  , _propPercussion(ObjectProperty::TYPE_FLOAT, "percussion")
  , _propFragility(ObjectProperty::TYPE_FLOAT, "fragility")
  , _propNx(ObjectProperty::TYPE_FLOAT, "Nx")
  , _propNy(ObjectProperty::TYPE_FLOAT, "Ny")
  , _propNw(ObjectProperty::TYPE_FLOAT, "Nw")
  , _propRotation(ObjectProperty::TYPE_FLOAT, "rotation")
{
	_propM.SetFloatRange(0, 10000);
	_propI.SetFloatRange(0, 10000);
	_propPercussion.SetFloatRange(0, 10000);
	_propFragility.SetFloatRange(0, 10000);
	_propNx.SetFloatRange(0, 10000);
	_propNy.SetFloatRange(0, 10000);
	_propNw.SetFloatRange(0, 10000);
	_propRotation.SetFloatRange(0, PI2);
}

int GC_RigidBodyDynamic::MyPropertySet::GetCount() const
{
	return BASE::GetCount() + 8;
}

ObjectProperty* GC_RigidBodyDynamic::MyPropertySet::GetProperty(int index)
{
	if( index < BASE::GetCount() )
		return BASE::GetProperty(index);

	switch( index - BASE::GetCount() )
	{
	case 0: return &_propM;
	case 1: return &_propI;
	case 2: return &_propPercussion;
	case 3: return &_propFragility;
	case 4: return &_propNx;
	case 5: return &_propNy;
	case 6: return &_propNw;
	case 7: return &_propRotation;
	}

	assert(false);
	return NULL;
}

void GC_RigidBodyDynamic::MyPropertySet::MyExchange(bool applyToObject)
{
	BASE::MyExchange(applyToObject);

	GC_RigidBodyDynamic *tmp = static_cast<GC_RigidBodyDynamic *>(GetObject());
	if( applyToObject )
	{
		tmp->_inv_m = _propM.GetFloatValue() > 0 ? 1.0f / _propM.GetFloatValue() : 0;
		tmp->_inv_i = _propI.GetFloatValue() > 0 ? 1.0f / _propI.GetFloatValue() : 0;
		tmp->_percussion = _propPercussion.GetFloatValue();
		tmp->_fragility = _propFragility.GetFloatValue();
		tmp->_Nx = _propNx.GetFloatValue();
		tmp->_Ny = _propNy.GetFloatValue();
		tmp->_Nw = _propNw.GetFloatValue();
		tmp->SetDirection(vec2d(_propRotation.GetFloatValue()));
	}
	else
	{
		_propM.SetFloatValue(tmp->_inv_m > 0 ? 1.0f / tmp->_inv_m : 0);
		_propI.SetFloatValue(tmp->_inv_i > 0 ? 1.0f / tmp->_inv_i : 0);
		_propPercussion.SetFloatValue(tmp->_percussion);
		_propFragility.SetFloatValue(tmp->_fragility);
		_propNx.SetFloatValue(tmp->_Nx);
		_propNy.SetFloatValue(tmp->_Ny);
		_propNw.SetFloatValue(tmp->_Nw);
		_propRotation.SetFloatValue(tmp->GetDirection().Angle());
	}
}

///////////////////////////////////////////////////////////////////////////////

GC_RigidBodyDynamic::ContactList GC_RigidBodyDynamic::_contacts;
std::stack<GC_RigidBodyDynamic::ContactList> GC_RigidBodyDynamic::_contactsStack;
bool GC_RigidBodyDynamic::_glob_parity = false;

GC_RigidBodyDynamic::GC_RigidBodyDynamic()
  : GC_RigidBodyStatic()
{
	_lv.Zero();
	_av     = 0;
	_inv_m  = 1.0f / 1;
	_inv_i  = _inv_m * 12.0f / (36*36 + 36*36);

	_Nx     = 0;
	_Ny     = 0;
	_Nw     = 0;

	_Mx     = 0;
	_My     = 0;
	_Mw     = 0;

	_percussion = 1;
	_fragility  = 1;

	_external_force.Zero();
	_external_momentum = 0;

	_external_impulse.Zero();
	_external_torque = 0;


	if( _glob_parity ) SetFlags(GC_FLAG_RBDYMAMIC_PARITY, true);
	SetEvents(GC_FLAG_OBJECT_EVENTS_TS_FIXED);
}

GC_RigidBodyDynamic::GC_RigidBodyDynamic(FromFile)
  : GC_RigidBodyStatic(FromFile())
{
}

PropertySet* GC_RigidBodyDynamic::NewPropertySet()
{
	return new MyPropertySet(this);
}

void GC_RigidBodyDynamic::MapExchange(MapFile &f)
{
	GC_RigidBodyStatic::MapExchange(f);

	float rotTmp = GetDirection().Angle();
	MAP_EXCHANGE_FLOAT(inv_m, _inv_m, 1);
	MAP_EXCHANGE_FLOAT(inv_i, _inv_i, 1);
	MAP_EXCHANGE_FLOAT(percussion, _percussion, 1);
	MAP_EXCHANGE_FLOAT(fragility, _fragility, 1);
	MAP_EXCHANGE_FLOAT(Nx, _Nx, 0);
	MAP_EXCHANGE_FLOAT(Ny, _Ny, 0);
	MAP_EXCHANGE_FLOAT(Nw, _Nw, 0);
	MAP_EXCHANGE_FLOAT(rotation, rotTmp, 0);
	if( f.loading() )
	{
		SetDirection(vec2d(rotTmp));
	}
}

void GC_RigidBodyDynamic::Serialize(SaveFile &f)
{
	GC_RigidBodyStatic::Serialize(f);

	f.Serialize(_av);
	f.Serialize(_lv);
	f.Serialize(_inv_m);
	f.Serialize(_inv_i);
	f.Serialize(_Nx);
	f.Serialize(_Ny);
	f.Serialize(_Nw);
	f.Serialize(_Mx);
	f.Serialize(_My);
	f.Serialize(_Mw);
	f.Serialize(_percussion);
	f.Serialize(_fragility);
	f.Serialize(_external_force);
	f.Serialize(_external_momentum);
	f.Serialize(_external_impulse);
	f.Serialize(_external_torque);
}

float GC_RigidBodyDynamic::GetSpinup() const
{
	float result;
	if( _Mw > 0 )
	{
		if( _Nw > 0 )
		{
			if( _av > 0 )
				result = (_av - log(1 + _av * _Mw/_Nw) * _Nw/_Mw) / _Mw;
			else
				result = (_av + log(1 - _av * _Mw/_Nw) * _Nw/_Mw) / _Mw;
		}
		else
		{
			result = _av / _Mw;
		}
	}
	else
	{
		assert(_Nw > 0);
		result = _av*fabs(_av)/_Nw*0.5f;
	}
	return result;
}

vec2d GC_RigidBodyDynamic::GetBrakingLength() const
{
	float result;
	float vx = _lv * GetDirection();
	if( _Mx > 0 )
	{
		if( _Nx > 0 )
		{
			if( vx > 0 )
				result = vx/_Mx - _Nx/(_Mx*_Mx)*(log(_Nx + _Mx*vx)-log(_Nx));
			else
				result = vx/_Mx + _Nx/(_Mx*_Mx)*(log(_Nx - _Mx*vx)-log(_Nx));
		}
		else
		{
			result = vx/_Mx;
		}
	}
	else
	{
		result = vx*fabs(vx)/_Nx*0.5f;
	}
	return GetDirection() * result; // FIXME: add y coordinate
}

void GC_RigidBodyDynamic::TimeStepFixed(float dt)
{
	vec2d dx = _lv * dt;
	vec2d da(_av * dt);
	bool cr=false;
	if (dynamic_cast<GC_Crate *>(this)) 
	{
		if (/*dx != this->GetPos() && da != this->GetDirection() &&*/ !this->IsKilled())
		{
		cr=true;
		g_level->_field.ProcessObject(this,false);
		}
	}
	MoveTo(GetPos() + dx);
	vec2d dirTmp = Vec2dAddDirection(GetDirection(), da);
	dirTmp.Normalize();
	SetDirection(dirTmp);


	//
	// apply external forces
	//

	_lv += (_external_force * dt + _external_impulse) * _inv_m;
	_av += (_external_momentum * dt + _external_torque) * _inv_i;
	_external_force.Zero();
	_external_impulse.Zero();
	_external_momentum = 0;
	_external_torque = 0;


	//
	// linear friction
	//

	vec2d dir_y(GetDirection().y, -GetDirection().x);

	float vx = _lv * GetDirection();
	float vy = _lv * dir_y;

	_lv.Normalize();
	vec2d dev(_lv * GetDirection(), _lv * dir_y);

	if( vx > 0 )
		vx = __max(0, vx - _Nx * dt * dev.x);
	else
		vx = __min(0, vx - _Nx * dt * dev.x);
	vx *= expf(-_Mx * dt);

	if( vy > 0 )
		vy = __max(0, vy - _Ny * dt * dev.y);
	else
		vy = __min(0, vy - _Ny * dt * dev.y);
	vy *= expf(-_My * dt);

	_lv = GetDirection() * vx + dir_y * vy;


	//
	// angular friction
	//
	if( _Mw > 0 )
	{
		float e = expf(-_Mw * dt);
		float nm = _Nw / _Mw * (e - 1);
		if( _av > 0 )
			_av = __max(0, _av * e + nm);
		else
			_av = __min(0, _av * e - nm);
	}
	else
	{
		if( _av > 0 )
			_av = __max(0, _av - _Nw * dt);
		else
			_av = __min(0, _av + _Nw * dt);
	}

	if( cr ) 
	{
		g_level->_field.ProcessObject(this, true);
	}
	assert(!_isnan(_av));
	assert(_finite(_av));

	//------------------------------------
	// collisions

	PtrList<ObjectList> receive;
	g_level->grid_rigid_s.OverlapPoint(receive, GetPos() / LOCATION_SIZE);
	g_level->grid_water.OverlapPoint(receive, GetPos() / LOCATION_SIZE);

	PtrList<ObjectList>::iterator rit = receive.begin();

	Contact c;
	c.depth = 0;
	c.total_np = 0;
	c.total_tp = 0;
	c.obj1_d   = this;

	vec2d myHalfSize(GetHalfLength(),GetHalfWidth());

	for( ; rit != receive.end(); ++rit )
	{
		ObjectList::iterator it = (*rit)->begin();
		for( ; it != (*rit)->end(); ++it )
		{
			GC_RigidBodyStatic *object = (GC_RigidBodyStatic *) (*it);
			if( this == object || Ignore(object) )
			{
				continue;
			}

//			if( object->parity() != _glob_parity )

			if( object->CollideWithRect(myHalfSize, GetPos(), GetDirection(), c.o, c.n, c.depth) )
			{
#ifndef NDEBUG
				for( int i = 0; i < 4; ++i )
				{
					g_level->DbgLine(object->GetVertex(i), object->GetVertex((i+1)&3));
				}
				g_level->DbgLine(c.o, c.o + c.n * 32, 0x00ff00ff);
#endif

				c.t.x =  c.n.y;
				c.t.y = -c.n.x;
				c.obj2_s = object;
				c.obj2_d = dynamic_cast<GC_RigidBodyDynamic*>(object);

				c.obj1_d->AddRef();
				c.obj2_s->AddRef();

				_contacts.push_back(c);
			}
		}
	}
}

float GC_RigidBodyDynamic::geta_s(const vec2d &n, const vec2d &c, const GC_RigidBodyStatic *obj) const
{
	float k1 = n.x*(c.y-GetPos().y) - n.y*(c.x-GetPos().x);
	return (float)
		(
			2 * ( _av * k1 - n.y*_lv.y - n.x*_lv.x ) /
				( k1*k1*_inv_i + n.sqr() * _inv_m )
		);
}

float GC_RigidBodyDynamic::geta_d(const vec2d &n, const vec2d &c, const GC_RigidBodyDynamic *obj) const
{
	float k1 = n.x*(c.y-GetPos().y) - n.y*(c.x-GetPos().x);
	float k2 = n.y*(c.x-obj->GetPos().x) - n.x*(c.y-obj->GetPos().y);
	return (float)
		(
			2 * ( n.y*(obj->_lv.y-_lv.y) + n.x*(obj->_lv.x-_lv.x) + _av*k1 + obj->_av*k2 ) /
			( k1*k1*_inv_i + k2*k2*obj->_inv_i + n.sqr() * (_inv_m+obj->_inv_m) )
		);
}

void GC_RigidBodyDynamic::PushState()
{
	_contactsStack.push(ContactList());
	_contactsStack.top().swap(_contacts);
}

void GC_RigidBodyDynamic::PopState()
{
	_contactsStack.top().swap(_contacts);
	_contactsStack.pop();
}

void GC_RigidBodyDynamic::ProcessResponse(float dt)
{
	for( int i = 0; i < 128; i++ )
	{
		for( ContactList::iterator it = _contacts.begin(); it != _contacts.end(); ++it )
		{
			if( it->obj1_d->IsKilled() || it->obj2_s->IsKilled() ) continue;

			float a;
			if( it->obj2_d )
				a = 0.65f * it->obj1_d->geta_d(it->n, it->o, it->obj2_d);
			else
				a = 0.65f * it->obj1_d->geta_s(it->n, it->o, it->obj2_s);

			if( a >= 0 )
			{
				a = __max(0.01f * (float) (i>>2), a);
				it->total_np += a;

				float nd = it->total_np/60;

				bool o1 = !it->obj1_d->CheckFlags(GC_FLAG_RBSTATIC_PHANTOM);
				bool o2 = !it->obj2_s->CheckFlags(GC_FLAG_RBSTATIC_PHANTOM);

				if( nd > 3 && o1 && o2 )
				{
					if( it->obj2_d )
					{
						it->obj1_d->TakeDamage(a/60 * it->obj2_d->_percussion * it->obj1_d->_fragility, it->o, it->obj2_d->GetOwner());
						it->obj2_d->TakeDamage(a/60 * it->obj1_d->_percussion * it->obj2_d->_fragility, it->o, it->obj1_d->GetOwner());
					}
					else
					{
						it->obj1_d->TakeDamage(a/60 * it->obj1_d->_fragility, it->o, it->obj1_d->GetOwner());
						it->obj2_s->TakeDamage(a/60 * it->obj1_d->_percussion, it->o, it->obj1_d->GetOwner());
					}
				}

				if( it->obj1_d->IsKilled() || it->obj2_s->IsKilled() )
					a *= 0.1f;

				// phantom may not affect real objects but it may affect other fhantoms
				vec2d delta_p = it->n * (a + it->depth);

				if( !o1 && !o2 || o2 )
					it->obj1_d->impulse(it->o, delta_p);

				if( it->obj2_d && (!o1 && !o2 || o1) ) 
					it->obj2_d->impulse(it->o, -delta_p);


				//
				// friction
				//
#if 0
				const float N = 0.1f;

				float maxb;
				if( it->obj2_d )
					maxb = it->obj1_d->geta_d(it->t, it->o, it->obj2_d) / 2;
				else
					maxb = it->obj1_d->geta_s(it->t, it->o, it->obj2_s) / 2;

				float signb = maxb > 0 ? 1.0f : -1.0f;
				if( (maxb = fabsf(maxb)) > 0 )
				{
					float b = __min(maxb, a*N);
					it->total_tp += b;
					delta_p = it->t * b * signb;
					it->obj1_d->impulse(it->o,  delta_p);
					if( it->obj2_d ) it->obj2_d->impulse(it->o, -delta_p);
				}
#endif
			}
		}
	}

	for( ContactList::iterator it = _contacts.begin(); it != _contacts.end(); ++it )
	{
		float nd = (it->total_np + it->total_tp)/60;

		if( nd > 3 )
		{
			if( nd > 10 )
				PLAY(SND_Impact2, it->o);
			else
				PLAY(SND_Impact1, it->o);
		}
		else if( it->total_tp > 10 )
		{
			PLAY(SND_Slide1, it->o);
		}

		it->obj1_d->Release();
		it->obj2_s->Release();
	}

	_contacts.clear();
}

void GC_RigidBodyDynamic::impulse(const vec2d &origin, const vec2d &impulse)
{
	_lv += impulse * _inv_m;
	_av += ((origin.x-GetPos().x)*impulse.y-(origin.y-GetPos().y)*impulse.x) * _inv_i;
	assert(!_isnan(_av) && _finite(_av));
}

void GC_RigidBodyDynamic::ApplyMomentum(float momentum)
{
	_external_momentum += momentum;
	assert(!_isnan(_external_momentum) && _finite(_external_momentum));
}

void GC_RigidBodyDynamic::ApplyForce(const vec2d &force)
{
	_external_force += force;
}

void GC_RigidBodyDynamic::ApplyForce(const vec2d &force, const vec2d &origin)
{
	_external_force += force;
	_external_momentum += (origin.x-GetPos().x)*force.y-(origin.y-GetPos().y)*force.x;
}

void GC_RigidBodyDynamic::ApplyImpulse(const vec2d &impulse, const vec2d &origin)
{
	_external_impulse += impulse;
	_external_torque  += (origin.x-GetPos().x)*impulse.y-(origin.y-GetPos().y)*impulse.x;
	assert(!_isnan(_external_torque) && _finite(_external_torque));
}

void GC_RigidBodyDynamic::ApplyImpulse(const vec2d &impulse)
{
	_external_impulse += impulse;
}

void GC_RigidBodyDynamic::ApplyTorque(float torque)
{
	_external_torque  += torque;
	assert(!_isnan(_external_torque) && _finite(_external_torque));
}

float GC_RigidBodyDynamic::Energy() const
{
	float e = 0;
	if( _inv_i != 0 ) e  = _av*_av / _inv_i;
	if( _inv_m != 0 ) e += _lv.sqr() / _inv_m;
	return e;
}

vec2d GC_RigidBodyDynamic::GetForce() const {	return _external_force;	}

void GC_RigidBodyDynamic::Sync(GC_RigidBodyDynamic *src)
{
//	GC_RigidBodyStatic::Sync(src);

	MoveTo(src->GetPos());
	SetDirection(src->GetDirection());

	_av = src->_av;
	_lv = src->_lv;
//	_inv_m = _inv_m;
//	_inv_i = _inv_i;
	_Nx = src->_Nx;
	_Ny = src->_Ny;
	_Nw = src->_Nw;
	_Mx = src->_Mx;
	_My = src->_My;
	_Mw = src->_Mw;
	_percussion = 0;//src->_percussion;
	_fragility = 0;//src->_fragility;
	_external_force = src->_external_force;
	_external_momentum = src->_external_momentum;
	_external_impulse = src->_external_impulse;
	_external_torque = src->_external_torque;
}

// end of file
