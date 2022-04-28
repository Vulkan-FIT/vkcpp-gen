#ifndef GENERATOR_HPP
#define GENERATOR_HPP

#include "XMLUtils.hpp"
#include "XMLVariableParser.hpp"
#include "tinyxml2.h"
#include <filesystem>
#include <functional>
#include <iostream>
#include <list>
#include <map>
#include <regex>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

static constexpr char const *RES_HEADER{
    R"(
//#include <vulkan/vulkan_core.h>
//#include <vulkan/vulkan.hpp>

#if defined( _MSVC_LANG )
#  define VULKAN_HPP_CPLUSPLUS _MSVC_LANG
#else
#  define VULKAN_HPP_CPLUSPLUS __cplusplus
#endif

#if 201703L < VULKAN_HPP_CPLUSPLUS
#  define VULKAN_HPP_CPP_VERSION 20
#elif 201402L < VULKAN_HPP_CPLUSPLUS
#  define VULKAN_HPP_CPP_VERSION 17
#elif 201103L < VULKAN_HPP_CPLUSPLUS
#  define VULKAN_HPP_CPP_VERSION 14
#elif 199711L < VULKAN_HPP_CPLUSPLUS
#  define VULKAN_HPP_CPP_VERSION 11
#else
#  error "vulkan.hpp needs at least c++ standard version 11"
#endif

#include <vulkan/vulkan.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <initializer_list>
#include <sstream>
#include <string>
#include <system_error>
#include <tuple>
#include <type_traits>
#include <vector>
#if 17 <= VULKAN_HPP_CPP_VERSION
#  include <string_view>
#endif

#if defined( VULKAN_HPP_DISABLE_ENHANCED_MODE )
#  if !defined( VULKAN_HPP_NO_SMART_HANDLE )
#    define VULKAN_HPP_NO_SMART_HANDLE
#  endif
#else
#  include <memory>
#  include <vector>
#endif

#if defined( VULKAN_HPP_NO_CONSTRUCTORS )
#  if !defined( VULKAN_HPP_NO_STRUCT_CONSTRUCTORS )
#    define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#  endif
#  if !defined( VULKAN_HPP_NO_UNION_CONSTRUCTORS )
#    define VULKAN_HPP_NO_UNION_CONSTRUCTORS
#  endif
#endif

#if defined( VULKAN_HPP_NO_SETTERS )
#  if !defined( VULKAN_HPP_NO_STRUCT_SETTERS )
#    define VULKAN_HPP_NO_STRUCT_SETTERS
#  endif
#  if !defined( VULKAN_HPP_NO_UNION_SETTERS )
#    define VULKAN_HPP_NO_UNION_SETTERS
#  endif
#endif

#if !defined( VULKAN_HPP_ASSERT )
#  include <cassert>
#  define VULKAN_HPP_ASSERT assert
#endif

#if !defined( VULKAN_HPP_ASSERT_ON_RESULT )
#  define VULKAN_HPP_ASSERT_ON_RESULT VULKAN_HPP_ASSERT
#endif

#if !defined( VULKAN_HPP_STATIC_ASSERT )
#  define VULKAN_HPP_STATIC_ASSERT static_assert
#endif

#if !defined( VULKAN_HPP_ENABLE_DYNAMIC_LOADER_TOOL )
#  define VULKAN_HPP_ENABLE_DYNAMIC_LOADER_TOOL 1
#endif

#if VULKAN_HPP_ENABLE_DYNAMIC_LOADER_TOOL == 1
#  if defined( __unix__ ) || defined( __APPLE__ ) || defined( __QNXNTO__ ) || defined( __Fuchsia__ )
#    include <dlfcn.h>
#  elif defined( _WIN32 )
typedef struct HINSTANCE__ * HINSTANCE;
#    if defined( _WIN64 )
typedef int64_t( __stdcall * FARPROC )();
#    else
typedef int( __stdcall * FARPROC )();
#    endif
extern "C" __declspec( dllimport ) HINSTANCE __stdcall LoadLibraryA( char const * lpLibFileName );
extern "C" __declspec( dllimport ) int __stdcall FreeLibrary( HINSTANCE hLibModule );
extern "C" __declspec( dllimport ) FARPROC __stdcall GetProcAddress( HINSTANCE hModule, const char * lpProcName );
#  endif
#endif

#if !defined( __has_include )
#  define __has_include( x ) false
#endif

#if ( 201711 <= __cpp_impl_three_way_comparison ) && __has_include( <compare> ) && !defined( VULKAN_HPP_NO_SPACESHIP_OPERATOR )
#  define VULKAN_HPP_HAS_SPACESHIP_OPERATOR
#endif
#if defined( VULKAN_HPP_HAS_SPACESHIP_OPERATOR )
#  include <compare>
#endif

#if ( 201803 <= __cpp_lib_span )
#  define VULKAN_HPP_SUPPORT_SPAN
#  include <span>
#endif

//static_assert( VK_HEADER_VERSION == 198, "Wrong VK_HEADER_VERSION!" );

// 32-bit vulkan is not typesafe for handles, so don't allow copy constructors on this platform by default.
// To enable this feature on 32-bit platforms please define VULKAN_HPP_TYPESAFE_CONVERSION
#if ( VK_USE_64_BIT_PTR_DEFINES == 1 )
#  if !defined( VULKAN_HPP_TYPESAFE_CONVERSION )
#    define VULKAN_HPP_TYPESAFE_CONVERSION
#  endif
#endif

// <tuple> includes <sys/sysmacros.h> through some other header
// this results in major(x) being resolved to gnu_dev_major(x)
// which is an expression in a constructor initializer list.
#if defined( major )
#  undef major
#endif
#if defined( minor )
#  undef minor
#endif

// Windows defines MemoryBarrier which is deprecated and collides
// with the VULKAN_HPP_NAMESPACE::MemoryBarrier struct.
#if defined( MemoryBarrier )
#  undef MemoryBarrier
#endif

#if !defined( VULKAN_HPP_HAS_UNRESTRICTED_UNIONS )
#  if defined( __clang__ )
#    if __has_feature( cxx_unrestricted_unions )
#      define VULKAN_HPP_HAS_UNRESTRICTED_UNIONS
#    endif
#  elif defined( __GNUC__ )
#    define GCC_VERSION ( __GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__ )
#    if 40600 <= GCC_VERSION
#      define VULKAN_HPP_HAS_UNRESTRICTED_UNIONS
#    endif
#  elif defined( _MSC_VER )
#    if 1900 <= _MSC_VER
#      define VULKAN_HPP_HAS_UNRESTRICTED_UNIONS
#    endif
#  endif
#endif

#if !defined( VULKAN_HPP_INLINE )
#  if defined( __clang__ )
#    if __has_attribute( always_inline )
#      define VULKAN_HPP_INLINE __attribute__( ( always_inline ) ) __inline__
#    else
#      define VULKAN_HPP_INLINE inline
#    endif
#  elif defined( __GNUC__ )
#    define VULKAN_HPP_INLINE __attribute__( ( always_inline ) ) __inline__
#  elif defined( _MSC_VER )
#    define VULKAN_HPP_INLINE inline
#  else
#    define VULKAN_HPP_INLINE inline
#  endif
#endif

#if defined( VULKAN_HPP_TYPESAFE_CONVERSION )
#  define VULKAN_HPP_TYPESAFE_EXPLICIT
#else
#  define VULKAN_HPP_TYPESAFE_EXPLICIT explicit
#endif

#if defined( __cpp_constexpr )
#  define VULKAN_HPP_CONSTEXPR constexpr
#  if __cpp_constexpr >= 201304
#    define VULKAN_HPP_CONSTEXPR_14 constexpr
#  else
#    define VULKAN_HPP_CONSTEXPR_14
#  endif
#  define VULKAN_HPP_CONST_OR_CONSTEXPR constexpr
#else
#  define VULKAN_HPP_CONSTEXPR
#  define VULKAN_HPP_CONSTEXPR_14
#  define VULKAN_HPP_CONST_OR_CONSTEXPR const
#endif

#if !defined( VULKAN_HPP_NOEXCEPT )
#  if defined( _MSC_VER ) && ( _MSC_VER <= 1800 )
#    define VULKAN_HPP_NOEXCEPT
#  else
#    define VULKAN_HPP_NOEXCEPT     noexcept
#    define VULKAN_HPP_HAS_NOEXCEPT 1
#    if defined( VULKAN_HPP_NO_EXCEPTIONS )
#      define VULKAN_HPP_NOEXCEPT_WHEN_NO_EXCEPTIONS noexcept
#    else
#      define VULKAN_HPP_NOEXCEPT_WHEN_NO_EXCEPTIONS
#    endif
#  endif
#endif

#if 14 <= VULKAN_HPP_CPP_VERSION
#  define VULKAN_HPP_DEPRECATED( msg ) [[deprecated( msg )]]
#else
#  define VULKAN_HPP_DEPRECATED( msg )
#endif

#if ( 17 <= VULKAN_HPP_CPP_VERSION ) && !defined( VULKAN_HPP_NO_NODISCARD_WARNINGS )
#  define VULKAN_HPP_NODISCARD [[nodiscard]]
#  if defined( VULKAN_HPP_NO_EXCEPTIONS )
#    define VULKAN_HPP_NODISCARD_WHEN_NO_EXCEPTIONS [[nodiscard]]
#  else
#    define VULKAN_HPP_NODISCARD_WHEN_NO_EXCEPTIONS
#  endif
#else
#  define VULKAN_HPP_NODISCARD
#  define VULKAN_HPP_NODISCARD_WHEN_NO_EXCEPTIONS
#endif

#define VULKAN_HPP_STRINGIFY2( text ) #text
#define VULKAN_HPP_STRINGIFY( text )  VULKAN_HPP_STRINGIFY2( text )
)"};

