// -*-c++-*-

/***************************************************************************
                            serializercoachstdv20.cpp
               Class for serializing data to std v20 offline coaches
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

#include "serializercoachstdv20.h"

#include "object.h"

namespace rcss {

SerializerCoachStdv20::SerializerCoachStdv20( const SerializerCommon::Ptr common )
    : SerializerCoachStdv14( common )
{

}

SerializerCoachStdv20::~SerializerCoachStdv20()
{

}

void
SerializerCoachStdv20::serializeVisualObject( std::ostream & strm,
                                              const std::string & name,
                                              const PVector & pos,
                                              const PVector & vel,
                                              const double & z ) const
{
    strm << " (" << name
         << ' ' << pos.x << ' ' << pos.y
         << ' ' << vel.x << ' ' << vel.y
         << ' ' << z
         << ')';
}

const
SerializerCoach::Ptr
SerializerCoachStdv20::create()
{
    // NOTE (Step 6 scaffolding, mirrors Step 5's SerializerPlayerStdv20):
    // reuses the v14 SerializerCommon for now. Step 7 will confirm/introduce
    // whatever SerializerCommon version is actually appropriate for the
    // finalized protocol version number, and will add the autoReg()
    // registration that makes this reachable via SerializerCoach::factory().
    SerializerCommon::Creator cre;
    if ( ! SerializerCommon::factory().getCreator( cre, 14 ) )
    {
        return SerializerCoach::Ptr();
    }

    SerializerCoach::Ptr ptr( new SerializerCoachStdv20( cre() ) );
    return ptr;
}

namespace {
// 3D ball extension plan, Step 7 ("Protocol/Version Plumbing"): registered
// as version 20 -- confirmed non-colliding by re-grepping serializercoachstdv14.cpp,
// whose highest registration is v19 (SerializerCoachStdv14::create at version 19).
RegHolder v20 = SerializerCoach::factory().autoReg( &SerializerCoachStdv20::create, 20 );
}

}
