/*
Abstract:
  PT2 modules playback support

Last changed:
  $Id$

Author:
  (C) Vitamin/CAIG/2001
*/

//local includes
#include "ay_base.h"
#include "ay_conversion.h"
#include "core/plugins/utils.h"
#include "core/plugins/registrator.h"
#include "core/plugins/players/creation_result.h"
#include "core/plugins/players/module_properties.h"
//common includes
#include <byteorder.h>
#include <error_tools.h>
#include <logging.h>
#include <range_checker.h>
#include <tools.h>
//library includes
#include <core/convert_parameters.h>
#include <core/core_parameters.h>
#include <core/error_codes.h>
#include <core/module_attrs.h>
#include <core/plugin_attrs.h>
#include <io/container.h>
//text includes
#include <core/text/core.h>
#include <core/text/plugins.h>
#include <core/text/warnings.h>

#define FILE_TAG 077C8579

namespace
{
  using namespace ZXTune;
  using namespace ZXTune::Module;

  const uint_t LIMITER = ~uint_t(0);

  //hints
  const std::size_t MAX_MODULE_SIZE = 0x3600;
  const uint_t MAX_PATTERN_SIZE = 64;
  const uint_t MAX_PATTERN_COUNT = 64;//TODO

  //////////////////////////////////////////////////////////////////////////
#ifdef USE_PRAGMA_PACK
#pragma pack(push,1)
#endif
  PACK_PRE struct PT2Header
  {
    uint8_t Tempo;
    uint8_t Length;
    uint8_t Loop;
    boost::array<uint16_t, 32> SamplesOffsets;
    boost::array<uint16_t, 16> OrnamentsOffsets;
    uint16_t PatternsOffset;
    char Name[30];
    uint8_t Positions[1];
  } PACK_POST;

  const uint8_t POS_END_MARKER = 0xff;

  PACK_PRE struct PT2Sample
  {
    uint8_t Size;
    uint8_t Loop;

    uint_t GetSize() const
    {
      return sizeof(*this) + (Size - 1) * sizeof(Data[0]);
    }

    static uint_t GetMinimalSize()
    {
      return sizeof(PT2Sample) - sizeof(PT2Sample::Line);
    }

    PACK_PRE struct Line
    {
      //nnnnnsTN
      //aaaaHHHH
      //LLLLLLLL

      //HHHHLLLLLLLL - vibrato
      //s - vibrato sign
      //a - level
      //N - noise off
      //T - tone off
      //n - noise value
      uint8_t NoiseAndFlags;
      uint8_t LevelHiVibrato;
      uint8_t LoVibrato;

      bool GetNoiseMask() const
      {
        return 0 != (NoiseAndFlags & 1);
      }

      bool GetToneMask() const
      {
        return 0 != (NoiseAndFlags & 2);
      }

      uint_t GetNoise() const
      {
        return NoiseAndFlags >> 3;
      }

      uint_t GetLevel() const
      {
        return LevelHiVibrato >> 4;
      }

      int_t GetVibrato() const
      {
        const int_t val(((LevelHiVibrato & 0x0f) << 8) | LoVibrato);
        return (NoiseAndFlags & 4) ? val : -val;
      }
    } PACK_POST;
    Line Data[1];
  } PACK_POST;

  PACK_PRE struct PT2Ornament
  {
    uint8_t Size;
    uint8_t Loop;
    int8_t Data[1];

    uint_t GetSize() const
    {
      return sizeof(PT2Ornament) + (Size - 1);
    }

    static uint_t GetMinimalSize()
    {
      return sizeof(PT2Ornament) - sizeof(int8_t);
    }
  } PACK_POST;

  PACK_PRE struct PT2Pattern
  {
    boost::array<uint16_t, 3> Offsets;

    bool Check() const
    {
      return Offsets[0] && Offsets[1] && Offsets[2];
    }
  } PACK_POST;
#ifdef USE_PRAGMA_PACK
#pragma pack(pop)
#endif

  BOOST_STATIC_ASSERT(sizeof(PT2Header) == 132);
  BOOST_STATIC_ASSERT(sizeof(PT2Sample) == 5);
  BOOST_STATIC_ASSERT(sizeof(PT2Ornament) == 3);

