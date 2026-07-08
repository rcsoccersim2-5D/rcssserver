// -*-c++-*-

/***************************************************************************
                            serializercoachstdv20.h
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

// NOTE (3D ball extension plan, Step 6 of 9): this class is scaffolding
// only, mirroring SerializerPlayerStdv20 (serializerplayerstdv20.h/.cpp,
// Step 5). It overrides ONLY the ball-z-carrying serializeVisualObject()
// overload (SerializerCoach, serializer.h) so the coach look/see channel
// can expose raw ball height for new-protocol connections. It is
// deliberately NOT wired into SerializerCoach::factory() (no autoReg call
// in the .cpp) and NOT added to Makefile.am/CMakeLists.txt yet -- picking
// the real next-free protocol version number, formal autoReg
// registration, and build-system wiring are explicitly Step 7 job
// ("Protocol/Version Plumbing"). Do not register/build this class until
// Step 7 lands.

#ifndef SERIALIZERCOACHSTDV20_H
#define SERIALIZERCOACHSTDV20_H

#include "serializercoachstdv14.h"

namespace rcss {

class SerializerCoachStdv20
    : public SerializerCoachStdv14 {
protected:
    SerializerCoachStdv20( const SerializerCommon::Ptr common );

public:
    virtual
    ~SerializerCoachStdv20() override;

    static
    const
    SerializerCoach::Ptr create();

    virtual
    void serializeVisualObject( std::ostream & strm,
                                const std::string & name,
                                const PVector & pos,
                                const PVector & vel,
                                const double & z ) const override;

};

}

#endif
