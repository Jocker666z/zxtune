/*
Abstract:
  Streamed modules support implementation

Last changed:
  $Id$

Author:
  (C) Vitamin/CAIG/2001
*/

//local includes
#include "streaming.h"
//boost includes
#include <boost/make_shared.hpp>

namespace
{
  using namespace ZXTune;
  using namespace ZXTune::Module;

  const uint_t STREAM_LOGICAL_CHANNELS = 1;

  class StreamInfo : public Information
  {
  public:
    explicit StreamInfo(uint_t frames, uint_t physChannels)
      : TotalFrames(frames)
      , PhysChans(physChannels)
    {
    }
    virtual uint_t PositionsCount() const
    {
      return 1;
    }
    virtual uint_t LoopPosition() const
    {
      return 0;
    }
    virtual uint_t PatternsCount() const
    {
      return 0;
    }
    virtual uint_t FramesCount() const
    {
      return TotalFrames;
    }
    virtual uint_t LoopFrame() const
    {
      return 0;
    }
    virtual uint_t LogicalChannels() const
    {
      return STREAM_LOGICAL_CHANNELS;
    }
    virtual uint_t PhysicalChannels() const
    {
      return PhysChans;
    }
    virtual uint_t Tempo() const
    {
      return 1;
    }
  private:
    const uint_t TotalFrames;
    const uint_t PhysChans;
  };

  class StreamStateIterator : public StateIterator
  {
  public:
    explicit StreamStateIterator(Information::Ptr info)
      : Info(info)
    {
      Reset();
    }

    //status functions
    virtual uint_t Position() const
    {
      return 0;
    }

    virtual uint_t Pattern() const
    {
      return 0;
    }

    virtual uint_t PatternSize() const
    {
      return 0;
    }

    virtual uint_t Line() const
    {
      return 0;
    }

    virtual uint_t Tempo() const
    {
      return 1;
    }

    virtual uint_t Quirk() const
    {
      return 0;
    }

    virtual uint_t Frame() const
    {
      return CurFrame;
    }

    virtual uint_t Channels() const
    {
      return STREAM_LOGICAL_CHANNELS;
    }

    virtual uint_t AbsoluteFrame() const
    {
      return AbsFrame;
    }

    virtual uint64_t AbsoluteTick() const
    {
      return AbsTick;
    }

    //iterator functions
    virtual void Reset()
    {
      AbsFrame = 0;
      AbsTick = 0;
      CurFrame = 0;
    }

    virtual void Seek(uint_t frameNum)
    {
      CurFrame = 0;
      if (frameNum)
      {
        SeekIterator(*this, frameNum);
      }
    }

    virtual bool NextFrame(uint64_t ticksToSkip, bool looped)
    {
      ++CurFrame;
      ++AbsFrame;
      AbsTick += ticksToSkip;
      if (CurFrame >= Info->FramesCount() &&
          !ProcessLoop(looped))
      {
        return false;
      }
      return true;
    }
  private:
    bool ProcessLoop(bool looped)
    {
      return looped
        ? ProcessNormalLoop()
        : ProcessNoLoop();
    }

    bool ProcessNormalLoop()
    {
      CurFrame = Info->LoopFrame();
      //++CurLoopsCount;
      return true;
    }

    bool ProcessNoLoop()
    {
      Seek(0);
      return false;
    }
  private:
    //context
    const Information::Ptr Info;
    //state
    uint_t CurFrame;
    uint_t AbsFrame;
    uint64_t AbsTick;
  };
}

namespace ZXTune
{
  namespace Module
  {
    Information::Ptr CreateStreamInfo(uint_t frames, uint_t physChannels)
    {
      return boost::make_shared<StreamInfo>(frames, physChannels);
    }

    StateIterator::Ptr CreateStreamStateIterator(Information::Ptr info)
    {
      return boost::make_shared<StreamStateIterator>(info);
    }
  }
}
