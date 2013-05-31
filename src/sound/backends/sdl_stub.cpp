/*
Abstract:
  Sdl backend sub implementation

Last changed:
  $Id$

Author:
  (C) Vitamin/CAIG/2001
*/

//local includes
#include "enumerator.h"
//library includes
#include <l10n/api.h>
#include <sound/backend_attrs.h>
//text includes
#include "text/backends.h"

namespace Sound
{
  void RegisterSdlBackend(BackendsEnumerator& enumerator)
  {
    enumerator.RegisterCreator(CreateDisabledBackendStub(Text::SDL_BACKEND_ID, L10n::translate("SDL support backend"), CAP_TYPE_SYSTEM)); 
  }
}