  //sample type
  struct Sample
  {
    Sample() : Loop(), Lines()
    {
    }

    Sample(uint_t loop, const PT2Sample::Line* from, const PT2Sample::Line* to)
      : Loop(loop), Lines(from, to)
    {
    }

    struct Line
    {
      Line() : Level(), Noise(), ToneOff(), NoiseOff(), Vibrato()
      {
      }
      /*explicit*/Line(const PT2Sample::Line& src)
        : Level(src.GetLevel()), Noise(src.GetNoise())
        , ToneOff(src.GetToneMask()), NoiseOff(src.GetNoiseMask())
        , Vibrato(src.GetVibrato())
      {
      }
      uint_t Level;//0-15
      uint_t Noise;//0-31
      bool ToneOff;
      bool NoiseOff;
      int_t Vibrato;
    };

    uint_t GetLoop() const
    {
      return Loop;
    }

    uint_t GetSize() const
    {
      return static_cast<uint_t>(Lines.size());
    }

    const Line& GetLine(uint_t idx) const
    {
      static const Line STUB;
      return Lines.size() > idx ? Lines[idx] : STUB;
    }
  private:
    uint_t Loop;
    std::vector<Line> Lines;
  };

  inline Sample ParseSample(const IO::FastDump& data, uint16_t offset, std::size_t& rawSize)
  {
    const uint_t off = fromLE(offset);
    const PT2Sample* const sample = safe_ptr_cast<const PT2Sample*>(&data[off]);
    if (!off)
    {
      //emulate invalid data
      const PT2Sample::Line* const dataAsSample = safe_ptr_cast<const PT2Sample::Line*>(&data[off]);
      return Sample(0, dataAsSample, dataAsSample + 1);
    }
    if (!sample->Size)
    {
      rawSize = std::max<std::size_t>(rawSize, off + sample->GetMinimalSize());
      return Sample();
    }
    Sample tmp(sample->Loop, sample->Data, sample->Data + sample->Size);
    rawSize = std::max<std::size_t>(rawSize, off + sample->GetSize());
    return tmp;
  }

  inline SimpleOrnament ParseOrnament(const IO::FastDump& data, uint16_t offset, std::size_t& rawSize)
  {
    const uint_t off = fromLE(offset);
    const PT2Ornament* const ornament = safe_ptr_cast<const PT2Ornament*>(&data[off]);
    if (!off)
    {
      //emulate invalid data
      const int8_t* const dataAsOrnament = safe_ptr_cast<const int8_t*>(&data[off]);
      return SimpleOrnament(0, dataAsOrnament, dataAsOrnament + 1);
    }
    if (!ornament->Size)
    {
      rawSize = std::max<std::size_t>(rawSize, off + ornament->GetMinimalSize());
      return SimpleOrnament();
    }
    rawSize = std::max<std::size_t>(rawSize, off + ornament->GetSize());
    return SimpleOrnament(ornament->Loop, ornament->Data, ornament->Data + ornament->Size);
  }

  //supported commands
  enum CmdType
  {
    //no parameters
    EMPTY,
    //r13,period
    ENVELOPE,
    //no parameters
    NOENVELOPE,
    //glissade
    GLISS,
    //glissade,target node
    GLISS_NOTE,
    //no parameters
    NOGLISS,
    //noise addon
    NOISE_ADD
  };

  typedef TrackingSupport<Devices::AYM::CHANNELS, CmdType, Sample> PT2Track;

  class PT2ModuleData : public PT2Track::ModuleData
  {
  public:
    typedef boost::shared_ptr<PT2ModuleData> Ptr;

    PT2ModuleData()
      : PT2Track::ModuleData()
    {
    }

    void ParseInformation(const IO::FastDump& data, ModuleProperties& properties)
    {
      const PT2Header* const header = safe_ptr_cast<const PT2Header*>(&data[0]);

      LoopPosition = header->Loop;
      InitialTempo = header->Tempo;
      properties.SetTitle(OptimizeString(FromCharArray(header->Name)));
      properties.SetProgram(Text::PT2_EDITOR);
      properties.SetFreqtable(TABLE_PROTRACKER2);
    }

