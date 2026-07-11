/* -*-C++-*-
 *Header:
 *File: object.h (for C++ & cc)
 *Author: Noda Itsuki
 *Date: 1995/02/21
 *EndHeader:
 */

/*
 *Copyright:

 Copyright (C) 1996-2000 Electrotechnical Laboratory.
 Itsuki Noda, Yasuo Kuniyoshi and Hitoshi Matsubara.
 Copyright (C) 2000, 2001 RoboCup Soccer Server Maintainance Group.
 Patrick Riley, Tom Howard, Daniel Polani, Itsuki Noda,
 Mikhail Prokopenko, Jan Wendler
 Copyright (C) 2002- RoboCup Soccer Simulator Maintainance Group.

 This file is a part of SoccerServer.

 This code is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 3 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.

 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

 *EndCopyright:
 */


/*
 *ModifyHistory
 *
 *EndModifyHistory
 */


#ifndef RCSSSERVER_OBJECT_H
#define RCSSSERVER_OBJECT_H

#include "types.h"
#include "utility.h"

#include <string>
#include <memory>
#include <iostream>
#include <cmath>
#include <rcss/vector.h>

/*
 *===================================================================
 *Part: Plain Vector
 *===================================================================
 */
class PVector {
public:
    double x;
    double y;

    explicit
    PVector( const double & xx = 0.0,
             const double & yy = 0.0 )
        : x( xx )
        , y( yy )
      { }

    const PVector & assign( const double & xx,
                            const double & yy )
      {
          x = xx;
          y = yy;
          return *this;
      }

    PVector	operator-() const
      {
          return PVector( -x, -y );
      }

    const PVector & operator+() const
      {
          return *this;
      }

    const PVector & operator+=( const PVector & v )
      {
          x += v.x;
          y += v.y;
          return *this;
      }

    const PVector & operator-=( const PVector & v )
      {
          x -= v.x;
          y -= v.y;
          return *this;
      }

    const PVector & operator*=( const double & a )
      {
          x *= a;
          y *= a;
          return *this;
      }

    const PVector & operator/=( const double & a )
      {
          x /= a;
          y /= a;
          return *this;
      }

    double r2() const
      {
          return x*x + y*y;
      }

    double r() const
      {
          return std::sqrt( x*x + y*y );
      }

    double th() const
      {
          return ( (x == 0.0) && ( y == 0.0 )
                   ? 0.0
                   : std::atan2( y, x ) );
      }

    const PVector & normalize( const double l = 1.0 )
      {
          *this *= ( l / std::max( r(), EPS ) );
          return *this;
      }

    double distance2( const PVector & orig ) const
      {
          return ( PVector( *this ) -= orig ).r2();
      }

    double distance( const PVector & orig ) const
      {
          return ( PVector( *this ) -= orig ).r();
      }

    double distance2 ( const rcss::geom::Vector2D & orig) const
      {
          return std::pow(this->x - orig.getX(), 2) + std::pow(this->y - orig.getY(), 2);
      }

    double distance ( const rcss::geom::Vector2D & orig) const
      {
          return std::sqrt(this->distance2(orig));
      }

    const PVector & rotate( const double ang )
      {
          double c = std::cos( ang );
          double s = std::sin( ang );
          return assign( this->x * c - this->y * s,
                         this->x * s + this->y * c );
      }

    bool between( const PVector & begin,
                  const PVector & end ) const;

    static
    PVector fromPolar( const double & r,
                       const double & ang )
      {
          return PVector( r * std::cos( ang ),
                          r * std::sin( ang ) );
      }

};

inline
bool
operator==( const PVector & lhs,
            const PVector & rhs )
{
    return ( lhs.x == rhs.x && lhs.y == rhs.y );
}

inline
bool
operator!=( const PVector & lhs,
            const PVector & rhs )
{
    return !( lhs == rhs );
}

