/*
Abstract:
  Backend helper interface implementation

Last changed:
  $Id$

Author:
  (C) Vitamin/CAIG/2001
*/

//local includes
#include "backend_impl.h"
//common includes
#include <tools.h>
#include <error_tools.h>
#include <logging.h>
//library includes
#include <async/job.h>
#include <sound/error_codes.h>
#include <sound/sound_parameters.h>
//boost includes
#include <boost/make_shared.hpp>
//text includes
#include <sound/text/sound.h>

#define FILE_TAG B3D60DB5

namespace
{
  using namespace ZXTune;
  using namespace ZXTune::Sound;

  const std::string THIS_MODULE("BackendBase");

  typedef boost::lock_guard<boost::mutex> Locker;

  //playing thread and starting/stopping thread
  const std::size_t TOTAL_WORKING_THREADS = 2;

  class SafeRendererWrapper : public Module::Renderer
  {
  public:
    explicit SafeRendererWrapper(Module::Renderer::Ptr player)
      : Delegate(player)
    {
      if (!Delegate.get())
      {
        throw Error(THIS_LINE, BACKEND_FAILED_CREATE, Text::SOUND_ERROR_BACKEND_INVALID_MODULE);
      }
    }

    virtual Module::TrackState::Ptr GetTrackState() const
    {
      Locker lock(Mutex);
      return Delegate->GetTrackState();
    }

    virtual Module::Analyzer::Ptr GetAnalyzer() const
    {
      Locker lock(Mutex);
      return Delegate->GetAnalyzer();
    }

    virtual bool RenderFrame(const Sound::RenderParameters& params)
    {
      Locker lock(Mutex);
      return Delegate->RenderFrame(params);
    }

    virtual void Reset()
    {
      Locker lock(Mutex);
      return Delegate->Reset();
    }

    virtual void SetPosition(uint_t frame)
    {
      Locker lock(Mutex);
      return Delegate->SetPosition(frame);
    }
  private:
    const Module::Renderer::Ptr Delegate;
    mutable boost::mutex Mutex;
  };

  class Unlocker
  {
  public:
    explicit Unlocker(boost::mutex& mtx) : Obj(mtx)
    {
      Obj.unlock();
    }
    ~Unlocker()
    {
      Obj.lock();
    }
  private:
    boost::mutex& Obj;
  };

  class BufferRenderer : public Receiver
  {
  public:
    explicit BufferRenderer(std::vector<MultiSample>& buf) : Buffer(buf)
    {
    }

    virtual void ApplyData(const MultiSample& samp)
    {
      Buffer.push_back(samp);
    }

    void Flush()
    {
    }
  private:
    std::vector<MultiSample>& Buffer;
  };
}

namespace ZXTune
{
  namespace Sound
  {
    BackendImpl::BackendImpl(CreateBackendParameters::Ptr params)
      : CurrentMixer(params->GetMixer())
      , Holder(params->GetModule())
      , Player(new SafeRendererWrapper(Holder->CreateRenderer(CurrentMixer)))
      , SoundParameters(params->GetParameters())
      , RenderingParameters(RenderParameters::Create(SoundParameters))
      , Signaller(Async::Signals::Dispatcher::Create())
      , SyncBarrier(TOTAL_WORKING_THREADS)
      , CurrentState(Backend::STOPPED), InProcess(false)
      , Renderer(new BufferRenderer(Buffer))
    {
      if (Converter::Ptr filter = params->GetFilter())
      {
        filter->SetTarget(Renderer);
        CurrentMixer->SetTarget(filter);
      }
      else
      {
        CurrentMixer->SetTarget(Renderer);
      }
    }

    BackendImpl::~BackendImpl()
    {
      assert(Backend::STOPPED == CurrentState);
    }

    Module::Information::Ptr BackendImpl::GetModuleInformation() const
    {
      return Holder->GetModuleInformation();
    }

    Module::TrackState::Ptr BackendImpl::GetTrackState() const
    {
      return Player->GetTrackState();
    }

    Module::Analyzer::Ptr BackendImpl::GetAnalyzer() const
    {
      return Player->GetAnalyzer();
    }

