/**
* 
* @file
*
* @brief  FastTracker chiptune factory implementation
*
* @author vitamin.caig@gmail.com
*
**/

//local includes
#include "fasttracker.h"
#include "aym_base.h"
#include "aym_base_track.h"
#include "aym_properties_helper.h"
//common includes
#include <make_ptr.h>
//library includes
#include <formats/chiptune/aym/fasttracker.h>
#include <math/numeric.h>
#include <module/players/properties_meta.h>
#include <module/players/simple_orderlist.h>

namespace Module
{
namespace FastTracker
{
  //supported commands and their parameters
  enum CmdType
  {
    //no parameters
    EMPTY,
    //r13,tone
    ENVELOPE,
    //no parameters
    ENVELOPE_OFF,
    //r5
    NOISE,
    //step
    SLIDE,
    //step,note
    SLIDE_NOTE
  };

  struct Sample : public Formats::Chiptune::FastTracker::Sample
  {
    Sample()
      : Formats::Chiptune::FastTracker::Sample()
    {
    }

    Sample(Formats::Chiptune::FastTracker::Sample rh)
      : Formats::Chiptune::FastTracker::Sample(std::move(rh))
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

    const Line& GetLine(uint_t idx) const
    {
      static const Line STUB;
      return Lines.size() > idx ? Lines[idx] : STUB;
    }
  };

  struct Ornament : public Formats::Chiptune::FastTracker::Ornament
  {
    Ornament()
      : Formats::Chiptune::FastTracker::Ornament()
    {
    }

    Ornament(Formats::Chiptune::FastTracker::Ornament rh)
      : Formats::Chiptune::FastTracker::Ornament(std::move(rh))
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

    const Line& GetLine(uint_t idx) const
    {
      static const Line STUB;
      return Lines.size() > idx ? Lines[idx] : STUB;
    }
  };

  typedef SimpleOrderListWithTransposition<Formats::Chiptune::FastTracker::PositionEntry> OrderListWithTransposition;

  class ModuleData : public TrackModel
  {
  public:
    typedef std::shared_ptr<const ModuleData> Ptr;
    typedef std::shared_ptr<ModuleData> RWPtr;

    ModuleData()
      : InitialTempo()
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
    OrderListWithTransposition::Ptr Order;
    PatternsSet::Ptr Patterns;
    SparsedObjectsStorage<Sample> Samples;
    SparsedObjectsStorage<Ornament> Ornaments;
  };

  class DataBuilder : public Formats::Chiptune::FastTracker::Builder
  {
  public:
    explicit DataBuilder(AYM::PropertiesHelper& props)
      : Data(MakeRWPtr<ModuleData>())
      , Properties(props)
      , Meta(props)
      , Patterns(PatternsBuilder::Create<AYM::TRACK_CHANNELS>())
    {
      Data->Patterns = Patterns.GetResult();
      Properties.SetFrequencyTable(TABLE_PROTRACKER3_ST);
    }

    Formats::Chiptune::MetaBuilder& GetMetaBuilder() override
    {
      return Meta;
    }

    void SetInitialTempo(uint_t tempo) override
    {
      Data->InitialTempo = tempo;
    }

    void SetSample(uint_t index, Formats::Chiptune::FastTracker::Sample sample) override
    {
      Data->Samples.Add(index, Sample(std::move(sample)));
    }

    void SetOrnament(uint_t index, Formats::Chiptune::FastTracker::Ornament ornament) override
    {
      Data->Ornaments.Add(index, Ornament(std::move(ornament)));
    }

    void SetPositions(std::vector<Formats::Chiptune::FastTracker::PositionEntry> positions, uint_t loop) override
    {
      Data->Order = MakePtr<OrderListWithTransposition>(loop, positions);
    }

    Formats::Chiptune::PatternBuilder& StartPattern(uint_t index) override
    {
      Patterns.SetPattern(index);
      return Patterns;
    }

    void StartChannel(uint_t index) override
    {
      Patterns.SetChannel(index);
    }

    void SetRest() override
    {
      Patterns.GetChannel().SetEnabled(false);
    }

