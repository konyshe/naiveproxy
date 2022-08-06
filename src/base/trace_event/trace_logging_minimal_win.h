// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TRACE_EVENT_TRACE_LOGGING_MINIMAL_WIN_H_
#define BASE_TRACE_EVENT_TRACE_LOGGING_MINIMAL_WIN_H_

/*
 * TraceLogging minimal dynamic provider
 *
 * TlmProvider is a simple class that implements an Event Tracing for Windows
 * (ETW) provider that generates TraceLogging events with string fields. Unlike
 * the Windows SDK's TraceLoggingProvider.h, this provider class supports
 * runtime-variable settings for event name, level, keyword, and field name.
 *
 * Note that this approach is not recommended for general use. Support for
 * runtime-variable settings is not normally needed, and it requires extra
 * buffering as compared to the approach used by TraceLoggingProvider.h. It is
 * needed in this case because we're trying to feed data from the existing call
 * sites (which use a runtime-variable function-call syntax) into ETW. If this
 * were new code, it would be better to update each call site to use a syntax
 * compatible with compile-time event settings compatible with structured
 * logging like TraceLoggingProvider.h.
 */

#include <stdint.h>
#include <windows.h>
// Evntprov.h must come after windows.h.
#include <evntprov.h>
// TODO(joel@microsoft.com) Update headers and use defined constants instead
// of magic numbers after crbug.com/1089996 is resolved.

/*
 * An instance of TlmProvider represents a logger through which data can be
 *  sent to Event Tracing for Windows (ETW). This logger generates
 * TraceLogging-encoded events (compatible with the events generated by the
 * Windows SDK's TraceLoggingProvider.h header). In most cases, a developer
 * would prefer using TraceLoggingProvider.h over TlmProvider
 * (TraceLoggingProvider.h is more efficient and more full-featured), but
 * TlmProvider allows for configuring the event parameters (event name,
 * level, keyword, field names) at runtime (TraceLoggingProvider.h requires
 * these to be set at compile time).
 *
 * Note that the Register/Unregister operations are relatively expensive, so
 * the TlmProvider instance should be a long-lived variable (i.e. global
 * variable, static variable, or field of a long-lived object), not a local
 * variable andnot a field of a short-lived object.
 *
 * Note that provider name and provider GUID are a tightly-bound pair, i.e.
 * they should each uniquely map to each other. Once a provider name and
 * provider GUID have been used together, no other GUID should be used with
 * that name and no other name should be used with that GUID. Normally this
 * goal is achieved by using a hashing algorithm to generate the GUID from
 * a hash of the name.
 *
 * Note that each event should use a non-zero level and a non-zero keyword.
 * Predefined level constants are defined in <evntrace.h>: 0=Always,
 * 1=Critical, 2=Error, 3=Warning, 4=Info, 5=Verbose (other level values can
 * be used but are not well-defined and are not generally useful). A keyword
 * is a bitmask of "category" bits, where each bit indicates whether or not
 * the event belongs in a particular category of event. The low 48 bits are
 * user-defined and the upper 16 bits are Microsoft-defined (in <winmeta.h>).
 *
 * General usage:
 *
 *     // During component initialization (main or DllMain), call Register().
 *     // Note that there is an overload of the TlmProvider constructor that
 *     // calls Register(), but it's often convenient to do this manually
 *     // (i.e. to control the timing of the call to Register).
 *     my_provider.Register(
 *         "MyCompany.MyComponentName",
 *         MyComponentGuid);
 *
 *     // To log an event with minimal code:
 *     my_provider.WriteEvent("MyEventName",
 *         TlmEventDescriptor(
 *             TRACE_LEVEL_VERBOSE, // Level defined in <evntrace.h>
 *             0x20),               // Keyword bits are user-defined.
 *         // Value must not be null for the string fields.
 *         TlmUtf8StringField("MyUtf8Field", GetValue1()),
 *         TlmMbcsStringField("MyAsciiField", GetValue2()));
 *
 *     // Note that the minimal-code example has a bit of overhead, as it
 *     // will make the calls to GetValue1(), GetValue2(), and WriteEvent()
 *     // even if nobody is listening to the event. WriteEvent() will return
       // immediately if nobody is listening, but there is still some
 *     // overhead. To minimize the overhead when nobody is listening,
 *     // add an extra IF condition:
 *     static const auto MyEventDescriptor = TlmEventDescriptor(
 *         TRACE_LEVEL_VERBOSE, // Level defined in <evntrace.h>
 *         0x20);               // Keyword bits are user-defined.
 *     if (my_provider.IsEnabled(MyEventDescriptor))
 *     {
 *         // The IF condition is primarily to prevent unnecessary
 *         // calls to GetValue1() and GetValue2().
 *         my_provider.WriteEvent("MyEventName",
 *             MyEventDescriptor,
 *             // Value must not be null for the string fields.
 *             TlmUtf8StringField("MyUtf8Field", GetValue1()),
 *             TlmMbcsStringField("MyAsciiField", GetValue2()));
 *     }
 *
 *     // During component shutdown (main or DllMain), call Unregister().
 *     // Note that the TlmProvider destructor will also call
 *     // Unregister(), butit's often convenient to do this manually
 *     // (i.e. to control the timingof the call to Unregister).
 *     my_provider.Unregister();
 */

