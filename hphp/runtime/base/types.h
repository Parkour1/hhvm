/*
   +----------------------------------------------------------------------+
   | HipHop for PHP                                                       |
   +----------------------------------------------------------------------+
   | Copyright (c) 2010-2013 Facebook, Inc. (http://www.facebook.com)     |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.01 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | http://www.php.net/license/3_01.txt                                  |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
*/

#ifndef incl_HPHP_TYPES_H_
#define incl_HPHP_TYPES_H_

#include "hphp/util/base.h"
#include "hphp/runtime/base/datatype.h"
#include "hphp/util/thread_local.h"
#include "hphp/util/mutex.h"
#include "hphp/util/case_insensitive.h"
#include <vector>
#include "hphp/runtime/base/macros.h"
#include "hphp/runtime/base/memory/memory_manager.h"

#include <boost/static_assert.hpp>
#include <boost/intrusive_ptr.hpp>
#include <type_traits>

namespace HPHP {
///////////////////////////////////////////////////////////////////////////////
// forward declarations of all data types

/**
 * Complex data types. Note that Numeric, Primitive, PlusOperand and Sequence
 * are actually all Variant type in implementation, but we'd keep them using
 * their own names in different places, so not to lose their inferred types.
 * Should we need to take advantage of this extra type inference information
 * in the future, we will be able to.
 */
class String;
class StaticString;
class Array;
class Object;
template<typename T> class SmartObject;
class Variant;
class VarNR;
class RefData;
typedef Variant Numeric;
typedef Variant Primitive;
typedef Variant PlusOperand;
typedef Variant Sequence;

/**
 * Macros related to Variant that are needed by StringData, ObjectData,
 * and ArrayData.
 */
extern const Variant null_variant;      // uninitialized variant
extern const Variant init_null_variant; // php null
extern const VarNR null_varNR;
extern const VarNR true_varNR;
extern const VarNR false_varNR;
extern const VarNR INF_varNR;
extern const VarNR NEGINF_varNR;
extern const VarNR NAN_varNR;
extern const String null_string;
extern const Array null_array;
extern const Array empty_array;

/*
 * All TypedValue-compatible types have their reference count field at
 * the same offset in the object.
 *
 * This offset assumes there will be no padding after the initial
 * pointer member in some of these types, and that the object/array
 * vtable is implemented with a single pointer at the front of the
 * object.  All this should be true pretty much anywhere you might
 * want to use hphp (if it's not, you'll hit compile-time assertions
 * in the relevant classes and may have to fiddle with this).
 */
const size_t FAST_REFCOUNT_OFFSET = sizeof(void*);

/**
 * These are underlying data structures for the above complex data types. Since
 * we use reference counting to achieve copy-on-write and automatic object
 * lifetime, we need these data objects to store real data that's shared by
 * multiple wrapper objects.
 */
class StringData;
class ArrayData;
class ObjectData;
class ResourceData;

/**
 * Miscellaneous objects to help arrays to construct or to iterate.
 */
class ArrayIter;
class MutableArrayIter;

class FullPos;

class VariableSerializer;
class VariableUnserializer;

///////////////////////////////////////////////////////////////////////////////

/**
 * Many functions may elect to take "litstr" separately from "String" class.
 * This code specialization helps speed a lot by not instantiating a String
 * object to box an otherwise literal value. This also means, though not
 * obviously thus dangerous not to know, whenever a function takes a parameter
 * with type of "litstr", one can only pass in a literal string that has
 * a "permanent" memory address to be stored. To make this really clear, I
 * invented "litstr" as a typedef-ed name for "const char *" that expects a
 * literal string only. Therefore, throughout this entire runtime library,
 *
 *   litstr == literal string
 *   const char * == any C-string pointer
 *
 */

namespace Uns {
enum class Mode {
  Value = 0,
  Key = 1,
  ColValue = 2,
  ColKey = 3,
};
}

namespace Collection {
enum Type {
  InvalidType = 0,
  VectorType = 1,
  MapType = 2,
  StableMapType = 3,
  SetType = 4,
  PairType = 5,
  MaxNumTypes = 6
};
}

/**
 * Some of these typedefs are for platform independency, including "int64".
 * Some of them are for clarity, for example, "litstr". Some of them are purely
 * for being able to vertically align type-specialized functions so they look
 * cleaner.
 */
typedef const char * litstr; /* literal string */
typedef const String & CStrRef;
typedef const Array & CArrRef;
typedef const Object & CObjRef;
typedef const Variant & CVarRef;

typedef const class VRefParamValue    &VRefParam;
typedef const class RefResultValue    &RefResult;
typedef const class VariantStrongBind &CVarStrongBind;
typedef const class VariantWithRefBind&CVarWithRefBind;

inline CVarStrongBind
strongBind(CVarRef v)     { return *(VariantStrongBind*)&v; }
inline CVarStrongBind
strongBind(RefResult v)   { return *(VariantStrongBind*)&v; }
inline CVarWithRefBind
withRefBind(CVarRef v)    { return *(VariantWithRefBind*)&v; }

inline CVarRef
variant(CVarStrongBind v) { return *(Variant*)&v; }
inline CVarRef
variant(CVarWithRefBind v){ return *(Variant*)&v; }
inline CVarRef
variant(RefResult v)      { return *(Variant*)&v; }
inline CVarRef
variant(CVarRef v)        { return v; }

/**
 * ref() can be used to cause strong binding
 *
 *   a = ref(b); // strong binding: now both a and b point to the same data
 *   a = b;      // weak binding: a will copy or copy-on-write
 *
 */
inline RefResult ref(CVarRef v) {
  return *(RefResultValue*)&v;
}

inline RefResult ref(Variant& v) {
  return *(RefResultValue*)&v;
}

