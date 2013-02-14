/*
Abstract:
  x86 architecture detection

Last changed:
  $Id: api.h 2089 2012-11-02 12:49:49Z vitamin.caig $

Author:
  (C) Vitamin/CAIG/2001
*/

#pragma once
#ifndef X86_H_DEFINED
#define X86_H_DEFINED

  const std::string ARCH("x86");
#if _M_IX86 == 6 || defined(__i686__)
  const std::string ARCH_VERSION("i686");
#elif _M_IX86 == 5 || defined(__i586__)
  const std::string ARCH_VERSION("i586");
#elif _M_IX86 == 4 || defined(__i486__)
  const std::string ARCH_VERSION("i486");
#elif _M_IX86 == 3 || defined(__i386__)
  const std::string ARCH_VERSION("i386");
#else
  #error "Not an x86 architecture"
#endif

#endif //X86_H_DEFINED
