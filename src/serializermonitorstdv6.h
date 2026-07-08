// -*-c++-*-

/***************************************************************************
                            serializermonitorstdv6.h
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

// NOTE (3D ball extension plan, Step 7 of 9): adds the ball's z field to
// the monitor protocol's serializeBall(). SerializerMonitorStdv5 (the
// prior latest, version 5) already provides a complete legacy
// serializeBall() implementation (inherited unmodified from
// SerializerMonitorStdv3, see serializermonitor.cpp), so this class only
// needs to override serializeBall() to append z -- no base-class default
// is empty/no-op here, so pre-v6 monitor connections (versions 1-5,
// unaffected/untouched) keep their exact existing byte layout.

#ifndef SERIALIZERMONITORSTDV6_H
#define SERIALIZERMONITORSTDV6_H

#include "serializermonitor.h"

namespace rcss {

/*!
  \class SerializerMonitorStdv6
  \brief class of the version 6 serialization for monitors. Adds the
  ball's z (height) field for the 3D ball extension.
*/
class SerializerMonitorStdv6
    : public SerializerMonitorStdv5 {
protected:

    explicit
    SerializerMonitorStdv6( const SerializerCommon::Ptr common );

public:

    virtual
    ~SerializerMonitorStdv6() override;

    static
    const
    Ptr create();

    virtual
    void serializeBall( std::ostream & os,
                        const Ball & ball ) const override;
};

}

#endif
