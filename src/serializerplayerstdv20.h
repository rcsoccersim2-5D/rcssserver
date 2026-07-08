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

// NOTE (3D ball extension plan, Step 5 of 9): this class is scaffolding
// only. It overrides serializeFSBall3D() so FullStateSenderPlayerV20
// (fullstatesender.h/.cpp) has a concrete serializer to call, allowing
// Step 5's new code paths to compile standalone. It is deliberately NOT
// wired into the SerializerPlayer::factory() (no autoReg call in the
// .cpp) and NOT added to Makefile.am/CMakeLists.txt yet -- picking the
// real next-free protocol version number, formal autoReg registration,
// and build-system wiring are explicitly Step 7's job ("Protocol/Version
// Plumbing"). Do not register/build this class until Step 7 lands.

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

    // NOTE (3D ball extension plan, Step 6 of 9): overrides the two
    // elevation-carrying serializeVisualObject() overloads (serializer.h,
    // SerializerPlayer) to actually emit the trailing elevation field --
    // the base SerializerPlayer default (inherited by every pre-v20
    // subclass) drops it and reproduces the legacy byte layout instead.
    virtual
    void serializeVisualObject( std::ostream & strm,
                                const std::string & name,
                                const double & dist,
                                const int dir,
                                const double & elevation ) const override;

    virtual
    void serializeVisualObject( std::ostream & strm,
                                const std::string & name,
                                const double & dist,
                                const int dir,
                                const double & dist_chg,
                                const double & dir_chg,
                                const double & elevation ) const override;

};

}

#endif
