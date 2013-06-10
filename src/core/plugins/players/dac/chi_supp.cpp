/*
Abstract:
  CHI modules playback support

Last changed:
  $Id$

Author:
  (C) Vitamin/CAIG/2001
*/

//local includes
#include "dac_base.h"
#include "core/plugins/registrator.h"
#include "core/plugins/players/creation_result.h"
#include "core/plugins/players/module_properties.h"
#include "core/plugins/players/simple_orderlist.h"
#include "core/plugins/players/tracking.h"
//common includes
#include <byteorder.h>
#include <error_tools.h>
#include <tools.h>
//library includes
#include <core/core_parameters.h>
#include <core/module_attrs.h>
#include <core/plugin_attrs.h>
#include <devices/dac/sample_factories.h>
#include <formats/chiptune/decoders.h>
#include <formats/chiptune/digital/chiptracker.h>
#include <sound/mixer_factory.h>

#define FILE_TAG AB8BEC8B

namespace ChipTracker
{
  using namespace ZXTune;
  using namespace ZXTune::Module;

  const std::size_t CHANNELS_COUNT = 4;

  const uint64_t Z80_FREQ = 3500000;
  //one cycle is 4 outputs
  const uint_t OUTS_PER_CYCLE = 4;
  const uint_t TICKS_PER_CYCLE = 890;
  const uint_t C_1_STEP = 72;
  //Z80_FREQ * (C_1_STEP / 256) / (TICKS_PER_CYCLE / OUTS_PER_CYCLE)
  const uint_t SAMPLES_FREQ = Z80_FREQ * C_1_STEP * OUTS_PER_CYCLE / TICKS_PER_CYCLE / 256;

  inline int_t StepToHz(int_t step)
  {
    //C-1 frequency is 32.7Hz
    //step * 32.7 / c-1_step
    return step * 3270 / int_t(C_1_STEP * 100);
  }
  
  enum CmdType
  {
    EMPTY,
    //offset in bytes
    SAMPLE_OFFSET,
    //step
    SLIDE
  };

  using namespace ZXTune;
  using namespace ZXTune::Module;

  class ModuleData : public TrackModel
  {
  public:
    typedef boost::shared_ptr<ModuleData> RWPtr;
    typedef boost::shared_ptr<const ModuleData> Ptr;

    ModuleData()
      : InitialTempo()
    {
    }

    virtual uint_t GetInitialTempo() const
    {
      return InitialTempo;
    }

    virtual const OrderList& GetOrder() const
    {
      return *Order;
    }

    virtual const PatternsSet& GetPatterns() const
    {
      return *Patterns;
    }

    uint_t InitialTempo;
    OrderList::Ptr Order;
    PatternsSet::Ptr Patterns;
    SparsedObjectsStorage<Devices::DAC::Sample::Ptr> Samples;
  };

  std::auto_ptr<Formats::Chiptune::ChipTracker::Builder> CreateDataBuilder(ModuleData::RWPtr data, ModuleProperties::RWPtr props);
}

namespace ChipTracker
{
  class DataBuilder : public Formats::Chiptune::ChipTracker::Builder
  {
  public:
    DataBuilder(ModuleData::RWPtr data, ModuleProperties::RWPtr props)
      : Data(data)
      , Properties(props)
      , Builder(PatternsBuilder::Create<CHANNELS_COUNT>())
    {
      Data->Patterns = Builder.GetPatterns();
      Properties->SetSamplesFreq(SAMPLES_FREQ);
    }

    virtual Formats::Chiptune::MetaBuilder& GetMetaBuilder()
    {
      return *Properties;
    }

    virtual void SetVersion(uint_t major, uint_t minor)
    {
      Properties->SetVersion(major, minor);
    }

    virtual void SetInitialTempo(uint_t tempo)
    {
      Data->InitialTempo = tempo;
    }

    virtual void SetSample(uint_t index, std::size_t loop, Binary::Data::Ptr sample)
    {
      Data->Samples.Add(index, Devices::DAC::CreateU8Sample(sample, loop));
    }

    virtual void SetPositions(const std::vector<uint_t>& positions, uint_t loop)
    {
      Data->Order = boost::make_shared<SimpleOrderList>(loop, positions.begin(), positions.end());
    }

    virtual Formats::Chiptune::PatternBuilder& StartPattern(uint_t index)
    {
      Builder.SetPattern(index);
      return Builder;
    }

    virtual void StartChannel(uint_t index)
    {
      Builder.SetChannel(index);
    }

    virtual void SetRest()
    {
      Builder.GetChannel().SetEnabled(false);
    }

    virtual void SetNote(uint_t note)
    {
      Builder.GetChannel().SetEnabled(true);
      Builder.GetChannel().SetNote(note);
    }

    virtual void SetSample(uint_t sample)
    {
      Builder.GetChannel().SetSample(sample);
    }