static constexpr char const *RES_LIB_LOADER{R"(
#ifdef _WIN32
#  define LIBHANDLE HINSTANCE
#else
#  define LIBHANDLE void*
#endif

  class LibraryLoader {
  protected:

    LIBHANDLE lib;
    uint32_t m_version;
    PFN_vkGetInstanceProcAddr m_vkGetInstanceProcAddr;
    PFN_vkCreateInstance m_vkCreateInstance;

    template<typename T>
    inline T getProcAddr(const char *name) const {
      return std::bit_cast<T>(m_vkGetInstanceProcAddr(nullptr, name));
    }

  public:
#ifdef _WIN32
    static constexpr char const* defaultName = "vulkan-1.dll";
#else
    static constexpr char const* defaultName = "libvulkan.so.1";
#endif

    LibraryLoader() {
      m_vkGetInstanceProcAddr = nullptr;
    }

    LibraryLoader(const std::string &name) {
      load(name);
    }

    void load(const std::string &name = defaultName) {

#ifdef _WIN32
      lib = LoadLibraryA(name.c_str());
#else
      lib = dlopen(name.c_str(), RTLD_NOW);
#endif
      if (!lib) {
          throw std::runtime_error("Cant load library");
      }

#ifdef _WIN32
      m_vkGetInstanceProcAddr = std::bit_cast<PFN_vkGetInstanceProcAddr>(GetProcAddress(lib, "vkGetInstanceProcAddr"));
#else
      m_vkGetInstanceProcAddr = std::bit_cast<PFN_vkGetInstanceProcAddr>(dlsym(lib, "vkGetInstanceProcAddr"));
#endif
      if (!m_vkGetInstanceProcAddr) {
          throw std::runtime_error("Cant load vkGetInstanceProcAddr");
      }

      PFN_vkEnumerateInstanceVersion m_vkEnumerateInstanceVersion = getProcAddr<PFN_vkEnumerateInstanceVersion>("vkEnumerateInstanceVersion");

      if (m_vkEnumerateInstanceVersion) {
          m_vkEnumerateInstanceVersion(&m_version);
      }
      else {
          m_version = VK_API_VERSION_1_0;
      }

      m_vkCreateInstance = getProcAddr<PFN_vkCreateInstance>("vkCreateInstance");
    }

    void unload() {
      if (lib) {
#ifdef _WIN32
        FreeLibrary(lib);
#else
        dlclose(lib);
#endif
        lib = nullptr;
        m_vkGetInstanceProcAddr = nullptr;
        m_vkCreateInstance = nullptr;
      }
    }

    uint32_t version() const {
      return m_version;
    }

    ~LibraryLoader() {
      unload();
    }

    PFN_vkGetInstanceProcAddr getInstanceProcAddr() {
      if (!m_vkGetInstanceProcAddr) {
        load();
      }
      return m_vkGetInstanceProcAddr;
    }

    {0}
  };
)"};

static constexpr char const *RES_ERROR_HPP{R"(
class ErrorCategoryImpl : public std::error_category
{
public:
  virtual const char * name() const VULKAN_HPP_NOEXCEPT override
  {
    return VULKAN_HPP_NAMESPACE_STRING "::Result";
  }
  virtual std::string message( int ev ) const override
  {
    return to_string( static_cast<Result>( ev ) );
  }
};
class Error
{
public:
  Error() VULKAN_HPP_NOEXCEPT                = default;
  Error( const Error & ) VULKAN_HPP_NOEXCEPT = default;
  virtual ~Error() VULKAN_HPP_NOEXCEPT       = default;
  virtual const char * what() const VULKAN_HPP_NOEXCEPT = 0;
};
class LogicError
  : public Error
  , public std::logic_error
{
public:
  explicit LogicError( const std::string & what ) : Error(), std::logic_error( what ) {}
  explicit LogicError( char const * what ) : Error(), std::logic_error( what ) {}
  virtual const char * what() const VULKAN_HPP_NOEXCEPT
  {
    return std::logic_error::what();
  }
};
class SystemError
  : public Error
  , public std::system_error
{
public:
  SystemError( std::error_code ec ) : Error(), std::system_error( ec ) {}
  SystemError( std::error_code ec, std::string const & what ) : Error(), std::system_error( ec, what ) {}
  SystemError( std::error_code ec, char const * what ) : Error(), std::system_error( ec, what ) {}
  SystemError( int ev, std::error_category const & ecat ) : Error(), std::system_error( ev, ecat ) {}
  SystemError( int ev, std::error_category const & ecat, std::string const & what )
    : Error(), std::system_error( ev, ecat, what )
  {}
  SystemError( int ev, std::error_category const & ecat, char const * what )
    : Error(), std::system_error( ev, ecat, what )
  {}
  virtual const char * what() const VULKAN_HPP_NOEXCEPT
  {
    return std::system_error::what();
  }
};
VULKAN_HPP_INLINE const std::error_category & errorCategory() VULKAN_HPP_NOEXCEPT
{
  static ErrorCategoryImpl instance;
  return instance;
}
VULKAN_HPP_INLINE std::error_code make_error_code( Result e ) VULKAN_HPP_NOEXCEPT
{
  return std::error_code( static_cast<int>( e ), errorCategory() );
}
VULKAN_HPP_INLINE std::error_condition make_error_condition( Result e ) VULKAN_HPP_NOEXCEPT
{
  return std::error_condition( static_cast<int>( e ), errorCategory() );
})"};

static constexpr char const *RES_RESULT_VALUE_STRUCT_HPP{R"(
 template <typename T>
  struct ResultValueType
  {
#ifdef VULKAN_HPP_NO_EXCEPTIONS
    typedef ResultValue<T> type;
#else
    typedef T    type;
#endif
  };

  template <>
  struct ResultValueType<void>
  {
#ifdef VULKAN_HPP_NO_EXCEPTIONS
    typedef Result type;
#else
    typedef void type;
#endif
  };)"};

