/* -*- Mode: C++ -*-
 *Header:
 *File: object.C (for C++ & cc)
 *Author: Noda Itsuki
 *Date: 1995/02/21
 *EndHeader:
 */

/*
 *Copyright:

 Copyright (C) 1996-2000 Electrotechnical Laboratory.
 Itsuki Noda, Yasuo Kuniyoshi and Hitoshi Matsubara.
 Copyright (C) 2000,2001 RoboCup Soccer Server Maintainance Group.
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "object.h"

#include "stadium.h"
#include "param.h"
#include "player.h"
#include "random.h"
#include "types.h"
#include "utility.h"

#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cassert>

/*
 *===================================================================
 *Part: Plane Vector
 *===================================================================
 */

bool
PVector::between( const PVector & begin,
                  const PVector & end ) const
{
    if ( begin.x > end.x )
    {
        return between( end, begin );
    }

    if ( begin.x <= x && x <= end.x )
    {
        if ( begin.y < end.y )
        {
            return begin.y <= y && y <= end.y;
        }
        else
        {
            return begin.y >= y && y >= end.y;
        }
    }
    //      std::cout << begin.x << " > " << x
    //                << "\n|| " << x << " > " << end.x;
    //      if( begin.y < end.y )
    //      {
    //          std::cout << "\n|| " << begin.y << " > " << y
    //                    << "\n|| " << y << " > " << end.y << std::endl;
    //      }
    //      else
    //      {
    //          std::cout << "\n|| " << begin.y << " < " << y
    //                    << "\n|| " << y << " < " << end.y << std::endl;
    //      }
    return false;
}
/*
 *===================================================================
 *Part: Area
 *===================================================================
 */

PVector
RArea::nearestHEdge( const PVector & p ) const
{
    return PVector( std::min( std::max( p.x, left() ), right() ),
                    ( std::fabs( p.y - top() ) < std::fabs( p.y - bottom() )
                      ? top()
                      : bottom() ) );
}

PVector
RArea::nearestVEdge( const PVector & p ) const
{
    return PVector( ( std::fabs( p.x - left() ) < std::fabs( p.x - right() )
                      ? left()
                      : right() ),
                    std::min( std::max( p.y, top() ), bottom() ) );
}

PVector
RArea::nearestEdge( const PVector & p ) const
{
    if ( std::min( std::fabs( p.x - left() ), std::fabs( p.x - right() ) )
         < std::min( std::fabs( p.y - top() ), std::fabs( p.y - bottom() ) ) )
    {
        return nearestVEdge( p );
    }
    else
    {
        return nearestHEdge( p );
    }
}

PVector
RArea::randomize() const
{
    return PVector( drand( left(), right() ),
                    drand( bottom(), top() ) );
}

std::ostream &
RArea::print( std::ostream& o ) const
{
    return o << "#A[h:" << left() << "~" << right()
             << ",v:" << top() << "~" << bottom() << "]";
}




PVector
CArea::nearestEdge( const PVector & p ) const
{
    PVector dif = p - M_center;
    if ( dif.x == 0.0 && dif.y == 0.0 )
        dif = PVector( EPS, EPS );

    dif.normalize( M_radius );

    return M_center + dif;
}

//std::ostream &
//operator<<( std::ostream & o, const CArea & a )
//{
//    return o << "#A[x:" << a.center.x <<
//				",y:" << a.center.y <<
//				",r:" << a.r << "]";
//}

