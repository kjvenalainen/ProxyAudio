// Copyright (c) 2026 Tap Turtle
// See LICENSE for license details.

#pragma once

#include <CoreAudio/AudioHardware.h>

#include <cstdint>
#include <type_traits>

#include "Error.hpp"
#include "TypeTraits.hpp"

namespace ProxyAudio {

// A pointer and size pair, often used to pass data to and from CoreAudio
// functions.
struct SizedPtr {
  void* ptr = nullptr;
  uint32_t size = 0;
};

/*!
    @function       GetPropertyDataSize
    @abstract       Queries an AudioObject to find the size of the data for the
   given property.
    @param          inObjectID
                        The AudioObject to query.
    @param          inAddress
                        An AudioObjectPropertyAddress indicating which property
   is being queried.
    @param          inputData
                        A UInt32 indicating the size of the buffer pointed to by
   inQualifierData. Note that not all properties require qualification, in which
   case this value will be 0. A buffer of data to be used in determining the
   data of the property being queried. Note that not all properties require
   qualification, in which case this value will be NULL.
    @result         A UInt32 indicating how many bytes the data for the
   given property occupies.
    @throws         OSStatusError if the operation fails.
*/
uint32_t GetPropertyDataSize(AudioObjectID objectID,
                             const AudioObjectPropertyAddress& address,
                             const SizedPtr& inputData) {
  uint32_t dataSize = 0;
  auto result = AudioObjectGetPropertyDataSize(
      objectID, &address, inputData.size, inputData.ptr, &dataSize);

  if (result != noErr) {
    throw OSStatusError(result);
  }

  return dataSize;
}

/*!
    @function       GetPropertyData
    @abstract       Queries an AudioObject to get the data of the given property
   and places it in the provided buffer. Only returns single values.
    @param          inObjectID
                        The AudioObject to query.
    @param          inAddress
                        An AudioObjectPropertyAddress indicating which property
   is being queried.
    @param          inputData
                        A buffer of data to be used in determining the data of
   the property being queried. Note that not all properties require
   qualification, in which case this value will be NULL. A UInt32 indicating
   the size of the buffer pointed to by inputData. Note that not all properties
   require qualification, in which case this value will be 0.
    @result         Result in the given type.
    @throws         OSStatusError if the operation fails.
*/
template <typename Result,
          typename std::enable_if_t<!is_vector<Result>::value, bool> = true>
Result GetPropertyData(AudioObjectID objectID,
                       const AudioObjectPropertyAddress& address,
                       const SizedPtr& inputData) {
  auto size = GetPropertyDataSize(objectID, address, inputData);

  if (size != sizeof(Result)) {
    throw OSStatusError(kAudioHardwareBadPropertySizeError);
  }

  Result outputData;
  auto result = AudioObjectGetPropertyData(objectID, &address, inputData.size,
                                           inputData.ptr, sizeof(outputData),
                                           &outputData);

  if (result != noErr) {
    throw OSStatusError(result);
  }

  return outputData;
}

/*!
    @function       GetPropertyData
    @abstract       Queries an AudioObject to get the data of the given property
   and places it in the provided buffer. Only returns std::vector values.
    @param          inObjectID
                        The AudioObject to query.
    @param          inAddress
                        An AudioObjectPropertyAddress indicating which property
   is being queried.
    @param          inputData
                        A buffer of data to be used in determining the data of
   the property being queried. Note that not all properties require
   qualification, in which case this value will be NULL. A UInt32 indicating
   the size of the buffer pointed to by inputData. Note that not all properties
   require qualification, in which case this value will be 0.
    @result         Result in the given type.
    @throws         OSStatusError if the operation fails.
*/
template <typename Result,
          typename std::enable_if_t<is_vector<Result>::value, bool> = true>
Result GetPropertyData(AudioObjectID objectID,
                       const AudioObjectPropertyAddress& address,
                       const SizedPtr& inputData) {
  auto size = GetPropertyDataSize(objectID, address, inputData);

  if (size == 0U) {
    return std::vector<typename Result::value_type>();
  }

  size_t count = size / sizeof(typename Result::value_type);
  Result outputData(count);
  auto result =
      AudioObjectGetPropertyData(objectID, &address, inputData.size,
                                 inputData.ptr, &size, outputData.data());

  if (result != noErr) {
    throw OSStatusError(result);
  }

  if (size != count * sizeof(typename Result::value_type)) {
    outputData.resize(size / sizeof(typename Result::value_type));
  }

  return outputData;
}

}  // namespace ProxyAudio