    std::size_t ParseSamples(const IO::FastDump& data)
    {
      const PT2Header* const header = safe_ptr_cast<const PT2Header*>(&data[0]);
      
      std::size_t res = sizeof(*header);
      Samples.reserve(header->SamplesOffsets.size());
      std::transform(header->SamplesOffsets.begin(), header->SamplesOffsets.end(),
        std::back_inserter(Samples), boost::bind(&ParseSample, boost::cref(data), _1, boost::ref(res)));
      return res;
    }

    std::size_t ParseOrnaments(const IO::FastDump& data)
    {
      const PT2Header* const header = safe_ptr_cast<const PT2Header*>(&data[0]);
      
      std::size_t res = sizeof(*header);
      Ornaments.reserve(header->OrnamentsOffsets.size());
      std::transform(header->OrnamentsOffsets.begin(), header->OrnamentsOffsets.end(),
        std::back_inserter(Ornaments), boost::bind(&ParseOrnament, boost::cref(data), _1, boost::ref(res)));
      return res;
    }

    std::size_t ParsePatterns(const IO::FastDump& data)
    {
      const PT2Header* const header = safe_ptr_cast<const PT2Header*>(&data[0]);
      
      std::size_t res = sizeof(*header);
      //fill patterns
      const PT2Pattern* patterns = safe_ptr_cast<const PT2Pattern*>(&data[fromLE(header->PatternsOffset)]);
      Patterns.resize(MAX_PATTERN_COUNT);
      uint_t index(0);
      for (const PT2Pattern* pattern = patterns; pattern->Check(); ++pattern, ++index)
      {
        PT2Track::Pattern& pat = Patterns[index];

        AYM::PatternCursors cursors;
        std::transform(pattern->Offsets.begin(), pattern->Offsets.end(), cursors.begin(), &fromLE<uint16_t>);
        uint_t& channelACursor = cursors.front().Offset;
        do
        {
          PT2Track::Line& line = pat.AddLine();
          ParsePattern(data, cursors, line);
          //skip lines
          if (const uint_t linesToSkip = cursors.GetMinCounter())
          {
            cursors.SkipLines(linesToSkip);
            pat.AddLines(linesToSkip);//add dummies
          }
        }
        while (channelACursor < data.Size() &&
          (0 != data[channelACursor] || 0 != cursors.front().Counter));
        res = std::max<std::size_t>(res, 1 + cursors.GetMaxOffset());
      }

      //fill order
      for (const uint8_t* curPos = header->Positions; POS_END_MARKER != *curPos; ++curPos)
      {
        if (!Patterns[*curPos].IsEmpty())
        {
          Positions.push_back(*curPos);
        }
      }
      return res;
    }
  private:
    static void ParsePattern(const IO::FastDump& data
      , AYM::PatternCursors& cursors
      , PT2Track::Line& line
      )
    {
      assert(line.Channels.size() == cursors.size());
      PT2Track::Line::ChannelsArray::iterator channel(line.Channels.begin());
      for (AYM::PatternCursors::iterator cur = cursors.begin(); cur != cursors.end(); ++cur, ++channel)
      {
        if (cur->Counter--)
        {
          continue;//has to skip
        }

        for (const std::size_t dataSize = data.Size(); cur->Offset < dataSize;)
        {
          const uint_t cmd(data[cur->Offset++]);
          const std::size_t restbytes = dataSize - cur->Offset;
          if (cmd >= 0xe1) //sample
          {
            const uint_t num = cmd - 0xe0;
            channel->SetSample(num);
          }
          else if (cmd == 0xe0) //sample 0 - shut up
          {
            channel->SetEnabled(false);
            break;
          }
          else if (cmd >= 0x80 && cmd <= 0xdf)//note
          {
            channel->SetEnabled(true);
            const uint_t note(cmd - 0x80);
            //for note gliss calculate limit manually
            const PT2Track::CommandsArray::iterator noteGlissCmd(
              std::find(channel->Commands.begin(), channel->Commands.end(), GLISS_NOTE));
            if (channel->Commands.end() != noteGlissCmd)
            {
              noteGlissCmd->Param2 = int_t(note);
            }
            else
            {
              channel->SetNote(note);
            }
            break;
          }
          else if (cmd == 0x7f) //env off
          {
            channel->Commands.push_back(NOENVELOPE);
          }
          else if (cmd >= 0x71 && cmd <= 0x7e) //envelope
          {
            //required 2 bytes
            if (restbytes < 2)
            {
              throw Error(THIS_LINE, ERROR_INVALID_FORMAT);//no details
            }
            channel->Commands.push_back(
              PT2Track::Command(ENVELOPE, cmd - 0x70, data[cur->Offset] | (uint_t(data[cur->Offset + 1]) << 8)));
            cur->Offset += 2;
          }
          else if (cmd == 0x70)//quit
          {
            break;
          }
          else if (cmd >= 0x60 && cmd <= 0x6f)//ornament
          {
            const uint_t num = cmd - 0x60;
            channel->SetOrnament(num);
          }
          else if (cmd >= 0x20 && cmd <= 0x5f)//skip
          {
            cur->Period = cmd - 0x20;
          }
          else if (cmd >= 0x10 && cmd <= 0x1f)//volume
          {
            channel->SetVolume(cmd - 0x10);
          }
          else if (cmd == 0x0f)//new delay
          {
            //required 1 byte
            if (restbytes < 1)
            {
              throw Error(THIS_LINE, ERROR_INVALID_FORMAT);//no details
            }
            line.SetTempo(data[cur->Offset++]);
          }
          else if (cmd == 0x0e)//gliss
          {
            //required 1 byte
            if (restbytes < 1)
            {
              throw Error(THIS_LINE, ERROR_INVALID_FORMAT);//no details
            }
            channel->Commands.push_back(PT2Track::Command(GLISS, static_cast<int8_t>(data[cur->Offset++])));
          }
          else if (cmd == 0x0d)//note gliss
          {
            //required 3 bytes
            if (restbytes < 3)
            {
              throw Error(THIS_LINE, ERROR_INVALID_FORMAT);//no details
            }
            channel->Commands.push_back(PT2Track::Command(GLISS_NOTE, static_cast<int8_t>(data[cur->Offset])));
            //ignore delta due to error
            cur->Offset += 3;
          }
          else if (cmd == 0x0c) //gliss off
          {
            //TODO: warn
            channel->Commands.push_back(NOGLISS);
          }
          else //noise add
          {
            //required 1 byte
            if (restbytes < 1)
            {
              throw Error(THIS_LINE, ERROR_INVALID_FORMAT);//no details
            }
            channel->Commands.push_back(PT2Track::Command(NOISE_ADD, static_cast<int8_t>(data[cur->Offset++])));
          }
        }
        cur->Counter = cur->Period;
      }
    }
  };