namespace {
bool
intersect( const PVector & begin,
           const PVector & end,
           const CArea & circle,
           PVector & inter )
{
    if ( begin == end )
    {
        return false;
    }

    if ( ( begin - end ).r() < ( begin - circle.center() ).r() - circle.radius() )
    {
        // object wont get within circles range
        return false;
    }

    if ( circle.center() == PVector() )
    {
        double dx = end.x - begin.x;
        double dy = end.y - begin.y;
        //          std::cout << dx << endl;
        //          std::cout << dy << endl;
        double dr = std::sqrt( dx*dx + dy*dy );
        //          std::cout << dr << endl;
        double D = begin.x * end.y - end.x * begin.y;
        double descrim = circle.radius() * circle.radius() * dr*dr - D*D;
        //          std::cout << descrim << endl;
        if ( descrim <= 0.0 )
        {
            // no collision of tagent
            //              std::cout << "Descrim < 0\n";
            return false;
        }
        else
        {
            descrim = std::sqrt( descrim );
            //              std::cout << descrim << endl;

            double x1 = (D*dy + dx * descrim) / (dr*dr);
            double x2 = (D*dy - dx * descrim) / (dr*dr);
            double y1 = (-D*dx + std::fabs( dy ) * descrim) / (dr*dr);
            double y2 = (-D*dx - std::fabs( dy ) * descrim) / (dr*dr);
            //              std::cout << x1 << endl;
            //              std::cout << x2 << endl;
            //              std::cout << y1 << endl;
            //              std::cout << y2 << endl;
            PVector first, second;
            if ( dy < 0 )
            {
                first = PVector( x2, y1 );
                second = PVector( x1, y2 );
            }
            else
            {
                first = PVector( x1, y1 );
                second = PVector( x2, y2 );
            }

            if ( ! first.between( begin, end )
                 && ! second.between( begin, end ) )
            {
                // intersections are not between the end points
                //                  std::cout << "Coll outside of end points\n"
                //                            << "begin = " << begin << std::endl
                //                            << "end = " << end << std::endl
                //                            << "first = " << first << std::endl
                //                            << "second = " << second << std::endl;

                return false;
            }

            if ( ! first.between( begin, end ) )
            {
                inter = second;
                second = first;
            }
            else if ( ! second.between( begin, end ) )
            {
                inter = first;
            }
            else
            {
                if ( ( begin - first ).r() < ( begin - second ).r() )
                {
                    inter = first;
                }
                else
                {
                    inter = second;
                    second = first;
                }
            }

            if ( inter == begin
                 && ! second.between( begin, end ) )
            {
                //                  std::cout << "Fake collision\n"
                //                            << "begin = " << begin << std::endl
                //                            << "end = " << end << std::endl
                //                            << "first = " << inter << std::endl
                //                            << "second = " << second << std::endl;
                // fake collision.  Object is tagent to the circle and moving away
                return false;
            }
            return true;
        }
    }
    else
    {
        if ( intersect( begin - circle.center(), end - circle.center(),
                        CArea( PVector(), circle.radius() ), inter ) )
        {
            inter += circle.center();
            return true;
        }
        return false;
    }
}
}

namespace {
CArea
nearestPost( const PVector & pos,
             const double & size )
{
    PVector nearest_gpost;

    if ( pos.y > 0 )
    {
        if ( pos.x > 0 )
        {
            nearest_gpost = PVector( ServerParam::PITCH_LENGTH*0.5
                                     - ServerParam::instance().goalPostRadius(),
                                     ServerParam::instance().goalWidth()*0.5
                                     + ServerParam::instance().goalPostRadius() );
        }
        else
        {
            nearest_gpost = PVector( - ServerParam::PITCH_LENGTH*0.5
                                     + ServerParam::instance().goalPostRadius(),
                                     ServerParam::instance().goalWidth()*0.5
                                     + ServerParam::instance().goalPostRadius() );
        }
    }
    else
    {
        if ( pos.x > 0 )
        {
            nearest_gpost = PVector( ServerParam::PITCH_LENGTH*0.5
                                     - ServerParam::instance().goalPostRadius(),
                                     - ServerParam::instance().goalWidth()*0.5
                                     - ServerParam::instance().goalPostRadius() );
        }
        else
        {
            nearest_gpost = PVector( - ServerParam::PITCH_LENGTH*0.5
                                     + ServerParam::instance().goalPostRadius(),
                                     - ServerParam::instance().goalWidth()*0.5
                                     - ServerParam::instance().goalPostRadius() );
        }
    }

    return CArea( nearest_gpost, ServerParam::instance().goalPostRadius() + size );
}
}