  class Class;

///////////////////////////////////////////////////////////////////////////////
// code injection classes

class RequestInjectionData {
public:
  static const ssize_t MemExceededFlag      = 1 << 0;
  static const ssize_t TimedOutFlag         = 1 << 1;
  static const ssize_t SignaledFlag         = 1 << 2;
  static const ssize_t EventHookFlag        = 1 << 3;
  static const ssize_t PendingExceptionFlag = 1 << 4;
  static const ssize_t InterceptFlag        = 1 << 5;
  static const ssize_t LastFlag             = InterceptFlag;

  RequestInjectionData()
    : cflagsPtr(nullptr), surprisePage(nullptr), started(0), timeoutSeconds(-1),
      m_debugger(false), m_dummySandbox(false),
      m_debuggerIntr(false), m_coverage(false),
      m_jit(false) {
  }

  inline volatile ssize_t* getConditionFlags() {
    assert(cflagsPtr);
    return cflagsPtr;
  }

  ssize_t* cflagsPtr;  // this points to the real condition flags,
                       // somewhere in the thread's targetcache
  void *surprisePage;  // beginning address of page to
                       // protect for error conditions
  Mutex surpriseLock;  // mutex controlling access to surprisePage

  time_t started;      // when a request was started
  int timeoutSeconds;  // how many seconds to timeout
 private:
  bool m_debugger;       // whether there is a DebuggerProxy attached to me
  bool m_dummySandbox;   // indicating it is from a dummy sandbox thread
  bool m_debuggerIntr;   // indicating we should force interrupt for debugger
  bool m_coverage;       // is coverage being collected
  bool m_jit;            // is the jit enabled
 public:
  bool getJit() const { return m_jit; }
  bool getDebugger() const { return m_debugger; }
  void setDebugger(bool d) {
    m_debugger = d;
    updateJit();
  }
  static uint32_t debuggerReadOnlyOffset() {
    return offsetof(RequestInjectionData, m_debugger);
  }
  bool getDebuggerIntr() const { return m_debuggerIntr; }
  void setDebuggerIntr(bool d) {
    m_debuggerIntr = d;
    updateJit();
  }
  bool getCoverage() const { return m_coverage; }
  void setCoverage(bool flag) {
    m_coverage = flag;
    updateJit();
  }
  bool getDummySandbox() const { return m_dummySandbox; }
  void setDummySandbox(bool ds) { m_dummySandbox = ds; }
  void updateJit();

  std::stack<void *> interrupts;   // CmdInterrupts this thread's handling

  void reset();

  void setMemExceededFlag();
  void setTimedOutFlag();
  void setSignaledFlag();
  void setEventHookFlag();
  void clearEventHookFlag();
  void setPendingExceptionFlag();
  void clearPendingExceptionFlag();
  void setInterceptFlag();
  void clearInterceptFlag();
  ssize_t fetchAndClearFlags();

  void onSessionInit();
};

class GlobalNameValueTableWrapper;
class ObjectAllocatorBase;
class Profiler;
class CodeCoverage;
typedef GlobalNameValueTableWrapper GlobalVariables;

int object_alloc_size_to_index(size_t);
size_t object_alloc_index_to_size(int);

// implemented in runtime/base/thread_info
typedef boost::intrusive_ptr<ArrayData> ArrayHolder;
void intrusive_ptr_add_ref(ArrayData* a);
void intrusive_ptr_release(ArrayData* a);

class ThreadInfo {
public:
  enum Executing {
    Idling,
    RuntimeFunctions,
    ExtensionFunctions,
    UserFunctions,
    NetworkIO,
  };