  inline uint_t GetVolume(uint_t volume, uint_t level)
  {
    return (volume * 17 + (volume > 7 ? 1 : 0)) * level / 256;
  }

  struct PT2ChannelState
  {
    PT2ChannelState()
      : Enabled(false), Envelope(false)
      , Note(), SampleNum(0), PosInSample(0)
      , OrnamentNum(0), PosInOrnament(0)
      , Volume(15), NoiseAdd(0)
      , Sliding(0), SlidingTargetNote(LIMITER), Glissade(0)
    {
    }
    bool Enabled;
    bool Envelope;
    uint_t Note;
    uint_t SampleNum;
    uint_t PosInSample;
    uint_t OrnamentNum;
    uint_t PosInOrnament;
    uint_t Volume;
    int_t NoiseAdd;
    int_t Sliding;
    uint_t SlidingTargetNote;
    int_t Glissade;
  };

  class PT2DataRenderer : public AYM::DataRenderer
  {
  public:
    explicit PT2DataRenderer(PT2Track::ModuleData::Ptr data)
       : Data(data)
    {
    }

    virtual void Reset()
    {
      std::fill(PlayerState.begin(), PlayerState.end(), PT2ChannelState());
    }

    virtual void SynthesizeData(const TrackState& state, AYM::TrackBuilder& track)
    {
      if (0 == state.Quirk())
      {
        GetNewLineState(state, track);
      }
      SynthesizeChannelsData(track);
    }
  private:
    void GetNewLineState(const TrackState& state, AYM::TrackBuilder& track)
    {
      if (const PT2Track::Line* line = Data->Patterns[state.Pattern()].GetLine(state.Line()))
      {
        for (uint_t chan = 0; chan != line->Channels.size(); ++chan)
        {
          const PT2Track::Line::Chan& src = line->Channels[chan];
          if (src.Empty())
          {
            continue;
          }
          GetNewChannelState(src, PlayerState[chan], track);
        }
      }
    }

