/**
* 
* @file
*
* @brief  VortexTracker-based chiptunes support
*
* @author vitamin.caig@gmail.com
*
**/

#pragma once

//local includes
#include "aym_base_track.h"
//library includes
#include <formats/chiptune/aym/protracker3.h>
#include <module/players/simple_ornament.h>
#include <sound/render_params.h>

namespace Module
{
  //at least two formats are based on Vortex, so it's useful to extract tracking-related types
  namespace Vortex
  {
    // Frequency table enumeration, compatible with binary format (PT3.x)
    enum NoteTable
    {
      PROTRACKER,
      SOUNDTRACKER,
      ASM,
      REAL,
      NATURAL
    };

    String GetFreqTable(NoteTable table, uint_t version);

    //sample type
    struct Sample : public Formats::Chiptune::ProTracker3::Sample
    {
      Sample() : Formats::Chiptune::ProTracker3::Sample()
      {
      }

      Sample(Formats::Chiptune::ProTracker3::Sample rh)
        : Formats::Chiptune::ProTracker3::Sample(std::move(rh))
      {
      }

      uint_t GetLoop() const
      {
        return Loop;
      }

      uint_t GetSize() const
      {
        return static_cast<uint_t>(Lines.size());
      }

      const Line& GetLine(const uint_t idx) const
      {
        static const Line STUB;
        return Lines.size() > idx ? Lines[idx] : STUB;
      }
    };

    //supported commands set and their parameters
    enum Commands
    {
      //no parameters
      EMPTY,
      //period,delta
      GLISS,
      //period,delta,target note
      GLISS_NOTE,
      //offset
      SAMPLEOFFSET,
      //offset
      ORNAMENTOFFSET,
      //ontime,offtime
      VIBRATE,
      //period,delta
      SLIDEENV,
      //no parameters
      NOENVELOPE,
      //r13,period
      ENVELOPE,
      //base
      NOISEBASE,
    };

    typedef SimpleOrnament Ornament;

    class ModuleData : public TrackModel
    {
    public:
      typedef std::shared_ptr<ModuleData> RWPtr;
      typedef std::shared_ptr<const ModuleData> Ptr;

      ModuleData()
        : InitialTempo()
        , Version(6)
      {
      }

      uint_t GetInitialTempo() const override
      {
        return InitialTempo;
      }

      const OrderList& GetOrder() const override
      {
        return *Order;
      }

      const PatternsSet& GetPatterns() const override
      {
        return *Patterns;
      }

      uint_t InitialTempo;
      OrderList::Ptr Order;
      PatternsSet::Ptr Patterns;
      SparsedObjectsStorage<Sample> Samples;
      SparsedObjectsStorage<Ornament> Ornaments;
      uint_t Version;
    };

    AYM::DataRenderer::Ptr CreateDataRenderer(ModuleData::Ptr data, uint_t trackChannelStart);
  }
}
