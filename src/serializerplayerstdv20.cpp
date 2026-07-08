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
                                               const double & dist,
                                               const int dir,
                                               const double & elevation ) const
{
    strm << " (" << name << ' ' << dist << ' ' << dir
         << ' ' << elevation
         << ')';
}

void
SerializerPlayerStdv20::serializeVisualObject( std::ostream & strm,
                                               const std::string & name,
                                               const double & dist,
                                               const int dir,
                                               const double & dist_chg,
                                               const double & dir_chg,
                                               const double & elevation ) const
{
    strm << " (" << name << ' ' << dist << ' ' << dir
         << ' ' << dist_chg << ' ' << dir_chg
         << ' ' << elevation
         << ')';
}


const
SerializerPlayer::Ptr
SerializerPlayerStdv20::create()
{
    // NOTE (Step 5 scaffolding): reuses the v18 SerializerCommon for now.
    // Step 7 will confirm/introduce whatever SerializerCommon version is
    // actually appropriate for the finalized protocol version number, and
    // will add the autoReg() registration that makes this reachable via
    // SerializerPlayer::factory().
    SerializerCommon::Creator cre;
    if ( ! SerializerCommon::factory().getCreator( cre, 18 ) )
    {
        return SerializerPlayer::Ptr();
    }

    SerializerPlayer::Ptr ptr( new SerializerPlayerStdv20( cre() ) );
    return ptr;
}

// Intentionally NOT registered via SerializerPlayer::factory().autoReg()
// here -- protocol version number assignment + factory registration is
// Step 7's responsibility ("Protocol/Version Plumbing"). This file is
// also not yet added to Makefile.am/CMakeLists.txt for the same reason.

}