static constexpr char const *RES_RESULT_VALUE_HPP{R"(
 [[noreturn]] void throwResultException( Result result, char const * message )
    {
      switch ( result )
      {
        case Result::eErrorOutOfHostMemory: throw OutOfHostMemoryError( message );
        case Result::eErrorOutOfDeviceMemory: throw OutOfDeviceMemoryError( message );
        case Result::eErrorInitializationFailed: throw InitializationFailedError( message );
        case Result::eErrorDeviceLost: throw DeviceLostError( message );
        case Result::eErrorMemoryMapFailed: throw MemoryMapFailedError( message );
        case Result::eErrorLayerNotPresent: throw LayerNotPresentError( message );
        case Result::eErrorExtensionNotPresent: throw ExtensionNotPresentError( message );
        case Result::eErrorFeatureNotPresent: throw FeatureNotPresentError( message );
        case Result::eErrorIncompatibleDriver: throw IncompatibleDriverError( message );
        case Result::eErrorTooManyObjects: throw TooManyObjectsError( message );
        case Result::eErrorFormatNotSupported: throw FormatNotSupportedError( message );
        case Result::eErrorFragmentedPool: throw FragmentedPoolError( message );
        case Result::eErrorUnknown: throw UnknownError( message );
        case Result::eErrorOutOfPoolMemory: throw OutOfPoolMemoryError( message );
        case Result::eErrorInvalidExternalHandle: throw InvalidExternalHandleError( message );
        case Result::eErrorFragmentation: throw FragmentationError( message );
        case Result::eErrorInvalidOpaqueCaptureAddress: throw InvalidOpaqueCaptureAddressError( message );
        case Result::eErrorSurfaceLostKHR: throw SurfaceLostKHRError( message );
        case Result::eErrorNativeWindowInUseKHR: throw NativeWindowInUseKHRError( message );
        case Result::eErrorOutOfDateKHR: throw OutOfDateKHRError( message );
        case Result::eErrorIncompatibleDisplayKHR: throw IncompatibleDisplayKHRError( message );
        case Result::eErrorValidationFailedEXT: throw ValidationFailedEXTError( message );
        case Result::eErrorInvalidShaderNV: throw InvalidShaderNVError( message );
        case Result::eErrorInvalidDrmFormatModifierPlaneLayoutEXT:
          throw InvalidDrmFormatModifierPlaneLayoutEXTError( message );
        case Result::eErrorNotPermittedEXT: throw NotPermittedEXTError( message );
#  if defined( VK_USE_PLATFORM_WIN32_KHR )
        case Result::eErrorFullScreenExclusiveModeLostEXT: throw FullScreenExclusiveModeLostEXTError( message );
#  endif /*VK_USE_PLATFORM_WIN32_KHR*/
        default: throw SystemError( make_error_code( result ) );
      }
    }

  template <typename T>
  void ignore( T const & ) VULKAN_HPP_NOEXCEPT
  {}

  template <typename T>
  struct ResultValue
  {
#ifdef VULKAN_HPP_HAS_NOEXCEPT
    ResultValue( Result r, T & v ) VULKAN_HPP_NOEXCEPT( VULKAN_HPP_NOEXCEPT( T( v ) ) )
#else
    ResultValue( Result r, T & v )
#endif
      : result( r ), value( v )
    {}

#ifdef VULKAN_HPP_HAS_NOEXCEPT
    ResultValue( Result r, T && v ) VULKAN_HPP_NOEXCEPT( VULKAN_HPP_NOEXCEPT( T( std::move( v ) ) ) )
#else
    ResultValue( Result r, T && v )
#endif
      : result( r ), value( std::move( v ) )
    {}

    Result result;
    T      value;

    operator std::tuple<Result &, T &>() VULKAN_HPP_NOEXCEPT
    {
      return std::tuple<Result &, T &>( result, value );
    }

#if !defined( VULKAN_HPP_DISABLE_IMPLICIT_RESULT_VALUE_CAST )
    VULKAN_HPP_DEPRECATED(
      "Implicit-cast operators on vk::ResultValue are deprecated. Explicitly access the value as member of ResultValue." )
    operator T const &() const & VULKAN_HPP_NOEXCEPT
    {
      return value;
    }

    VULKAN_HPP_DEPRECATED(
      "Implicit-cast operators on vk::ResultValue are deprecated. Explicitly access the value as member of ResultValue." )
    operator T &() & VULKAN_HPP_NOEXCEPT
    {
      return value;
    }

    VULKAN_HPP_DEPRECATED(
      "Implicit-cast operators on vk::ResultValue are deprecated. Explicitly access the value as member of ResultValue." )
    operator T const &&() const && VULKAN_HPP_NOEXCEPT
    {
      return std::move( value );
    }

    VULKAN_HPP_DEPRECATED(
      "Implicit-cast operators on vk::ResultValue are deprecated. Explicitly access the value as member of ResultValue." )
    operator T &&() && VULKAN_HPP_NOEXCEPT
    {
      return std::move( value );
    }
#endif
  };

//#if !defined( VULKAN_HPP_NO_SMART_HANDLE )
//  template <typename Type, typename Dispatch>
//  struct ResultValue<UniqueHandle<Type, Dispatch>>
//  {
//#  ifdef VULKAN_HPP_HAS_NOEXCEPT
//    ResultValue( Result r, UniqueHandle<Type, Dispatch> && v ) VULKAN_HPP_NOEXCEPT
//#  else
//    ResultValue( Result r, UniqueHandle<Type, Dispatch> && v )
//#  endif
//      : result( r )
//      , value( std::move( v ) )
//    {}

//    std::tuple<Result, UniqueHandle<Type, Dispatch>> asTuple()
//    {
//      return std::make_tuple( result, std::move( value ) );
//    }

//#  if !defined( VULKAN_HPP_DISABLE_IMPLICIT_RESULT_VALUE_CAST )
//    VULKAN_HPP_DEPRECATED(
//      "Implicit-cast operators on vk::ResultValue are deprecated. Explicitly access the value as member of ResultValue." )
//    operator UniqueHandle<Type, Dispatch> &() & VULKAN_HPP_NOEXCEPT
//    {
//      return value;
//    }

//    VULKAN_HPP_DEPRECATED(
//      "Implicit-cast operators on vk::ResultValue are deprecated. Explicitly access the value as member of ResultValue." )
//    operator UniqueHandle<Type, Dispatch>() VULKAN_HPP_NOEXCEPT
//    {
//      return std::move( value );
//    }
//#  endif

//    Result                       result;
//    UniqueHandle<Type, Dispatch> value;
//  };

//  template <typename Type, typename Dispatch>
//  struct ResultValue<std::vector<UniqueHandle<Type, Dispatch>>>
//  {
//#  ifdef VULKAN_HPP_HAS_NOEXCEPT
//    ResultValue( Result r, std::vector<UniqueHandle<Type, Dispatch>> && v ) VULKAN_HPP_NOEXCEPT
//#  else
//    ResultValue( Result r, std::vector<UniqueHandle<Type, Dispatch>> && v )
//#  endif
//      : result( r )
//      , value( std::move( v ) )
//    {}

//    std::tuple<Result, std::vector<UniqueHandle<Type, Dispatch>>> asTuple()
//    {
//      return std::make_tuple( result, std::move( value ) );
//    }

//    Result                                    result;
//    std::vector<UniqueHandle<Type, Dispatch>> value;

//#  if !defined( VULKAN_HPP_DISABLE_IMPLICIT_RESULT_VALUE_CAST )
//    VULKAN_HPP_DEPRECATED(
//      "Implicit-cast operators on vk::ResultValue are deprecated. Explicitly access the value as member of ResultValue." )
//    operator std::tuple<Result &, std::vector<UniqueHandle<Type, Dispatch>> &>() VULKAN_HPP_NOEXCEPT
//    {
//      return std::tuple<Result &, std::vector<UniqueHandle<Type, Dispatch>> &>( result, value );
//    }
//#  endif
//  };
//#endif

  VULKAN_HPP_INLINE ResultValueType<void>::type createResultValue( Result result, char const * message )
  {
#ifdef VULKAN_HPP_NO_EXCEPTIONS
    ignore( message );
    VULKAN_HPP_ASSERT_ON_RESULT( result == Result::eSuccess );
    return result;
#else
    if ( result != Result::eSuccess )
    {
      throwResultException( result, message );
    }
#endif
  }

  template <typename T>
  VULKAN_HPP_INLINE typename ResultValueType<T>::type createResultValue( Result result, T & data, char const * message )
  {
#ifdef VULKAN_HPP_NO_EXCEPTIONS
    ignore( message );
    VULKAN_HPP_ASSERT_ON_RESULT( result == Result::eSuccess );
    return ResultValue<T>( result, std::move( data ) );
#else
    if ( result != Result::eSuccess )
    {
      throwResultException( result, message );
    }
    return std::move( data );
#endif
  }

  VULKAN_HPP_INLINE Result createResultValue( Result                        result,
                                              char const *                  message,
                                              std::initializer_list<Result> successCodes )
  {
#ifdef VULKAN_HPP_NO_EXCEPTIONS
    ignore( message );
    ignore( successCodes );  // just in case VULKAN_HPP_ASSERT_ON_RESULT is empty
    VULKAN_HPP_ASSERT_ON_RESULT( std::find( successCodes.begin(), successCodes.end(), result ) != successCodes.end() );
#else
    if ( std::find( successCodes.begin(), successCodes.end(), result ) == successCodes.end() )
    {
      throwResultException( result, message );
    }
#endif
    return result;
  }

  template <typename T>
  VULKAN_HPP_INLINE ResultValue<T>
    createResultValue( Result result, T & data, char const * message, std::initializer_list<Result> successCodes )
  {
#ifdef VULKAN_HPP_NO_EXCEPTIONS
    ignore( message );
    ignore( successCodes );  // just in case VULKAN_HPP_ASSERT_ON_RESULT is empty
    VULKAN_HPP_ASSERT_ON_RESULT( std::find( successCodes.begin(), successCodes.end(), result ) != successCodes.end() );
#else
    if ( std::find( successCodes.begin(), successCodes.end(), result ) == successCodes.end() )
    {
      throwResultException( result, message );
    }
#endif
    return ResultValue<T>( result, std::move( data ) );
  }

//#ifndef VULKAN_HPP_NO_SMART_HANDLE
//  template <typename T, typename D>
//  VULKAN_HPP_INLINE typename ResultValueType<UniqueHandle<T, D>>::type createResultValue(
//    Result result, T & data, char const * message, typename UniqueHandleTraits<T, D>::deleter const & deleter )
//  {
//#  ifdef VULKAN_HPP_NO_EXCEPTIONS
//    ignore( message );
//    VULKAN_HPP_ASSERT_ON_RESULT( result == Result::eSuccess );
//    return ResultValue<UniqueHandle<T, D>>( result, UniqueHandle<T, D>( data, deleter ) );
//#  else
//    if ( result != Result::eSuccess )
//    {
//      throwResultException( result, message );
//    }
//    return UniqueHandle<T, D>( data, deleter );
//#  endif
//  }

//  template <typename T, typename D>
//  VULKAN_HPP_INLINE ResultValue<UniqueHandle<T, D>>
//                    createResultValue( Result                                             result,
//                                       T &                                                data,
//                                       char const *                                       message,
//                                       std::initializer_list<Result>                      successCodes,
//                                       typename UniqueHandleTraits<T, D>::deleter const & deleter )
//  {
//#  ifdef VULKAN_HPP_NO_EXCEPTIONS
//    ignore( message );
//    ignore( successCodes );  // just in case VULKAN_HPP_ASSERT_ON_RESULT is empty
//    VULKAN_HPP_ASSERT_ON_RESULT( std::find( successCodes.begin(), successCodes.end(), result ) != successCodes.end() );
//#  else
//    if ( std::find( successCodes.begin(), successCodes.end(), result ) == successCodes.end() )
//    {
//      throwResultException( result, message );
//    }
//#  endif
//    return ResultValue<UniqueHandle<T, D>>( result, UniqueHandle<T, D>( data, deleter ) );
//  }

//  template <typename T, typename D>
//  VULKAN_HPP_INLINE typename ResultValueType<std::vector<UniqueHandle<T, D>>>::type
//    createResultValue( Result result, std::vector<UniqueHandle<T, D>> && data, char const * message )
//  {
//#  ifdef VULKAN_HPP_NO_EXCEPTIONS
//    ignore( message );
//    VULKAN_HPP_ASSERT_ON_RESULT( result == Result::eSuccess );
//    return ResultValue<std::vector<UniqueHandle<T, D>>>( result, std::move( data ) );
//#  else
//    if ( result != Result::eSuccess )
//    {
//      throwResultException( result, message );
//    }
//    return std::move( data );
//#  endif
//  }

//  template <typename T, typename D>
//  VULKAN_HPP_INLINE ResultValue<std::vector<UniqueHandle<T, D>>>
//                    createResultValue( Result                             result,
//                                       std::vector<UniqueHandle<T, D>> && data,
//                                       char const *                       message,
//                                       std::initializer_list<Result>      successCodes )
//  {
//#  ifdef VULKAN_HPP_NO_EXCEPTIONS
//    ignore( message );
//    ignore( successCodes );  // just in case VULKAN_HPP_ASSERT_ON_RESULT is empty
//    VULKAN_HPP_ASSERT_ON_RESULT( std::find( successCodes.begin(), successCodes.end(), result ) != successCodes.end() );
//#  else
//    if ( std::find( successCodes.begin(), successCodes.end(), result ) == successCodes.end() )
//    {
//      throwResultException( result, message );
//    }
//#  endif
//    return ResultValue<std::vector<UniqueHandle<T, D>>>( result, std::move( data ) );
//  }
//#endif
)"};