/*
 *===================================================================
 *Part: PObject
 *===================================================================
 */

int PObject::S_object_count = 0;

/* pfr 06/07/200 added short name support */
PObject::PObject( const std::string & name,
                  const std::string & short_name,
                  const std::string & close_name,
                  const std::string & short_close_name,
                  const PVector & p,
                  const double & v )
    : M_id( S_object_count ),
      M_name( name ),
      M_short_name( short_name ),
      M_close_name( close_name ),
      M_short_close_name( short_close_name ),
      M_object_version( v ),
      M_size( 1.0 ),
      M_pos( p ),
      M_enable( true )
{
    ++S_object_count;
}

std::ostream &
PObject::print( std::ostream & o ) const
{
    o << "#Ob[" << this->id();
    if( ! name().empty() )
        o << ":" << name() << "";
    return o << ":pos=" << this->M_pos
             << ",size=" << this->M_size
        //<< ",angle=" << this->M_angle
             << "]";
}


/*
 *===================================================================
 *Part: MPObject
 *===================================================================
 */

/* pfr 06/07/200 added short name support */
MPObject::MPObject( Stadium & stadium,
                    const std::string & name,
                    const std::string & short_name,
                    const std::string & close_name,
                    const std::string & short_close_name )
    : PObject( name, short_name,
               close_name, short_close_name )
    , M_stadium( stadium )
    , M_vel( 0.0,0.0 )
    , M_accel( 0.0,0.0 )
{
    //assert( stadium );
    //M_weather = &( stadium->weather() );
}

std::ostream &
MPObject::print( std::ostream & o ) const
{
    o << "#Ob[" << this->id();
    if( ! name().empty() )
        o << ":" << name() << "";
    return o << ":pos=" << this->M_pos
             << ",size=" << this->M_size
        //<< ",angle=" << Rad2IDegRound( this->M_angle )
             << ",vel=" << this->M_vel
             << ",acc=" << this->M_accel
             << ",decay=" << this->M_decay
             << ",randp=" << this->M_randp
             << "]";
}

void
MPObject::moveTo( const PVector & pos,
                  const PVector & vel,
                  const PVector & accel )
{
    M_pos = pos;
    M_vel = vel;
    M_accel = accel;
}

void
MPObject::setConstant( const double & size,
                       const double & decay,
                       const double & randp,
                       const double & weight,
                       const double & max_speed,
                       const double & max_accel )
{
    M_size = size;
    M_decay = decay;
    M_randp = randp;
    M_weight = weight;
    M_max_speed = max_speed;
    M_max_accel = max_accel;
}

PVector
MPObject::noise()
{
    double maxrnd = M_randp * vel().r();
    return PVector::fromPolar( drand( 0.0, maxrnd ),
                               drand( -M_PI, M_PI ) );
    //return PVector( drand( -maxrnd, maxrnd ),
    //                drand( -maxrnd, maxrnd ) );
}

PVector
MPObject::wind()
{
    const Weather & w = M_stadium.weather();

    if ( w.windRand() < EPS )
    {
        return PVector( 0.0, 0.0 );
    }

    const double speed = M_vel.r();
    return PVector( speed * ( w.windVector().x +
                              drand( - w.windRand(), + w.windRand() ) ) /
                    ( M_weight * ServerParam::instance().windWeight() ),
                    speed * ( w.windVector().y +
                              drand( - w.windRand(), + w.windRand() ) ) /
                    ( M_weight * ServerParam::instance().windWeight() ));
}