  static void GetExecutionSamples(std::map<Executing, int> &counts);

public:
  static DECLARE_THREAD_LOCAL_NO_CHECK(ThreadInfo, s_threadInfo);

  std::vector<ObjectAllocatorBase *> m_allocators;
  RequestInjectionData m_reqInjectionData;

  // For infinite recursion detection.  m_stacklimit is the lowest
  // address the stack can grow to.
  char *m_stacklimit;

  // Either null, or populated by initialization of ThreadInfo as an
  // approximation of the highest address of the current thread's
  // stack.
  static __thread char* t_stackbase;

  // This is the amount of "slack" in stack usage checks - if the
  // stack pointer gets within this distance from the end (minus
  // overhead), throw an infinite recursion exception.
  static const int StackSlack = 1024 * 1024;

  MemoryManager* m_mm;

  // This pointer is set by ProfilerFactory
  Profiler *m_profiler;
  CodeCoverage *m_coverage;

  Executing m_executing;

  // A C++ exception which will be thrown by the next surprise check.
  Exception* m_pendingException;

  ThreadInfo();
  ~ThreadInfo();

  void onSessionInit();
  void onSessionExit();
  void setPendingException(Exception* e);
  void clearPendingException();
  ObjectAllocatorBase* instanceSizeAllocator(size_t size) {
    int index = object_alloc_size_to_index(size);
    ASSERT_NOT_IMPLEMENTED(index != -1);
    return m_allocators[index];
  }

  ObjectAllocatorBase* instanceIdxAllocator(int index) {
    return m_allocators[index];
  }

  static bool valid(ThreadInfo* info);
};

extern void throw_infinite_recursion_exception();
extern void throw_call_non_object() ATTRIBUTE_COLD ATTRIBUTE_NORETURN;

inline void* stack_top_ptr() {
  DECLARE_STACK_POINTER(sp);
  return sp;
}

inline bool stack_in_bounds(ThreadInfo *&info) {
  return stack_top_ptr() >= info->m_stacklimit;
}

inline bool is_stack_ptr(void* p) {
  return p > stack_top_ptr() && ThreadInfo::t_stackbase >= p;
}

// The ThreadInfo pointer itself must be from the current stack frame.
inline void check_recursion(ThreadInfo *&info) {
  if (!stack_in_bounds(info)) {
    throw_infinite_recursion_exception();
  }
}

// implemented in runtime/base/builtin_functions.cpp
extern ssize_t check_request_surprise(ThreadInfo *info) ATTRIBUTE_COLD;

// implemented in runtime/ext/ext_hotprofiler.cpp
extern void begin_profiler_frame(Profiler *p, const char *symbol);
extern void end_profiler_frame(Profiler *p);

///////////////////////////////////////////////////////////////////////////////

class ExecutionProfiler {
public:
  ExecutionProfiler(ThreadInfo *info, bool builtin) : m_info(info) {
    m_executing = m_info->m_executing;
    m_info->m_executing =
      builtin ? ThreadInfo::ExtensionFunctions : ThreadInfo::UserFunctions;
  }
  explicit ExecutionProfiler(ThreadInfo::Executing executing) {
    m_info = ThreadInfo::s_threadInfo.getNoCheck();
    m_executing = m_info->m_executing;
    m_info->m_executing = executing;
  }
  ~ExecutionProfiler() {
    m_info->m_executing = m_executing;
  }
private:
  ThreadInfo *m_info;
  ThreadInfo::Executing m_executing;
};

class AccessFlags {
public:
  enum Type {
    None = 0,

    Error = 1,
    CheckExist = 2,
    Key = 4,
    NoHipHop = 8,

    Error_Key = Error | Key,
    CheckExist_Key = CheckExist | Key,
    Error_NoHipHop = Error | NoHipHop,
  };
  static Type IsKey(bool s) { return s ? Key : None; }
  static Type IsError(bool e) { return e ? Error : None; }
};

#define ACCESSPARAMS_DECL AccessFlags::Type flags = AccessFlags::None
#define ACCESSPARAMS_IMPL AccessFlags::Type flags

///////////////////////////////////////////////////////////////////////////////
}

#endif // incl_HPHP_TYPES_H_