static constexpr char const *RES_ARRAY_PROXY_HPP{R"(
template <typename T>
  class ArrayProxy
  {
  public:
    VULKAN_HPP_CONSTEXPR ArrayProxy() VULKAN_HPP_NOEXCEPT
      : m_count( 0 )
      , m_ptr( nullptr )
    {}

    VULKAN_HPP_CONSTEXPR ArrayProxy( std::nullptr_t ) VULKAN_HPP_NOEXCEPT
      : m_count( 0 )
      , m_ptr( nullptr )
    {}

    ArrayProxy( T & value ) VULKAN_HPP_NOEXCEPT
      : m_count( 1 )
      , m_ptr( &value )
    {}

    template <typename B = T, typename std::enable_if<std::is_const<B>::value, int>::type = 0>
    ArrayProxy( typename std::remove_const<T>::type & value ) VULKAN_HPP_NOEXCEPT
      : m_count( 1 )
      , m_ptr( &value )
    {}

    ArrayProxy( uint32_t count, T * ptr ) VULKAN_HPP_NOEXCEPT
      : m_count( count )
      , m_ptr( ptr )
    {}

    template <typename B = T, typename std::enable_if<std::is_const<B>::value, int>::type = 0>
    ArrayProxy( uint32_t count, typename std::remove_const<T>::type * ptr ) VULKAN_HPP_NOEXCEPT
      : m_count( count )
      , m_ptr( ptr )
    {}

#  if __GNUC__ >= 9
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Winit-list-lifetime"
#  endif

    ArrayProxy( std::initializer_list<T> const & list ) VULKAN_HPP_NOEXCEPT
      : m_count( static_cast<uint32_t>( list.size() ) )
      , m_ptr( list.begin() )
    {}

    template <typename B = T, typename std::enable_if<std::is_const<B>::value, int>::type = 0>
    ArrayProxy( std::initializer_list<typename std::remove_const<T>::type> const & list ) VULKAN_HPP_NOEXCEPT
      : m_count( static_cast<uint32_t>( list.size() ) )
      , m_ptr( list.begin() )
    {}

    ArrayProxy( std::initializer_list<T> & list ) VULKAN_HPP_NOEXCEPT
      : m_count( static_cast<uint32_t>( list.size() ) )
      , m_ptr( list.begin() )
    {}

    template <typename B = T, typename std::enable_if<std::is_const<B>::value, int>::type = 0>
    ArrayProxy( std::initializer_list<typename std::remove_const<T>::type> & list ) VULKAN_HPP_NOEXCEPT
      : m_count( static_cast<uint32_t>( list.size() ) )
      , m_ptr( list.begin() )
    {}

#  if __GNUC__ >= 9
#    pragma GCC diagnostic pop
#  endif

    // Any type with a .data() return type implicitly convertible to T*, and a .size() return type implicitly
    // convertible to size_t. The const version can capture temporaries, with lifetime ending at end of statement.
    template <typename V,
              typename std::enable_if<
                std::is_convertible<decltype( std::declval<V>().data() ), T *>::value &&
                std::is_convertible<decltype( std::declval<V>().size() ), std::size_t>::value>::type * = nullptr>
    ArrayProxy( V const & v ) VULKAN_HPP_NOEXCEPT
      : m_count( static_cast<uint32_t>( v.size() ) )
      , m_ptr( v.data() )
    {}

    template <typename V,
              typename std::enable_if<
                std::is_convertible<decltype( std::declval<V>().data() ), T *>::value &&
                std::is_convertible<decltype( std::declval<V>().size() ), std::size_t>::value>::type * = nullptr>
    ArrayProxy( V & v ) VULKAN_HPP_NOEXCEPT
      : m_count( static_cast<uint32_t>( v.size() ) )
      , m_ptr( v.data() )
    {}

    const T * begin() const VULKAN_HPP_NOEXCEPT
    {
      return m_ptr;
    }

    const T * end() const VULKAN_HPP_NOEXCEPT
    {
      return m_ptr + m_count;
    }

    const T & front() const VULKAN_HPP_NOEXCEPT
    {
      VULKAN_HPP_ASSERT( m_count && m_ptr );
      return *m_ptr;
    }

    const T & back() const VULKAN_HPP_NOEXCEPT
    {
      VULKAN_HPP_ASSERT( m_count && m_ptr );
      return *( m_ptr + m_count - 1 );
    }

    bool empty() const VULKAN_HPP_NOEXCEPT
    {
      return ( m_count == 0 );
    }

    uint32_t size() const VULKAN_HPP_NOEXCEPT
    {
      return m_count;
    }

    T * data() const VULKAN_HPP_NOEXCEPT
    {
      return m_ptr;
    }

  private:
    uint32_t m_count;
    T *      m_ptr;
  };
)"};

static constexpr char const *RES_BASE_TYPES{R"(
//==================
//=== BASE TYPEs ===
//==================

using Bool32          = uint32_t;
using DeviceAddress   = uint64_t;
using DeviceSize      = uint64_t;
using RemoteAddressNV = void *;
using SampleMask      = uint32_t;)"};

static constexpr char const *RES_FLAGS_HPP{R"(
template <typename FlagBitsType>
struct FlagTraits
{
    enum
    {
        allFlags = 0
    };
};

template <typename BitType>
class Flags {
public:
    using MaskType = typename std::underlying_type<BitType>::type;

    // constructors
    VULKAN_HPP_CONSTEXPR Flags() VULKAN_HPP_NOEXCEPT : m_mask( 0 ) {}

    VULKAN_HPP_CONSTEXPR Flags( BitType bit ) VULKAN_HPP_NOEXCEPT : m_mask( static_cast<MaskType>( bit ) ) {}

    VULKAN_HPP_CONSTEXPR Flags( Flags<BitType> const & rhs ) VULKAN_HPP_NOEXCEPT = default;

    VULKAN_HPP_CONSTEXPR explicit Flags( MaskType flags ) VULKAN_HPP_NOEXCEPT : m_mask( flags ) {}

    // relational operators
#if defined( VULKAN_HPP_HAS_SPACESHIP_OPERATOR )
    auto operator<=>( Flags<BitType> const & ) const = default;
#else
    VULKAN_HPP_CONSTEXPR bool operator<( Flags<BitType> const & rhs ) const VULKAN_HPP_NOEXCEPT
    {
      return m_mask < rhs.m_mask;
    }

    VULKAN_HPP_CONSTEXPR bool operator<=( Flags<BitType> const & rhs ) const VULKAN_HPP_NOEXCEPT
    {
      return m_mask <= rhs.m_mask;
    }

    VULKAN_HPP_CONSTEXPR bool operator>( Flags<BitType> const & rhs ) const VULKAN_HPP_NOEXCEPT
    {
      return m_mask > rhs.m_mask;
    }

    VULKAN_HPP_CONSTEXPR bool operator>=( Flags<BitType> const & rhs ) const VULKAN_HPP_NOEXCEPT
    {
      return m_mask >= rhs.m_mask;
    }

    VULKAN_HPP_CONSTEXPR bool operator==( Flags<BitType> const & rhs ) const VULKAN_HPP_NOEXCEPT
    {
      return m_mask == rhs.m_mask;
    }

    VULKAN_HPP_CONSTEXPR bool operator!=( Flags<BitType> const & rhs ) const VULKAN_HPP_NOEXCEPT
    {
      return m_mask != rhs.m_mask;
    }
#endif

    // logical operator
    VULKAN_HPP_CONSTEXPR bool operator!() const VULKAN_HPP_NOEXCEPT
    {
      return !m_mask;
    }

    // bitwise operators
    VULKAN_HPP_CONSTEXPR Flags<BitType> operator&( Flags<BitType> const & rhs ) const VULKAN_HPP_NOEXCEPT
    {
      return Flags<BitType>( m_mask & rhs.m_mask );
    }

    VULKAN_HPP_CONSTEXPR Flags<BitType> operator|( Flags<BitType> const & rhs ) const VULKAN_HPP_NOEXCEPT
    {
      return Flags<BitType>( m_mask | rhs.m_mask );
    }

    VULKAN_HPP_CONSTEXPR Flags<BitType> operator^( Flags<BitType> const & rhs ) const VULKAN_HPP_NOEXCEPT
    {
      return Flags<BitType>( m_mask ^ rhs.m_mask );
    }

    VULKAN_HPP_CONSTEXPR Flags<BitType> operator~() const VULKAN_HPP_NOEXCEPT
    {
      return Flags<BitType>( m_mask ^ FlagTraits<BitType>::allFlags );
    }

    // assignment operators
    VULKAN_HPP_CONSTEXPR_14 Flags<BitType> & operator=( Flags<BitType> const & rhs ) VULKAN_HPP_NOEXCEPT = default;

    VULKAN_HPP_CONSTEXPR_14 Flags<BitType> & operator|=( Flags<BitType> const & rhs ) VULKAN_HPP_NOEXCEPT
    {
      m_mask |= rhs.m_mask;
      return *this;
    }

    VULKAN_HPP_CONSTEXPR_14 Flags<BitType> & operator&=( Flags<BitType> const & rhs ) VULKAN_HPP_NOEXCEPT
    {
      m_mask &= rhs.m_mask;
      return *this;
    }

    VULKAN_HPP_CONSTEXPR_14 Flags<BitType> & operator^=( Flags<BitType> const & rhs ) VULKAN_HPP_NOEXCEPT
    {
      m_mask ^= rhs.m_mask;
      return *this;
    }

    // cast operators
    explicit VULKAN_HPP_CONSTEXPR operator bool() const VULKAN_HPP_NOEXCEPT
    {
      return !!m_mask;
    }

    explicit VULKAN_HPP_CONSTEXPR operator MaskType() const VULKAN_HPP_NOEXCEPT
    {
      return m_mask;
    }

#if defined( VULKAN_HPP_FLAGS_MASK_TYPE_AS_PUBLIC )
public:
#else
private:
#endif
    MaskType m_mask;
};)"};