void
MPObject::_inc()
{
    if ( M_accel.x || M_accel.y )
    {
        double max_a = maxAccel();
        double max_s = maxSpeed();

        double tmp = M_accel.r();
        if ( tmp > max_a )
        {
            M_accel *= ( max_a / tmp );
        }

        M_vel += M_accel;
        tmp = M_vel.r();
        if ( tmp > max_s )
        {
            M_vel *= ( max_s / tmp );
        }
    }

    updateAngle();

    M_vel += noise();
    M_vel += wind();

    // 3D ball extension: a ball flying above the goal frame's solid height
    // (see aboveGoalHeight(), always false for Player/anything but Ball, and
    // always false in 2d_mode) must sail over the post's (x,y) footprint
    // instead of bouncing off it -- so skip the shared 2D post-collision
    // handling below entirely while airborne above the post/crossbar.
    if ( ! aboveGoalHeight() )
    {
        CArea post = nearestPost( pos(), M_size );

        //      std::cout << "pos = " << pos << endl;
        //      std::cout << "nearest post = " << post << endl;
        //      std::cout << "dist = " << (pos - post.center).r() << endl;
        while ( ( pos() - post.center() ).r() < post.radius() )
        {
            //          std::cout << "In post\n";
            // then the ball has overlapped the post.  Either it was moved
            // there or "pushed".  Either way, we just move the ball away
            // from the post
            PVector diff = pos() - post.center();
            if ( diff == PVector() )
            {
                diff = PVector::fromPolar( post.radius(),
                                           drand( -M_PI, +M_PI ) );
            }
            else
            {
                diff.normalize( post.radius() );
            }

            M_pos = post.center() + diff;

            while ( ( pos() - post.center() ).r() < post.radius() )
            {
                // noise keeps it inside the post, move it a bit further out
                diff.normalize( diff.r() * 1.01 );
                M_pos = post.center() + diff;
            }

            if ( M_vel.x != 0.0 || M_vel.y != 0.0 )
            {
                PVector pos2center = post.center() - pos();
                M_vel.rotate( -pos2center.th() );
                M_vel.x = - M_vel.x;
                M_vel.rotate( pos2center.th() );
            }

            post = nearestPost( pos(), M_size );
            //         std::cout << M_stadium.time() << ": Colliding with post\n"
            //                   << "  pos = " << pos() << '\n'
            //                   << "  vel = " << vel() << '\n'
            //                   << "  nearest post = " << post.center() << '\n'
            //                   << "  dist = " << ( pos() - post.center() ).r() << std::endl;

            collidedWithPost();
        }

        PVector new_pos = pos() + M_vel;
        CArea second_post = nearestPost( new_pos, M_size );
        PVector inter;
        bool second = false;

        //      std::cout << "vel = " << vel << endl;
        //      std::cout << "new_pos = " << new_pos << endl;
        //int loop_count = 0;
        while ( pos() != new_pos
                && ( ( intersect( pos(), new_pos, post, inter ) )
                     || ( post != second_post
                          && ( second = intersect( pos(), new_pos, second_post, inter ) )
                          )
                     )
                )
        {
            //         ++loop_count;
            //         std::cout << M_stadium.time() <<": Collision: " << loop_count << "\n"
            //                   << "  pos=" << pos() << '\n';

            // handle collision
            M_pos = inter;

            //         std::cout << "  intersect=" << pos() << '\n';

            PVector rem = new_pos - pos();
            PVector coll_2_circle;
            if ( second )
            {
                coll_2_circle = second_post.center() - pos();
            }
            else
            {
                coll_2_circle = post.center() - pos();
            }

            // 2008-05-22 akiyama
            // fixed endless-loop bug.
            // If this small vector is not added to M_pos, intersect() may still
            // return pos() as the intersect point.
            M_pos += PVector::fromPolar( EPS, coll_2_circle.th() + 180.0 );

            //         std::cout << "  rem = " << rem << '\n';

            rem.rotate( -coll_2_circle.th() );
            rem.x = -rem.x;
            rem.rotate( coll_2_circle.th() );

            new_pos = pos() + rem;
            //         std::cout << "  rem = " << rem << '\n';

            // setup post and second post for next loop
            post = nearestPost( pos(), M_size );
            second_post = nearestPost( new_pos, M_size );

            //         std::cout << "  pos = " << pos() << '\n'
            //                   << "  new_pos = " << new_pos << '\n'
            //                   << "  nearest post = " << post.center() << '\n'
            //                   << "  dist = " << ( pos() - post.center() ).r() << '\n';

            // setup vel so it will decay normally.  The collisions are
            // elastic, so the maginitude does not change, but the heading
            // does
            M_vel = PVector::fromPolar( M_vel.r(), rem.th() );
            //         std::cout << "  vel = " << vel() << std::endl;

            second = false;

            collidedWithPost();
        }

        M_pos = new_pos;
    }
    else
    {
        // 3D ball extension: above the post/crossbar there is nothing solid
        // to collide with, so just apply the plain (uncollided) movement.
        M_pos = pos() + M_vel;
    }

    M_vel *= M_decay;
    M_accel *= 0.0;
}

