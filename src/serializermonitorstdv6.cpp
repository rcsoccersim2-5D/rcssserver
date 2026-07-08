// -*-c++-*-

/***************************************************************************
                            serializermonitorstdv6.cpp
                  Class for serializing data to std v6 monitors
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

#include "serializermonitorstdv6.h"

#include "param.h"
#include "utility.h"
#include "object.h"

namespace rcss {

SerializerMonitorStdv6::SerializerMonitorStdv6( const SerializerCommon::Ptr common )
    : SerializerMonitorStdv5( common )
{

}

SerializerMonitorStdv6::~SerializerMonitorStdv6()
{

}

const
SerializerMonitor::Ptr
SerializerMonitorStdv6::create()
{
    SerializerCommon::Creator cre_common;
    if ( ! SerializerCommon::factory().getCreator( cre_common, 18 ) )
    {
        return SerializerMonitor::Ptr();
    }

    SerializerMonitor::Ptr ptr( new SerializerMonitorStdv6( cre_common() ) );
    return ptr;
}

void
SerializerMonitorStdv6::serializeBall( std::ostream & os,
                                       const Ball & ball ) const
{
    os << " (" << BALL_NAME_SHORT
       << ' ' << Quantize( ball.pos().x, PREC )
       << ' ' << Quantize( ball.pos().y, PREC )
       << ' ' << Quantize( ball.vel().x, PREC )
       << ' ' << Quantize( ball.vel().y, PREC )
       << ' ' << Quantize( ball.posZ(), PREC )
       << ')';
}

namespace {
// 3D ball extension plan, Step 7 ("Protocol/Version Plumbing"): registered
// as monitor version 6 -- confirmed non-colliding by re-grepping
// serializermonitor.cpp, whose highest registration is v5
// (SerializerMonitorStdv5::create at version 5).
RegHolder v6 = SerializerMonitor::factory().autoReg( &SerializerMonitorStdv6::create, 6 );
}

}
