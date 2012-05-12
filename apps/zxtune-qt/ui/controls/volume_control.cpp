/*
Abstract:
  Volume control widget implementation

Last changed:
  $Id$

Author:
  (C) Vitamin/CAIG/2001

  This file is a part of zxtune-qt application based on zxtune library
*/

//local includes
#include "volume_control.h"
#include "volume_control.ui.h"
#include "supp/playback_supp.h"
#include "ui/styles.h"
//common includes
#include <contract.h>
#include <error.h>
//std includes
#include <ctime>
#include <numeric>

namespace
{
  class VolumeControlImpl : public VolumeControl
                          , public Ui::VolumeControl
  {
  public:
    VolumeControlImpl(QWidget& parent, PlaybackSupport& supp)
      : ::VolumeControl(parent)
      , LastUpdateTime(0)
    {
      //setup self
      setupUi(this);
      setEnabled(false);
      Require(connect(volumeLevel, SIGNAL(valueChanged(int)), SLOT(SetLevel(int))));

      Require(connect(&supp, SIGNAL(OnUpdateState()), SLOT(UpdateState())));
      Require(connect(&supp, SIGNAL(OnSetBackend(ZXTune::Sound::Backend::Ptr)), SLOT(SetBackend(ZXTune::Sound::Backend::Ptr))));
      volumeLevel->setStyle(new UI::ClickNGoSliderStyle(*volumeLevel));
    }

    virtual void SetBackend(ZXTune::Sound::Backend::Ptr backend)
    {
      Controller = backend->GetVolumeControl();
      setEnabled(Controller != 0);
    }

    virtual void UpdateState()
    {
      //slight optimization
      const std::time_t thisTime = std::time(0);
      if (thisTime == LastUpdateTime)
      {
        return;
      }
      LastUpdateTime = thisTime;
      if (Controller && !volumeLevel->isSliderDown())
      {
        ZXTune::Sound::MultiGain vol;
        //TODO: check result
        Controller->GetVolume(vol);
        const ZXTune::Sound::Gain gain = *std::max_element(vol.begin(), vol.end());
        volumeLevel->setValue(static_cast<int>(gain * volumeLevel->maximum() + 0.5));
      }
    }

    virtual void SetLevel(int level)
    {
      if (Controller)
      {
        const ZXTune::Sound::Gain gain = ZXTune::Sound::Gain(level) / volumeLevel->maximum();
        ZXTune::Sound::MultiGain vol;
        vol.assign(gain);
        //TODO: check result
        Controller->SetVolume(vol);
      }
    }
  private:
    ZXTune::Sound::VolumeControl::Ptr Controller;
    std::time_t LastUpdateTime;
  };
}

VolumeControl::VolumeControl(QWidget& parent) : QWidget(&parent)
{
}

VolumeControl* VolumeControl::Create(QWidget& parent, PlaybackSupport& supp)
{
  return new VolumeControlImpl(parent, supp);
}