    Error BackendImpl::Play()
    {
      try
      {
        Locker lock(PlayerMutex);

        const Backend::State prevState = CurrentState;
        if (Backend::STOPPED == prevState)
        {
          Log::Debug(THIS_MODULE, "Starting playback");
          Player->Reset();
          RenderThread = boost::thread(std::mem_fun(&BackendImpl::RenderFunc), this);
          SyncBarrier.wait();//wait until real start
          if (Backend::STARTED != CurrentState)
          {
            ThrowIfError(RenderError);
          }
          Log::Debug(THIS_MODULE, "Started");
        }
        else if (Backend::PAUSED == prevState)
        {
          Log::Debug(THIS_MODULE, "Resuming playback");
          boost::unique_lock<boost::mutex> locker(PauseMutex);
          PauseEvent.notify_one();
          //wait until really resumed
          PauseEvent.wait(locker);
          Log::Debug(THIS_MODULE, "Resumed");
        }
        return Error();
      }
      catch (const Error& e)
      {
        return Error(THIS_LINE, BACKEND_CONTROL_ERROR, Text::SOUND_ERROR_BACKEND_PLAYBACK).AddSuberror(e);
      }
      catch (const boost::thread_resource_error&)
      {
        return Error(THIS_LINE, BACKEND_NO_MEMORY, Text::SOUND_ERROR_BACKEND_NO_MEMORY);
      }
    }

    Error BackendImpl::Pause()
    {
      try
      {
        Locker lock(PlayerMutex);
        if (Backend::STARTED == CurrentState)
        {
          Log::Debug(THIS_MODULE, "Pausing playback");
          boost::unique_lock<boost::mutex> locker(PauseMutex);
          CurrentState = Backend::PAUSED;
          //wait until really Backend::PAUSED
          PauseEvent.wait(locker);
          Log::Debug(THIS_MODULE, "Paused");
        }
        return Error();
      }
      catch (const Error& e)
      {
        return Error(THIS_LINE, BACKEND_CONTROL_ERROR, Text::SOUND_ERROR_BACKEND_PAUSE).AddSuberror(e);
      }
    }

    Error BackendImpl::Stop()
    {
      try
      {
        Locker lock(PlayerMutex);
        StopPlayback();
        return Error();
      }
      catch (const Error& e)
      {
        return Error(THIS_LINE, BACKEND_CONTROL_ERROR, Text::SOUND_ERROR_BACKEND_STOP).AddSuberror(e);
      }
    }

    Error BackendImpl::SetPosition(uint_t frame)
    {
      try
      {
        Locker lock(PlayerMutex);
        if (Backend::STOPPED == CurrentState)
        {
          return Error();
        }
        Player->SetPosition(frame);
        SendSignal(Backend::MODULE_SEEK);
        return Error();
      }
      catch (const Error& e)
      {
        return Error(THIS_LINE, BACKEND_CONTROL_ERROR, Text::SOUND_ERROR_BACKEND_SEEK).AddSuberror(e);
      }
    }

    Backend::State BackendImpl::GetCurrentState() const
    {
      return CurrentState;
    }

    Async::Signals::Collector::Ptr BackendImpl::CreateSignalsCollector(uint_t signalsMask) const
    {
      return Signaller->CreateCollector(signalsMask);
    }

    //internal functions
    void BackendImpl::DoStartup()
    {
      OnStartup();
      SendSignal(Backend::MODULE_START);
    }

    void BackendImpl::DoShutdown()
    {
      OnShutdown();
      SendSignal(Backend::MODULE_STOP);
    }

    void BackendImpl::DoPause()
    {
      OnPause();
      SendSignal(Backend::MODULE_PAUSE);
    }

    void BackendImpl::DoResume()
    {
      OnResume();
      SendSignal(Backend::MODULE_RESUME);
    }

    void BackendImpl::StopPlayback()
    {
      const Backend::State curState = CurrentState;
      if (Backend::STARTED == curState ||
          Backend::PAUSED == curState ||
          (Backend::STOPPED == curState && InProcess))
      {
        Log::Debug(THIS_MODULE, "Stopping playback");
        if (Backend::PAUSED == curState)
        {
          boost::unique_lock<boost::mutex> locker(PauseMutex);
          PauseEvent.notify_one();
          //wait until really resumed
          PauseEvent.wait(locker);
        }
        CurrentState = Backend::STOPPED;
        InProcess = true;//stopping now
        {
          assert(!PlayerMutex.try_lock());
          Unlocker unlock(PlayerMutex);
          SyncBarrier.wait();//wait for thread stop
          RenderThread.join();//cleanup thread
        }
        Log::Debug(THIS_MODULE, "Stopped");
        ThrowIfError(RenderError);
      }
    }

    bool BackendImpl::RenderFrame()
    {
      bool res = false;
      OnFrame();
      {
        Locker lock(PlayerMutex);
        Buffer.reserve(RenderingParameters->SamplesPerFrame());
        Buffer.clear();
        res = Player->RenderFrame(*RenderingParameters);
        if (!res)
        {
          CurrentMixer->Flush();
        }
      }
      OnBufferReady(Buffer);
      return res;
    }