// void MPObject::collide(MPObject& obj)
// {
//     double r = size + obj.size;
//     PVector dif = (pos - obj.pos);
//     double d = pos.distance(obj.pos);
//     Angle th = fabs(dif.angle(vel));
//     double l1 = d * cos(th);
//     double h = d * sin(th);
//     double cosp = h / r;
//     double sinp = sqrt(1.0 - square(cosp));
//     double l2 = r * sinp;
//     PVector dv = vel;

//     dv.normalize(-(l1 + l2));

//     pos += dv;
// }


void
MPObject::moveToCollisionPos()
{
    if ( M_collision_count > 0 )
    {
        /*        cout << "oldpos = " << obj->pos << std::endl; */
        /*        cout << "colpos = " << obj->post_col_pos << std::endl; */
        /*        cout << "colcount = " << obj->col_count << std::endl; */
        M_post_collision_pos /= M_collision_count;
        M_pos = M_post_collision_pos;
        /*        cout << "newpos = " << obj->pos << std::endl; */
    }

    M_post_collision_pos.assign( 0.0, 0.0 );
    M_collision_count = 0;
}


Ball::Ball( Stadium & stadium )
    : MPObject( stadium,
                BALL_NAME, BALL_NAME_SHORT,
                O_TYPE_BALL_NAME, O_TYPE_BALL_NAME_SHORT ),
      M_pos_z( 0.0 ),
      M_vel_z( 0.0 ),
      M_accel_z( 0.0 )
{

}


bool
Ball::aboveGoalHeight() const
{
    // Always false in 2d_mode, since M_pos_z never leaves 0.0 there --
    // this override is provably inert unless a 3D-mode kick has actually
    // lofted the ball above the goal frame's solid height.
    return M_pos_z > ServerParam::instance().goalHeight();
}


double
Ball::bounceSettleThreshold() const
{
    const ServerParam & SP = ServerParam::instance();

    if ( SP.ballBounceRestitution() >= 1.0 || SP.gravity() <= 0.0 )
    {
        return SP.bounceStopSpeed();
    }

    // analytic fixed point of the bounce recurrence -- a bounce candidate at
    // or below this speed would only ever re-bounce at the same (or a
    // smaller) speed forever, so treat it as settled instead of chasing an
    // asymptote that never quite reaches bounce_stop_speed for some
    // gravity/restitution combinations. Ported from
    // 3d-kick-lab/physics.js's _bounceSettleThreshold().
    const double vz_star = ( SP.ballBounceRestitution() * SP.gravity() )
        / ( 1.0 + SP.ballBounceRestitution() );

    return std::max( SP.bounceStopSpeed(), vz_star * 1.0001 );
}

void
Ball::applyBounceEnergyLoss()
{
    const ServerParam & SP = ServerParam::instance();

    // Uniform "whole-speed" energy loss: scale the horizontal components by
    // the SAME coefficient already used (by the caller) to reflect vel_z at
    // this bounce, so a single number represents the fraction of the ball's
    // total kinetic energy retained across ALL three axes. Ported from
    // 3d-kick-lab/physics.js's _applyBounceEnergyLoss() -- replaces the
    // former two-parameter model (ball_bounce_restitution for vel_z only,
    // plus a separate ball_bounce_friction Coulomb-style coupling for
    // vel.x/vel.y, now merged into this one coefficient).
    M_vel.x *= SP.ballBounceRestitution();
    M_vel.y *= SP.ballBounceRestitution();
}