    void SetNote(uint_t note) override
    {
      MutableCell& channel = Patterns.GetChannel();
      channel.SetEnabled(true);
      if (Command* cmd = channel.FindCommand(SLIDE_NOTE))
      {
        cmd->Param2 = note;
      }
      else
      {
        channel.SetNote(note);
      }
    }

    void SetSample(uint_t sample) override
    {
      Patterns.GetChannel().SetSample(sample);
    }

    void SetOrnament(uint_t ornament) override
    {
      Patterns.GetChannel().SetOrnament(ornament);
    }

    void SetVolume(uint_t vol) override
    {
      Patterns.GetChannel().SetVolume(vol);
    }

    void SetEnvelope(uint_t type, uint_t tone) override
    {
      Patterns.GetChannel().AddCommand(ENVELOPE, int_t(type), int_t(tone));
    }

    void SetNoEnvelope() override
    {
      Patterns.GetChannel().AddCommand(ENVELOPE_OFF);
    }

    void SetNoise(uint_t val) override
    {
      Patterns.GetChannel().AddCommand(NOISE, val);
    }

    void SetSlide(uint_t step) override
    {
      Patterns.GetChannel().AddCommand(SLIDE, int_t(step));
    }

    void SetNoteSlide(uint_t step) override
    {
      Patterns.GetChannel().AddCommand(SLIDE_NOTE, int_t(step));
    }

    ModuleData::Ptr GetResult() const
    {
      return Data;
    }
  private:
    const ModuleData::RWPtr Data;
    AYM::PropertiesHelper& Properties;
    MetaProperties Meta;
    PatternsBuilder Patterns;
  };

  template<class Object>
  class ObjectLinesIterator
  {
  public:
    ObjectLinesIterator()
      : Obj()
      , Position()
    {
    }

    void Set(const Object& obj)
    {
      //do not reset for original position update algo
      Obj = &obj;
    }

    void Reset()
    {
      Position = 0;
    }

    void Disable()
    {
      Position = Obj->GetSize();
    }

    const typename Object::Line* GetLine() const
    {
      return Position < Obj->GetSize()
        ? &Obj->GetLine(Position)
        : nullptr;
    }

    void Next()
    {
      if (++Position == Obj->GetSize())
      {
        Position = Obj->GetLoop();
      }
    }
  private:
    const Object* Obj;
    uint_t Position;
  };

  class Accumulator
  {
  public:
    Accumulator()
      : Value()
    {
    }

    void Reset()
    {
      Value = 0;
    }

    int_t Update(int_t delta, bool accumulate)
    {
      const int_t res = Value + delta;
      if (accumulate)
      {
        Value = res;
      }
      return res;
    }
  private:
    int_t Value;
  };

  class ToneSlider
  {
  public:
    ToneSlider()
      : Sliding()
      , Glissade()
      , Direction()
    {
    }

    void SetSlide(int_t slide)
    {
      Sliding = slide;
    }

    void SetGlissade(int_t gliss)
    {
      Glissade = gliss;
    }

    void SetSlideDirection(int_t direction)
    {
      Direction = direction;
    }

    int_t Update()
    {
      Sliding += Glissade;
      if ((Direction > 0 && Sliding >= 0)
       || (Direction < 0 && Sliding < 0))
      {
        Sliding = 0;
        Glissade = 0;
      }
      return Sliding;
    }

    void Reset()
    {
      Sliding = Glissade = Direction = 0;
    }
  private:
    int_t Sliding;
    int_t Glissade;
    int_t Direction;
  };

  struct ChannelState
  {
    ChannelState()
      : Note()
      , Envelope(), EnvelopeEnabled()
      , Volume(15), VolumeSlide()
      , Noise()
      , ToneAddon()
    {
    }

    uint_t Note;
    uint_t Envelope;
    bool EnvelopeEnabled;
    uint_t Volume;
    int_t VolumeSlide;
    int_t Noise;
    ObjectLinesIterator<Sample> SampleIterator;
    ObjectLinesIterator<Ornament> OrnamentIterator; 
    Accumulator NoteAccumulator;
    Accumulator ToneAccumulator;
    Accumulator NoiseAccumulator;
    Accumulator SampleNoiseAccumulator;
    Accumulator EnvelopeAccumulator;
    int_t ToneAddon;
    ToneSlider ToneSlide;
  };