    void BackendImpl::RenderFunc()
    {
      Log::Debug(THIS_MODULE, "Started playback thread");
      try
      {
        CurrentState = Backend::STARTED;
        InProcess = true;//starting begin
        DoStartup();//throw
        SyncBarrier.wait();
        InProcess = false;//starting finished
        for (;;)
        {
          const Backend::State curState = CurrentState;
          if (Backend::STOPPED == curState)
          {
            break;
          }
          else if (Backend::STARTED == curState)
          {
            if (!RenderFrame())
            {
              CurrentState = Backend::STOPPED;
              InProcess = true; //stopping begin
              SendSignal(Backend::MODULE_FINISH);
              break;
            }
          }
          else if (Backend::PAUSED == curState)
          {
            boost::unique_lock<boost::mutex> locker(PauseMutex);
            DoPause();
            //pausing finished
            PauseEvent.notify_one();
            //wait until unpause
            PauseEvent.wait(locker);
            DoResume();
            CurrentState = Backend::STARTED;
            //unpausing finished
            PauseEvent.notify_one();
          }
        }
        Log::Debug(THIS_MODULE, "Stopping playback thread");
        DoShutdown();//throw
        SyncBarrier.wait();
        InProcess = false; //stopping finished
      }
      catch (const Error& e)
      {
        RenderError = e;
        //if any...
        PauseEvent.notify_all();
        Log::Debug(THIS_MODULE, "Stopping playback thread by error");
        CurrentState = Backend::STOPPED;
        SendSignal(Backend::MODULE_STOP);
        SyncBarrier.wait();
        InProcess = false;
      }
      Log::Debug(THIS_MODULE, "Stopped playback thread");
    }

    void BackendImpl::SendSignal(uint_t sig)
    {
      Signaller->Notify(sig);
    }
  }
}

//new interface
namespace
{
  using namespace ZXTune;
  using namespace Sound;

  class MixerWithFilter : public Mixer
  {
  public:
    MixerWithFilter(Mixer::Ptr mixer, Converter::Ptr filter)
      : Delegate(mixer)
      , Filter(filter)
    {
      Delegate->SetTarget(Filter);
    }
    
    virtual void ApplyData(const Mixer::InDataType& data)
    {
      Delegate->ApplyData(data);
    }

    virtual void Flush()
    {
      Delegate->Flush();
    }

    virtual void SetTarget(DataReceiver<Mixer::OutDataType>::Ptr target)
    {
      Filter->SetTarget(target);
    }

    virtual Error SetMatrix(const std::vector<MultiGain>& data)
    {
      return Delegate->SetMatrix(data);
    }
  private:
    const Mixer::Ptr Delegate;
    const Converter::Ptr Filter;
  };

  Mixer::Ptr CreateMixer(const CreateBackendParameters& params)
  {
    const Mixer::Ptr mixer = params.GetMixer();
    const Converter::Ptr filter = params.GetFilter();
    return filter ? boost::make_shared<MixerWithFilter>(mixer, filter) : mixer;
  }

  class Renderer
  {
    Renderer(RenderParameters::Ptr renderParams, Module::Renderer::Ptr renderer, Mixer::Ptr mixer)
      : RenderingParameters(renderParams)
      , Source(renderer)
      , Mix(mixer)
    {
      const Receiver::Ptr target(new BufferRenderer(Buffer));
      Mix->SetTarget(target);
    }
  public:
    typedef boost::shared_ptr<Renderer> Ptr;

    static Ptr Create(const CreateBackendParameters& params, Module::Renderer::Ptr renderer, Mixer::Ptr mixer)
    {
      const RenderParameters::Ptr renderParams = RenderParameters::Create(params.GetParameters());
      return Renderer::Ptr(new Renderer(renderParams, renderer, mixer));
    }

    bool ApplyFrame()
    {
      Buffer.reserve(RenderingParameters->SamplesPerFrame());
      Buffer.clear();
      const bool res = Source->RenderFrame(*RenderingParameters);
      if (!res)
      {
        Mix->Flush();
      }
      return res;
    }

    std::vector<MultiSample>& GetBuffer()
    {
      return Buffer;
    }
  private:
    const RenderParameters::Ptr RenderingParameters;
    const Module::Renderer::Ptr Source;
    const Mixer::Ptr Mix;
    std::vector<MultiSample> Buffer;
  };

  class AsyncWrapper : public Async::Job::Worker
  {
  public:
    AsyncWrapper(BackendWorker::Ptr worker, Async::Signals::Dispatcher& signaller, Renderer::Ptr render)
      : Delegate(worker)
      , Signaller(signaller)
      , Render(render)
      , Playing(false)
    {
    }

