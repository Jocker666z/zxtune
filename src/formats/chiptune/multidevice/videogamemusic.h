/**
*
* @file
*
* @brief  VGM support interface
*
* @author vitamin.caig@gmail.com
*
**/

#pragma once

//local includes
#include "formats/chiptune/builder_meta.h"
//library includes
#include <formats/chiptune.h>
#include <time/duration.h>

namespace Formats
{
  namespace Chiptune
  {
    namespace VideoGameMusic
    {
      class Builder
      {
      public:
        virtual ~Builder() = default;

        virtual MetaBuilder& GetMetaBuilder() = 0;

        virtual void SetTimings(Time::Milliseconds total, Time::Milliseconds loop) = 0;
      };

      Builder& GetStubBuilder();

      Formats::Chiptune::Container::Ptr Parse(const Binary::Container& data, Builder& target);
    }

    Decoder::Ptr CreateVideoGameMusicDecoder();
  }
}