    void GetNewChannelState(const PT2Track::Line::Chan& src, PT2ChannelState& dst, AYM::TrackBuilder& track)
    {
      if (src.Enabled)
      {
        if (!(dst.Enabled = *src.Enabled))
        {
          dst.Sliding = dst.Glissade = 0;
          dst.SlidingTargetNote = LIMITER;
        }
        dst.PosInSample = dst.PosInOrnament = 0;
      }
      if (src.Note)
      {
        assert(src.Enabled);
        dst.Note = *src.Note;
        dst.Sliding = dst.Glissade = 0;
        dst.SlidingTargetNote = LIMITER;
      }
      if (src.SampleNum)
      {
        dst.SampleNum = *src.SampleNum;
        dst.PosInSample = 0;
      }
      if (src.OrnamentNum)
      {
        dst.OrnamentNum = *src.OrnamentNum;
        dst.PosInOrnament = 0;
      }
      if (src.Volume)
      {
        dst.Volume = *src.Volume;
      }
      for (PT2Track::CommandsArray::const_iterator it = src.Commands.begin(), lim = src.Commands.end(); it != lim; ++it)
      {
        switch (it->Type)
        {
        case ENVELOPE:
          track.SetEnvelopeType(it->Param1);
          track.SetEnvelopeTone(it->Param2);
          dst.Envelope = true;
          break;
        case NOENVELOPE:
          dst.Envelope = false;
          break;
        case NOISE_ADD:
          dst.NoiseAdd = it->Param1;
          break;
        case GLISS_NOTE:
          dst.Sliding = 0;
          dst.Glissade = it->Param1;
          dst.SlidingTargetNote = it->Param2;
          break;
        case GLISS:
          dst.Glissade = it->Param1;
          dst.SlidingTargetNote = LIMITER;
          break;
        case NOGLISS:
          dst.Glissade = 0;
          break;
        default:
          assert(!"Invalid command");
        }
      }
    }

    void SynthesizeChannelsData(AYM::TrackBuilder& track)
    {
      for (uint_t chan = 0; chan != PlayerState.size(); ++chan)
      {
        AYM::ChannelBuilder channel = track.GetChannel(chan);
        SynthesizeChannel(PlayerState[chan], channel, track);
      }
    }

    void SynthesizeChannel(PT2ChannelState& dst, AYM::ChannelBuilder& channel, AYM::TrackBuilder& track)
    {
      if (!dst.Enabled)
      {
        channel.SetLevel(0);
        return;
      }

      const Sample& curSample = Data->Samples[dst.SampleNum];
      const Sample::Line& curSampleLine = curSample.GetLine(dst.PosInSample);
      const SimpleOrnament& curOrnament = Data->Ornaments[dst.OrnamentNum];

      //apply tone
      const int_t halftones = int_t(dst.Note) + curOrnament.GetLine(dst.PosInOrnament);
      channel.SetTone(halftones, dst.Sliding + curSampleLine.Vibrato);
      if (curSampleLine.ToneOff)
      {
        channel.DisableTone();
      }
      //apply level
      channel.SetLevel(GetVolume(dst.Volume, curSampleLine.Level));
      //apply envelope
      if (dst.Envelope)
      {
        channel.EnableEnvelope();
      }
      //apply noise
      if (!curSampleLine.NoiseOff)
      {
        track.SetNoise(curSampleLine.Noise + dst.NoiseAdd);
      }
      else
      {
        channel.DisableNoise();
      }

      //recalculate gliss
      if (dst.SlidingTargetNote != LIMITER)
      {
        const int_t absoluteSlidingRange = track.GetSlidingDifference(dst.Note, dst.SlidingTargetNote);
        const int_t realSlidingRange = absoluteSlidingRange - (dst.Sliding + dst.Glissade);

        if ((dst.Glissade > 0 && realSlidingRange <= 0) ||
            (dst.Glissade < 0 && realSlidingRange >= 0))
        {
          dst.Note = dst.SlidingTargetNote;
          dst.SlidingTargetNote = LIMITER;
          dst.Sliding = dst.Glissade = 0;
        }
      }
      dst.Sliding += dst.Glissade;

      if (++dst.PosInSample >= curSample.GetSize())
      {
        dst.PosInSample = curSample.GetLoop();
      }
      if (++dst.PosInOrnament >= curOrnament.GetSize())
      {
        dst.PosInOrnament = curOrnament.GetLoop();
      }
    }
  private:
    const PT2Track::ModuleData::Ptr Data;
    boost::array<PT2ChannelState, Devices::AYM::CHANNELS> PlayerState;
  };

