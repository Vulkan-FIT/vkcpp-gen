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
#include <optional>
#include <regex>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

static constexpr char const *RES_HEADER{
    R"(
//#include <vulkan/vulkan_core.h>
//#include <vulkan/vulkan.hpp>
#include <iostream>

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

  template <typename T>
  class ArrayProxyNoTemporaries
  {
  public:
    VULKAN_HPP_CONSTEXPR ArrayProxyNoTemporaries() VULKAN_HPP_NOEXCEPT
      : m_count( 0 )
      , m_ptr( nullptr )
    {}

    VULKAN_HPP_CONSTEXPR ArrayProxyNoTemporaries( std::nullptr_t ) VULKAN_HPP_NOEXCEPT
      : m_count( 0 )
      , m_ptr( nullptr )
    {}

    ArrayProxyNoTemporaries( T & value ) VULKAN_HPP_NOEXCEPT
      : m_count( 1 )
      , m_ptr( &value )
    {}

    template <typename V>
    ArrayProxyNoTemporaries( V && value ) = delete;

    template <typename B = T, typename std::enable_if<std::is_const<B>::value, int>::type = 0>
    ArrayProxyNoTemporaries( typename std::remove_const<T>::type & value ) VULKAN_HPP_NOEXCEPT
      : m_count( 1 )
      , m_ptr( &value )
    {}

    template <typename B = T, typename std::enable_if<std::is_const<B>::value, int>::type = 0>
    ArrayProxyNoTemporaries( typename std::remove_const<T>::type && value ) = delete;

    ArrayProxyNoTemporaries( uint32_t count, T * ptr ) VULKAN_HPP_NOEXCEPT
      : m_count( count )
      , m_ptr( ptr )
    {}

    template <typename B = T, typename std::enable_if<std::is_const<B>::value, int>::type = 0>
    ArrayProxyNoTemporaries( uint32_t count, typename std::remove_const<T>::type * ptr ) VULKAN_HPP_NOEXCEPT
      : m_count( count )
      , m_ptr( ptr )
    {}

    ArrayProxyNoTemporaries( std::initializer_list<T> const & list ) VULKAN_HPP_NOEXCEPT
      : m_count( static_cast<uint32_t>( list.size() ) )
      , m_ptr( list.begin() )
    {}

    ArrayProxyNoTemporaries( std::initializer_list<T> const && list ) = delete;

    template <typename B = T, typename std::enable_if<std::is_const<B>::value, int>::type = 0>
    ArrayProxyNoTemporaries( std::initializer_list<typename std::remove_const<T>::type> const & list )
      VULKAN_HPP_NOEXCEPT
      : m_count( static_cast<uint32_t>( list.size() ) )
      , m_ptr( list.begin() )
    {}

    template <typename B = T, typename std::enable_if<std::is_const<B>::value, int>::type = 0>
    ArrayProxyNoTemporaries( std::initializer_list<typename std::remove_const<T>::type> const && list ) = delete;

    ArrayProxyNoTemporaries( std::initializer_list<T> & list ) VULKAN_HPP_NOEXCEPT
      : m_count( static_cast<uint32_t>( list.size() ) )
      , m_ptr( list.begin() )
    {}

    ArrayProxyNoTemporaries( std::initializer_list<T> && list ) = delete;

    template <typename B = T, typename std::enable_if<std::is_const<B>::value, int>::type = 0>
    ArrayProxyNoTemporaries( std::initializer_list<typename std::remove_const<T>::type> & list ) VULKAN_HPP_NOEXCEPT
      : m_count( static_cast<uint32_t>( list.size() ) )
      , m_ptr( list.begin() )
    {}

    template <typename B = T, typename std::enable_if<std::is_const<B>::value, int>::type = 0>
    ArrayProxyNoTemporaries( std::initializer_list<typename std::remove_const<T>::type> && list ) = delete;

    // Any type with a .data() return type implicitly convertible to T*, and a // .size() return type implicitly
    // convertible to size_t.
    template <typename V,
              typename std::enable_if<
                std::is_convertible<decltype( std::declval<V>().data() ), T *>::value &&
                std::is_convertible<decltype( std::declval<V>().size() ), std::size_t>::value>::type * = nullptr>
    ArrayProxyNoTemporaries( V & v ) VULKAN_HPP_NOEXCEPT
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

template <template <class...> class TContainer, class T, class A, class E>
static bool isInContainter(const TContainer<T, A> &array, E entry) {
    return std::find(std::begin(array), std::end(array), entry) !=
           std::end(array);
}

using namespace tinyxml2;

class Generator {

  public:
    struct Macro {
        std::string value;
        std::string define;
        bool usesDefine;

        std::string get() const { return usesDefine ? define : value; }
    };

    struct Config {
        Macro mcName;
        Macro mcConstexpr;
        Macro mcInline;
        Macro mcNoexcept;
        Macro mcExplicit;
        std::string fileProtect;
        std::string loaderClassName;
        bool genObjectParents;
        bool genStructNoinit;
        bool genStructProxy;

        bool dbgMethodTags;
    };

    Config cfg;
    bool loaded;
    XMLDocument doc;
    XMLElement *root;

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

    using Variables = std::vector<std::shared_ptr<VariableData>>;

    void bindVars(Variables &vars) const;

    enum class PFNReturnCategory {
        OTHER,
        VOID,
        VK_RESULT //, VK_STRUCT
    };

    enum class ArraySizeArgument { INVALID, COUNT, SIZE, CONST_COUNT };

    enum class MemberNameCategory { UNKNOWN, GET, ALLOCATE, CREATE, ENUMERATE, WRITE, DESTROY, FREE };

    enum class HandleCreationCategory { UNKNOWN, ALLOCATE, CREATE };

    struct HandleData;

    struct CommandData {
        String name;      // identifier
        std::string type; // return type
        Variables params; // list of arguments
        std::vector<std::string> successCodes;
        MemberNameCategory nameCat;
        // CommandFlags cmdFlags;
        PFNReturnCategory pfnReturn;

        CommandData() : name("") {}

        void setName(const Generator &gen, const std::string &name) {
            this->name.convert(name);
            pfnReturn = gen.evaluatePFNReturn(type);
            gen.evalCommand(*this);
        }       

        bool containsPointerVariable() {
            for (auto it = params.rbegin(); it != params.rend(); it++) {
                if (it->get()->original.isPointer()) {
                    return true;
                }
            }
            return false;
        }

        std::shared_ptr<VariableData> getLastVar() {
            return *std::prev(params.end());
        }

        std::shared_ptr<VariableData> getLastVisibleVar() {
            for (auto it = params.rbegin(); it != params.rend(); it++) {
                if (!it->get()->getIgnoreFlag()) {
                    return *it;
                }
            }
            throw std::runtime_error("can't get param (last visible)");
        }

        std::shared_ptr<VariableData> getLastPointerVar() {
            for (auto it = params.rbegin(); it != params.rend(); it++) {
                if (it->get()->original.isPointer()) {
                    return *it;
                }
            }
            throw std::runtime_error("can't get param (last pointer)");
        }

    };

    // holds information about class member (function)
    struct ClassCommandData : CommandData {
        using Var = const std::shared_ptr<VariableData>;

        const Generator &gen;
        HandleData *cls;

        ClassCommandData(const ClassCommandData &o)
            : ClassCommandData(o.gen, o.cls, o) {}

        ClassCommandData(const Generator &gen, HandleData *cls,
                         const CommandData &o)
            : gen(gen), cls(cls) {
            name = o.name;
            name = std::regex_replace(name, std::regex(cls->name),
                                      ""); // filtered prototype name
            type = o.type;
            successCodes = o.successCodes;
            nameCat = o.nameCat;            
            pfnReturn = o.pfnReturn;

            std::transform(
                o.params.begin(), o.params.end(), std::back_inserter(params),
                [](const std::shared_ptr<VariableData> &var) {
                    return std::make_shared<VariableData>(*var.get());
                });

            gen.bindVars(params);
        }

        ClassCommandData &operator=(ClassCommandData o) {
            *this = ClassCommandData(o.gen, o.cls, o);
            return *this;
        }

        bool valid() const { return !name.empty(); }

        std::string createProtoArguments(bool useOriginal = false) const {
            return createArguments([&](Var &v){ return filterProto(v); }, [&](Var &v){
                return useOriginal? v->originalToString() : v->toString();
            });
        }

        std::string createPFNArguments() const {
            return createArguments([&](Var &v){ return filterPFN(v); }, [&](Var &v){
                return v->toArgument();
            });
        }

        std::string createPassArguments() const {
            return createArguments([&](Var &v){ return filterPFN(v); }, [&](Var &v){
                return v->identifier();
            });
        }

        Variables getFilteredProtoVars() const {
            Variables out;
              for (const auto &p : params) {
                if (filterProto(p)) {
                    out.push_back(p);
                }
            }
            return out;
        }

    private:
        std::string createArguments(std::function<bool(Var&)> filter,
                                    std::function<std::string(Var&)> function) const
        {
            std::string out;
            for (const auto &p : params) {
                if (!filter(p)) {
                    continue;
                }
                if (p->original.type() == cls->name.original) {
                    if (p->isPointer()) {
                        out += "&";
                    }
                    out += cls->vkhandle;
                } else {
                    out += function(p);
                }
                out += ", ";
            }
            strStripSuffix(out, ", ");
            return out;
        }

        bool filterProto(Var &v) const {
            return !v->getIgnoreFlag() && v->original.type() != cls->name.original;
        }

        bool filterPFN(Var &v) const {
            return !v->getIgnorePFN();
        }

    };

    struct EnumValue {
        std::string_view name;
        const char *bitpos;
        const char *protect;
        const char *alias;
        bool supported;
    };

    struct EnumData {
        std::vector<EnumValue> extendValues;
        std::vector<std::string> aliases;
        std::vector<std::pair<std::string, const char *>> members;

        bool containsValue(const std::string &value);
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
    std::unordered_map<std::string, EnumData> enums;
    std::unordered_map<std::string,
                       std::vector<std::pair<std::string, const char *>>>
        enumMembers;

    struct GenOutputClass {
        std::string sPublic;
        std::string sPrivate;
        std::string sProtected;
        std::string inherits;
    };

    template <class... Args>
    std::string format(const std::string &format, const Args... args) const;

    bool defaultWhitelistOption = true;

    void parsePlatforms(XMLNode *node);

    void parseFeature(XMLNode *node);

    void parseExtensions(XMLNode *node);

    void parseTags(XMLNode *node);

    // tries to match str in extensions, if found returns pointer to protect,
    // otherwise nullptr
    ExtensionData *findExtensionProtect(const std::string &str);

    bool idLookup(const std::string &name, std::string_view &protect) const;

    // #if defined encapsulation
    void withProtect(std::string &output, const char *protect,
                     std::function<void(std::string &)> function) const;

    void genOptional(const std::string &name, std::function<void()> function);

    std::string genOptional(const std::string &name,
                            std::function<void(std::string &)> function) const;

    bool isStructOrUnion(const std::string &name) const {
        return structs.find(name) != structs.end();
    }

    std::string strRemoveTag(std::string &str) const;

    std::string strWithoutTag(const std::string &str) const;

    std::pair<std::string, std::string> snakeToCamelPair(std::string str) const;

    std::string snakeToCamel(const std::string &str) const;

    std::string enumConvertCamel(const std::string &enumName,
                                 std::string value) const;

    void generateOutput();

    std::string generateFile();

    Variables parseStructMembers(XMLElement *node,
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
                            const Variables &members);

    void generateUnionCode(std::string name,
                           const Variables &members);

    void generateStruct(std::string name, const std::string &structType,
                        const std::string &structTypeValue,
                        const Variables &members,
                        const std::vector<std::string> &typeList,
                        bool structOrUnion);

    void parseStruct(XMLElement *node, std::string name, bool structOrUnion);

    void parseEnumExtend(XMLElement &node, const char *protect,
                         bool flagSupported);

    void generateEnum(std::string name, XMLNode *node,
                      const std::string &bitmask);

    void genEnums(XMLNode *node);

    void genFlags();

    struct MemberContext : ClassCommandData {
        bool lastHandleVariant;

        MemberContext(const ClassCommandData &m) : ClassCommandData(m) {
            lastHandleVariant = false;

            const auto &last = params.rbegin(); // last argument
            if (last->get()->flagHandle()) {
                const auto& h = gen.findHandle(last->get()->original.type());
                lastHandleVariant = h.usesHandleVariant();
            }
        }
    };

    struct HandleData {
        String name;
        std::string vkhandle;
        // std::string vktype;
        std::string superclass;
        std::string_view alias;
        std::pair<const std::string_view, HandleData> *parent;
        HandleCreationCategory creationCat;

        std::optional<ClassCommandData> getAddrCmd;
        std::vector<ClassCommandData> members;
        std::vector<ClassCommandData> ctorCmds;

        std::vector<MemberContext> generated;

        int effectiveMembers;
        bool parentMember;
        bool uniqueVariant;

        HandleData(const std::string &name) : name(name, true) {            
            vkhandle = "m_" + strFirstLower(this->name);
            parent = nullptr;
            effectiveMembers = 0;
            parentMember = false;
            uniqueVariant = false;
            creationCat = HandleCreationCategory::UNKNOWN;
        }

        /*
        HandleData(const HandleData &o) {
            name = o.name;
            vkhandle = o.vkhandle;
            // vktype = o.vktype;
            parent = o.parent;
            alias = o.alias;
            superclass = o.superclass;
            getAddrCmd = o.getAddrCmd;
            effectiveMembers = o.effectiveMembers;
            parentMember = o.parentMember;
            uniqueVariant = o.uniqueVariant;

            std::copy(o.members.begin(), o.members.end(),
                      std::back_inserter(members));
            std::copy(o.ctorCmds.begin(), o.ctorCmds.end(),
                      std::back_inserter(ctorCmds));
        }
        */

        void calculate(const Generator &gen,
                       const std::string &loaderClassName);

        int totalMembers() const;

        bool usesHandleVariant() const;

        void addCommand(const Generator &gen, const CommandData &cmd);

        bool isSubclass() const {
            return name.original != "VkInstance" && name.original != "VkDevice";
        }
    };

    using Handles = std::unordered_map<std::string_view, HandleData>;
    Handles handles;
    HandleData loader;

    bool isHandle(const std::string_view &name) const {
        return handles.find(name) != handles.end();
    }

    HandleData& findHandle(const std::string_view &name) {
        if (name == loader.name.original) {
            return loader;
        }
        const auto &handle = handles.find(name);
        if (handle == handles.end()) {            
            throw std::runtime_error("Handle not found: " + std::string(name));
        }
        return handle->second;
    }

    const HandleData& findHandle(const std::string_view &name) const {
        if (name == loader.name.original) {
            return loader;
        }
        const auto &handle = handles.find(name);
        if (handle == handles.end()) {
            throw std::runtime_error("Handle not found: " + std::string(name));
        }
        return handle->second;
    }

    void parseTypeDeclarations(XMLNode *node);

    void generateStructDecl(const std::string &name, const StructData &d);

    void generateClassDecl(const HandleData &data);

    std::string generateClassString(const std::string &className,
                                    const GenOutputClass &from);

    void genTypes();

    CommandData parseClassMember(XMLElement *command,
                                 const std::string &className) {
        CommandData m;

        // iterate contents of <command>
        std::string dbg;
        std::string name;
        for (XMLElement &child : Elements(command)) {
            // <proto> section
            dbg += std::string(child.Value()) + "\n";
            if (std::string_view(child.Value()) == "proto") {
                // get <name> field in proto
                XMLElement *nameElement = child.FirstChildElement("name");
                if (nameElement) {
                    name = nameElement->GetText();
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
                const std::shared_ptr<XMLVariableParser> &parser =
                    std::make_shared<XMLVariableParser>(&child, *this);
                // add proto data to list
                m.params.push_back(parser);
            }
        }
        if (name.empty()) {
            std::cerr << "Command has no name" << std::endl;
        }

        const char *successcodes = command->Attribute("successcodes");
        if (successcodes) {
            for (const auto &str : split(std::string(successcodes), ",")) {
                m.successCodes.push_back(str);
            }
        }

        bindVars(m.params);
        m.setName(*this, name);
        return m;
    }
    std::string generateClassMemberCStyle(const std::string &className,
                                          const std::string &handle,
                                          const std::string &protoName,
                                          const ClassCommandData &m);

    void evalCommand(CommandData &ctx) const;

    static MemberNameCategory evalNameCategory(const std::string &name);

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

    class MemberResolver {

      private:
        void transformMemberArguments() {
            auto &params = ctx.params;

            const auto transformToProxy = [&](const std::shared_ptr<VariableData> &var) {
                if (var->hasLengthVar()) {

                    const std::shared_ptr<VariableData> &sizeVar =
                        var->getLengthVar();

                    if (!sizeVar->hasArrayVar()) {
                        sizeVar->bindArrayVar(var);
                    } else {
                        // std::cout << ctx.name << ": length var " <<
                        // sizeVar->identifier() << " has multiple array
                        // bindings" << std::endl;
                    }

                    if (!var->isLenAttribIndirect()) {
                        sizeVar->setIgnoreFlag(true);
                    }

                    if (var->original.type() == "void") {
                        if (var->flagArrayIn()) {
                            var->setFullType("", "DataType", "");
                            var->setTemplate("DataType");
                        }
                        else if (var->flagArrayOut()) {
                            var->setFullType("", "uint8_t", "");
                        }
                    }

                    var->convertToArrayProxy(false);
                }
            };

            const auto convertName = [](const std::shared_ptr<VariableData> &var) {
                const std::string &id = var->identifier();
                if (id.size() >= 2 && id[0] == 'p' && std::isupper(id[1])) {
                    var->setIdentifier(strFirstLower(id.substr(1)));
                }
            };

            for (const auto &p : params) {
                transformToProxy(p);
            }

            for (const auto &p : params) {
                if (p->flagArray()) {
                    convertName(p);
                    continue;
                }
                const std::string &type = p->original.type();
                if (ctx.gen.isStructOrUnion(type)) {
                    p->convertToReference();
                    p->setConst(true);
                    convertName(p);
                }
                if (ctx.gen.isHandle(type)) {
                    const auto &handle = ctx.gen.findHandle(type);
                    if (handle.effectiveMembers > 0 || handle.parentMember) {
                        p->convertToReference();
                        p->setConst(true);
                        convertName(p);
                    }
                }
            }

        }

        void transformFinalize() {
            const auto &var = ctx.getLastVisibleVar();            
            if (var->original.type() == "VkAllocationCallbacks") {
                var->convertToOptional();
            }
        }

        std::string declareReturnVar(const std::string assignment) {
            resultVar.setSpecialType(VariableData::TYPE_DEFAULT);
            resultVar.setIdentifier("result");
            resultVar.setFullType("", "Result", "");

            std::string out = resultVar.toString();
            if (!assignment.empty()) {
                out += " = " + assignment;
            }
            return out += ";";
        }

      protected:
        MemberContext ctx;
        std::string returnType;        
        VariableData resultVar;
        bool specifierInline;
        bool specifierExplicit;
        bool specifierConst;        

        virtual std::string generateMemberBody() {
            std::string output;
            bool immediate = ctx.pfnReturn != PFNReturnCategory::VK_RESULT &&
                             ctx.successCodes.size() <= 1;
            output += "    " + generatePFNcall(immediate) + "\n";
            if (!immediate) {
                output +=
                    "    " + generateReturn(resultVar.identifier()) + "\n";
            }
            return output;
        }

        std::string castTo(const std::string &type,
                           const std::string &src) const {
            if (type != ctx.type) {
                return "static_cast<" + type + ">(" + src + ")";
            }
            return src;
        }

        std::string generatePFNcall(bool immediateReturn = false) {
            std::string arguments = ctx.createPFNArguments();
            std::string call = "m_" + ctx.name.original + "(" + arguments + ")";

            switch (ctx.pfnReturn) {
            case PFNReturnCategory::VK_RESULT:
                call = castTo("Result", call);
                if (!immediateReturn) {
                    return assignToResult(call);
                }
                break;
            case PFNReturnCategory::OTHER:
                call = castTo(returnType, call);
                break;
            case PFNReturnCategory::VOID:
                return call + ";";
            default:
                break;
            }
            if (immediateReturn) {
                call = "return " + call;
            }
            return call + ";";
        }

        std::string assignToResult(const std::string &assignment) {
            if (resultVar.isInvalid()) {
                return declareReturnVar(assignment);
            } else {
                return resultVar.identifier() + " = " + assignment + ";";
            }
        }

        std::string generateReturn(const std::string &identifier) {
            if (resultVar.isInvalid()) {
                return "return " + identifier + ";";
            }
            std::string message;
            const auto &ns = ctx.gen.getConfig().mcName;
            if (ns.usesDefine) {
                message = ns.define + "_STRING \"";
            } else {
                message = "\"" + ns.value;
            }
            message += ctx.gen.format("::{0}::{1}\"", ctx.cls->name, ctx.name);

            std::string out = "return createResultValue";
            if (returnType != "void" && returnType != "Result") {
                out += "<" + returnType + ">";
            }
            out += "(\n";
            if (resultVar.identifier() != identifier) {
                out += "             " + resultVar.identifier() + ",\n";
            }
            out += "             " + identifier + ",\n";
            out += "             " + message;

            if (ctx.successCodes.size() > 1) {
                out += ",\n             {\n";
                for (const auto &s : ctx.successCodes) {
                    out += "               " + ns.get() +
                           "::Result::" + ctx.gen.enumConvertCamel("Result", s);
                    out += ",\n";
                }
                strStripSuffix(out, ",\n");
                out += "\n             }";
            }
            out += "\n           );";
            return out;
        }

        std::string generateReturnType() {
            if (ctx.pfnReturn == PFNReturnCategory::VK_RESULT &&
                returnType != "Result") {
                return "ResultValueType<" + returnType + ">::type";
            }
            return returnType;
        }

        std::string getSpecifiers(bool decl) {
            std::string output;
            const auto &cfg = ctx.gen.getConfig();
            if (specifierInline && !decl) {
                output += cfg.mcInline.get() + " ";
            }
            if (specifierExplicit && decl) {
                output += cfg.mcExplicit.get() + " ";
            }            
            return output;
        }

        std::string getProto(const std::string &indent, bool declaration) {
            std::string tag = getDbgtag();
            std::string output;
            if (!tag.empty()) {
                output += indent + tag;
            }

            std::string temp;
            for (const auto &p : ctx.params) {
                const std::string &str = p->getTemplate();
                if (!str.empty()) {
                    temp += "typename " + str + ", ";
                }
            }
            strStripSuffix(temp, ", ");
            if (!temp.empty()) {
                output += indent + "template <" + temp + ">\n";
            }

            std::string spec = getSpecifiers(declaration);
            std::string ret = generateReturnType();
            output += indent;
            if (!spec.empty()) {
                output += spec;
            }
            if (!ret.empty()) {
                output += ret + " ";
            }
            if (!declaration) {
                output += ctx.cls->name + "::";
            }
            output += ctx.name + "(" + ctx.createProtoArguments() + ")";
            if (specifierConst) {
                output += " const";
            }
            return output;
        }

        std::string getDbgtag();

        std::string generateDeclaration() {
            return ctx.gen.genOptional(
                ctx.name.original, [&](std::string &output) {
                    output += getProto("    ", true) + ";\n";
                });
        }

        std::string generateDefinition() {
            return ctx.gen.genOptional(
                ctx.name.original, [&](std::string &output) {
                    output += getProto("  ", false) + " {\n";
                    output += generateMemberBody();
                    output += "  }\n";
                });
        }

      public:
        std::string dbgtag;

        MemberResolver(MemberContext &refCtx)
            : ctx(refCtx), resultVar(VariableData::TYPE_INVALID) {
            returnType = strStripVk(std::string(ctx.type));

            transformMemberArguments();
            dbgtag =
                "default"; // sc: " + std::to_string(ctx.successCodes.size());
            specifierInline = true;
            specifierExplicit = false;
            specifierConst = true;

            if (ctx.pfnReturn == PFNReturnCategory::VK_RESULT &&
                ctx.successCodes.size() <= 1) {
                returnType = "void";
            }
        }

        virtual ~MemberResolver() {}

        void generate(std::string &decl, std::string &def) {
            transformFinalize();
            decl += generateDeclaration();
            def += generateDefinition();

            ctx.cls->generated.push_back(ctx);
        }

    };

    class MemberResolverEmpty
        : public MemberResolver { // used only for debug markup
        std::string comment;

      public:
        MemberResolverEmpty(MemberContext &refCtx, std::string comment)
            : MemberResolver(refCtx), comment(comment) {
            dbgtag = "dbg";
        }

        virtual std::string generateMemberBody() override {
            return "      // " + comment + "\n";
        }
    };

    class MemberResolverInit : public MemberResolver {
        String name;
        std::shared_ptr<VariableData> parent;

      public:
        MemberResolverInit(MemberContext &refCtx)
            : MemberResolver(refCtx), name(refCtx.name.original, false) {
            parent = *ctx.params.begin();
            parent->setIgnorePFN(true);

            ctx.params.rbegin()->get()->setIgnorePFN(true);
            ctx.params.rbegin()->get()->setIgnoreFlag(true);

            if (ctx.lastHandleVariant) {
                name += "Handle";
            }
            ctx.name = std::string(ctx.cls->name);

            ctx.pfnReturn = PFNReturnCategory::VOID;
            returnType = "";
            specifierInline = false;
            specifierExplicit = true;
            specifierConst = false;
            dbgtag = "constructor";
        }

        virtual std::string generateMemberBody() override {
            std::string output;
            const std::string &id = parent->identifier();
            const std::string &call = ctx.createPassArguments();

            output +=
                ctx.gen.format("    {0} = {1}.{2}({3});\n", ctx.cls->vkhandle, id, name, call);

            if (ctx.cls->parentMember) {
                output += "    this->parent = &" + id + ";\n";
            }

            if (ctx.cls->effectiveMembers > 0) {
                output += "    loadPFNs(" + id + ");\n";
            }

            return output;
        }

        bool checkMethod() const {
            try {                
                const auto &handle =
                    ctx.gen.findHandle(parent->original.type());
                bool blacklisted = true;
                ctx.gen.genOptional(name.original, [&](std::string &output) {
                    blacklisted = false;
                });
                if (blacklisted) {                    
                    std::cout << "checkMethod: " << name.original << " blacklisted" << std::endl;
                    return false;
                }
                for (const auto &m : handle.members) {
                    if (m.name.original == name.original) {
                        return true;
                    }
                }
                std::cout << "checkMethod: " << name.original << " not in " << handle.name << " members" << std::endl;
            } catch (std::runtime_error e) {                
                return false;
            }
            return false;
        }

        std::string getName() const { return name; }
    };

    class MemberResolverCreateHandle : public MemberResolver {
        std::shared_ptr<VariableData> returnVar;

      public:
        MemberResolverCreateHandle(MemberContext &refCtx)
            : MemberResolver(refCtx) {
            returnVar = ctx.getLastPointerVar();
            returnVar->convertToReturn();
            returnType = returnVar->original.type();
            returnVar->setType(returnType);

            ctx.name += "Handle";
            dbgtag = "create handle";
        }

        virtual std::string generateMemberBody() override {
            const std::string &id = returnVar->identifier();
            const std::string &call = generatePFNcall();
            return ctx.gen.format(R"(
    {0} {1};
    {2}
    {3}
)",
                                  returnType, id, call, generateReturn(id));
        }
    };

     class MemberResolverCreateUnique : public MemberResolver {
        std::shared_ptr<VariableData> last;

      public:
        MemberResolverCreateUnique(MemberContext &refCtx)
            : MemberResolver(refCtx) {

            last = ctx.getLastPointerVar();
            last->convertToReturn();
            last->setIgnorePFN(true);
            returnType = "Unique" + last->type();
            ctx.pfnReturn = PFNReturnCategory::VOID;

            if (ctx.params.begin()->get()->original.type() == ctx.cls->name.original) {
                ctx.params.begin()->get()->setIgnorePFN(true);                
            }

            ctx.name += "Unique";
            dbgtag = "create unique";
        }

        virtual std::string generateMemberBody() override {
            const std::string &id = last->identifier();
            const std::string &pass = ctx.createPassArguments();
            const auto &handle = ctx.gen.findHandle(last->original.type());
            std::string ownerArgument;
            if (handle.isSubclass()) {
                ownerArgument = ", this";
            }

            return ctx.gen.format(R"(
    {0} {1}(*this, {2});
    return Unique{0}({1}{3});
)",
                                  last->type(), id, pass, ownerArgument);

        }
    };

    class MemberResolverGet : public MemberResolver {
      protected:
        std::shared_ptr<VariableData> last;        

        std::string generateArray() {
            std::shared_ptr<VariableData> lenVar = last->getLengthVar();            

            std::string output;
            std::string decl;

            std::string arrayDecl = last->toString();
            std::string id = last->identifier();
            if (ctx.lastHandleVariant) {
                last->setType(last->original.type());
                last->setIdentifier(last->identifier() + "Handles");
                decl += "    " + last->toString() + ";\n";
            }
            else {
                decl += "    " + arrayDecl + ";\n";
            }
            if (ctx.pfnReturn == PFNReturnCategory::VK_RESULT) {
                decl += "    " + assignToResult("");
            }

            std::string call = generatePFNcall();
            std::string finish;
            if (ctx.lastHandleVariant) {
                finish += "    " + arrayDecl + ";\n";
            }
            finish += "    " + generateReturn(id) + "\n";

            if (lenVar->getIgnoreFlag()) {
                output += "    " + lenVar->type() + " " + lenVar->identifier() +
                          ";\n";
            }

            output += decl;
            if (ctx.pfnReturn == PFNReturnCategory::VK_RESULT) {

                output += ctx.gen.format(R"(    
    do {
      {0}
      if (result == Result::eSuccess && {1}) {
        {2}.resize({1});
        {0}
        //VULKAN_HPP_ASSERT({1} <= {2}.size());
      }
    } while (result == Result::eIncomplete);
    if (result == Result::eSuccess && {1} < {2}.size()) {
      {2}.resize({1});
    }    
)",
                                         call, lenVar->identifier(), id);

            } else {
                if (ctx.lastHandleVariant) {
                    std::cerr << "Warning: unhandled situation in " << ctx.name << std::endl;
                }
                last->setAltPFN("nullptr");
                std::string firstcall = generatePFNcall();
                output += ctx.gen.format(R"(
    {0}
    {1}.resize({2});
    {3}
    // VULKAN_HPP_ASSERT({2} <= {1}.size());
)",
                                         firstcall, id, lenVar->identifier(), call);
            }
            output += finish;
            return output;
        }

        std::string generateArrayWithSize() {
            std::string output;
            if (ctx.lastHandleVariant) {
                std::cerr << "Warning: unhandled situation in " << ctx.name << std::endl;
                return "";
            }

            std::string id = last->identifier();
            std::string sizeid = last->getLengthVar()->identifier();
            output += "    " + last->toString() + ";\n";
            if (const auto str = last->getTemplate(); !str.empty()) {
                output += "    " + id + ".resize(" + sizeid + " / sizeof(" + str + "));\n";
            }
            else {
                output += "    " + id + ".resize(" + sizeid + ");\n";
            }
            output += "    " + generatePFNcall() + "\n";
            output += "    " + generateReturn(id) + "\n";

            return output;
        }

        std::string generateSingle() {
            std::string output;
            if (ctx.lastHandleVariant) {
                std::cerr << "Warning: unhandled situation in " << ctx.name << std::endl;
                return "";
            } else {
                const std::string id = last->identifier();
                output += "    " + returnType + " " + id + ";\n";
                output += "    " + generatePFNcall() + "\n";
                output += "    " + generateReturn(id) + "\n";
            }
            return output;
        }

      public:
        MemberResolverGet(MemberContext &refCtx) : MemberResolver(refCtx) {
            last = ctx.getLastPointerVar();

            if (last->hasLengthVar() && last->flagArray()) {
                if (last.get()->original.type() == "void") {
                    if (last->flagArrayIn()) {
                        last->getLengthVar()->setIgnoreFlag(false);
                    }                    
                }

                last->getLengthVar()->removeLastAsterisk();

                last->convertToStdVector();
            } else {
                last->convertToReturn();
            }

            last->setIgnoreFlag(true);
            returnType = last->fullType();
            dbgtag = "get";
        }

        virtual std::string generateMemberBody() override {
            if (last->flagArray()) {
                if (last->hasLengthVar() && last->getLengthVar()->getIgnoreFlag()) {
                    return generateArray();
                }
                return generateArrayWithSize();
            }
            return generateSingle();
        }
    };

    class MemberResolverCreate : public MemberResolver {
      protected:
        std::shared_ptr<VariableData> last;

      public:
        MemberResolverCreate(MemberContext &refCtx) : MemberResolver(refCtx) {

            last = ctx.getLastPointerVar();

            if (last->flagArray()) {
                last->convertToStdVector();
            } else {
                last->convertToReturn();
            }

            last->setIgnoreFlag(true);
            last->setConst(false);
            returnType = last->fullType();

            if (ctx.lastHandleVariant) {
                last->setType(last->original.type());
                last->setIgnorePFN(true);
            }

            dbgtag = "create";
        }

        virtual std::string generateMemberBody() override {
            std::string output;
            if (last->flagArray()) {
                const auto &lenVar = last->getLengthVar();
                std::string id = lenVar->identifier();
                if (lenVar->getIgnoreFlag()) {
                    id = lenVar->getArrayVar()->identifier();
                }

                std::string size = id;
                if (last->isLenAttribIndirect()) {
                    if (lenVar->getSpecialType() == VariableData::TYPE_REFERENCE) {
                        size += ".";
                    }
                    else {
                        size += "->";
                    }
                    size += last->getLenAttribRhs();
                }
                else {
                    size = lenVar->toArgument();
                }

                output += "    " + last->toString() + ";\n";
                output +=
                    "    " + last->identifier() + ".resize(" + size + ");\n";

                output += "    " + generatePFNcall() + "\n";

                output += "    " + generateReturn(last->identifier()) + "\n";
            } else {
                if (ctx.lastHandleVariant) {
                    std::string arguments = ctx.createPassArguments();
                    std::string call = ctx.name + "Handle(" + arguments + ")";
                    output += "      return " + returnType + "(" + call +
                              ", *this);\n";
                } else {
                    const std::string &call = generatePFNcall();
                    std::string id = last->identifier();
                    output += "    " + last->type() + " " + id + ";\n";
                    output += "    " + call + "\n";
                    output += "    " + generateReturn(id) + "\n";
                }
            }
            return output;
        }

    };

    class MemberResolverAllocate : public MemberResolverCreate {
      public:
        MemberResolverAllocate(MemberContext &refCtx)
            : MemberResolverCreate(refCtx) {
            dbgtag = "allocate";
        }
    };

    class MemberResolverEnumerate : public MemberResolverGet {
      public:
        MemberResolverEnumerate(MemberContext &refCtx)
            : MemberResolverGet(refCtx) {
            dbgtag = "enumerate";
        }

    };

    PFNReturnCategory evaluatePFNReturn(const std::string &type) const {
        if (type == "void") {
            return PFNReturnCategory::VOID;
        } else if (type == "VkResult") {
            return PFNReturnCategory::VK_RESULT;
        }
        return PFNReturnCategory::OTHER;
    }

    bool getLastTwo(MemberContext &ctx,
                    std::pair<Variables::reverse_iterator,
                              Variables::reverse_iterator> &data) { // rename ?
        Variables::reverse_iterator last = ctx.params.rbegin();
        if (last != ctx.params.rend()) {
            Variables::reverse_iterator prevlast = std::next(last);
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

    template <typename T>
    void createOverload(MemberContext &ctx, const std::string &name, std::vector<std::unique_ptr<MemberResolver>> &secondary);

    void generateClassMemberCpp(MemberContext &ctx, GenOutputClass &out);

    void generateClassMembers(const HandleData &data, GenOutputClass &out);

    void generateClassConstructors(const HandleData &data, GenOutputClass &out);

    void generateUniqueClass(const HandleData &data);

    std::string_view getHandleSuperclass(const HandleData &data);

    void generateClass(const std::string &name, HandleData data);

    void parseCommands(XMLNode *node);

    std::string generateLoader();

    std::string genMacro(const Macro &m);

  public:
    Generator();

    void resetConfig();

    bool isLoaded() const { return root != nullptr; }

    void setOutputFilePath(const std::string &path);

    bool isOuputFilepathValid() const {
        return !std::filesystem::is_directory(outputFilePath);
    }

    std::string getOutputFilePath() const { return outputFilePath; }

    void load(const std::string &xmlPath);
    void unload();

    void generate();

    Platforms &getPlatforms() { return platforms; };

    Extensions &getExtensions() { return extensions; };

    Config &getConfig() { return cfg; }
    const Config &getConfig() const { return cfg; }

    bool isInNamespace(const std::string &str) const;
};

#endif // GENERATOR_HPP