#include "base/memory/raw_ptr.h"

class TlmProvider {
 public:
  // Initialize a provider in the unregistered state.
  // Note that WriteEvent and Unregister operations on an unregistered
  // provider are safe no-ops.
  constexpr TlmProvider() noexcept = default;

  // Initializes a provider and attempts to register it.
  // If there is an error, provider will be left unregistered.
  // Note that WriteEvent and Unregister operations on an unregistered
  // provider are safe no-ops.
  TlmProvider(const char* provider_name,
              const GUID& provider_guid,
              PENABLECALLBACK enable_callback = nullptr,
              void* enable_callback_context = nullptr) noexcept;

  // If provider is registered, unregisters provider.
  ~TlmProvider();

  // Disable copy operations.
  TlmProvider(const TlmProvider&) = delete;
  TlmProvider& operator=(const TlmProvider&) = delete;

  // Unregisters this provider.
  // Calling Unregister on an unregistered provider is a safe no-op.
  // Not thread safe - caller must ensure serialization between calls to
  // Register() and calls to Unregister().
  void Unregister() noexcept;

  // Registers this provider. Returns Win32 error code or 0 for success.
  // Error code is primarily for debugging and should generally be ignored
  // in production (failure to register means Unregister and WriteEvent are
  // safe no-ops.)
  // Calling Register on an already-registered provider is a fatal error.
  // Not thread safe - caller must ensure serialization between calls to
  // Register() and calls to Unregister().
  int32_t Register(const char* provider_name,
                   const GUID& provider_guid,
                   PENABLECALLBACK enable_callback = nullptr,
                   void* enable_callback_context = nullptr) noexcept;

  // Returns true if any active trace listeners are interested in any events
  // from this provider.
  // Equivalent to IsEnabled(0, 0).
  bool IsEnabled() const noexcept;

  // Returns true if any active trace listeners are interested in events
  // from this provider with the specified level.
  // Equivalent to IsEnabled(level, 0).
  bool IsEnabled(uint8_t level) const noexcept;

  // Returns true if any active trace listeners are interested in events
  // from this provider with the specified level and keyword.
  bool IsEnabled(uint8_t level, uint64_t keyword) const noexcept;

  // Returns true if any active trace listeners are interested in events
  // from this provider with the specified level and keyword.
  // Equivalent to IsEnabled(event_descriptor.level, event_descriptor.keyword).
  bool IsEnabled(const EVENT_DESCRIPTOR& event_descriptor) const noexcept;

