// SPDX-License-Identifier: MIT
//
// Copyright (c) 2025 Tapturtle
//
// See the LICENSE.txt file for licensing information.

#include "ProxyDriverInterface.hpp"

#include "Context.hpp"
#include "Utils.hpp"

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
    Log("Requested type UUID is not kAudioServerPlugInTypeUUID");

    return nullptr;
  }

  const auto driverRef =
      ProxyAudio::ProxyDriverInterface::GetInstance().GetDriverRef();

  std::shared_ptr<ProxyAudio::Context> context =
      std::make_shared<ProxyAudio::Context>();

  Log("Success [driver: %p]", driverRef);

  return driverRef;
}

#ifdef __cplusplus
}
#endif