    virtual void SetSlide(int_t step)
    {
      Builder.GetChannel().AddCommand(SLIDE, step);
    }

    virtual void SetSampleOffset(uint_t offset)
    {
      Builder.GetChannel().AddCommand(SAMPLE_OFFSET, offset);
    }
  private:
    const ModuleData::RWPtr Data;
    const ModuleProperties::RWPtr Properties;
    PatternsBuilder Builder;
  };

  // perform module 'playback' right after creating (debug purposes)
  #ifndef NDEBUG
  #define SELF_TEST
  #endif

  Renderer::Ptr CreateModuleRenderer(Parameters::Accessor::Ptr params, ModuleData::Ptr data, Devices::DAC::Chip::Ptr device);

  class ModuleHolder : public Holder
  {
  public:
    ModuleHolder(ModuleData::Ptr data, ModuleProperties::Ptr properties)
      : Data(data)
      , Properties(properties)
      , Info(CreateTrackInfo(Data, CHANNELS_COUNT))
    {
    }

    virtual Information::Ptr GetModuleInformation() const
    {
      return Info;
    }

    virtual Parameters::Accessor::Ptr GetModuleProperties() const
    {
      return Properties;
    }

    virtual Renderer::Ptr CreateRenderer(Parameters::Accessor::Ptr params, Sound::Receiver::Ptr target) const
    {
      const Sound::FourChannelsMatrixMixer::Ptr mixer = Sound::FourChannelsMatrixMixer::Create();
      Sound::FillMixer(*params, *mixer);
      const Devices::DAC::ChipParameters::Ptr chipParams = DAC::CreateChipParameters(params);
      const Devices::DAC::Chip::Ptr chip(Devices::DAC::CreateChip(chipParams, mixer, target));
      for (uint_t idx = 0, lim = Data->Samples.Size(); idx != lim; ++idx)
      {
        chip->SetSample(idx, Data->Samples.Get(idx));
      }
      return CreateModuleRenderer(params, Data, chip);
    }
  private:
    const ModuleData::Ptr Data;
    const ModuleProperties::Ptr Properties;
    const Information::Ptr Info;
  };

  class ModuleRenderer : public Renderer
  {
    struct GlissData
    {
      GlissData() : Sliding(), Glissade()
      {
      }
      int_t Sliding;
      int_t Glissade;

      void Reset()
      {
        Sliding = Glissade = 0;
      }

      bool Update()
      {
        Sliding += Glissade;
        return Glissade != 0;
      }
    };
  public:
    ModuleRenderer(Parameters::Accessor::Ptr params, ModuleData::Ptr data, Devices::DAC::Chip::Ptr device)
      : Data(data)
      , Params(DAC::TrackParameters::Create(params))
      , Device(device)
      , Iterator(CreateTrackStateIterator(Data))
      , LastRenderTime(0)
    {
#ifdef SELF_TEST
//perform self-test
      Devices::DAC::DataChunk chunk;
      while (Iterator->IsValid())
      {
        RenderData(chunk);
        Iterator->NextFrame(false);
      }
      Reset();
#endif
    }

    virtual TrackState::Ptr GetTrackState() const
    {
      return Iterator->GetStateObserver();
    }

    virtual Analyzer::Ptr GetAnalyzer() const
    {
      return DAC::CreateAnalyzer(Device);
    }

    virtual bool RenderFrame()
    {
      if (Iterator->IsValid())
      {
        LastRenderTime += Params->FrameDuration();
        Devices::DAC::DataChunk chunk;
        RenderData(chunk);
        chunk.TimeStamp = LastRenderTime;
        Device->RenderData(chunk);
        Device->Flush();
        Iterator->NextFrame(Params->Looped());
      }
      return Iterator->IsValid();
    }

    virtual void Reset()
    {
      Device->Reset();
      Iterator->Reset();
      std::fill(Gliss.begin(), Gliss.end(), GlissData());
      LastRenderTime = Time::Microseconds();
    }

    virtual void SetPosition(uint_t frame)
    {
      const TrackState::Ptr state = Iterator->GetStateObserver();
      if (frame < state->Frame())
      {
        //reset to beginning in case of moving back
        Iterator->Reset();
      }
      //fast forward
      Devices::DAC::DataChunk chunk;
      while (state->Frame() < frame && Iterator->IsValid())
      {
        //do not update tick for proper rendering
        RenderData(chunk);
        Iterator->NextFrame(false);
      }
    }

  private:
    void RenderData(Devices::DAC::DataChunk& chunk)
    {
      const TrackModelState::Ptr state = Iterator->GetStateObserver();
      DAC::TrackBuilder track;
      SynthesizeData(*state, track);
      track.GetResult(chunk.Channels);
    }