void
Ball::checkPostAndCrossbar3D()
{
    const ServerParam & SP = ServerParam::instance();

    // Reuse the existing 2D post-finder purely to obtain the nearest post's
    // (x,y)/radius; this is a side-effect-free free-function call (CArea
    // returned by value) and does not touch MPObject::_inc()'s shared
    // collision-loop state.
    const CArea post = nearestPost( pos(), M_size );

    const double half_goal_width = SP.goalWidth() * 0.5;
    const bool within_goal_mouth_y = std::fabs( pos().y ) <= half_goal_width + post.radius();
    const bool at_goal_line_x = std::fabs( pos().x ) >= ServerParam::PITCH_LENGTH * 0.5 - post.radius();

    if ( ! within_goal_mouth_y || ! at_goal_line_x )
    {
        // ball is nowhere near either goal's (x,y) mouth -- nothing to do
        return;
    }

    // Crossbar: reflect vel_z (restitution-style, same as a ground bounce)
    // if the ball is at/above goal height and still rising while inside the
    // goal-mouth (x,y) span, instead of letting it fly through unimpeded.
    if ( M_pos_z >= SP.goalHeight() && M_vel_z > 0.0 )
    {
        const double vz_impact = M_vel_z;
        const double candidate = -vz_impact * SP.ballBounceRestitution();
        M_vel_z = candidate;
        applyBounceEnergyLoss();
        M_pos_z = SP.goalHeight();
    }

    // Post-at-height: below goal height and inside the post's (x,y) circle
    // -- treat like a vertical wall and reflect vel_z the same
    // restitution-style way the (unmodified) 2D post bounce in
    // MPObject::_inc() reflects vel.x/vel.y.
    if ( M_pos_z <= SP.goalHeight()
         && ( pos() - post.center() ).r() < post.radius()
         && M_vel_z != 0.0 )
    {
        const double vz_impact = M_vel_z;
        const double candidate = -vz_impact * SP.ballBounceRestitution();
        M_vel_z = candidate;
        applyBounceEnergyLoss();
    }
}