    virtual Error Initialize()
    {
      try
      {
        Delegate->OnStartup();
        Playing = true;
        Signaller.Notify(Backend::MODULE_START);
        return Error();
      }
      catch (const Error& e)
      {
        return Error(THIS_LINE, BACKEND_CONTROL_ERROR, Text::SOUND_ERROR_BACKEND_PLAYBACK).AddSuberror(e);
      }
    }

    virtual Error Finalize()
    {
      try
      {
        Playing = false;
        Signaller.Notify(Backend::MODULE_STOP);
        Delegate->OnShutdown();
        return Error();
      }
      catch (const Error& e)
      {
        return Error(THIS_LINE, BACKEND_CONTROL_ERROR, Text::SOUND_ERROR_BACKEND_STOP).AddSuberror(e);
      }
    }

    virtual Error Suspend()
    {
      try
      {
        Delegate->OnPause();
        Signaller.Notify(Backend::MODULE_PAUSE);
        return Error();
      }
      catch (const Error& e)
      {
        return Error(THIS_LINE, BACKEND_CONTROL_ERROR, Text::SOUND_ERROR_BACKEND_PAUSE).AddSuberror(e);
      }
    }

    virtual Error Resume() 
    {
      try
      {
        Delegate->OnResume();
        Signaller.Notify(Backend::MODULE_RESUME);
        return Error();
      }
      catch (const Error& e)
      {
        return Error(THIS_LINE, BACKEND_CONTROL_ERROR, Text::SOUND_ERROR_BACKEND_PLAYBACK).AddSuberror(e);
      }
    }

    virtual Error ExecuteCycle()
    {
      try
      {
        Delegate->OnFrame();
        Playing = Render->ApplyFrame();
        Delegate->OnBufferReady(Render->GetBuffer());
        if (IsFinished())
        {
          Signaller.Notify(Backend::MODULE_FINISH);
        }
        return Error();
      }
      catch (const Error& err)
      {
        return err;
      }
    }

    virtual bool IsFinished() const
    {
      return !Playing;
    }
  private:
    const BackendWorker::Ptr Delegate;
    Async::Signals::Dispatcher& Signaller;
    const Renderer::Ptr Render;
    bool Playing;
  };

  class BackendInternal : public Backend
  {
  public:
    BackendInternal(CreateBackendParameters::Ptr params, BackendWorker::Ptr worker)
      : Worker(worker)
      , Signaller(Async::Signals::Dispatcher::Create())
      , Mix(CreateMixer(*params))
      , Holder(params->GetModule())
      , Renderer(new SafeRendererWrapper(Holder->CreateRenderer(Mix)))
      , Job(Async::Job::Create(Async::Job::Worker::Ptr(new AsyncWrapper(Worker, *Signaller, Renderer::Create(*params, Renderer, Mix)))))
    {
    }

    virtual Module::Information::Ptr GetModuleInformation() const
    {
      return Holder->GetModuleInformation();
    }

    virtual Module::TrackState::Ptr GetTrackState() const
    {
      return Renderer->GetTrackState();
    }

    virtual Module::Analyzer::Ptr GetAnalyzer() const
    {
      return Renderer->GetAnalyzer();
    }

    virtual Error Play()
    {
      return Job->Start();
    }

    virtual Error Pause()
    {
      return Job->Pause();
    }

    virtual Error Stop()
    {
      return Job->Stop();
    }

    virtual Error SetPosition(uint_t frame)
    {
      try
      {
        Renderer->SetPosition(frame);
        Signaller->Notify(Backend::MODULE_SEEK);
        return Error();
      }
      catch (const Error& e)
      {
        return Error(THIS_LINE, BACKEND_CONTROL_ERROR, Text::SOUND_ERROR_BACKEND_SEEK).AddSuberror(e);
      }
    }

    virtual State GetCurrentState() const
    {
      return Job->IsActive()
        ? (Job->IsPaused() ? Backend::PAUSED : Backend::STARTED)
        : Backend::STOPPED;
    }

    virtual Async::Signals::Collector::Ptr CreateSignalsCollector(uint_t signalsMask) const
    {
      return Signaller->CreateCollector(signalsMask);
    }

    virtual VolumeControl::Ptr GetVolumeControl() const
    {
      return Worker->GetVolumeControl();
    }
  private:
    const BackendWorker::Ptr Worker;
    const Async::Signals::Dispatcher::Ptr Signaller;
    const Mixer::Ptr Mix;
    const Module::Holder::Ptr Holder;
    const Module::Renderer::Ptr Renderer;
    const Async::Job::Ptr Job;
  };
}

namespace ZXTune
{
  namespace Sound
  {
    Backend::Ptr CreateBackend(CreateBackendParameters::Ptr params, BackendWorker::Ptr worker)
    {
      return boost::make_shared<BackendInternal>(params, worker);
    }
  }
}