static constexpr char const *RES_OTHER_HPP{R"(
VULKAN_HPP_INLINE std::string toHexString(uint32_t value)
{
    std::stringstream stream;
    stream << std::hex << value;
    return stream.str();
}

template <typename T>
class ArrayProxy;
)"};

// static const std::string NAMESPACE{"vk20"};
static const std::string FILEPROTECT{"VULKAN_20_HPP"};

template <template <class...> class TContainer, class T, class A, class E>
static bool isInContainter(const TContainer<T, A> &array, E entry) {
    return std::find(std::begin(array), std::end(array), entry) !=
           std::end(array);
}

using namespace tinyxml2;

static std::string
regex_replace(const std::string &input, const std::regex &regex,
              std::function<std::string(std::smatch const &match)> format) {

    std::ostringstream output;
    std::sregex_iterator begin(input.begin(), input.end(), regex), end;
    for (; begin != end; begin++) {
        output << begin->prefix() << format(*begin);
    }
    output << input.substr(input.size() - begin->position());
    return output.str();
}

template <class... Args>
static std::string format(const std::string &format, Args... args) {

    std::vector<std::string> list;
    (
        [&](auto &a) {
            std::stringstream ss;
            ss << a;
            list.push_back(ss.str());
        }(args),
        ...);

    std::regex rgx("\\{[0-9]+\\}");

    std::string str = regex_replace(format, rgx, [&](auto match) {
        std::string m = match.str();
        int index = std::stoi(m.substr(1, m.size() - 1));
        if (index >= list.size()) {
            throw std::runtime_error{"format index out of range"};
        }
        return list[index];
    });

    return str;
}

class Generator {

  public:

    struct Macro {
        std::string value;
        std::string define;
        bool usesDefine;

        std::string get() const {
            return usesDefine? define : value;
        }
    };

    struct Config {
        Macro vkname;
        bool genStructNoinit;
        bool genStructProxy;
    };

    Config cfg;
    XMLDocument doc;
    XMLElement *root;
    // std::<std::string, XMLNode *> rootTable;

//    std::string genOutput;
//    std::string genOutputFuncs;

    struct PlatformData {
        std::string_view protect;
        int guiState;
    };
    using Platforms = std::map<std::string, PlatformData>;
    Platforms platforms; // maps platform name to protect (#if defined PROTECT)

    using Tags = std::unordered_set<std::string>;
    Tags tags; // list of tags from <tags>

    struct ExtensionData {
        Platforms::pointer platform;
        bool enabled;
        bool whitelisted;
    };

    using Extensions = std::map<std::string, ExtensionData>;
    Extensions extensions; // maps extension to protect as reference

    using Namemap = std::map<std::string, Extensions::reference>;
    Namemap namemap; // maps name to extension

    using GenFunction = std::function<void(XMLNode *)>;
    using OrderPair = std::pair<std::string, GenFunction>;

    using VariableArray = std::vector<std::shared_ptr<VariableData>>;

    // holds information about class member (function)
    struct ClassCommandData {
        std::string name;     // identifier
        std::string type;     // return type
        VariableArray params; // list of arguments
        std::vector<std::string> successCodes;

        bool valid() const { return !name.empty(); }
        // creates function arguments signature
        std::string createProtoArguments(const std::string &className,
                                         bool useOriginal = false) const {
            std::string out;
            for (const auto &data : params) {
                if (data->original.type() == "Vk" + className ||
                    data->getIgnoreFlag()) {
                    continue;
                }
                out +=
                    useOriginal ? data->originalToString() : data->toString();
                out += ", ";
            }
            strStripSuffix(out, ", ");
            return out;
        }

        // arguments for calling vulkan API functions
        std::string createPFNArguments(const std::string &className,
                                       const std::string &handle,
                                       bool useOriginal = false) const {
            std::string out;
            for (size_t i = 0; i < params.size(); ++i) {
                if (params[i]->getIgnorePFN()) {
                    continue;
                }
                if (params[i]->original.type() == "Vk" + className) {
                    if (params[i]->isReturn()) {
                        out += "&";
                    }
                    out += handle;
                } else {
                    out += useOriginal ? params[i]->original.identifier()
                                       : params[i]->toArgument();
                }
                if (i != params.size() - 1) {
                    out += ", ";
                }
            }
            return out;
        }
    };

    struct EnumValue {
        std::string_view name;
        const char *bitpos;
        const char *protect;
        const char *alias;
        bool supported;
    };

    struct EnumDeclaration {
        std::vector<EnumValue> extendValues;
        std::vector<std::string> aliases;
    };

    std::string output;
    std::string outputFuncs;

    std::string outputFilePath;

    std::unordered_set<std::string> generatedStructs; // helper list

    struct StructData {
        enum VkStructType { VK_STRUCT, VK_UNION } type;
        XMLNode *node;
        std::vector<std::string> aliases;
    };

    std::unordered_map<std::string, StructData> structs;

    struct FlagData {
        std::string name;
        bool hasRequire;
        bool alias;
    };

    std::unordered_map<std::string, FlagData> flags;
    std::unordered_map<std::string, EnumDeclaration> enums;
    std::unordered_map<std::string,
                       std::vector<std::pair<std::string, const char *>>>
        enumMembers;

    bool defaultWhitelistOption = true;

    void parsePlatforms(XMLNode *node);

    void parseFeature(XMLNode *node);

    void parseExtensions(XMLNode *node);

    void parseTags(XMLNode *node);

    // tries to match str in extensions, if found returns pointer to protect,
    // otherwise nullptr
    ExtensionData *findExtensionProtect(const std::string &str);

    bool idLookup(const std::string &name, std::string_view &protect);

    // #if defined encapsulation
    void withProtect(std::string &output, const char *protect,
                     std::function<void(std::string &)> function);

    void genOptional(const std::string &name, std::function<void()> function);

    std::string genOptional(const std::string &name,
                            std::function<void(std::string &)> function);

    bool isStructOrUnion(const std::string &name) {
        return structs.find(name) != structs.end();
    }

    std::string strRemoveTag(std::string &str);

    std::string strWithoutTag(const std::string &str);

    std::pair<std::string, std::string> snakeToCamelPair(std::string str);

    std::string snakeToCamel(const std::string &str);

    std::string enumConvertCamel(const std::string &enumName,
                                 std::string value);

    static std::string toCppStyle(const std::string &str,
                                  bool firstLetterCapital = false);

    void parseXML();

    std::string generateFile();

    std::vector<VariableData> parseStructMembers(XMLElement *node,
                                                 std::string &structType,
                                                 std::string &structTypeValue);

    struct GenStructTempData {
        std::string name;
        bool structOrUnion;
        XMLElement *node;
        std::vector<std::string> typeList;
    };

    std::list<GenStructTempData> genStructStack;

    void generateStructCode(std::string name, const std::string &structType,
                            const std::string &structTypeValue,
                            const std::vector<VariableData> &members);

