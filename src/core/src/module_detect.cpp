/*
Abstract:
  Module detection in container

Last changed:
  $Id$

Author:
  (C) Vitamin/CAIG/2001
*/

//local includes
#include "callback.h"
#include "core.h"
#include "core/plugins/plugins_types.h"
#include "core/plugins/players/ay/ay_base.h"
//common includes
#include <error.h>
//library includes
#include <debug/log.h>
//boost includes
#include <boost/make_shared.hpp>
//text includes
#include <src/core/text/plugins.h>

namespace
{
  using namespace ZXTune;

  const Debug::Stream Dbg("Core::Detection");

  const String ARCHIVE_PLUGIN_PREFIX(Text::ARCHIVE_PLUGIN_PREFIX);

  String EncodeArchivePluginToPath(const String& pluginId)
  {
    return ARCHIVE_PLUGIN_PREFIX + pluginId;
  }

  class OpenModuleCallback : public Module::DetectCallback
  {
  public:
    virtual Parameters::Accessor::Ptr GetPluginsParameters() const
    {
      return Parameters::Container::Create();
    }

    virtual void ProcessModule(DataLocation::Ptr /*location*/, Module::Holder::Ptr holder) const
    {
      Result = holder;
    }

    virtual Log::ProgressCallback* GetProgress() const
    {
      return 0;
    }

    Module::Holder::Ptr GetResult() const
    {
      return Result;
    }
  private:
    mutable Module::Holder::Ptr Result;
  };
  
  template<class T>
  std::size_t DetectByPlugins(typename T::Iterator::Ptr plugins, DataLocation::Ptr location, const Module::DetectCallback& callback)
  {
    for (; plugins->IsValid(); plugins->Next())
    {
      const typename T::Ptr plugin = plugins->Get();
      const Analysis::Result::Ptr result = plugin->Detect(location, callback);
      if (std::size_t usedSize = result->GetMatchedDataSize())
      {
        Dbg("Detected %1% in %2% bytes at %3%.", plugin->GetDescription()->Id(), usedSize, location->GetPath()->AsString());
        return usedSize;
      }
    }
    return 0;
  }

  //TODO: remove
  class MixedPropertiesHolder : public Module::Holder
  {
  public:
    MixedPropertiesHolder(Module::Holder::Ptr delegate, Parameters::Accessor::Ptr props)
      : Delegate(delegate)
      , Properties(props)
    {
    }

    virtual Module::Information::Ptr GetModuleInformation() const
    {
      return Delegate->GetModuleInformation();
    }

    virtual Parameters::Accessor::Ptr GetModuleProperties() const
    {
      return Parameters::CreateMergedAccessor(Properties, Delegate->GetModuleProperties());
    }

    virtual Module::Renderer::Ptr CreateRenderer(Parameters::Accessor::Ptr params, Sound::Receiver::Ptr target) const
    {
      return Delegate->CreateRenderer(Parameters::CreateMergedAccessor(params, Properties), target);
    }
  private:
    const Module::Holder::Ptr Delegate;
    const Parameters::Accessor::Ptr Properties;
  };

  class MixedPropertiesAYMHolder : public Module::AYM::Holder
  {
  public:
    MixedPropertiesAYMHolder(Module::AYM::Holder::Ptr delegate, Parameters::Accessor::Ptr props)
      : Delegate(delegate)
      , Properties(props)
    {
    }

    virtual Module::Information::Ptr GetModuleInformation() const
    {
      return Delegate->GetModuleInformation();
    }

    virtual Parameters::Accessor::Ptr GetModuleProperties() const
    {
      return Parameters::CreateMergedAccessor(Properties, Delegate->GetModuleProperties());
    }

    virtual Module::Renderer::Ptr CreateRenderer(Parameters::Accessor::Ptr params, Sound::Receiver::Ptr target) const
    {
      //???
      return Module::AYM::CreateRenderer(*this, params, target);
    }

    virtual Module::Renderer::Ptr CreateRenderer(Parameters::Accessor::Ptr params, Devices::AYM::Device::Ptr chip) const
    {
      return Delegate->CreateRenderer(Parameters::CreateMergedAccessor(params, Properties), chip);
    }
  private:
    const Module::AYM::Holder::Ptr Delegate;
    const Parameters::Accessor::Ptr Properties;
  };
}

namespace ZXTune
{
  namespace Module
  {
    Holder::Ptr Open(DataLocation::Ptr location)
    {
      const PlayerPluginsEnumerator::Ptr usedPlugins = PlayerPluginsEnumerator::Create();
      const OpenModuleCallback callback;
      DetectByPlugins<PlayerPlugin>(usedPlugins->Enumerate(), location, callback);
      return callback.GetResult();
    }

    std::size_t Detect(DataLocation::Ptr location, const DetectCallback& callback)
    {
      if (std::size_t usedSize = DetectByPlugins<ArchivePlugin>(ArchivePluginsEnumerator::Create()->Enumerate(), location, callback))
      {
        return usedSize;
      }
      return DetectByPlugins<PlayerPlugin>(PlayerPluginsEnumerator::Create()->Enumerate(), location, callback);
    }

    Holder::Ptr CreateMixedPropertiesHolder(Holder::Ptr delegate, Parameters::Accessor::Ptr props)
    {
      if (const AYM::Holder::Ptr aym = boost::dynamic_pointer_cast<const AYM::Holder>(delegate))
      {
        return boost::make_shared<MixedPropertiesAYMHolder>(aym, props);
      }
      return boost::make_shared<MixedPropertiesHolder>(delegate, props);
    }
  }
}
