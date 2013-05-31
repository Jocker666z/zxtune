/*
Abstract:
  Mixer on params implementation

Last changed:
  $Id$

Author:
  (C) Vitamin/CAIG/2001
*/

//library includes
#include <sound/matrix_mixer.h>
#include <sound/mixer_parameters.h>
//boost includes
#include <boost/make_shared.hpp>

namespace Sound
{
  typedef boost::array<Gain::Type, Sample::CHANNELS> MultiConfigValue;

  void GetMatrixRow(const Parameters::Accessor& params, uint_t channels, uint_t inChan, MultiConfigValue& out)
  {
    for (uint_t outChan = 0; outChan != out.size(); ++outChan)
    {
      const Parameters::NameType name = Parameters::ZXTune::Sound::Mixer::LEVEL(channels, inChan, outChan);
      Parameters::IntType val = Parameters::ZXTune::Sound::Mixer::LEVEL_DEFAULT(channels, inChan, outChan);
      params.FindValue(name, val);
      out[outChan] = Gain::Type(static_cast<uint_t>(val), 100);
    }
  }

  void GetMatrix(const Parameters::Accessor& params, MultiConfigValue* outBegin, MultiConfigValue* outEnd)
  {
    const uint_t channels = outEnd - outBegin;
    for (MultiConfigValue* it = outBegin; it != outEnd; ++it)
    {
      GetMatrixRow(params, channels, it - outBegin, *it);
    }
  }

  template<unsigned Channels>
  class PollingMixer : public FixedChannelsMixer<Channels>
  {
    typedef FixedChannelsMatrixMixer<Channels> MixerType;
  public:
    explicit PollingMixer(Parameters::Accessor::Ptr params)
      : Params(params)
      , Delegate(MixerType::Create())
    {
      UpdateMatrix();
    }

    virtual void ApplyData(const typename MixerType::InDataType& inData)
    {
      Delegate->ApplyData(inData);
    }

    virtual void Flush()
    {
      Delegate->Flush();
      UpdateMatrix();
    }
    
    virtual void SetTarget(Receiver::Ptr rcv)
    {
      Delegate->SetTarget(rcv);
    }
  private:
    void UpdateMatrix()
    {
      const MatrixType oldMatrix = LastMatrix;
      GetMatrix(*Params, &LastMatrix.front(), &LastMatrix.back() + 1);
      if (oldMatrix != LastMatrix)
      {
        SetMatrix();
      }
    }

    void SetMatrix()
    {
      typename MixerType::Matrix mtx;
      for (uint_t inChan = 0; inChan != Channels; ++inChan)
      {
        const MultiConfigValue& cfg = LastMatrix[inChan];
        mtx[inChan] = Gain(cfg[0], cfg[1]);
      }
      Delegate->SetMatrix(mtx);
    }
  private:
    const Parameters::Accessor::Ptr Params;
    const typename MixerType::Ptr Delegate;
    typedef boost::array<MultiConfigValue, Channels> MatrixType;
    MatrixType LastMatrix;
  };
}

namespace Sound
{
  OneChannelMixer::Ptr CreateOneChannelMixer(Parameters::Accessor::Ptr params)
  {
    return boost::make_shared<PollingMixer<1> >(params);
  }

  TwoChannelsMixer::Ptr CreateTwoChannelsMixer(Parameters::Accessor::Ptr params)
  {
    return boost::make_shared<PollingMixer<2> >(params);
  }

  ThreeChannelsMixer::Ptr CreateThreeChannelsMixer(Parameters::Accessor::Ptr params)
  {
    return boost::make_shared<PollingMixer<3> >(params);
  }

  FourChannelsMixer::Ptr CreateFourChannelsMixer(Parameters::Accessor::Ptr params)
  {
    return boost::make_shared<PollingMixer<4> >(params);
  }
}
