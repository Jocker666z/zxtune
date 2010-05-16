/*
Abstract:
  Playitems process thread implementation

Last changed:
  $Id$

Author:
  (C) Vitamin/CAIG/2001

  This file is a part of zxtune-qt application based on zxtune library
*/

//local includes
#include "playlist_thread.h"
#include "playlist_thread_moc.h"
#include "playlist_ui.h"
//common includes
#include <error.h>
#include <logging.h>
//qt includes
#include <QtCore/QMutex>
//std includes
#include <ctime>
#include <queue>
//boost includes
#include <boost/bind.hpp>

namespace
{
  class ProcessThreadImpl : public ProcessThread
  {
  public:
    explicit ProcessThreadImpl(QWidget* owner)
      : Provider(PlayitemsProvider::Create())
      , Canceled(false)
    {
      setParent(owner);
      //detectParams.Filter = 0;
      DetectParams.Logger = boost::bind(&ProcessThreadImpl::DispatchProgress, this, _1);
      DetectParams.Callback = boost::bind(&ProcessThreadImpl::OnProcessItem, this, _1);
    }

    virtual void AddItemPath(const String& path)
    {
      QMutexLocker lock(&QueueLock);
      Queue.push(path);
      this->start();
    }

    virtual void Cancel()
    {
      Canceled = true;
      this->wait();
    }
    
    virtual void run()
    {
      Canceled = false;
      OnScanStart();
      for (;;)
      {
        String path;
        {
          QMutexLocker lock(&QueueLock);
          if (Queue.empty())
          {
            break;
          }
          path = Queue.front();
          Queue.pop();
        }
        
        if (const Error& e = Provider->DetectModules(path, 
          Parameters::Map(),//TODO
          DetectParams))
        {
          //TODO: check and show error
          e.GetText();
        }
      }
      OnScanStop();
    }
  private:
    void DispatchProgress(const Log::MessageData& msg)
    {
      if (0 == msg.Level)
      {
        OnProgress(msg);
      }
    }

    bool OnProcessItem(const Playitem::Ptr& item)
    {
      OnGetItem(item);
      return !Canceled;
    }

  private:
    const PlayitemsProvider::Ptr Provider;
    PlayitemDetectParameters DetectParams;
    QMutex QueueLock;
    std::queue<String> Queue;
    //TODO: possibly use events
    volatile bool Canceled;
  };
}

ProcessThread* ProcessThread::Create(QWidget* owner)
{
  assert(owner);
  return new ProcessThreadImpl(owner);
}