  class DataRenderer : public AYM::DataRenderer
  {
  public:
    explicit DataRenderer(ModuleData::Ptr data)
      : Data(std::move(data))
      , Transposition()
    {
      Reset();
    }

    void Reset() override
    {
      const Sample& stubSample = Data->Samples.Get(0);
      const Ornament& stubOrnament = Data->Ornaments.Get(0);
      for (uint_t chan = 0; chan != PlayerState.size(); ++chan)
      {
        ChannelState& dst = PlayerState[chan];
        dst = ChannelState();
        dst.SampleIterator.Set(stubSample);
        dst.SampleIterator.Disable();
        dst.OrnamentIterator.Set(stubOrnament);
        dst.OrnamentIterator.Reset();
      }
      Transposition = 0;
    }

    void SynthesizeData(const TrackModelState& state, AYM::TrackBuilder& track) override
    {
      if (0 == state.Quirk())
      {
        if (0 == state.Line())
        {
          Transposition = Data->Order->GetTransposition(state.Position());
        }
        GetNewLineState(state, track);
      }
      SynthesizeChannelsData(track);
    }
  private:
    void GetNewLineState(const TrackModelState& state, AYM::TrackBuilder& track)
    {
      if (const Line::Ptr line = state.LineObject())
      {
        for (uint_t chan = 0; chan != PlayerState.size(); ++chan)
        {
          if (const Cell::Ptr src = line->GetChannel(chan))
          {
            GetNewChannelState(*src, PlayerState[chan], track);
          }
        }
      }
    }

