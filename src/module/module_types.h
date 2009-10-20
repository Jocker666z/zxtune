/*
Abstract:
  Modules types definitions

Last changed:
  $Id$

Author:
  (C) Vitamin/CAIG/2001
*/

#ifndef __MODULE_TYPES_H_DEFINED__
#define __MODULE_TYPES_H_DEFINED__

#include <types.h>

namespace ZXTune
{
  namespace Module
  {
    /// Track position descriptor
    struct Tracking
    {
      Tracking() : Position(), Pattern(), Line(), Frame(), Tempo(), Channels()
      {
      }
      /// Position in order list or total positions count
      unsigned Position;
      /// Current pattern or total patterns count
      unsigned Pattern;
      /// Current line
      unsigned Line;
      /// Current frame
      unsigned Frame;
      /// Current tempo or initial tempo
      unsigned Tempo;
      /// Currently active channels or total channels count (logical)
      unsigned Channels;
    };

    /// Common module information
    struct Information
    {
      Information() : Loop(), PhysicalChannels(), Capabilities()
      {
      }
      /// Tracking statistic (values are used in second meaning)
      Tracking Statistic;
      /// Loop position index
      unsigned Loop;
      /// Actual channels for playback
      unsigned PhysicalChannels;
      /// Different parameters
      ParametersMap Properties;
      /// Special capabilities
      uint32_t Capabilities;
    };
  }
}

#endif //__MODULE_TYPES_H_DEFINED__