inline
PVector
operator+( const PVector & lhs,
           const PVector & rhs )
{
    return PVector( lhs ) += rhs;
}

inline
PVector
operator-( const PVector & lhs,
           const PVector & rhs )
{
    return PVector( lhs ) -= rhs;
}


inline
std::ostream &
operator<<( std::ostream & o,
            const PVector & v )
{
    return o << "#V[" << v.x << "," << v.y << "]";
}



// inline
// double
// PVector::vangle( const PVector & target,
//                  const PVector & origin ) const
// {
//     return ( origin - *this ).angle( target - *this );
// }

// inline
// double
// PVector::vangle( const PVector & target,
//                  const double & origin ) const
// {
//     return normalize_angle( ( target - *this ).angle() - origin );
// }




/*
 *===================================================================
 *Part: Area
 *===================================================================
 */
class RArea {
private:
    double M_left;
    double M_right;
    double M_top;
    double M_bottom;

    // not used
    RArea() = delete;
public:
    RArea( const double & l,
           const double & r,
           const double & t,
           const double & b )
        : M_left( l ),
          M_right( r ),
          M_top( t ),
          M_bottom( b )
      { }

    RArea( const PVector & center,
           const PVector & size )
        : M_left( center.x - size.x*0.5 ),
          M_right( center.x + size.x*0.5 ),
          M_top( center.y - size.y*0.5 ),
          M_bottom( center.y + size.y*0.5 )
      { }

    const double & left() const { return M_left; }
    const double & right() const { return M_right; }
    const double & top() const { return M_top; }
    const double & bottom() const { return M_bottom; }

    bool inArea( const PVector & p ) const
      {
          return ( p.x >= left() )
              && ( p.x <= right() )
              && ( p.y >= top() )
              && ( p.y <= bottom() );
      }

    /*! find nearest horizontal line */
    PVector nearestHEdge( const PVector & p ) const;

		/*! find nearest vertical line */
    PVector nearestVEdge( const PVector & p ) const;

    PVector nearestEdge( const PVector & p ) const;

    PVector randomize() const;

    std::ostream & print( std::ostream & o ) const;
};

//inline
//std::ostream &
//operator<<( std::ostream & o, const RArea & a )
//{
//    return a.print( o );
//}

class CArea {
private:
    PVector M_center;
    double M_radius;

    // not used
    CArea() = delete;
public:
    CArea( const PVector & center,
           const double & radius )
        : M_center( center ),
          M_radius( radius )
      { }

    const
    PVector & center() const
      {
          return M_center;
      }

    const
    double & radius() const
      {
          return M_radius;
      }

    bool inArea( const PVector & p ) const
      {
          return M_center.distance( p ) <= M_radius;
      }

    PVector nearestEdge( const PVector & p ) const;

};


inline
bool
operator==( const CArea & lhs,
            const CArea & rhs  )
{
    return ( lhs.center() == rhs.center()
             && lhs.radius() == rhs.radius() );
}

inline
bool
operator!=( const CArea & lhs,
            const CArea & rhs )
{
    return !( lhs == rhs );
}

//extern std::ostream& operator<< (std::ostream& o, const CArea& a);

// bool
// intersect( const PVector & begin,
//            const PVector & end,
//            const CArea & circle,
//            PVector & inter );
//CArea
//nearestPost( const PVector & pos,
//             const double & size );

/*
 *===================================================================
 *Part: PObject
 *===================================================================
 */
class PObject {
private:

    static int S_object_count;

    const int M_id;

    std::string M_name;
    std::string M_short_name;
    std::string M_close_name;
    std::string M_short_close_name;

    const double M_object_version;

protected:
    double	M_size; //! object's radiuos value
    PVector M_pos;
    bool M_enable;

private:

    // not used
    PObject() = delete;
    const PObject & operator=( const PObject & ) = delete;

public:

    PObject( const std::string & name,
             const std::string & short_name,
             const std::string & close_name,
             const std::string & short_close_name,
             const PVector& p = PVector( 0.0,0.0 ),
             const double & v = 3.0 );

    virtual
    ~PObject()
      { }

    int id() const
      {
          return M_id;
      }

    const std::string & name() const
      {
          return M_name;
      }

    const std::string & shortName() const
      {
          return M_short_name;
      }

    const std::string & closeName() const
      {
          return M_close_name;
      }

    const std::string & shortCloseName() const
      {
          return M_short_close_name;
      }

    double objectVersion() const
      {
          return M_object_version;
      }

    double size() const
      {
          return M_size;
      }

    const PVector & pos() const
      {
          return M_pos;
      }

    bool isEnabled() const
      {
          return M_enable;
      }

    std::ostream & print( std::ostream & o ) const;

protected:

    void setName( const std::string & name,
                  const std::string & short_name )
      {
          M_name = name;
          M_short_name = short_name;
      }

};

inline
std::ostream &
operator<<( std::ostream & o, const PObject & v )
{
    return v.print( o );
}


class Stadium;
//class Weather;

class MPObject
    : public PObject {
protected:
    Stadium	& M_stadium;

    PVector	M_vel;

    PVector	M_accel;
    double	M_decay;
    double	M_randp;

    double M_weight;
    double M_max_speed;
    // th 6.3.00
    double M_max_accel;

private:
    //const Weather * M_weather;

    /* new collision stuff */
    PVector M_post_collision_pos; //!< accumulated collision pos
    int M_collision_count;
    bool M_collided;

private:

    // not used
    MPObject() = delete;
    const MPObject & operator=( const MPObject & ) = delete;

public:

    MPObject( Stadium & stadium,
              const std::string & name,
              const std::string & short_name,
              const std::string & close_name,
              const std::string & short_close_name );

    virtual
    ~MPObject()
      { }

    const
    Stadium & stadium() const
      {
          return M_stadium;
      }

    const
    PVector & vel() const
      {
          return M_vel;
      }
    const
    PVector & accel() const
      {
          return M_accel;
      }

    void _inc();

    void _turn()
      {
          turnImpl();
      }
private:
    PVector noise();
    PVector wind();

public:
    void moveTo( const PVector & pos )
      {
          M_pos = pos;
      }

    void moveTo( const PVector & pos,
                 const PVector & vel,
                 const PVector & accel );

    void setConstant( const double & size,
                      const double & decay,
                      const double & randp,
                      const double & weight,
                      const double & max_speed,
                      const double & max_accel );

    void push( const PVector & f )
      {
          M_accel += f;
      }

    void clearCollision()
      {
          M_post_collision_pos.assign( 0.0, 0.0 );
          M_collision_count = 0;
          // M_collided is reset in updateCollVel()
      }
    void collide( const PVector & col_pos )
      {
          M_post_collision_pos += col_pos;
          ++M_collision_count;
          M_collided = true;
      }
    void updateCollisionVel()
      {
          if ( M_collided )
          {
              M_vel *= -0.1;
              M_collided = false;
          }
      }
    void moveToCollisionPos();

    std::ostream & print( std::ostream & o ) const;


protected:

    virtual
    void turnImpl() = 0;

    virtual
    void updateAngle() = 0;

    virtual
    void collidedWithPost() = 0;

    //! 3D ball extension: hook allowing a subclass to report that it is
    //! currently flying above the goal frame's solid height, so the shared
    //! 2D post-collision handling in _inc() (unchanged below) can let it
    //! pass over the post's (x,y) footprint instead of bouncing off it.
    //! Defaults to false (post is always solid) for every MPObject other
    //! than Ball -- e.g. Player -- so this is a no-op/byte-identical change
    //! for anything that isn't the ball. Ball overrides this using its
    //! own height vs. ServerParam::goalHeight() (see object.cpp); it is
    //! always false in 2d_mode since the ball's height never leaves 0.0.
    virtual
    bool aboveGoalHeight() const
      {
          return false;
      }

    virtual
    double maxAccel() const = 0;

    virtual
    double maxSpeed() const = 0;

};