  // If any active trace listeners are interested in events from this provider
  // with the specified level and keyword, packs the data into an event and
  // sends it to ETW. Returns Win32 error code or 0 for success.
  template <class... FieldTys>
  int32_t WriteEvent(const char* event_name,
                     const EVENT_DESCRIPTOR& event_descriptor,
                     const FieldTys&... event_fields) const noexcept {
    if (!IsEnabled(event_descriptor)) {
      // If nobody is listening, report success.
      return 0;
    }
    // Pack the event metadata.
    char metadata[kMaxEventMetadataSize];
    uint16_t metadata_index;
    metadata_index = EventBegin(metadata, event_name);
    {  // scope for dummy array (simulates a C++17 comma-fold expression)
      char dummy[sizeof...(FieldTys) == 0 ? 1 : sizeof...(FieldTys)] = {
          EventAddField(metadata, &metadata_index, event_fields.in_type_,
                        event_fields.out_type_, event_fields.Name())...};
      DCHECK(dummy);
    }

    // Pack the event data.
    constexpr uint8_t kDescriptorsCount =
        2 + DataDescCountSum<FieldTys...>::value;
    EVENT_DATA_DESCRIPTOR descriptors[kDescriptorsCount];
    uint8_t descriptors_index = 2;
    {  // scope for dummy array (simulates a C++17 comma-fold expression)
      char dummy[sizeof...(FieldTys) == 0 ? 1 : sizeof...(FieldTys)] = {
          EventDescriptorFill(descriptors, &descriptors_index,
                              event_fields)...};
      DCHECK(dummy);
    }

    // Finalize event and call EventWrite.
    return EventEnd(metadata, metadata_index, descriptors, descriptors_index,
                    event_descriptor);
  }

 private:
  // Size of the buffer used for provider metadata (field within the
  // TlmProvider object). Provider metadata consists of the nul-terminated
  // provider name plus a few sizes and flags, so this buffer needs to be
  // just a few bytes larger than the largest expected provider name.
  static constexpr uint16_t kMaxProviderMetadataSize = 128;

  // Size of the buffer used for event metadata (stack-allocated in the
  // WriteEvent method). Event metadata consists of nul-terminated event
  // name, nul-terminated field names, field types (1 or 2 bytes per field),
  // and a few bytes for sizes and flags.
  static constexpr uint16_t kMaxEventMetadataSize = 256;

  template <class... FieldTys>
  struct DataDescCountSum;  // undefined

  template <>
  struct DataDescCountSum<> {
    static constexpr uint8_t value = 0;
  };

  template <class FieldTy1, class... FieldTyRest>
  struct DataDescCountSum<FieldTy1, FieldTyRest...> {
    static constexpr uint8_t value =
        FieldTy1::data_desc_count_ + DataDescCountSum<FieldTyRest...>::value;
  };

  template <class FieldTy>
  static char EventDescriptorFill(EVENT_DATA_DESCRIPTOR* descriptors,
                                  uint8_t* pdescriptors_index,
                                  const FieldTy& event_field) noexcept {
    event_field.FillEventDescriptor(&descriptors[*pdescriptors_index]);
    *pdescriptors_index += FieldTy::data_desc_count_;
    return 0;
  }

  // This is called from the OS, so use the required call type.
  static void __stdcall StaticEnableCallback(
      const GUID* source_id,
      ULONG is_enabled,
      UCHAR level,
      ULONGLONG match_any_keyword,
      ULONGLONG match_all_keyword,
      PEVENT_FILTER_DESCRIPTOR FilterData,
      PVOID callback_context);

  // Returns initial value of metadata_index.
  uint16_t EventBegin(char* metadata, const char* event_name) const noexcept;

  char EventAddField(char* metadata,
                     uint16_t* metadata_index,
                     uint8_t in_type,
                     uint8_t out_type,
                     const char* field_name) const noexcept;

  // Returns Win32 error code, or 0 for success.
  int32_t EventEnd(char* metadata,
                   uint16_t metadata_index,
                   EVENT_DATA_DESCRIPTOR* descriptors,
                   uint32_t descriptors_index,
                   const EVENT_DESCRIPTOR& event_descriptor) const noexcept;

  bool KeywordEnabled(uint64_t keyword) const noexcept;

  uint16_t AppendNameToMetadata(char* metadata,
                                uint16_t metadata_size,
                                uint16_t metadata_index,
                                const char* name) const noexcept;