  class PT2Chiptune : public AYM::Chiptune
  {
  public:
    PT2Chiptune(PT2Track::ModuleData::Ptr data, ModuleProperties::Ptr properties)
      : Data(data)
      , Properties(properties)
      , Info(CreateTrackInfo(Data, Devices::AYM::CHANNELS))
    {
    }

    virtual Information::Ptr GetInformation() const
    {
      return Info;
    }

    virtual ModuleProperties::Ptr GetProperties() const
    {
      return Properties;
    }

    virtual AYM::DataIterator::Ptr CreateDataIterator(AYM::TrackParameters::Ptr trackParams) const
    {
      const StateIterator::Ptr iterator = CreateTrackStateIterator(Info, Data);
      const AYM::DataRenderer::Ptr renderer = boost::make_shared<PT2DataRenderer>(Data);
      return AYM::CreateDataIterator(trackParams, iterator, renderer);
    }
  private:
    const PT2Track::ModuleData::Ptr Data;
    const ModuleProperties::Ptr Properties;
    const Information::Ptr Info;
  };

  //////////////////////////////////////////////////
  bool CheckPT2Module(const uint8_t* data, std::size_t size)
  {
    if (sizeof(PT2Header) > size)
    {
      return false;
    }

    const PT2Header* const header(safe_ptr_cast<const PT2Header*>(data));
    if (header->Tempo < 2 || header->Loop >= header->Length ||
        header->Length < 1 || header->Length + 1 + sizeof(*header) > size)
    {
      return false;
    }
    const std::size_t lowlimit(1 +
      std::find(header->Positions, data + header->Length + sizeof(*header) + 1, POS_END_MARKER) - data);
    if (lowlimit - sizeof(*header) != header->Length)//too big positions list
    {
      return false;
    }

    //check patterns offset
    const uint_t patOff(fromLE(header->PatternsOffset));
    if (!in_range<std::size_t>(patOff, lowlimit, size))
    {
      return false;
    }

    RangeChecker::Ptr checker(RangeChecker::CreateShared(size));
    checker->AddRange(0, lowlimit);
    //check patterns
    {
      uint_t patternsCount(0);
      for (const PT2Pattern* patPos(safe_ptr_cast<const PT2Pattern*>(data + patOff));
        patPos->Check();
        ++patPos, ++patternsCount)
      {
        if (patternsCount > MAX_PATTERN_COUNT ||
            !checker->AddRange(patOff + sizeof(PT2Pattern) * patternsCount, sizeof(PT2Pattern)) ||
            //simple check if in range- performance issue
            patPos->Offsets.end() != std::find_if(patPos->Offsets.begin(), patPos->Offsets.end(),
              !boost::bind(&in_range<std::size_t>, boost::bind(&fromLE<uint16_t>, _1), lowlimit, size)))
        {
          return false;
        }
      }
      if (!patternsCount)
      {
        return false;
      }
      //find invalid patterns in position
      if (header->Positions + header->Length !=
          std::find_if(header->Positions, header->Positions + header->Length, std::bind2nd(std::greater_equal<uint_t>(),
          patternsCount)))
      {
        return false;
      }
    }

    //check samples
    for (const uint16_t* sampOff = header->SamplesOffsets.begin(); sampOff != header->SamplesOffsets.end(); ++sampOff)
    {
      if (const uint_t offset = fromLE(*sampOff))
      {
        if (offset + PT2Sample::GetMinimalSize() > size)
        {
          return false;
        }
        const PT2Sample* const sample(safe_ptr_cast<const PT2Sample*>(data + offset));
        if (!checker->AddRange(offset, sample->GetSize()))
        {
          return false;
        }
      }
    }
    //check ornaments
    for (const uint16_t* ornOff = header->OrnamentsOffsets.begin(); ornOff != header->OrnamentsOffsets.end(); ++ornOff)
    {
      if (const uint_t offset = fromLE(*ornOff))
      {
        if (offset + PT2Ornament::GetMinimalSize() > size)
        {
          return false;
        }
        const PT2Ornament* const ornament(safe_ptr_cast<const PT2Ornament*>(data + offset));
        if (!checker->AddRange(offset, ornament->GetSize()))
        {
          return false;
        }
      }
    }
    return true;
  }
}