    void GetNewChannelState(const Cell& src, ChannelState& dst, AYM::TrackBuilder& track)
    {
      if (const bool* enabled = src.GetEnabled())
      {
        if (*enabled)
        {
          dst.SampleIterator.Reset();
        }
        else
        {
          dst.SampleIterator.Disable();
        }
        dst.SampleNoiseAccumulator.Reset();
        dst.VolumeSlide = 0;
        dst.NoiseAccumulator.Reset();
        dst.NoteAccumulator.Reset();
        dst.OrnamentIterator.Reset();
        dst.ToneAccumulator.Reset();
        dst.EnvelopeAccumulator.Reset();
        dst.ToneSlide.Reset();
      }
      if (const uint_t* note = src.GetNote())
      {
        dst.Note = *note + Transposition;
      }
      if (const uint_t* sample = src.GetSample())
      {
        dst.SampleIterator.Set(Data->Samples.Get(*sample));
      }
      if (const uint_t* ornament = src.GetOrnament())
      {
        dst.OrnamentIterator.Set(Data->Ornaments.Get(*ornament));
        dst.OrnamentIterator.Reset();
        dst.NoiseAccumulator.Reset();
        dst.NoteAccumulator.Reset();
      }
      if (const uint_t* volume = src.GetVolume())
      {
        dst.Volume = *volume;
      }
      for (CommandsIterator it = src.GetCommands(); it; ++it)
      {
        switch (it->Type)
        {
        case ENVELOPE:
          track.SetEnvelopeType(it->Param1);
          dst.Envelope = it->Param2;
          dst.EnvelopeEnabled = true;
          break;
        case ENVELOPE_OFF:
          dst.EnvelopeEnabled = false;
          break;
        case NOISE:
          dst.Noise = it->Param1;
          break;
        case SLIDE:
          dst.ToneSlide.SetGlissade(it->Param1);
          break;
        case SLIDE_NOTE:
          {
            const int_t slide = track.GetSlidingDifference(it->Param2, dst.Note);
            const int_t gliss = slide >= 0 ? -it->Param1 : it->Param1;
            const int_t direction = slide >= 0 ? -1 : +1;
            dst.ToneSlide.SetSlide(slide);
            dst.ToneSlide.SetGlissade(gliss);
            dst.ToneSlide.SetSlideDirection(direction);
            dst.Note = it->Param2 + Transposition;
          }
          break;
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

    void SynthesizeChannel(ChannelState& dst, AYM::ChannelBuilder& channel, AYM::TrackBuilder& track)
    {
      const Ornament::Line& curOrnamentLine = *dst.OrnamentIterator.GetLine();
      const int_t noteAddon = dst.NoteAccumulator.Update(curOrnamentLine.NoteAddon, curOrnamentLine.KeepNoteAddon);
      const int_t noiseAddon = dst.NoiseAccumulator.Update(curOrnamentLine.NoiseAddon, curOrnamentLine.KeepNoiseAddon);
      dst.OrnamentIterator.Next();

      if (const Sample::Line* curSampleLine = dst.SampleIterator.GetLine())
      {
        //apply noise
        const int_t sampleNoiseAddon = dst.SampleNoiseAccumulator.Update(curSampleLine->Noise, curSampleLine->AccumulateNoise);
        if (curSampleLine->NoiseMask)
        {
          channel.DisableNoise();
        }
        else
        {
          track.SetNoise(dst.Noise + noiseAddon + sampleNoiseAddon);
        }
        //apply tone
        dst.ToneAddon = dst.ToneAccumulator.Update(curSampleLine->Tone, curSampleLine->AccumulateTone);
        if (curSampleLine->ToneMask)
        {
          channel.DisableTone();
        }
        //apply level
        dst.VolumeSlide += curSampleLine->VolSlide;
        const int_t volume = Math::Clamp<int_t>(curSampleLine->Level + dst.VolumeSlide, 0, 15);
        const uint_t level = ((dst.Volume * 17 + (dst.Volume > 7)) * volume + 128) / 256;
        channel.SetLevel(level);

        const uint_t envelope = dst.EnvelopeAccumulator.Update(curSampleLine->EnvelopeAddon, curSampleLine->AccumulateEnvelope);
        if (curSampleLine->EnableEnvelope && dst.EnvelopeEnabled)
        {
          channel.EnableEnvelope();
          track.SetEnvelopeTone(dst.Envelope - envelope);
        }

        dst.SampleIterator.Next();
      }
      else
      {
        channel.SetLevel(0);
        channel.DisableTone();
        channel.DisableNoise();
      }
      channel.SetTone(dst.Note + noteAddon, dst.ToneAddon + dst.ToneSlide.Update());
    }
  private:
    const ModuleData::Ptr Data;
    std::array<ChannelState, AYM::TRACK_CHANNELS> PlayerState;
    int_t Transposition;
  };

  class Chiptune : public AYM::Chiptune
  {
  public:
    Chiptune(ModuleData::Ptr data, Parameters::Accessor::Ptr properties)
      : Data(std::move(data))
      , Properties(std::move(properties))
      , Info(CreateTrackInfo(Data, AYM::TRACK_CHANNELS))
    {
    }

    Information::Ptr GetInformation() const override
    {
      return Info;
    }

    Parameters::Accessor::Ptr GetProperties() const override
    {
      return Properties;
    }

    AYM::DataIterator::Ptr CreateDataIterator(AYM::TrackParameters::Ptr trackParams) const override
    {
      const TrackStateIterator::Ptr iterator = CreateTrackStateIterator(Data);
      const AYM::DataRenderer::Ptr renderer = MakePtr<DataRenderer>(Data);
      return AYM::CreateDataIterator(trackParams, iterator, renderer);
    }
  private:
    const ModuleData::Ptr Data;
    const Parameters::Accessor::Ptr Properties;
    const Information::Ptr Info;
  };

  class Factory : public AYM::Factory
  {
  public:
    AYM::Chiptune::Ptr CreateChiptune(const Binary::Container& rawData, Parameters::Container::Ptr properties) const override
    {
      AYM::PropertiesHelper props(*properties);
      DataBuilder dataBuilder(props);
      if (const Formats::Chiptune::Container::Ptr container = Formats::Chiptune::FastTracker::Parse(rawData, dataBuilder))
      {
        props.SetSource(*container);
        return MakePtr<Chiptune>(dataBuilder.GetResult(), properties);
      }
      else
      {
        return AYM::Chiptune::Ptr();
      }
    }
  };

  Factory::Ptr CreateFactory()
  {
    return MakePtr<Factory>();
  }
}
}