    void generateUnionCode(std::string name,
                           const std::vector<VariableData> &members);

    void generateStruct(std::string name, const std::string &structType,
                        const std::string &structTypeValue,
                        const std::vector<VariableData> &members,
                        const std::vector<std::string> &typeList,
                        bool structOrUnion);

    void parseStruct(XMLElement *node, std::string name, bool structOrUnion);

    void parseEnumExtend(XMLElement &node, const char *protect,
                         bool flagSupported);

    void generateEnum(std::string name, XMLNode *node,
                      const std::string &bitmask);

    void genEnums(XMLNode *node);

    void genFlags();

    struct HandleData {
        std::pair<const std::string_view, HandleData> *parent;
        std::string_view alias;
        std::string_view superclass;
        std::vector<ClassCommandData> members;

        ClassCommandData getAddrCmd;
        ClassCommandData createCmd;

        //        std::string getAddrSource;
        //        ClassCommandData createMember;
        //        ClassCommandData getAddrMember;
        //        std::string getAddrObject;
    };
    using Handles = std::unordered_map<std::string_view, HandleData>;
    Handles handles;
    HandleData loader;
    std::string outputLoaderMethods;

    Handles::iterator findHandle(const std::string_view &name) {
        auto handle = handles.find(name);
        if (handle == handles.end()) {
            throw std::runtime_error(format("Handle not found: {0}", name));
        }
        return handle;
    }

    void parseTypeDeclarations(XMLNode *node);
    // static void generateClass(const std::string &name);

    void generateStructDecl(const std::string &name, const StructData &d);

    void generateClassDecl(const std::string &name);

    void genTypes();

    ClassCommandData parseClassMember(XMLElement *command) {
        ClassCommandData m;

        const char *successcodes = command->Attribute("successcodes");
        if (successcodes) {
            std::string delim = ",";
            std::string scodes = std::string(successcodes);

            size_t start = 0U;
            size_t end = scodes.find(delim);
            while (end != std::string::npos) {
                std::string code = scodes.substr(start, end - start);
                m.successCodes.push_back(code);

                start = end + delim.length();
                end = scodes.find(delim, start);
            }
        }

        // iterate contents of <command>
        std::string dbg;
        for (XMLElement &child : Elements(command)) {
            // <proto> section
            dbg += std::string(child.Value()) + "\n";
            if (std::string_view(child.Value()) == "proto") {
                // get <name> field in proto
                XMLElement *nameElement = child.FirstChildElement("name");
                if (nameElement) {
                    m.name = nameElement->GetText();
                }
                // get <type> field in proto
                XMLElement *typeElement = child.FirstChildElement("type");
                if (typeElement) {
                    m.type = typeElement->GetText();
                }
            }
            // <param> section
            else if (std::string_view(child.Value()) == "param") {
                // parse inside of param
                std::shared_ptr<XMLVariableParser> parser =
                    std::make_shared<XMLVariableParser>(&child);
                // add proto data to list
                m.params.push_back(parser);
            }
        }

        return m;
    }
    std::string generateClassMemberCStyle(const std::string &className,
                                          const std::string &handle,
                                          const std::string &protoName,
                                          const ClassCommandData &m);

    enum class PFNReturnCategory { OTHER, VOID, VK_RESULT, VK_OBJECT };

    enum class ArraySizeArgument { INVALID, COUNT, SIZE, CONST_COUNT };

    enum class MemberNameCategory {
        UNKNOWN,
        GET,
        GET_ARRAY,
        ALLOCATE,
        ALLOCATE_ARRAY,
        CREATE,
        CREATE_ARRAY,
        ENUMERATE
    };

    struct MemberContext {
        Generator &gen;
        const std::string &className;
        const std::string &handle;
        const std::string &protoName;
        const PFNReturnCategory &pfnReturn;
        ClassCommandData mdata;
    };

    bool containsCountVariable(const VariableArray &params) {
        for (const auto &v : params) {
            if (v->identifier().ends_with("Count")) {
                return true;
            }
        }
        return false;
    }

    bool containsLengthAttrib(const VariableArray &params) {
        for (const auto &v : params) {
            if (v->hasLenAttrib()) {
                return true;
            }
        }
        return false;
    }

    MemberNameCategory evalMemberNameCategory(MemberContext &ctx);

    static bool isTypePointer(const VariableData &m) {
        return strContains(m.suffix(), "*");
    }

    ArraySizeArgument evalArraySizeArgument(const VariableData &m) {
        if (m.identifier().ends_with("Count")) {
            return isTypePointer(m) ? ArraySizeArgument::COUNT
                                    : ArraySizeArgument::CONST_COUNT;
        }
        if (m.identifier().ends_with("Size")) {
            return ArraySizeArgument::SIZE;
        }
        return ArraySizeArgument::INVALID;
    }

    // static std::string mname2;

    class MemberResolver {
        // friend Generator;

      private:
        void transformMemberArguments() {
            auto &params = ctx.mdata.params;

            VariableArray::iterator it = params.begin();

            const auto findSizeVar = [&](std::string identifier) {
                for (const auto &it : params) {
                    if (it->identifier() == identifier) {
                        return it;
                        break;
                    }
                }
                return std::make_shared<VariableData>(
                    VariableData::TYPE_INVALID);
            };

            const auto transformArgument = [&]() {
                if (it->get()->hasLenAttrib()) {
                    std::string lenAttrib = it->get()->lenAttrib();
                    if (lenAttrib == "null-terminated") {
                        return;
                    }
                    std::string len = it->get()->lenAttribVarName();
                    const std::shared_ptr<VariableData> &sizeVar =
                        findSizeVar(len);

                    if (sizeVar->isInvalid()) {
                        // length param not found
                        std::cerr << "(in: " << ctx.protoName << ")"
                                  << " Length param not found: " << len
                                  << std::endl;
                        return;
                    }
                    if (!strContains(lenAttrib, "->")) {
                        if (!sizeVar->getIgnoreFlag()) {
                            sizeVar->setIgnoreFlag(true);
                            std::string alt =
                                it->get()->identifier() + ".size()";
                            if (it->get()->type() == "void") {
                                alt += " * sizeof(T)";
                            }
                            sizeVar->setAltPFN(alt);
                        }
                    }
                    it->get()->bindLengthAttrib(sizeVar);
                    if (it->get()->type() == "void") {
                        templates.push_back("typename T");
                        it->get()->setFullType("", "T", "");
                    }
                    it->get()->convertToArrayProxy(false);
                }
            };

            while (it != params.end()) {
                transformArgument();
                if (it == params.end()) {
                    break;
                }
                it++;
            }
        }

        std::string declareReturnVar(const std::string assignment) {
            returnVar.setIdentifier("result");
            returnVar.setFullType("", "Result", "");
            returnVar.setSpecialType(VariableData::TYPE_DEFAULT);
            if (!assignment.empty()) {
                return returnVar.toString() + " = " + assignment + ";";
            } else {
                return returnVar.toString() + ";";
            }
        }

      protected:
        MemberContext ctx;
        std::string returnType;
        std::vector<std::string> templates;

        VariableData returnVar;

        virtual std::string generateMemberBody() {
            std::string output;
            output += "        " + generatePFNcall() + "\n";
            if (returnType == "Result") {
                output += "        return " + returnVar.identifier() + ";\n";
            }
            return output;
        }

        std::string generatePFNcall() {
            std::string arguments =
                ctx.mdata.createPFNArguments(ctx.className, ctx.handle);
            std::string call = "m_" + ctx.mdata.name + "(" + arguments + ")";
            switch (ctx.pfnReturn) {
            case PFNReturnCategory::VK_RESULT:
                return assignToResult("static_cast<Result>(" + call + ")");
                break;
            case PFNReturnCategory::VK_OBJECT:
                return "return static_cast<" + returnType + ">(" + call + ");";
                break;
            case PFNReturnCategory::OTHER:
                return "return " + call + ";";
                break;
            case PFNReturnCategory::VOID:
            default:
                return call + ";";
                break;
            }
        }

        std::string assignToResult(const std::string &assignment) {
            if (returnVar.isInvalid()) {
                return declareReturnVar(assignment);
            } else {
                return returnVar.identifier() + " = " + assignment + ";";
            }
        }

        std::string generateReturn(const std::string &identifier) {
            if (!returnVar.isInvalid()) {
                std::string out =
                    "return createResultValue<" + returnType + ">(\n" +
                    "               " + returnVar.identifier() + ",\n" +
                    "               " + identifier + ",\n" + "               " +
                    "\"" + ctx.gen.getConfig().vkname.get() + "::" + ctx.className +
                    "::" + ctx.protoName + "\"";
                if (ctx.mdata.successCodes.size() > 1) {
                    out += ",\n               {\n";
                    for (const auto &s : ctx.mdata.successCodes) {
                        out += "                 Result::" +
                               ctx.gen.enumConvertCamel("Result", s);
                        out += ",\n";
                    }
                    strStripSuffix(out, ",\n");
                    out += " }";
                }
                out += "\n               );";
                return out;
            }
            return "return " + identifier + ";";
        }

        std::string generateReturnType() {
            if (ctx.pfnReturn == PFNReturnCategory::VK_RESULT &&
                returnType != "Result") {
                return "ResultValueType<" + returnType + ">::type";
            }
            return returnType;
        }