void
Ball::incZ()
{
    const ServerParam & SP = ServerParam::instance();

    const bool held = ( M_stadium.ballCatcher() != static_cast< const Player * >( 0 ) );

    // Consume any loft-kick vertical impulse pushed this cycle via
    // Player::kickImpl()/Stadium::kickTaken() -- mirrors how MPObject::_inc()
    // consumes M_accel into M_vel, just for the z axis. This MUST happen
    // BEFORE the "resting" check below: a ball sitting at pos_z==0/vel_z==0
    // that gets kicked airborne this very cycle must be recognized as
    // no-longer-resting immediately, using the POST-impulse velocity --
    // otherwise (bug fixed here) a freshly-kicked resting ball falls through
    // to the ground-bounce catch-all further down, which mistakes the brand
    // new upward impulse for a zero-height "landing impact" and reflects it
    // via -vel*ballBounceRestitution(), silently destroying most of the
    // kick's vertical velocity and delaying/flattening the resulting arc.
    M_vel_z += M_accel_z;
    M_accel_z = 0.0;

    // A ball resting flat on the ground must not have gravity re-applied to
    // it (gravity alone is typically bigger than bounce_stop_speed, so a
    // resting ball would "fall" every cycle, bounce back up, and never
    // settle -- see 3d-kick-lab/physics.js step()'s "resting" guard). A ball
    // currently held by a goalie must not silently accumulate vel_z either,
    // or releasing it would cause an unphysical velocity spike. Computed
    // AFTER the impulse consumption above so a same-cycle kick is honored.
    const bool resting = ( M_pos_z <= 0.0 && M_vel_z == 0.0 );

    if ( held )
    {
        return;
    }

    if ( ! resting )
    {
        M_vel_z -= SP.gravity();
    }

    const double z0 = M_pos_z;
    const double new_z = z0 + M_vel_z;
    bool bounced = false;

    if ( ! resting )
    {
        if ( SP.preciseBounceTiming() && z0 > 0.0 && new_z <= 0.0 )
        {
            // Exact ground-crossing fraction within this cycle (linear
            // interpolation -- vel_z is constant across one cycle), then
            // continue moving for the remaining fraction with the reflected
            // velocity, instead of clamping straight to z=0 and discarding
            // the rest of the cycle's fall. Ported from
            // 3d-kick-lab/physics.js's precise_bounce_timing branch.
            const double frac = z0 / ( z0 - new_z );
            const double vz_impact = M_vel_z;
            const double candidate = -vz_impact * SP.ballBounceRestitution();

            if ( std::fabs( candidate ) < bounceSettleThreshold() )
            {
                M_vel_z = 0.0;
                applyBounceEnergyLoss();
                M_pos_z = 0.0;
            }
            else
            {
                const double remaining = 1.0 - frac;
                M_vel_z = candidate;
                applyBounceEnergyLoss();
                M_pos_z = std::max( 0.0,
                                    candidate * remaining
                                    - 0.5 * SP.gravity() * remaining * remaining );
            }
            bounced = true;
        }
        else
        {
            M_pos_z = new_z;
        }
    }

    // Legacy/clamp-to-zero bounce path (used when precise_bounce_timing is
    // false, or as a catch-all for any case the precise branch above didn't
    // already resolve this cycle). BUG FIX: this block MUST also be gated on
    // `! resting`, in addition to `! bounced` -- without it, this fired on
    // EVERY cycle a resting/rolling ball spent at pos_z<=0 (not just on a
    // genuine new impact), because a resting ball never sets `bounced=true`
    // (only the airborne precise-timing branch above does, itself skipped
    // while resting). That meant applyBounceEnergyLoss() -- unconditionally
    // scaling vel.x/vel.y by ball_bounce_restitution -- was being applied
    // every single cycle to a plain rolling grounder, on top of the normal
    // ball_decay friction below, killing its speed almost instantly. Ported
    // from 3d-kick-lab/physics.js's step() (same fix, same root cause).
    if ( ! resting && ! bounced && M_pos_z <= 0.0 )
    {
        M_pos_z = 0.0;

        // Check the PREDICTED post-bounce velocity against
        // bounceSettleThreshold(), not the incoming fall velocity -- the
        // incoming velocity converges to a stable, non-decaying value
        // whenever gravity > bounce_stop_speed and never actually settles.
        const double vz_impact = M_vel_z;
        const double candidate = -vz_impact * SP.ballBounceRestitution();

        if ( std::fabs( candidate ) < bounceSettleThreshold() )
        {
            M_vel_z = 0.0;
            applyBounceEnergyLoss();
        }
        else
        {
            M_vel_z = candidate;
            applyBounceEnergyLoss();
        }
    }

    checkPostAndCrossbar3D();

    // No friction at all while airborne (air_decay was removed entirely --
    // horizontal speed is fully conserved in flight, gravity governs z
    // only); ball_decay friction applies ONLY once the ball is on the
    // ground. MPObject::_inc() (run earlier this same cycle, shared 2D
    // code, unmodified) already applied one full M_decay reduction to
    // vel.x/vel.y unconditionally, regardless of z -- on the ground that is
    // exactly the desired ball_decay friction, so nothing more is needed
    // here. While airborne, undo that already-applied M_decay by dividing
    // it back out, since a 3D-mode ball must not lose any horizontal speed
    // in flight. Ported from 3d-kick-lab/physics.js's step(), which has no
    // airborne friction term at all.
    if ( M_pos_z > 1.0e-6 && M_decay > 1.0e-9 )
    {
        M_vel.x /= M_decay;
        M_vel.y /= M_decay;
    }

    // Ground roll-stop: once a resting ball's horizontal speed decays below
    // roll_stop_speed, freeze it fully instead of simulating an
    // effectively-motionless ball forever.
    if ( M_pos_z <= 0.0 && M_vel_z == 0.0 )
    {
        const double speed_xy = M_vel.r();
        if ( speed_xy > 0.0 && speed_xy < SP.rollStopSpeed() )
        {
            M_vel.assign( 0.0, 0.0 );
        }
    }
}