namespace
{
  using namespace ZXTune;

  //plugin attributes
  const Char ID[] = {'P', 'T', '2', 0};
  const Char* const INFO = Text::PT2_PLUGIN_INFO;
  const uint_t CAPS = CAP_STOR_MODULE | CAP_DEV_AYM | CAP_CONV_RAW | GetSupportedAYMFormatConvertors();

  const std::string PT2_FORMAT(
    "02-0f"      // uint8_t Tempo; 2..15
    "01-ff"      // uint8_t Length; 1..100
    "00-fe"      // uint8_t Loop; 0..99
    //boost::array<uint16_t, 32> SamplesOffsets;
    "(?00-36){32}"
    //boost::array<uint16_t, 16> OrnamentsOffsets;
    "(?00-36){16}"
    "?00-36" // uint16_t PatternsOffset;
    "+30+"   // char Name[30];
    "00-fe"  // uint8_t Positions[1]; at least one
  );

  //////////////////////////////////////////////////////////////////////////
  class PT2ModulesFactory : public ModulesFactory
  {
  public:
    PT2ModulesFactory()
      : Format(Binary::Format::Create(PT2_FORMAT))
    {
    }

    virtual bool Check(const Binary::Container& inputData) const
    {
      const uint8_t* const data = static_cast<const uint8_t*>(inputData.Data());
      const std::size_t size = inputData.Size();
      return Format->Match(data, size) && CheckPT2Module(data, size);
    }

    virtual Binary::Format::Ptr GetFormat() const
    {
      return Format;
    }

    virtual Holder::Ptr CreateModule(ModuleProperties::RWPtr properties, Binary::Container::Ptr rawData, std::size_t& usedSize) const
    {
      try
      {
        assert(Check(*rawData));

        //assume all data is correct
        const IO::FastDump& data = IO::FastDump(*rawData, 0, MAX_MODULE_SIZE);

        const PT2ModuleData::Ptr parsedData = boost::make_shared<PT2ModuleData>();

        parsedData->ParseInformation(data, *properties);
        const std::size_t endOfSamples = parsedData->ParseSamples(data);
        const std::size_t endOfOrnaments = parsedData->ParseOrnaments(data);
        const std::size_t endOfPatterns = parsedData->ParsePatterns(data);

        //fill region
        const std::size_t rawSize = std::max(std::max(endOfSamples, endOfOrnaments), endOfPatterns);
        usedSize = std::min(rawSize, data.Size());

        //meta properties
        {
          const PT2Header* const header = safe_ptr_cast<const PT2Header*>(&data[0]);
          const std::size_t fixedOffset(sizeof(PT2Header) + header->Length - 1);
          const ModuleRegion fixedRegion(fixedOffset, usedSize -  fixedOffset);
          properties->SetSource(usedSize, fixedRegion);
        }

        const AYM::Chiptune::Ptr chiptune = boost::make_shared<PT2Chiptune>(parsedData, properties);
        return AYM::CreateHolder(chiptune);
      }
      catch (const Error&/*e*/)
      {
        Log::Debug("Core::PT2Supp", "Failed to create holder");
      }
      return Holder::Ptr();
    }
  private:
    const Binary::Format::Ptr Format;
  };
}

namespace ZXTune
{
  void RegisterPT2Support(PluginsRegistrator& registrator)
  {
    const ModulesFactory::Ptr factory = boost::make_shared<PT2ModulesFactory>();
    const PlayerPlugin::Ptr plugin = CreatePlayerPlugin(ID, INFO, CAPS, factory);
    registrator.RegisterPlugin(plugin);
  }
}