      public:
        std::string dbgtag;

        MemberResolver(MemberContext &refCtx)
            : ctx(refCtx), returnVar(VariableData::TYPE_INVALID) {
            returnType = ctx.mdata.type;
            returnType = strStripVk(returnType.data());

            transformMemberArguments();
            dbgtag = "default";
        }

        virtual ~MemberResolver() {}

        std::string generateDeclaration() {
            std::string output;

            // file.writeLine("");
            if (!templates.empty()) {
                std::string temp;
                for (const std::string &str : templates) {
                    temp += str;
                    temp += ", ";
                }
                strStripSuffix(temp, ", ");
                output += "      template <" + temp + ">\n";
            }

            std::string protoArgs =
                ctx.mdata.createProtoArguments(ctx.className);
            std::string proto = generateReturnType() + " " + ctx.protoName +
                                "(" + protoArgs + ");" + " // [" + dbgtag +
                                "]\n";

            output += "      " + proto;
            return output;
        }

        std::string generateDefinition() {
            std::string output;

            output +=
                ctx.gen.genOptional(ctx.mdata.name, [&](std::string &output) {
                    if (!templates.empty()) {
                        std::string temp;
                        for (const std::string &str : templates) {
                            temp += str;
                            temp += ", ";
                        }
                        strStripSuffix(temp, ", ");
                        output += "      template <" + temp + ">\n";
                    }

                    std::string protoArgs =
                        ctx.mdata.createProtoArguments(ctx.className);
                    std::string proto = "inline " + generateReturnType() + " " +
                                        ctx.className + "::" + ctx.protoName +
                                        "(" + protoArgs + ") {" + " // [" +
                                        dbgtag + "]\n";

                    output += "  " + proto;

                    output += generateMemberBody();

                    output += "  }\n";
                });
            return output;
        }
    };

    class MemberResolverInit : public MemberResolver {
        VariableData returnVar;

      public:
        MemberResolverInit(MemberContext &refCtx) : MemberResolver(refCtx) {
           dbgtag = "create";
           returnVar = *ctx.mdata.params.rbegin()->get();
           returnType = returnVar.type();
        }


        std::string generateMemberBody() override {
            std::string output;
            output += "        " + returnType + " " + returnVar.identifier() + ";\n";
            output += "        " + generatePFNcall() + "\n";
            output += "        " + generateReturn(returnVar.identifier()) + "\n";
            return output;
        }
    };

    static std::vector<MemberResolver *> classMemberResolvers;

    class MemberResolverStruct : public MemberResolver {
      protected:
        std::shared_ptr<VariableData> lastVar;

      public:
        MemberResolverStruct(MemberContext &refCtx) : MemberResolver(refCtx) {
            lastVar = ctx.mdata.params[ctx.mdata.params.size() - 1];

            lastVar->convertToReturn();

            returnType = lastVar->fullType();

            dbgtag = "single return";
        }

        std::string generateMemberBody() override {
            std::string output;
            output +=
                "        " + returnType + " " + lastVar->identifier() + ";\n";

            output += "        " + generatePFNcall() + "\n";

            output += "        " + generateReturn(lastVar->identifier()) + "\n";
            return output;
        }
    };

    class MemberResolverGet : public MemberResolver {
      protected:
        std::shared_ptr<VariableData> last;
        std::shared_ptr<VariableData> lenVar;
        std::string initSize;

        std::shared_ptr<VariableData> getReturnVar() {
            std::string name = ctx.protoName;
            ctx.gen.strRemoveTag(name);
            strStripPrefix(name, "get");

            VariableArray::reverse_iterator it = ctx.mdata.params.rbegin();
            while (it != ctx.mdata.params.rend()) {
                if (strContains(it->get()->type(), name)) {
                    return *it;
                }
                if (it->get()->hasLengthAttribVar()) {
                    return *it;
                }
                it++;
            }
            std::cerr << "Can't get return var" << std::endl;
            return std::make_shared<VariableData>(VariableData::TYPE_INVALID);
        }

        bool returnsArray;

        std::string generateArray() {
            std::string output;
            output += "        " + last->toString() + initSize + ";\n";
            std::string size = last->lenAttrib();
            if (lenVar->getIgnoreFlag()) {
                output += "        " + lenVar->type() + " " +
                          lenVar->identifier() + ";\n";
            }

            if (ctx.pfnReturn == PFNReturnCategory::VK_RESULT) {
                output += "        " + assignToResult("") + "\n";
                std::string call = generatePFNcall();
                output += "        do {\n";
                // file2.pushIndent();

                output += "          " + call + "\n";
                output += "          if (( result == Result::eSuccess ) && " +
                          lenVar->identifier() + ") {\n";

                output += "            " + last->identifier() + ".resize(" +
                          size + ");\n";
                output += "            " + call + "\n";

                output += "            //VULKAN_HPP_ASSERT( " +
                          lenVar->identifier() + " <= " + last->identifier() +
                          ".size() );\n";

                // file2.popIndent();
                output += "        }\n";

                // file2.popIndent();
                output += "      } while ( result == Result::eIncomplete );\n";

                output += "      if ( ( result == Result::eSuccess ) && ( " +
                          lenVar->identifier() + "< " + last->identifier() +
                          ".size() ) ) {\n";
                output += "        " + last->identifier() + ".resize(" + size +
                          ");\n";

                output += "      }\n";

                output += "      " + generateReturn(last->identifier()) + "\n";
            } else {
                std::string call = generatePFNcall();

                output += "        " + call + "\n";

                output += "        " + last->identifier() + ".resize(" + size +
                          ");\n";

                output += "        " + call + "\n";
                output += "        //VULKAN_HPP_ASSERT( " +
                          lenVar->identifier() + " <= " + last->identifier() +
                          ".size() );\n";

                output +=
                    "        " + generateReturn(last->identifier()) + "\n";
            }
            return output;
        }

      public:
        MemberResolverGet(MemberContext &refCtx) : MemberResolver(refCtx) {
            dbgtag = "get array";

            last = getReturnVar();

            returnsArray = last->hasLengthAttribVar();

            if (returnsArray) {
                lenVar = last->lengthAttribVar();

                if (last->type() == "void") {
                    last->setFullType("", "T", "");
                    templates.push_back("typename T");
                    lenVar->setIgnoreFlag(false);
                    initSize = "(" + lenVar->identifier() + " / sizeof(T))";
                    lenVar->setAltPFN(lenVar->identifier() + " * sizeof(T)");

                } else {
                    if (!strContains(last->lenAttrib(), "->")) {
                        std::string ref =
                            strContains(lenVar->original.suffix(), "*") ? "&"
                                                                        : "";
                        lenVar->setAltPFN(ref + lenVar->identifier());
                    }
                }

                last->convertToStdVector();
                last->setIgnoreFlag(true);

            } else {
                last->convertToReturn();
            }

            returnType = last->fullType();
        }

        std::string generateMemberBody() override {
            std::string output;
            if (returnsArray) {
                output += generateArray();
            } else {
                output +=
                    "        " + returnType + " " + last->identifier() + ";\n";
                output += "        " + generatePFNcall() + "\n";
                output +=
                    "        " + generateReturn(last->identifier()) + "\n";
            }
            return output;
        }
    };

    class MemberResolverCreateArray : public MemberResolver {
      protected:
        std::shared_ptr<VariableData> infoVar;
        std::shared_ptr<VariableData> lastVar;

      public:
        MemberResolverCreateArray(MemberContext &refCtx)
            : MemberResolver(refCtx) {
            for (auto &var : ctx.mdata.params) {
                if (strContains(var->type(), "CreateInfo")) {
                    infoVar = var;
                    break;
                }
            }
            if (!infoVar.get()) {
                std::cerr
                    << "Error: MemberResolverAllocateArray: pCreateInfo not "
                       "found. ("
                    << ctx.protoName << ") " << ctx.className << std::endl;
            }

            lastVar = ctx.mdata.params[ctx.mdata.params.size() - 1];

            lastVar->convertToStdVector();
            lastVar->setIgnoreFlag(true);
            returnType = lastVar->fullType();

            dbgtag = "create array";
        }

        std::string generateMemberBody() override {
            std::string output;
            std::string vectorSize = infoVar->identifier() + ".size()";
            output +=
                "        " + lastVar->toString() + "(" + vectorSize + ");\n";

            output += "        " + generatePFNcall() + "\n";

            output += "        " + generateReturn(lastVar->identifier()) + "\n";
            return output;
        }
    };

    class MemberResolverAllocateArray : public MemberResolver {
      protected:
        std::shared_ptr<VariableData> lastVar;

      public:
        MemberResolverAllocateArray(MemberContext &refCtx)
            : MemberResolver(refCtx) {
            lastVar = ctx.mdata.params[ctx.mdata.params.size() - 1];

            lastVar->convertToStdVector();
            lastVar->setIgnoreFlag(true);
            returnType = lastVar->fullType();
        }

        std::string generateMemberBody() override {
            std::string output;
            std::string vectorSize = lastVar->lenAttrib();

            output +=
                "        " + lastVar->toString() + "(" + vectorSize + ");\n";

            output += "        " + generatePFNcall() + "\n";

            output += "        " + generateReturn(lastVar->identifier()) + "\n";
            return output;
        }
    };

