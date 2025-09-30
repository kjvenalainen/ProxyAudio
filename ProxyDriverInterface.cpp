// SPDX-License-Identifier: MIT
//
// Copyright (c) 2025 Tapturtle
//
// See the LICENSE.txt file for licensing information.

#include "ProxyDriverInterface.hpp"

#ifdef __cplusplus
extern "C" {
#endif

void* ProxyDriverInterfaceCreate(CFAllocatorRef inAllocator,
                                 CFUUIDRef inRequestedTypeUUID) {
  //	This is the CFPlugIn factory function. Its job is to create the
  // implementation for the given 	type provided that the type is
  // supported. Because this driver is simple and all its 	initialization
  // is handled via static iniitalization when the bundle is loaded, all that
  // needs to be done is to return the AudioServerPlugInDriverRef that points to
  // the driver's 	interface. A more complicated driver would create any
  // base
  // line objects it needs to satisfy 	the IUnknown methods that are used to
  // discover that actual interface to talk to the driver. 	The majority of
  // the driver's initilization should be handled in the Initialize() method of
  // the driver's AudioServerPlugInDriverInterface.

#pragma unused(inAllocator)

  if (!CFEqual(inRequestedTypeUUID, kAudioServerPlugInTypeUUID)) {
    return nullptr;
  }

  return ProxyAudio::ProxyDriverInterface::GetInstance().GetDriverRef();
}

#ifdef __cplusplus
}
#endif
