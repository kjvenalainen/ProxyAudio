// Copyright (c) 2026 Tap Turtle
// See LICENSE for license details.

#pragma once

#include <CoreAudio/AudioHardware.h>
#include <CoreFoundation/CFBase.h>

#include <cstdint>
#include <type_traits>

#include "CFUtils.hpp"
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
inline uint32_t GetPropertyDataSize(AudioObjectID objectID,
                                    const AudioObjectPropertyAddress& address,
                                    const SizedPtr& inputData) {
  uint32_t dataSize = 0;
  auto result = AudioObjectGetPropertyDataSize(
      objectID, &address, inputData.size, inputData.ptr, &dataSize);

  if (result != noErr) {
    throw OSStatusError(result, address);
  }

  return dataSize;
}

/*!
    @function       GetPropertyData
    @abstract       Queries an AudioObject to get the data of the given property
   and places it in the provided buffer.
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
          typename std::enable_if_t<!is_vector<Result>::value &&
                                        !std::is_same_v<Result, std::string>,
                                    bool> = true>
Result GetPropertyData(AudioObjectID objectID,
                       const AudioObjectPropertyAddress& address,
                       const SizedPtr& inputData) {
  auto size = GetPropertyDataSize(objectID, address, inputData);

  if (size != sizeof(Result)) {
    throw OSStatusError(kAudioHardwareBadPropertySizeError, address);
  }

  Result outputData;
  auto result = AudioObjectGetPropertyData(objectID, &address, inputData.size,
                                           inputData.ptr, &size, &outputData);

  if (result != noErr) {
    throw OSStatusError(result, address);
  }

  return outputData;
}

/*!
    @function       GetPropertyData
    @abstract       Queries an AudioObject to get the data of the given property
   and places it in the provided buffer.
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
          typename std::enable_if_t<is_vector<Result>::value &&
                                        !std::is_same_v<Result, std::string>,
                                    bool> = true>
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
    throw OSStatusError(result, address);
  }

  if (size != count * sizeof(typename Result::value_type)) {
    outputData.resize(size / sizeof(typename Result::value_type));
  }

  return outputData;
}

/*!
    @function       GetPropertyData
    @abstract       Queries an AudioObject to get the data of the given property
   and places it in the provided buffer.
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
          typename std::enable_if_t<!is_vector<Result>::value &&
                                        std::is_same_v<Result, std::string>,
                                    bool> = true>
Result GetPropertyData(AudioObjectID objectID,
                       const AudioObjectPropertyAddress& address,
                       const SizedPtr& inputData) {
  auto size = GetPropertyDataSize(objectID, address, inputData);

  if (size != sizeof(CFStringRef)) {
    throw OSStatusError(kAudioHardwareBadPropertySizeError, address);
  }

  ProxyAudio::CFAutoRef<CFStringRef> outputData;
  auto result = AudioObjectGetPropertyData(objectID, &address, inputData.size,
                                           inputData.ptr, &size, &outputData);

  if (result != noErr) {
    throw OSStatusError(result, address);
  }

  return StringFromCFStringRef(*outputData);
}

/*!
    @function       AudioObjectSetPropertyData
    @abstract       Tells an AudioObject to change the value of the given
   property using the provided data.
    @discussion     Note that the value of the property should not be
   considered changed until the HAL has called the listeners as many
   properties values are changed asynchronously.
    @param          inObjectID
                        The AudioObject to change.
    @param          inAddress
                        An AudioObjectPropertyAddress indicating which
   property is being changed.
    @param          qualifierData
                        A UInt32 indicating the size of the buffer pointed to
   by inQualifierData. Note that not all properties require qualification, in
   which case this value will be 0. A buffer of data to be used in determining
   the data of the property being queried. Note that not all properties
   require qualification, in which case this value will be NULL.
    @param          inputData
                        A buffer of data to be used to change the property's
   value.
    @result         void
    @throws         OSStatusError if the operation fails.
  */
inline void SetPropertyData(AudioObjectID objectID,
                            const AudioObjectPropertyAddress& address,
                            const SizedPtr& qualifierData,
                            const SizedPtr& inputData) {
  auto result = AudioObjectSetPropertyData(
      objectID, &address, qualifierData.size, qualifierData.ptr, inputData.size,
      inputData.ptr);

  if (result != noErr) {
    throw OSStatusError(result, address);
  }
}

}  // namespace ProxyAudio
