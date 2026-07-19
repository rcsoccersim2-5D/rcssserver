// -*-c++-*-

/***************************************************************************
                            serializerplayerstdv20.cpp
                  Class for serializing data to std v20 players
                             -------------------
    begin                : 2024
    copyright            : (C) 2024 by The RoboCup Soccer Server
                           Maintenance Group.
***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU LGPL as published by the Free Software  *
 *   Foundation; either version 3 of the License, or (at your option) any  *
 *   later version.                                                        *
 *                                                                         *
 ***************************************************************************/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "serializerplayerstdv20.h"

#include "player.h"

namespace rcss {

SerializerPlayerStdv20::SerializerPlayerStdv20( const SerializerCommon::Ptr common )
    : SerializerPlayerStdv18( common )
{

}

SerializerPlayerStdv20::~SerializerPlayerStdv20()
{

}


void
SerializerPlayerStdv20::serializeFSBall3D( std::ostream & strm,
                                           const double & x,
                                           const double & y,
                                           const double & z,
                                           const double & vel_x,
                                           const double & vel_y,
                                           const double & vel_z ) const
{
    strm << " ((b)"
         << ' ' << x
         << ' ' << y
         << ' ' << z
         << ' ' << vel_x
         << ' ' << vel_y
         << ' ' << vel_z
         << ')';
}

void
SerializerPlayerStdv20::serializeVisualObject( std::ostream & strm,
                                               const std::string & name,
                                               const int dir,
                                               const double & z ) const
{
    strm << " (" << name << ' ' << dir
         << ' ' << z
         << ')';
}

void
SerializerPlayerStdv20::serializeVisualObject( std::ostream & strm,
                                               const std::string & name,
                                               const double & dist,
                                               const int dir,
                                               const double & z ) const
{
    strm << " (" << name << ' ' << dist << ' ' << dir
         << ' ' << z
         << ')';
}

void
SerializerPlayerStdv20::serializeVisualObject( std::ostream & strm,
                                               const std::string & name,
                                               const double & dist,
                                               const int dir,
                                               const double & dist_chg,
                                               const double & dir_chg,
                                               const double & z,
                                               const double & vz ) const
{
    strm << " (" << name << ' ' << dist << ' ' << dir
         << ' ' << dist_chg << ' ' << dir_chg
         << ' ' << z << ' ' << vz
         << ')';
}


const
SerializerPlayer::Ptr
SerializerPlayerStdv20::create()
{
    // Protocol v20 does not change common parameter serialization, so it
    // reuses the latest SerializerCommon implementation (v18).
    SerializerCommon::Creator cre;
    if ( ! SerializerCommon::factory().getCreator( cre, 18 ) )
    {
        return SerializerPlayer::Ptr();
    }

    SerializerPlayer::Ptr ptr( new SerializerPlayerStdv20( cre() ) );
    return ptr;
}

namespace {
// Protocol v20 is the first player serializer that emits vertical ball
// state in normal visual observations and fullstate.
RegHolder v20 = SerializerPlayer::factory().autoReg( &SerializerPlayerStdv20::create, 20 );
}

}
