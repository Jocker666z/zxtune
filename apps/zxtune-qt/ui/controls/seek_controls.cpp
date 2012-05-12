/*
Abstract:
  Seek controller creation implementation

Last changed:
  $Id$

Author:
  (C) Vitamin/CAIG/2001

  This file is a part of zxtune-qt application based on zxtune library
*/

//local includes
#include "seek_controls.h"
#include "seek_controls.ui.h"
#include "supp/playback_supp.h"
#include "ui/styles.h"
#include "ui/utils.h"
//common includes
#include <format.h>

namespace
{
  class SeekControlsImpl : public SeekControls
                         , private Ui::SeekControls
  {
  public:
    SeekControlsImpl(QWidget& parent, PlaybackSupport& supp)
      : ::SeekControls(parent)
    {
      //setup self
      setupUi(this);
      timePosition->setRange(0, 0);
      this->connect(timePosition, SIGNAL(sliderReleased()), SLOT(EndSeeking()));

      this->connect(&supp, SIGNAL(OnStartModule(ZXTune::Sound::Backend::Ptr, Playlist::Item::Data::Ptr)),
        SLOT(InitState(ZXTune::Sound::Backend::Ptr)));
      this->connect(&supp, SIGNAL(OnUpdateState()), SLOT(UpdateState()));
      this->connect(&supp, SIGNAL(OnStopModule()), SLOT(CloseState()));
      supp.connect(this, SIGNAL(OnSeeking(int)), SLOT(Seek(int)));
      timePosition->setStyle(new UI::ClickNGoSliderStyle(*timePosition));
    }

    virtual void InitState(ZXTune::Sound::Backend::Ptr player)
    {
      const ZXTune::Module::Information::Ptr info = player->GetModuleInformation();
      timePosition->setRange(0, info->FramesCount());
      TrackState = player->GetTrackState();
    }

    virtual void UpdateState()
    {
      const uint_t curFrame = TrackState->Frame();
      if (!timePosition->isSliderDown())
      {
        timePosition->setSliderPosition(curFrame);
      }
      timeDisplay->setText(ToQString(Strings::FormatTime(curFrame, 20000/*TODO*/)));
    }

    virtual void CloseState()
    {
      timePosition->setRange(0, 0);
      timeDisplay->setText(QString::fromUtf8("-:-.-"));
    }

    virtual void EndSeeking()
    {
      OnSeeking(timePosition->value());
    }
  private:
    ZXTune::Module::TrackState::Ptr TrackState;
  };
}

SeekControls::SeekControls(QWidget& parent) : QWidget(&parent)
{
}

SeekControls* SeekControls::Create(QWidget& parent, PlaybackSupport& supp)
{
  return new SeekControlsImpl(parent, supp);
}