inline
std::ostream &
operator<<( std::ostream & o, const MPObject & v )
{
    return v.print( o );
}



class Ball
    : public MPObject {
private:

    //! ball's height above the pitch (0 = resting on the ground). Ball-only
    //! field -- PVector/MPObject stay 2D, per the 3D extension's confirmed
    //! architecture (see plan_spec.md Step 2).
    double M_pos_z;
    //! ball's vertical velocity, same per-cycle integration convention as M_vel
    //! (pos_z += vel_z; no dt multiplication).
    double M_vel_z;
    //! vertical acceleration accumulated by a loft kick this cycle, consumed
    //! (and reset to 0) by incZ() -- mirrors MPObject::push()/M_accel's
    //! push-then-consume pattern, just for the z axis.
    double M_accel_z;

public:

    explicit
    Ball( Stadium & stadium );

    virtual
    void turnImpl() override
      { }

    virtual
    void updateAngle() override
      { }

    virtual
    void collidedWithPost() override
      { }

    virtual
    bool aboveGoalHeight() const override;

    virtual
    double maxAccel() const override
      {
          return M_max_accel;
      }

    virtual
    double maxSpeed() const override
      {
          return M_max_speed;
      }

    double posZ() const
      {
          return M_pos_z;
      }

    double velZ() const
      {
          return M_vel_z;
      }

    void setPosZ( double z )
      {
          M_pos_z = z;
      }

    void setVelZ( double vz )
      {
          M_vel_z = vz;
      }

    //! Accumulates a vertical kick impulse to be applied by incZ() this
    //! cycle. Ball-only counterpart of MPObject::push() (which only
    //! accumulates into the 2D M_accel).
    void pushZ( double az )
      {
          M_accel_z += az;
      }

    //! Ball-only z/gravity/bounce/loft-decay integration. Called by
    //! Stadium::incMovableObjects() immediately after the existing, UNCHANGED
    //! MPObject::_inc() loop finishes for every movable object, and only
    //! when !ServerParam::instance().is2dMode(). Never touches PVector or
    //! MPObject::_inc()'s shared 2D integration/post-collision code -- see
    //! plan_spec.md Step 2 for the full rationale.
    void incZ();

private:

    //! Analytic fixed-point bounce-settle speed (ported from
    //! 3d-kick-lab/physics.js's _bounceSettleThreshold()) -- guards against a
    //! ball bouncing forever at a tiny, non-decaying amplitude for
    //! gravity/restitution combinations where the raw bounce_stop_speed
    //! check alone never triggers.
    double bounceSettleThreshold() const;

    //! Scales the ball's ENTIRE velocity vector (vx, vy, and the
    //! just-reflected vz) by the single ball_bounce_restitution
    //! coefficient, once per genuine ground/post/crossbar bounce event --
    //! a uniform "whole-speed" kinetic-energy loss rather than treating the
    //! vertical and horizontal components separately. Ported from
    //! 3d-kick-lab/physics.js's _applyBounceEnergyLoss(). Call this ONCE per
    //! ground-touch, right after vel_z has been reflected (or zeroed on
    //! settle) -- this method only scales vel.x/vel.y; the caller is
    //! responsible for reflecting/scaling vel_z itself.
    void applyBounceEnergyLoss();


    //! NEW, additive, Ball-only 3D goalpost + crossbar collision test,
    //! called from incZ(). May reuse the existing free functions
    //! nearestPost()/intersect() (they operate on PVector/CArea only, not
    //! MPObject members, so calling them here is safe) but never modifies
    //! the shared 2D post-collision loop inside MPObject::_inc().
    void checkPostAndCrossbar3D();
};


#endif // _H_OBJECT