    void SynthesizeData(const TrackModelState& state, DAC::TrackBuilder& track)
    {
      SynthesizeChannelsData(track);
      if (0 == state.Quirk())
      {
        GetNewLineState(state, track);
      }
    }

    void SynthesizeChannelsData(DAC::TrackBuilder& track)
    {
      for (uint_t chan = 0; chan != CHANNELS_COUNT; ++chan)
      {
        GlissData& gliss(Gliss[chan]);
        if (gliss.Update())
        {
          DAC::ChannelDataBuilder builder = track.GetChannel(chan);
          builder.SetFreqSlideHz(StepToHz(gliss.Sliding));
        }
      }
    }

    void GetNewLineState(const TrackModelState& state, DAC::TrackBuilder& track)
    {
      Gliss.assign(GlissData());
      if (const Line::Ptr line = state.LineObject())
      {
        for (uint_t chan = 0; chan != CHANNELS_COUNT; ++chan)
        {
          DAC::ChannelDataBuilder builder = track.GetChannel(chan);
          if (const Cell::Ptr src = line->GetChannel(chan))
          {
            GetNewChannelState(*src, Gliss[chan], builder);
          }
        }
      }
    };
      
    void GetNewChannelState(const Cell& src, GlissData& gliss, DAC::ChannelDataBuilder& builder)
    {
      if (const bool* enabled = src.GetEnabled())
      {
        builder.SetEnabled(*enabled);
        if (!*enabled)
        {
          builder.SetPosInSample(0);
        }
      }
      if (const uint_t* note = src.GetNote())
      {
        builder.SetNote(*note);
        builder.SetPosInSample(0);
      }
      if (const uint_t* sample = src.GetSample())
      {
        builder.SetSampleNum(*sample);
        builder.SetPosInSample(0);
      }
      builder.SetFreqSlideHz(0);
      gliss.Reset();
      for (CommandsIterator it = src.GetCommands(); it; ++it)
      {
        switch (it->Type)
        {
        case SAMPLE_OFFSET:
          builder.SetPosInSample(it->Param1);
          break;
        case SLIDE:
          gliss.Glissade = it->Param1;
          break;
        default:
          assert(!"Invalid command");
        }
      }
    }
  private:
    const ModuleData::Ptr Data;
    const DAC::TrackParameters::Ptr Params;
    const Devices::DAC::Chip::Ptr Device;
    const TrackStateIterator::Ptr Iterator;
    boost::array<GlissData, CHANNELS_COUNT> Gliss;
    Time::Microseconds LastRenderTime;
  };

  Renderer::Ptr CreateModuleRenderer(Parameters::Accessor::Ptr params, ModuleData::Ptr data, Devices::DAC::Chip::Ptr device)
  {
    return Renderer::Ptr(new ModuleRenderer(params, data, device));
  }
}

namespace CHI
{
  using namespace ZXTune;
  using namespace ZXTune::Module;

  //plugin attributes
  const Char ID[] = {'C', 'H', 'I', 0};
  const uint_t CAPS = CAP_STOR_MODULE | CAP_DEV_4DAC | CAP_CONV_RAW;

  class Factory : public ModulesFactory
  {
  public:
    explicit Factory(Formats::Chiptune::Decoder::Ptr decoder)
      : Decoder(decoder)
    {
    }

    virtual bool Check(const Binary::Container& data) const
    {
      return Decoder->Check(data);
    }

    virtual Binary::Format::Ptr GetFormat() const
    {
      return Decoder->GetFormat();
    }

    virtual Holder::Ptr CreateModule(ModuleProperties::RWPtr properties, Binary::Container::Ptr data, std::size_t& usedSize) const
    {
      const ::ChipTracker::ModuleData::RWPtr modData = boost::make_shared< ::ChipTracker::ModuleData>();
      ::ChipTracker::DataBuilder builder(modData, properties);
      if (const Formats::Chiptune::Container::Ptr container = Formats::Chiptune::ChipTracker::Parse(*data, builder))
      {
        usedSize = container->Size();
        properties->SetSource(container);
        return boost::make_shared< ::ChipTracker::ModuleHolder>(modData, properties);
      }
      return Holder::Ptr();
    }
  private:
    const Formats::Chiptune::Decoder::Ptr Decoder;
  };
}

namespace ZXTune
{
  void RegisterCHISupport(PlayerPluginsRegistrator& registrator)
  {
    const Formats::Chiptune::Decoder::Ptr decoder = Formats::Chiptune::CreateChipTrackerDecoder();
    const ModulesFactory::Ptr factory = boost::make_shared<CHI::Factory>(decoder);
    const PlayerPlugin::Ptr plugin = CreatePlayerPlugin(CHI::ID, decoder->GetDescription(), CHI::CAPS, factory);
    registrator.RegisterPlugin(plugin);
  }
}