    class MemberResolverReturnProxy : public MemberResolver {
      protected:
        std::shared_ptr<VariableData> last;
        std::shared_ptr<VariableData> sizeVar;

      public:
        MemberResolverReturnProxy(MemberContext &refCtx)
            : MemberResolver(refCtx) {
            last = *ctx.mdata.params.rbegin();
            sizeVar = *std::next(ctx.mdata.params.rbegin());

            last->convertToTemplate();
            sizeVar->setAltPFN("sizeof(T)");

            sizeVar->setIgnoreFlag(true);
            last->setIgnoreFlag(true);

            returnType = last->type();
            templates.push_back("typename T");

            dbgtag = "Return template proxy";
        }

        std::string generateMemberBody() override {
            std::string output;
            output += "        T " + last->identifier() + ";\n";

            output += "        " + generatePFNcall() + "\n"; // TODO missing &

            output += "        " + generateReturn(last->identifier()) + "\n";
            return output;
        }
    };

    class MemberResolverReturnVectorOfProxy : public MemberResolver {
      protected:
        std::shared_ptr<VariableData> last;
        std::shared_ptr<VariableData> sizeVar;

      public:
        MemberResolverReturnVectorOfProxy(MemberContext &refCtx)
            : MemberResolver(refCtx) {
            last = *ctx.mdata.params.rbegin();
            sizeVar = *std::next(ctx.mdata.params.rbegin());

            last->setFullType("", "T", "");
            last->convertToStdVector();

            sizeVar->setAltPFN(last->identifier() + ".size()"
                                                    " * "
                                                    "sizeof(T)");

            last->setIgnoreFlag(true);
            templates.push_back("typename T");

            dbgtag = "Return vector of template proxy";
        }

        std::string generateMemberBody() override {
            std::string output;
            output += "        VULKAN_HPP_ASSERT( " + sizeVar->identifier() +
                      " % sizeof( T ) == 0 );\n";

            output += "        " + last->toString() + "( " +
                      sizeVar->identifier() +
                      " / sizeof(T)"
                      " );\n";
            output += "        " + generatePFNcall() + "\n";

            output += "        " + generateReturn(last->identifier()) + "\n";
            return output;
        }
    };

    class MemberResolverReturnVectorData : public MemberResolver {
      protected:
        std::shared_ptr<VariableData> last;
        std::shared_ptr<VariableData> lenVar;

      public:
        MemberResolverReturnVectorData(MemberContext &refCtx)
            : MemberResolver(refCtx) {
            dbgtag = "vector data";
            last = *ctx.mdata.params.rbegin();

            lenVar = last->lengthAttribVar();

            last->convertToStdVector();
            last->setIgnoreFlag(true);

            returnType = last->fullType();
            lenVar->setAltPFN("&" + lenVar->original.identifier());
        }

        std::string generateMemberBody() override {
            std::string output;
            output += "        " + last->toString() + ";\n";
            output += "        " + lenVar->type() + " " + lenVar->identifier() +
                      ";\n";

            if (ctx.pfnReturn == PFNReturnCategory::VK_RESULT) {
                output += "        " + assignToResult("") + "\n";
                std::string call = generatePFNcall();
                output += "        do {\n";

                output += "          " + call + "\n";

                output += "          if (( result == Result::eSuccess ) && " +
                          lenVar->identifier() + ") {\n";
                output += "            " + last->identifier() + ".resize(" +
                          lenVar->identifier() + ");\n";
                output += "            " + call + "\n";

                output += "            //VULKAN_HPP_ASSERT( " +
                          lenVar->identifier() + " <= " + last->identifier() +
                          ".size() );\n";

                output += "          }\n";

                output +=
                    "        } while ( result == Result::eIncomplete );\n";

                output += "        if ( ( result == Result::eSuccess ) && ( " +
                          lenVar->identifier() + "< " + last->identifier() +
                          ".size() ) ) {\n";
                output += "          " + last->identifier() + ".resize(" +
                          lenVar->identifier() + ");\n";

                output += "        }\n";

                output +=
                    "        " + generateReturn(last->identifier()) + "\n";
            }
            return output;
        }
    };

    class MemberResolverEnumerate : public MemberResolver {
      protected:
        std::shared_ptr<VariableData> last;
        std::shared_ptr<VariableData> lenVar;

      public:
        MemberResolverEnumerate(MemberContext &refCtx)
            : MemberResolver(refCtx) {
            dbgtag = "enumerate";
            last = *ctx.mdata.params.rbegin();

            lenVar = last->lengthAttribVar();

            last->convertToStdVector();
            last->setIgnoreFlag(true);

            returnType = last->fullType();
            lenVar->setAltPFN("&" + lenVar->original.identifier());
        }

        std::string generateMemberBody() override {
            std::string output;
            bool returnsObject = ctx.gen.handles.find(last->original.type()) !=
                                 ctx.gen.handles.end();

            std::string objvector = last->fullType();
            if (returnsObject) {
                last->setType(last->original.type());
            }
            output += "        " + last->toString() + ";\n";
            output += "        " + lenVar->type() + " " + lenVar->identifier() +
                      ";\n";

            if (ctx.pfnReturn == PFNReturnCategory::VK_RESULT) {
                output += "        " + assignToResult("") + "\n";
                std::string call = generatePFNcall();
                last->setAltPFN("nullptr");
                std::string call2 = generatePFNcall();
                output += "        do {\n";
                output += "          " + call2 + "\n";
                output += "          if (( result == Result::eSuccess ) && " +
                          lenVar->identifier() + ") {\n";

                output += "            " + last->identifier() + ".resize(" +
                          lenVar->identifier() + ");\n";

                output += "            " + call + "\n";

                output += "            //VULKAN_HPP_ASSERT( " +
                          lenVar->identifier() + " <= " + last->identifier() +
                          ".size() );\n";

                output += "          }\n";

                output +=
                    "        } while ( result == Result::eIncomplete );\n";

                output += "        if ( ( result == Result::eSuccess ) && ( " +
                          lenVar->identifier() + "< " + last->identifier() +
                          ".size() ) ) {\n";
                output += "          " + last->identifier() + ".resize(" +
                          lenVar->identifier() + ");\n";

                output += "        }\n";

                if (returnsObject) {
                    output += "        " + objvector + " out{" +
                              last->identifier() + ".begin(), " +
                              last->identifier() + ".end()};\n";
                    output += "        " + generateReturn("out") + "\n";
                } else {
                    output +=
                        "        " + generateReturn(last->identifier()) + "\n";
                }
            }
            return output;
        }
    };

    static PFNReturnCategory evaluatePFNReturn(const std::string &type) {
        if (type == "void") {
            return PFNReturnCategory::VOID;
        } else if (type == "VkResult") {
            return PFNReturnCategory::VK_RESULT;
        } else if (type.starts_with("Vk")) {
            return PFNReturnCategory::VK_OBJECT;
        } else {
            return PFNReturnCategory::OTHER;
        }
    }

    bool
    getLastTwo(MemberContext &ctx,
               std::pair<VariableArray::reverse_iterator,
                         VariableArray::reverse_iterator> &data) { // rename ?
        VariableArray::reverse_iterator last = ctx.mdata.params.rbegin();
        if (last != ctx.mdata.params.rend()) {
            VariableArray::reverse_iterator prevlast = std::next(last);
            data.first = last;
            data.second = prevlast;
            return true;
        }
        return false;
    }

    bool isPointerToCArray(const std::string &id) {
        if (id.size() >= 2) {
            if (id[0] == 'p' && std::isupper(id[1])) {
                return true;
            }
        }
        return false;
    }

    std::string stripStartingP(const std::string &str) {
        std::string out = str;
        if (isPointerToCArray(str)) {
            out = std::tolower(str[1]);
            out += str.substr(2);
        }
        return out;
    }

    std::vector<MemberResolver *> createMemberResolvers(MemberContext &ctx);

    void generateClassMemberCpp(MemberContext &ctx) {
        std::string dbgProtoComment; // debug info
        std::vector<MemberResolver *> resolvers = createMemberResolvers(ctx);

        for (size_t i = 0; i < resolvers.size(); ++i) {
            MemberResolver *resolver = resolvers[i];
            genOptional(ctx.mdata.name,
                        [&] { output += resolver->generateDeclaration(); });

            genOptional(ctx.mdata.name,
                        [&] { outputFuncs += resolver->generateDefinition(); });
            delete resolver;
        }
    }

    static VariableData createClassVar(const std::string_view &type);

    void generateClassMembers(const std::string &className,
                              const std::string &handle,
                              const HandleData &data);

    static std::string_view getHandleSuperclass(const HandleData &data);

    void generateClass(const std::string &name, HandleData &data);

    void parseCommands(XMLNode *node);

  public:
    Generator();

    bool isLoaded() const { return root != nullptr; }

    void setOutputFilePath(const std::string &path) { outputFilePath = path; }

    bool isOuputFilepathValid() const {
        return !std::filesystem::is_directory(outputFilePath);
    }

    std::string getOutputFilePath() const { return outputFilePath; }

    void load(const std::string &xmlPath);

    void generate();

    Platforms &getPlatforms() { return platforms; };

    Extensions &getExtensions() { return extensions; };

    Config &getConfig() { return cfg; }
};

#endif // GENERATOR_HPP