  uint32_t level_plus1_ = 0;
  uint32_t provider_metadata_size_ = 0;
  uint64_t keyword_any_ = 0;
  uint64_t keyword_all_ = 0;
  uint64_t reg_handle_ = 0;
  PENABLECALLBACK enable_callback_ = nullptr;
  raw_ptr<void> enable_callback_context_ = nullptr;
  char provider_metadata_[kMaxProviderMetadataSize] = {};
};

// Base class for field types.
template <uint8_t data_desc_count,
          uint8_t in_type,
          uint8_t out_type = 0>  // Default out_type is TlgOutNULL
class TlmFieldBase {
 public:
  constexpr const char* Name() const noexcept { return name_; }

 protected:
  explicit constexpr TlmFieldBase(const char* name) noexcept : name_(name) {}

 private:
  friend class TlmProvider;

  static constexpr uint8_t data_desc_count_ = data_desc_count;
  static constexpr uint8_t in_type_ = in_type;
  static constexpr uint8_t out_type_ = out_type;

  const char* name_;
};

// Class that represents an event field containing nul-terminated MBCS data.
class TlmMbcsStringField
    : public TlmFieldBase<1, 2>  // 1 data descriptor, Type = TlgInANSISTRING
{
 public:
  // name is a utf-8 nul-terminated string.
  // value is MBCS nul-terminated string (assumed to be in system's default code
  // page).
  TlmMbcsStringField(const char* name, const char* value) noexcept;

  const char* Value() const noexcept;

  void FillEventDescriptor(EVENT_DATA_DESCRIPTOR* descriptors) const noexcept;

 private:
  const char* value_;
};

// Class that represents an event field containing nul-terminated UTF-8 data.
class TlmUtf8StringField
    : public TlmFieldBase<1, 2, 35>  // 1 data descriptor, Type =
                                     // TlgInANSISTRING + TlgOutUTF8
{
 public:
  // name and value are utf-8 nul-terminated strings.
  TlmUtf8StringField(const char* name, const char* value) noexcept;

  const char* Value() const noexcept;

  void FillEventDescriptor(EVENT_DATA_DESCRIPTOR* descriptors) const noexcept;

 private:
  const char* value_;
};

// Helper for creating event descriptors for use with WriteEvent.
constexpr EVENT_DESCRIPTOR TlmEventDescriptor(uint8_t level,
                                              uint64_t keyword) noexcept {
  return {
      // Id
      // TraceLogging generally uses the event's Name instead of Id+Version,
      // so Id is normally set to 0 for TraceLogging events.
      0,

      // Version
      // TraceLogging generally uses the event's Name instead of Id+Version,
      // so Version is normally set to 0 for TraceLogging events.
      0,

      // Channel (WINEVENT_CHANNEL_*)
      // Setting Channel = 11 allows TraceLogging events to be decoded
      // correctly even if they were collected on older operating systems.
      // If a TraceLogging event sets channel to a value other than 11, the
      // event will only decode correctly if it was collected on an
      // operating system that has built-in TraceLogging support, i.e.
      // Windows 7sp1 + patch, Windows 8.1 + patch, or Windows 10+.
      11,  // = WINEVENT_CHANNEL_TRACELOGGING

      // Level (WINEVENT_LEVEL_*)
      // 0=always, 1=fatal, 2=error, 3=warning, 4=info, 5=verbose.
      // Levels higher than 5 are for user-defined debug levels.
      level,

      // Opcode (WINEVENT_OPCODE_*)
      // Set Opcode for special semantics such as starting/ending an
      // activity.
      0,  // = WINEVENT_OPCODE_INFO

      // Task
      // Set Task for user-defined semantics.
      0,  // = WINEVENT_TASK_NONE

      // Keyword
      // A keyword is a 64-bit value used for filtering events. Each bit of
      // the keyword indicates whether the event belongs to a particular
      // category of events. The top 16 bits of keyword have
      // Microsoft-defined semantics and should be set to 0. The low 48 bits
      // of keyword have user-defined semantics. All events should use a
      // nonzero keyword to support effective event filtering (events with
      // keyword set to 0 always pass keyword filtering).
      keyword};
}

#endif  // BASE_TRACE_EVENT_TRACE_LOGGING_MINIMAL_WIN_H_
