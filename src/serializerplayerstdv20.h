// -*-c++-*-

/***************************************************************************
                            serializerplayerstdv20.h
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

// Player protocol v20 serializer for the 3D ball extension.  It emits raw
// ball z/vz in normal visual observations and the existing 3D fullstate
// ball layout.  Earlier serializers inherit legacy-preserving defaults.

#ifndef SERIALIZERPLAYERSTDV20_H
#define SERIALIZERPLAYERSTDV20_H

#include "serializerplayerstdv18.h"

namespace rcss {

class SerializerPlayerStdv20
    : public SerializerPlayerStdv18 {
protected:
    SerializerPlayerStdv20( const SerializerCommon::Ptr common );

public:
    virtual
    ~SerializerPlayerStdv20() override;

    static
    const
    SerializerPlayer::Ptr create();

    virtual
    void serializeFSBall3D( std::ostream & strm,
                            const double & x,
                            const double & y,
                            const double & z,
                            const double & vel_x,
                            const double & vel_y,
                            const double & vel_z ) const override;

    // These three ball-only overloads emit the v20 vertical fields.  The
    // SerializerPlayer defaults drop z/vz for protocols 1-19.
    virtual
    void serializeVisualObject( std::ostream & strm,
                                const std::string & name,
                                const int dir,
                                const double & z ) const override;

    virtual
    void serializeVisualObject( std::ostream & strm,
                                const std::string & name,
                                const double & dist,
                                const int dir,
                                const double & z ) const override;

    virtual
    void serializeVisualObject( std::ostream & strm,
                                const std::string & name,
                                const double & dist,
                                const int dir,
                                const double & dist_chg,
                                const double & dir_chg,
                                const double & z,
                                const double & vz ) const override;

};

}

#endif
