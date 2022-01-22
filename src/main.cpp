/*
MIT License

Copyright (c) 2021 guritchi

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#include <algorithm>
#include <functional>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <regex>
#include <stdexcept>
#include <unordered_set>
#include <vector>

#include "ArgumentsParser.hpp"
#include "FileHandle.hpp"
#include "XMLUtils.hpp"
#include "XMLVariableParser.hpp"
#include "tinyxml2.h"
using namespace tinyxml2;

static constexpr char const *HELP_TEXT{
    R"(Usage:
    -r, --reg       path to source registry file
    -s, --source    path to source directory
    -d, --dest      path to destination file)"};

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

static_assert( VK_HEADER_VERSION == 198, "Wrong VK_HEADER_VERSION!" );

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

#if !defined( VULKAN_HPP_NAMESPACE )
#  define VULKAN_HPP_NAMESPACE vk
#endif

#define VULKAN_HPP_STRINGIFY2( text ) #text
#define VULKAN_HPP_STRINGIFY( text )  VULKAN_HPP_STRINGIFY2( text )
#define VULKAN_HPP_NAMESPACE_STRING   VULKAN_HPP_STRINGIFY( VULKAN_HPP_NAMESPACE ))"};

static constexpr char const *RES_LIB_LOADER{
    R"(#ifdef _WIN32
#  define LIBHANDLE HINSTANCE
#else
#  define LIBHANDLE void*
#endif

class LibraryLoader {
protected:

    LIBHANDLE lib;
    uint32_t m_version;
    PFN_vkGetInstanceProcAddr m_vkGetInstanceProcAddr;

public:
#ifdef _WIN32
    static constexpr char const* defaultName = "vulkan-1.dll";
#else
    static constexpr char const* defaultName = "libvulkan.so.1";
#endif

    LibraryLoader() {}

    LibraryLoader(const std::string &name) {
        load(name);
    }

    void load(const std::string &name = defaultName) {
        unload();
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
        }
    }

    uint32_t version() const {
        return m_version;
    }

    ~LibraryLoader() {
        unload();
    }

    template<typename T>
    inline T getProcAddr(const char *name) const {
        return std::bit_cast<T>(m_vkGetInstanceProcAddr(nullptr, name));
    }

    const PFN_vkGetInstanceProcAddr &vkGetInstanceProcAddr {m_vkGetInstanceProcAddr};

};)"};

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

template <size_t N>
ArrayProxy( std::array<T, N> const & data ) VULKAN_HPP_NOEXCEPT
  : m_count( N )
  , m_ptr( data.data() )
{}

template <size_t N, typename B = T, typename std::enable_if<std::is_const<B>::value, int>::type = 0>
ArrayProxy( std::array<typename std::remove_const<T>::type, N> const & data ) VULKAN_HPP_NOEXCEPT
  : m_count( N )
  , m_ptr( data.data() )
{}

template <size_t N>
ArrayProxy( std::array<T, N> & data ) VULKAN_HPP_NOEXCEPT
  : m_count( N )
  , m_ptr( data.data() )
{}

template <size_t N, typename B = T, typename std::enable_if<std::is_const<B>::value, int>::type = 0>
ArrayProxy( std::array<typename std::remove_const<T>::type, N> & data ) VULKAN_HPP_NOEXCEPT
  : m_count( N )
  , m_ptr( data.data() )
{}

template <class Allocator = std::allocator<typename std::remove_const<T>::type>>
ArrayProxy( std::vector<T, Allocator> const & data ) VULKAN_HPP_NOEXCEPT
  : m_count( static_cast<uint32_t>( data.size() ) )
  , m_ptr( data.data() )
{}

template <class Allocator = std::allocator<typename std::remove_const<T>::type>,
          typename B      = T,
          typename std::enable_if<std::is_const<B>::value, int>::type = 0>
ArrayProxy( std::vector<typename std::remove_const<T>::type, Allocator> const & data ) VULKAN_HPP_NOEXCEPT
  : m_count( static_cast<uint32_t>( data.size() ) )
  , m_ptr( data.data() )
{}

template <class Allocator = std::allocator<typename std::remove_const<T>::type>>
ArrayProxy( std::vector<T, Allocator> & data ) VULKAN_HPP_NOEXCEPT
  : m_count( static_cast<uint32_t>( data.size() ) )
  , m_ptr( data.data() )
{}

template <class Allocator = std::allocator<typename std::remove_const<T>::type>,
          typename B      = T,
          typename std::enable_if<std::is_const<B>::value, int>::type = 0>
ArrayProxy( std::vector<typename std::remove_const<T>::type, Allocator> & data ) VULKAN_HPP_NOEXCEPT
  : m_count( static_cast<uint32_t>( data.size() ) )
  , m_ptr( data.data() )
{}

#  if defined( VULKAN_HPP_SUPPORT_SPAN )
template <size_t N = std::dynamic_extent>
ArrayProxy( std::span<T, N> const & data ) VULKAN_HPP_NOEXCEPT
  : m_count( static_cast<uint32_t>( data.size() ) )
  , m_ptr( data.data() )
{}

template <size_t N                                                    = std::dynamic_extent,
          typename B                                                  = T,
          typename std::enable_if<std::is_const<B>::value, int>::type = 0>
ArrayProxy( std::span<typename std::remove_const<T>::type, N> const & data ) VULKAN_HPP_NOEXCEPT
  : m_count( static_cast<uint32_t>( data.size() ) )
  , m_ptr( data.data() )
{}

template <size_t N = std::dynamic_extent>
ArrayProxy( std::span<T, N> & data ) VULKAN_HPP_NOEXCEPT
  : m_count( static_cast<uint32_t>( data.size() ) )
  , m_ptr( data.data() )
{}

template <size_t N                                                    = std::dynamic_extent,
          typename B                                                  = T,
          typename std::enable_if<std::is_const<B>::value, int>::type = 0>
ArrayProxy( std::span<typename std::remove_const<T>::type, N> & data ) VULKAN_HPP_NOEXCEPT
  : m_count( static_cast<uint32_t>( data.size() ) )
  , m_ptr( data.data() )
{}
#  endif

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
};)"};

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

struct EnumExtendsValue {
    std::string_view name;
    const char *bitpos;
    const char *protect;
    const char *alias;
    bool supported;
};

struct EnumDeclaration {
    std::string attribRequires;
    std::vector<EnumExtendsValue> extendValues;
};

static const std::string NAMESPACE{"vk20"};
static const std::string FILEPROTECT{"VULKAN_20_HPP"};

static FileHandle file;       // main output file
static FileHandle file2;      // temporary file
static std::string sourceDir; // additional source files directory

static std::unordered_set<std::string> generatedStructs; // helper list
static std::unordered_set<std::string> tags; // list of tags from <tags>

static std::map<std::string, std::string_view>
    platforms; // maps platform name to protect (#if defined PROTECT)
static std::map<std::string, std::pair<std::string_view, bool>>
    extensions; // maps extension to protect as reference

struct VkStructData {
    enum VkStructType { VK_STRUCT, VK_UNION } type;
    XMLNode *node;
    std::vector<std::string> aliases;
};

static std::unordered_map<std::string, VkStructData> vkStructs;

struct VkFlagData {
    std::string name;
    bool hasRequire;
    bool alias;
};

static std::unordered_map<std::string, VkFlagData> vkFlags;
static std::unordered_map<std::string, EnumDeclaration> enums;
static std::unordered_map<std::string, std::vector<std::string>> enumsAlias;
static std::unordered_map<std::string,
                          std::vector<std::pair<std::string, const char *>>>
    enumMembers;

// main parse functions
static void parsePlatforms(XMLNode *);
static void parseTypeDeclarations(XMLNode *);
static void parseFeature(XMLNode *);
static void parseExtensions(XMLNode *);
static void parseTags(XMLNode *);
static void parseEnums(XMLNode *);
static void parseTypes(XMLNode *);
static void parseCommands(XMLNode *);

static void generateEnumFlags();

// specifies order of parsing vk.xml registry,
static const std::vector<std::pair<std::string, std::function<void(XMLNode *)>>>
    rootParseOrder{
        {"platforms", parsePlatforms}, {"types", parseTypeDeclarations},
        {"feature", parseFeature},     {"extensions", parseExtensions},
        {"tags", parseTags},           {"enums", parseEnums},
        {"commands", parseCommands},   {"types", parseTypes}};

static VariableData invalidVar(VariableData::TYPE_INVALID);

static std::string stripVkPrefix(const std::string &str);

using VariableArray = std::vector<std::shared_ptr<VariableData>>;

// holds information about class member (function)
struct ClassMemberData {
    std::string name;     // identifier
    std::string type;     // return type
    VariableArray params; // list of arguments

    // creates function arguments signature
    std::string createProtoArguments(const std::string &className,
                                     bool useOriginal = false) const {
        std::string out;
        for (const auto &data : params) {
            if (data->original.type() == "Vk" + className ||
                data->getIgnoreFlag()) {
                continue;
            }
            out += useOriginal ? data->originalToString() : data->toString();
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

struct ClassData {
    std::string getAddrSource;
    std::string parent;
    std::vector<ClassMemberData> members;
    ClassMemberData createMember;
    ClassMemberData getAddrMember;
};

static std::unordered_map<std::string, ClassData> classMetaData;

template <template <class...> class TContainer, class T, class A>
static bool isInContainter(const TContainer<T, A> &array, T entry) {
    return std::find(std::begin(array), std::end(array), entry) !=
           std::end(array);
}

static bool isStructOrUnion(const std::string &name) {
    return vkStructs.find(name) != vkStructs.end();
}

// tries to match str in extensions, if found returns pointer to protect,
// otherwise nullptr
static std::pair<std::string_view, bool> *
findExtensionProtect(const std::string &str) {
    auto it = extensions.find(str);
    if (it != extensions.end()) {
        return &it->second;
    }
    return nullptr;
}

// #if defined encapsulation
static void withExtensionProtect(const std::string &name,
                                 std::function<void()> function) {
    const std::pair<std::string_view, bool> *protect =
        findExtensionProtect(name);
    if (protect && !protect->second) {
        // vk does not support
        return;
    }
    if (protect) {
        file.get() << "#if defined(" << protect->first << ")" << ENDL;
    }

    function();

    if (protect) {
        file.get() << "#endif //" << protect->first << ENDL;
    }
}

static void withExtensionProtect2(const std::string &name,
                                  std::function<void()> function) {
    const std::pair<std::string_view, bool> *protect =
        findExtensionProtect(name);
    if (protect && !protect->second) {
        // vk does not support
        return;
    }
    if (protect) {
        file2.get() << "#if defined(" << protect->first << ")" << ENDL;
    }

    function();

    if (protect) {
        file2.get() << "#endif //" << protect->first << ENDL;
    }
}

static void withProtect(const char *protect, std::function<void()> function) {
    if (protect) {
        file.get() << "#if defined(" << protect << ")" << ENDL;
    }

    function();

    if (protect) {
        file.get() << "#endif //" << protect << ENDL;
    }
}

static std::string strRemoveTag(std::string &str) {
    std::string suffix;
    auto it = str.rfind('_');
    if (it != std::string::npos) {
        suffix = str.substr(it + 1);
        if (tags.find(suffix) != tags.end()) {
            str.erase(it);
        } else {
            suffix.clear();
        }
    }

    for (auto &t : tags) {
        if (str.ends_with(t)) {
            str.erase(str.size() - t.size());
            return t;
        }
    }
    return suffix;
}

static std::string strWithoutTag(const std::string &str) {
    std::string out = str;
    for (const std::string &tag : tags) {
        if (out.ends_with(tag)) {
            out.erase(out.size() - tag.size());
            break;
        }
    }
    return out;
}

static std::pair<std::string, std::string> snakeToCamelPair(std::string str) {
    std::string suffix = strRemoveTag(str);
    std::string out = convertSnakeToCamel(str);

    out = std::regex_replace(out, std::regex("bit"), "Bit");
    out = std::regex_replace(out, std::regex("Rgba10x6"), "Rgba10X6");

    return std::make_pair(out, suffix);
}

static std::string snakeToCamel(const std::string &str) {
    const auto &p = snakeToCamelPair(str);
    return p.first + p.second;
}

static std::string enumConvertCamel(const std::string &enumName,
                                    std::string value) {
    std::string out = snakeToCamel(value);
    strStripPrefix(out, "vk");

    size_t i, s = enumName.size();
    for (i = s; i >= 0; --i) {
        std::string p = enumName.substr(0, i);
        if ((i == s || std::isupper(enumName[i])) && out.starts_with(p)) {
            out.erase(0, i);
            break;
        }
    }

    for (int j = i; j < s; ++j) {
        std::string p = enumName.substr(j, s);
        if (std::isupper(p[0]) && out.ends_with(p)) {
            out.erase(out.size() - (s - j));
            break;
        }
    }

    return "e" + out;
}

static std::string toCppStyle(const std::string &str,
                              bool firstLetterCapital = false) {
    std::string out = stripVkPrefix(str);
    if (!out.empty()) {
        if (!firstLetterCapital) {
            out[0] = std::tolower(out[0]);
        }
    }
    return out;
}

static void parseXML(XMLElement *root) {
    // maps all root XMLNodes to their tag identifier
    std::vector<std::pair<std::string, XMLNode *>> rootTable;
    for (XMLNode &node : Nodes(root)) {
        rootTable.push_back(std::make_pair(node.Value(), &node));
    }

    // call each function in rootParseOrder with corresponding XMLNode
    for (auto &key : rootParseOrder) {
        for (auto &n : rootTable) {
            // find tag id

            if (n.first == key.first) {
                if (key.first == "commands") {
                    generateEnumFlags();
                }
                key.second(n.second); // call function(node*)
            }
        }
    }
}

static void generateReadFromFile(const std::string &path) {
    std::ifstream inputFile;
    inputFile.open(path);
    if (!inputFile.is_open()) {
        throw std::runtime_error("Can't open file: " + path);
    }

    for (std::string line; getline(inputFile, line);) {
        file.writeLine(line);
    }

    inputFile.close();
}

static void generateFile(XMLElement *root, std::string f2path) {
    //    file2.writeLine("#ifndef " + FILEPROTECT + "_FUNCS");
    //    file2.writeLine("#define " + FILEPROTECT + "_FUNCS");
    //    file2.writeLine("namespace " + NAMESPACE);
    //    file2.writeLine("{");
    //    file2.pushIndent();

    file.writeLine("#ifndef " + FILEPROTECT);
    file.writeLine("#define " + FILEPROTECT);

    file.write(RES_HEADER);

    file.writeLine("namespace " + NAMESPACE);
    file.writeLine("{");

    file.pushIndent();
#ifdef VK_DEPENDENCY
    file.writeLine("using namespace vk;");
#endif

    file.write(RES_BASE_TYPES);
    file.write(RES_OTHER_HPP);
    file.write(RES_FLAGS_HPP);
    file.write(RES_LIB_LOADER);

    parseXML(root);

    file.write(RES_ARRAY_PROXY_HPP);
    file.write(RES_ERROR_HPP);

    file.popIndent();

    file.get() << std::endl;

    file2.close();

    std::ifstream infile(f2path);

    std::string line;
    while (std::getline(infile, line)) {
        file.get() << line << std::endl;
    }
    file.get() << std::endl;

    file.writeLine("}");

    file.writeLine("#endif //" + FILEPROTECT);

    //    file2.popIndent();
    //    file2.writeLine("}");
    //    file2.writeLine("#endif //" + FILEPROTECT + "_FUNCS");
}

int main(int argc, char **argv) {
    try {
        ArgOption helpOption{"-h", "--help"};
        ArgOption xmlOption{"-r", "--reg", true};
        ArgOption sourceOption{"-s", "--source", true};
        ArgOption destOption{"-d", "--dest", true};
        ArgParser p({&helpOption, &xmlOption, &sourceOption, &destOption});
        p.parse(argc, argv);
        // help option block
        if (helpOption.set) {
            std::cout << HELP_TEXT;
            return 0;
        }
        // argument check
        if (!destOption.set || !xmlOption.set || !sourceOption.set) {
            throw std::runtime_error("Missing arguments. See usage.");
        }

        sourceDir = sourceOption.value;

        file.open(destOption.value);
        std::string f2path = std::regex_replace(
            destOption.value, std::regex(".hpp"), "_funcs.hpp");
        file2.open(f2path);
        file2.pushIndent();

        XMLDocument doc;
        if (XMLError e = doc.LoadFile(xmlOption.value.c_str());
            e != XML_SUCCESS) {
            throw std::runtime_error("XML load failed: " + std::to_string(e) +
                                     " (file: " + xmlOption.value + ")");
        }

        XMLElement *root = doc.RootElement();
        if (!root) {
            throw std::runtime_error("XML file is empty");
        }

        generateFile(root, f2path);

        std::cout << "Parsing done" << ENDL;

    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }

    file.close();
    return 0;
}

std::vector<VariableData> parseStructMembers(XMLElement *node,
                                             std::string &structType,
                                             std::string &structTypeValue) {
    std::vector<VariableData> members;
    // iterate contents of <type>, filter only <member> children
    for (XMLElement *member : Elements(node) | ValueFilter("member")) {
        std::string out;

        XMLVariableParser parser{member}; // parse <member>

        std::string type = parser.type();
        std::string name = parser.identifier();

        if (const char *values = member->ToElement()->Attribute("values")) {
            std::string value = enumConvertCamel(type, values);
            parser.setAssignment(type + "::" + value);
            if (name == "sType") { // save sType information for structType
                structType = type;
                structTypeValue = value;
            }
        } else {
            if (name == "sType") {
                out += " = StructureType::eApplicationInfo";
            } else {
                out += " = {}";
            }
        }

        members.push_back(parser);
    }
    return members;
}

struct GenStructTempData {
    std::string name;
    bool structOrUnion;
    XMLElement *node;
    std::vector<std::string> typeList;
};

std::list<GenStructTempData> genStructStack;

static void generateStructCode(std::string name, const std::string &structType,
                               const std::string &structTypeValue,
                               const std::vector<VariableData> &members) {
    file.writeLine("struct " + name);
    file.writeLine("{");

    file.pushIndent();
    if (!structType.empty() && !structTypeValue.empty()) { // structType
        file.writeLine("static VULKAN_HPP_CONST_OR_CONSTEXPR " + structType +
                       " structureType = " + structType +
                       "::" + structTypeValue + ";");
        file.get() << ENDL;
    }
    // structure members
    for (const auto &m : members) {
        file.writeLine(m.toStringWithAssignment() + ";");
    }
    file.get() << ENDL;

    std::string alt = "Noinit";
    file.writeLine("struct " + alt + " {");
    file.pushIndent();
    for (const auto &m : members) {
        file.writeLine(m.toString() + ";");
    }
    file.get() << ENDL;
    file.writeLine(alt + "() = default;");

    file.get() << ENDL;
    file.writeLine("operator " + name +
                   " const&() const { return *std::bit_cast<const " + name +
                   "*>(this); }");
    file.writeLine("operator " + name + " &() { return *std::bit_cast<" +
                   name + "*>(this); }");
    file.popIndent();
    file.writeLine("};");

    file.writeLine("operator " + NAMESPACE + "::" + name +
                   "*() { return this; }");
#ifdef VK_DEPENDENCY
    file.writeLine("operator vk::" + name +
                   "&() { return *std::bit_cast<vk::" + name +
                   "*>(this); }");
#endif

    file.writeLine("operator Vk" + name +
                   " const&() const { return *std::bit_cast<const Vk" +
                   name + "*>(this); }");
    file.writeLine("operator Vk" + name + " &() { return *std::bit_cast<Vk" +
                   name + "*>(this); }");

    file.popIndent();

    file.writeLine("};");
}

static void generateUnionCode(std::string name,
                              const std::vector<VariableData> &members) {
    file.writeLine("union " + name);
    file.writeLine("{");

    file.pushIndent();
    // union members
    for (const auto &m : members) {
        file.writeLine(m.toString() + ";");
    }
    file.get() << ENDL;

    file.writeLine("operator " + NAMESPACE + "::" + name +
                   "*() { return this; }");
#ifdef VK_DEPENDENCY
    file.writeLine("operator vk::" + name +
                   "&() { return *std::bit_cast<vk::" + name +
                   "*>(this); }");
#endif

    file.writeLine("operator Vk" + name +
                   " const&() const { return *std::bit_cast<const Vk" +
                   name + "*>(this); }");
    file.writeLine("operator Vk" + name + " &() { return *std::bit_cast<Vk" +
                   name + "*>(this); }");

    file.popIndent();
    file.writeLine("};");
}

static void generateStruct(std::string name, const std::string &structType,
                           const std::string &structTypeValue,
                           const std::vector<VariableData> &members,
                           bool structOrUnion) {
    withExtensionProtect(name, [&] {
        strStripVk(name);

        if (structOrUnion) {
            generateStructCode(name, structType, structTypeValue, members);
        } else {
            generateUnionCode(name, members);
        }

        generatedStructs.emplace(name);
    });
}

void parseStruct(XMLElement *node, std::string name, bool structOrUnion) {
    if (const char *aliasAttrib = node->Attribute("alias")) {
        return;
    }

    std::string structType{}, structTypeValue{}; // placeholders
    std::vector<VariableData> members =
        parseStructMembers(node, structType, structTypeValue);

    std::vector<std::string> typeList;
    for (const auto &m : members) {
        std::string t = m.type();
        if (!m.isPointer() && isStructOrUnion("Vk" + t)) {
            typeList.push_back(t);
        }
    }
    const auto hasAllDeps = [](const std::vector<std::string> &types) {
        for (const auto &t : types) {
            if (isStructOrUnion("Vk" + t)) {
                const auto &it = generatedStructs.find(t);
                if (it == generatedStructs.end()) {
                    return false;
                }
            }
        }
        return true;
    };

    if (!hasAllDeps(typeList) && name != "VkBaseInStructure" &&
        name != "VkBaseOutStructure") {
        GenStructTempData d;
        d.name = name;
        d.node = node;
        d.structOrUnion = structOrUnion;
        for (const auto &t : typeList) {
            d.typeList.push_back(t);
        }
        genStructStack.push_back(d);
        hasAllDeps(typeList);
        return;
    }

    generateStruct(name, structType, structTypeValue, members, structOrUnion);

    for (auto it = genStructStack.begin(); it != genStructStack.end(); ++it) {
        if (hasAllDeps(it->typeList)) {
            std::string structType{}, structTypeValue{};
            std::vector<VariableData> members =
                parseStructMembers(it->node, structType, structTypeValue);
            generateStruct(it->name, structType, structTypeValue, members,
                           it->structOrUnion);
            it = genStructStack.erase(it);
            --it;
        }
    }
}

void parsePlatforms(XMLNode *node) {
    std::cout << "Parsing platforms" << ENDL;
    // iterate contents of <platforms>, filter only <platform> children
    for (XMLElement *platform : Elements(node) | ValueFilter("platform")) {
        const char *name = platform->Attribute("name");
        const char *protect = platform->Attribute("protect");
        if (name && protect) {
            platforms.emplace(name, protect);
        }
    }
    std::cout << "Parsing platforms done" << ENDL;
}

static void parseEnumExtend(XMLElement &node, const char *protect,
                            bool flagSupported) {
    const char *extends = node.Attribute("extends");
    const char *bitpos = node.Attribute("bitpos");
    const char *name = node.Attribute("name");
    const char *alias = node.Attribute("alias");

    if (extends && name) {
        auto it = enums.find(extends);
        if (it != enums.end()) {
            EnumExtendsValue data;
            data.name = name;
            data.bitpos = bitpos;
            data.protect = protect;
            data.supported = flagSupported;
            data.alias = alias;

            bool dup = false;
            for (auto &v : it->second.extendValues) {
                if (std::string_view{v.name} == std::string_view{name}) {
                    dup = true;
                    break;
                }
            }

            if (!dup) {
                it->second.extendValues.push_back(data);
            }

        } else {
            std::cerr << "err: Cant find enum: " << extends << std::endl;
        }
    }
}

void parseFeature(XMLNode *node) {
    std::cout << "Parsing feature" << ENDL;
    const char *name = node->ToElement()->Attribute("name");
    if (name) {
        for (XMLElement *require : Elements(node) | ValueFilter("require")) {
            for (XMLElement &entry : Elements(require)) {
                const std::string_view value = entry.Value();

                if (value == "enum") {
                    parseEnumExtend(entry, nullptr, true);
                }
            }
        }
    }

    std::cout << "Parsing feature done" << ENDL;
}

void parseExtensions(XMLNode *node) {
    std::cout << "Parsing extensions" << ENDL;
    // iterate contents of <extensions>, filter only <extension> children
    for (XMLElement *extension : Elements(node) | ValueFilter("extension")) {
        const char *supported = extension->Attribute("supported");
        bool flagSupported = true;
        if (supported && std::string_view(supported) == "disabled") {
            flagSupported = false;
        }

        const char *platform = extension->Attribute("platform");

        const char *protect = nullptr;

        if (platform) {
            auto it = platforms.find(platform);
            if (it != platforms.end()) {
                protect = it->second.data();
            } else {
                std::cerr << "err: Unknown platform in extensions: " << platform
                          << std::endl;
            }
        } else {
            // std::cerr << "err: Cant find platform" << std::endl;
        }

        // iterate contents of <extension>, filter only <require> children
        for (XMLElement *require :
             Elements(extension) | ValueFilter("require")) {
            // iterate contents of <require>
            for (XMLElement &entry : Elements(require)) {
                const std::string_view value = entry.Value();
                if (value == "command" && protect) {
                    const char *name = entry.Attribute("name");
                    if (name) { // pair extension name with platform protect
                        extensions.emplace(
                            name, std::make_pair(protect, flagSupported));
                    }
                } else if (value == "type" && protect) {
                    const char *name = entry.Attribute("name");
                    if (name) { // pair extension name with platform protect
                        extensions.emplace(
                            name, std::make_pair(protect, flagSupported));
                    }
                } else if (value == "enum") {
                    parseEnumExtend(entry, protect, flagSupported);
                }
            }
        }
    }
    std::cout << "Parsing extensions done" << ENDL;
}

void parseTags(XMLNode *node) {
    std::cout << "Parsing tags" << ENDL;
    // iterate contents of <tags>, filter only <tag> children
    for (XMLElement *tag : Elements(node) | ValueFilter("tag")) {
        const char *name = tag->Attribute("name");
        if (name) {
            tags.emplace(name);
        }
    }
    std::cout << "Parsing tags done" << ENDL;
}

void generateEnum(std::string name, XMLNode *node, const std::string &bitmask) {
    auto it = enums.find(name);
    if (it == enums.end()) {
        std::cerr << "cant find " << name << "in enums" << std::endl;
        return;
    }

    auto alias = enumsAlias.find(name);

    std::string ext = name;
    if (!bitmask.empty()) {
        ext = std::regex_replace(ext, std::regex("FlagBits"), "Flags");
    }

    withExtensionProtect(ext, [&] {
        name = toCppStyle(name, true);
        std::string enumStr =
            bitmask.empty()
                ? name
                : std::regex_replace(name, std::regex("FlagBits"), "Bit");

        std::vector<std::pair<std::string, const char *>> values;

        // iterate contents of <enums>, filter only <enum> children
        for (XMLElement *e : Elements(node) | ValueFilter("enum")) {
            const char *name = e->Attribute("name");
            const char *alias = e->Attribute("alias");
            if (name) {
                values.push_back(std::make_pair(name, alias));
            }
        }

        std::string optionalInherit = "";
        if (!bitmask.empty()) {
            optionalInherit += " : " + bitmask;
        }
        file.writeLine("enum class " + name + optionalInherit +
                       " {"); // + " // " + cname + " - " + tag);
        file.pushIndent();

        std::vector<std::pair<std::string, const char *>> enumMembersList;
        const auto genEnumValue = [&](std::string value, bool last,
                                      const char *protect, bool add) {
            std::string cpp = enumConvertCamel(enumStr, value);
            // strRemoveTag(cpp);
            std::string comma = "";
            if (!last) {
                comma = ",";
            }
            bool dup = false;
            for (auto &e : enumMembersList) {
                if (e.first == cpp) {
                    dup = true;
                    break;
                }
            }

            if (!dup) {
                file.writeLine(cpp + " = " + value + comma);
                if (add) {
                    enumMembersList.push_back(std::make_pair(cpp, protect));
                }
            }
        };

        size_t extSize = it->second.extendValues.size();
        for (size_t i = 0; i < values.size(); ++i) {
            std::string c = values[i].first;
            genEnumValue(c, i == values.size() - 1 && extSize == 0, nullptr,
                         values[i].second == nullptr);
        }

        for (size_t i = 0; i < extSize; ++i) {
            const auto &v = it->second.extendValues[i];
            if (!v.supported) {
                continue;
            }

            if (v.protect) {
                withProtect(v.protect, [&] {
                    genEnumValue(v.name.data(), i == extSize, v.protect,
                                 v.alias == nullptr);
                });
            } else {
                genEnumValue(v.name.data(), i == extSize, nullptr,
                             v.alias == nullptr);
            }
        }

        file.popIndent();
        file.writeLine("};\n");

        file.writeLine("VULKAN_HPP_INLINE std::string to_string(" + name +
                       " value) {");
        file.pushIndent();
        file.writeLine("switch (value){");
        file.pushIndent();

        for (const auto &m : enumMembersList) {
            std::string str = m.first.c_str();
            strStripPrefix(str, "e");
            std::string code =
                "case " + name + "::" + m.first + ": return \"" + str + "\";";
            withProtect(m.second, [&] { file.writeLine(code); });
        }

        file.writeLine("default: return \"invalid ( \" + "
                       "toHexString(static_cast<uint32_t>(value)) + \" )\";");

        file.popIndent();
        file.writeLine("}");
        file.popIndent();
        file.writeLine("}");

        if (alias != enumsAlias.end()) {
            for (auto &a : alias->second) {
                std::string n = toCppStyle(a, true);
                file.writeLine("using " + n + " = " + name + ";");
            }
        }

        enumMembers[name] = enumMembersList;
    });
}

void parseEnums(XMLNode *node) {
    const char *name = node->ToElement()->Attribute("name");
    if (!name) {
        std::cerr << "Can't get name of enum" << std::endl;
        return;
    }
    if (std::string_view{name} != "VkDebugReportFlagBitsEXT") {
        // return;
    }

    const char *type = node->ToElement()->Attribute("type");
    if (!type) {
        return;
    }
    bool isBitmask = std::string_view(type) == "bitmask";
    bool isEnum = std::string_view(type) == "enum";
    std::string bitmask;
    if (isBitmask) {
        isEnum = true;

        auto it = vkFlags.find(name);
        if (it != vkFlags.end()) {
            bitmask = it->second.name;

        } else {
            bitmask = name;
            bitmask =
                std::regex_replace(bitmask, std::regex("FlagBits"), "Flags");
            if (std::string(name) == "VkShaderModuleCreateFlagBits") {
                std::cerr << "Warn: missing bitmask information: " << name
                          << std::endl;
                std::cerr << "bitmask: " << bitmask << std::endl;
            }
        }
    }

    if (isEnum) {
        generateEnum(name, node, bitmask);
    }
}

void generateEnumFlags() {
    std::cout << "Generating enum flags" << std::endl;

    for (auto &e : vkFlags) {
        std::string l = toCppStyle(e.second.name, true);
        std::string r = toCppStyle(e.first, true);

        r = std::regex_replace(r, std::regex("Flags"), "FlagBits");

        withExtensionProtect(e.second.name, [&] {
            if (e.second.alias) {
                file.writeLine("using " + l + " = " + r + ";");
            } else {
                std::string _l =
                    std::regex_replace(l, std::regex("Flags"), "FlagBits");
                if (!e.second.hasRequire &&
                    enumMembers.find(_l) == enumMembers.end()) {
                    std::string _r = e.first;
                    file.writeLine("enum class " + _l + " : " + _r + " {");
                    file.writeLine("};");
                }
                file.writeLine("using " + l + " = Flags<" + r + ">;");

                bool hasInfo = false;
                const auto it = enumMembers.find(r);
                if (it != enumMembers.end()) {
                    hasInfo = !it->second.empty();
                }

                if (hasInfo) {
                    file.writeLine("template <>");
                    file.writeLine("struct FlagTraits<" + r + "> {");
                    file.pushIndent();
                    file.writeLine("enum : VkFlags {");
                    file.pushIndent();

                    file.writeLine("allFlags = " +
                                   std::string(hasInfo ? "" : "0"));
                    file.pushIndent();
                    const auto &members = it->second;
                    for (size_t i = 0; i < members.size(); ++i) {
                        const auto &pair = members[i];
                        std::string member =
                            "VkFlags(" + r + "::" + pair.first + ")";
                        if (i != 0) {
                            member = "| " + member;
                        }
                        withProtect(pair.second,
                                    [&] { file.writeLine(member); });
                    }
                    file.popIndent();

                    file.popIndent();
                    file.writeLine("};");
                    file.popIndent();
                    file.writeLine("};");
                }
            }
        });
    }

    std::cout << "Generating enum flags done" << std::endl;
}

static std::unordered_set<std::string> vkHandles;

void parseTypeDeclarations(XMLNode *node) {
    std::cout << "Parsing declarations" << std::endl;

    // iterate contents of <types>, filter only <type> children
    for (XMLElement *type : Elements(node) | ValueFilter("type")) {
        const char *cat = type->Attribute("category");
        if (!cat) {
            // warn?
            continue;
        }
        const char *name = type->Attribute("name");

        if (strcmp(cat, "enum") == 0) {
            if (name) {
                const char *alias = type->Attribute("alias");
                if (alias) {
                    auto it = enumsAlias.find(alias);
                    if (it == enumsAlias.end()) {
                        enumsAlias.emplace(alias,
                                           std::vector<std::string>{{name}});
                    } else {
                        it->second.push_back(name);
                    }
                } else {
                    enums.emplace(name, EnumDeclaration{.attribRequires = ""});
                }
            }
        } else if (strcmp(cat, "bitmask") == 0) {
            // typedef VkFlags ...

            const char *name = type->Attribute("name");
            const char *aliasAttrib = type->Attribute("alias");
            std::string alias = aliasAttrib ? aliasAttrib : "";
            const char *reqAttrib = type->Attribute("requires");
            std::string req; // rename
            bool hasReq = false;
            bool hasAlias = false;

            if (!aliasAttrib) {
                XMLElement *nameElem = type->FirstChildElement("name");
                if (!nameElem) {
                    continue;
                }
                name = nameElem->GetText();
                if (!name) {
                    continue;
                }

                if (reqAttrib) {
                    req = reqAttrib;
                    hasReq = true;
                } else {
                    const char *bitAttrib = type->Attribute("bitvalues");

                    if (bitAttrib) {
                        req = bitAttrib;
                    } else {
                        req = name;
                    }
                }
            } else {
                if (!name) {
                    std::cerr << "Error: bitmas alias has no name" << std::endl;
                    continue;
                }
                req = alias;
                hasAlias = true;
            }

            vkFlags.emplace(req, VkFlagData{.name = name,
                                            .hasRequire = hasReq,
                                            .alias = hasAlias});

        } else if (strcmp(cat, "handle") == 0) {
            XMLElement *nameElem = type->FirstChildElement("name");
            if (nameElem) {
                const char *name = nameElem->GetText();
                if (name) {
                    vkHandles.emplace(name);
                }
            }
        } else if (strcmp(cat, "struct") == 0 || strcmp(cat, "union") == 0) {
            if (name) {
                const char *alias = type->Attribute("alias");
                VkStructData d;
                d.type = (strcmp(cat, "struct") == 0) ? VkStructData::VK_STRUCT
                                                      : VkStructData::VK_UNION;

                if (alias) {
                    const auto &it = vkStructs.find(alias);
                    if (it == vkStructs.end()) {
                        d.node = nullptr;
                        d.aliases.push_back(name);
                        vkStructs.emplace(alias, d);
                    } else {
                        it->second.aliases.push_back(name);
                    }
                } else {
                    const auto &it = vkStructs.find(name);
                    if (it == vkStructs.end()) {
                        d.node = type;
                        vkStructs.emplace(name, d);
                    } else {
                        it->second.node = type;
                    }
                }
            }
        }
    }

    std::cout << "Parsing declarations done" << ENDL;
}

static void generateClass(const std::string &name);

static void generateStructDecl(const std::string &name, const VkStructData &d) {
    withExtensionProtect(name, [&] {
        std::string cppname = strStripVk(name);

        if (d.type == VkStructData::VK_STRUCT) {
            file.writeLine("struct " + cppname + ";");
        } else {
            file.writeLine("union " + cppname + ";");
        }

        for (auto &a : d.aliases) {
            file.writeLine("using " + strStripVk(a) + " = " + cppname + ";");
        }
    });
}

static void generateClassDecl(const std::string &name) {
    withExtensionProtect(name, [&] {
        std::string className = toCppStyle(name, true);
        std::string handle = "m_" + toCppStyle(name);
        file.writeLine("class " + className + "Base {");
        file.writeLine("protected:");
        file.pushIndent();
        file.writeLine(name + " " + handle + " = {};");
        file.popIndent();
        file.writeLine("public:");
        file.pushIndent();
        // operators
        file.writeLine("operator "
                       "Vk" +
                       className + "() const {");
        file.pushIndent();
        file.writeLine("return " + handle + ";");
        file.popIndent();
        file.writeLine("}");

        file.popIndent();
        file.writeLine("};");
        file.writeLine("");

        file.writeLine("class " + className + ";");
    });
}

void parseTypes(XMLNode *node) {
    static const auto parseType = [&](XMLElement *type) {
        const char *cat = type->Attribute("category");
        if (!cat) {
            return;
        }
        const char *name = type->Attribute("name");
        if (strcmp(cat, "struct") == 0) {
            if (name) {
                parseStruct(type, name, true);
            }
        } else if (strcmp(cat, "union") == 0) {
            if (name) {
                parseStruct(type, name, false);
            }
        }
    };

    std::cout << "Parsing types" << std::endl;

    for (auto &e : vkStructs) {
        generateStructDecl(e.first, e.second);
    }

    for (auto &e : vkHandles) {
        generateClassDecl(e);
    }
    for (auto &e : vkHandles) {
        generateClass(e);
    }

    // iterate contents of <types>, filter only <type> children
    for (XMLElement *type : Elements(node) | ValueFilter("type")) {
        parseType(type);
    }

    for (auto it = genStructStack.begin(); it != genStructStack.end(); ++it) {
        std::string structType{}, structTypeValue{}; // placeholders
        std::vector<VariableData> members =
            parseStructMembers(it->node, structType, structTypeValue);
        generateStruct(it->name, structType, structTypeValue, members,
                       it->structOrUnion);
    }

    std::cout << "Parsing types done" << ENDL;
}

ClassMemberData parseClassMember(XMLElement *command) {
    ClassMemberData m;
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

std::vector<ClassMemberData>
parseClassMembers(const std::vector<XMLElement *> &elements) {
    std::vector<ClassMemberData> list;
    for (XMLElement *command : elements) {
        list.push_back(parseClassMember(command));
    }
    return list;
}

std::string generateClassMemberCStyle(const std::string &className,
                                      const std::string &handle,
                                      const std::string &protoName,
                                      const ClassMemberData &m) {
    std::string protoArgs = m.createProtoArguments(className, true);
    std::string innerArgs = m.createPFNArguments(className, handle, true);
    file.writeLine("inline " + m.type + " " + protoName + "(" + protoArgs +
                   ") { // C");

    file.pushIndent();
    std::string cmdCall;
    if (m.type != "void") {
        cmdCall += "return ";
    }
    cmdCall += ("m_" + m.name + "(" + innerArgs + ");");
    file.writeLine(cmdCall);
    file.popIndent();

    file.writeLine("}");
    return protoArgs;
}

enum class PFNReturnCategory { OTHER, VOID, VK_RESULT, VK_OBJECT };

// debug
// static std::string toString(MEMBER_RETURN category) {
//    switch (category) {
//    case MEMBER_RETURN::INVALID:         return "N/A";
//    case MEMBER_RETURN::VOID:            return "void";
//    case MEMBER_RETURN::RESULT:          return "result";
//    case MEMBER_RETURN::STRUCT:          return "struct";
//    case MEMBER_RETURN::ARRAY_IN:        return "array_input";
//    case MEMBER_RETURN::ARRAY_OUT:       return "array_output";
//    case MEMBER_RETURN::ARRAY_OUT_ALLOC: return "array_alloc";
//    }
//    return "";
//}

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
    const std::string &className;
    const std::string &handle;
    const std::string &protoName;
    const PFNReturnCategory &pfnReturn;
    ClassMemberData mdata;
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

MemberNameCategory evalMemberNameCategory(MemberContext &ctx) {
    const std::string &name = ctx.protoName;

    bool containsCountVar = containsLengthAttrib(ctx.mdata.params);
    bool arrayFlag = containsCountVar;

    if (name.starts_with("get")) {
        return arrayFlag ? MemberNameCategory::GET_ARRAY
                         : MemberNameCategory::GET;
    }
    if (name.starts_with("allocate")) {
        if (arrayFlag) {
            return MemberNameCategory::ALLOCATE_ARRAY;
        } else {
            return MemberNameCategory::ALLOCATE;
        }
    }
    if (name.starts_with("create")) {
        return arrayFlag ? MemberNameCategory::CREATE_ARRAY
                         : MemberNameCategory::CREATE;
    }
    if (name.starts_with("enumerate")) {
        return MemberNameCategory::ENUMERATE;
    }
    return MemberNameCategory::UNKNOWN;
}

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

static std::string mname2;

class MemberResolver {
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
            return std::make_shared<VariableData>(VariableData::TYPE_INVALID);
        };

        const auto transformArgument = [&]() {
            if (it->get()->hasLenAttrib()) {
                std::string lenAttrib = it->get()->lenAttrib();
                if (lenAttrib == "null-terminated") {
                    return;
                }
                std::string len = it->get()->lenAttribVarName();
                const std::shared_ptr<VariableData> &sizeVar = findSizeVar(len);

                if (sizeVar->isInvalid()) {
                    // length param not found
                    std::cerr << "(in: " << ctx.protoName << ")"
                              << " Length param not found: " << len << ENDL;
                    return;
                }
                if (!strContains(lenAttrib, "->")) {
                    if (!sizeVar->getIgnoreFlag()) {
                        sizeVar->setIgnoreFlag(true);
                        std::string alt = it->get()->identifier() + ".size()";
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

    virtual void generateMemberBody() {
        file2.writeLine(generatePFNcall());
        if (returnType == "Result") {
            file2.writeLine("return " + returnVar.identifier() + ";");
        }
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

    void generateDeclaration() {
        file.writeLine("");
        if (!templates.empty()) {
            std::string temp;
            for (const std::string &str : templates) {
                temp += str;
                temp += ", ";
            }
            strStripSuffix(temp, ", ");
            file.writeLine("template <" + temp + ">");
        }

        std::string protoArgs = ctx.mdata.createProtoArguments(ctx.className);
        std::string proto = returnType + " " + ctx.protoName + "(" + protoArgs +
                            ");" + " // test [" + dbgtag + "]";

        file.writeLine(proto);
    }

    void generateDefinition() {
        withExtensionProtect2(mname2, [&] {
            file2.writeLine("");

            if (!templates.empty()) {
                std::string temp;
                for (const std::string &str : templates) {
                    temp += str;
                    temp += ", ";
                }
                strStripSuffix(temp, ", ");
                file2.writeLine("template <" + temp + ">");
            }

            std::string protoArgs =
                ctx.mdata.createProtoArguments(ctx.className);
            std::string proto = "inline " + returnType + " " + ctx.className +
                                "::" + ctx.protoName + "(" + protoArgs + ") {" +
                                " // [" + dbgtag + "]";

            file2.writeLine(proto);
            file2.pushIndent();

            generateMemberBody();

            file2.popIndent();
            file2.writeLine("}");
        });
    }
};

class MemberResolverInit : public MemberResolver {
  public:
    MemberResolverInit(MemberContext &refCtx) : MemberResolver(refCtx) {}
    void generateMemberBody() override { file2.writeLine(generatePFNcall()); }
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

    void generateMemberBody() override {
        file2.writeLine(returnType + " " + lastVar->identifier() + ";");

        file2.writeLine(generatePFNcall());

        file2.writeLine("return " + lastVar->identifier() + ";");
    }
};

class MemberResolverGet : public MemberResolver {
  protected:
    std::shared_ptr<VariableData> last;
    std::shared_ptr<VariableData> lenVar;
    std::string initSize;

    std::shared_ptr<VariableData> getReturnVar() {
        std::string name = ctx.protoName;
        strRemoveTag(name);
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

    void generateArray() {
        file2.writeLine(last->toString() + initSize + ";");
        std::string size = last->lenAttrib();
        if (lenVar->getIgnoreFlag()) {
            file2.writeLine(lenVar->type() + " " + lenVar->identifier() + ";");
        }

        if (ctx.pfnReturn == PFNReturnCategory::VK_RESULT) {
            file2.writeLine(assignToResult(""));
            std::string call = generatePFNcall();
            file2.writeLine("do {");
            file2.pushIndent();

            file2.writeLine(call);
            file2.writeLine("if (( result == Result::eSuccess ) && " +
                            lenVar->identifier() + ") {");
            file2.pushIndent();

            file2.writeLine(last->identifier() + ".resize(" + size + ");");
            file2.writeLine(call);

            file2.writeLine("//VULKAN_HPP_ASSERT( " + lenVar->identifier() +
                            " <= " + last->identifier() + ".size() );");

            file2.popIndent();
            file2.writeLine("}");

            file2.popIndent();
            file2.writeLine("} while ( result == Result::eIncomplete );");

            file2.writeLine("if ( ( result == Result::eSuccess ) && ( " +
                            lenVar->identifier() + "< " + last->identifier() +
                            ".size() ) ) {");
            file2.pushIndent();

            file2.writeLine(last->identifier() + ".resize(" + size + ");");

            file2.popIndent();
            file2.writeLine("}");

            file2.writeLine("return " + last->identifier() + ";");
        } else {
            std::string call = generatePFNcall();

            file2.writeLine(call);
            file2.writeLine(last->identifier() + ".resize(" + size + ");");

            file2.writeLine(call);
            file2.writeLine("//VULKAN_HPP_ASSERT( " + lenVar->identifier() +
                            " <= " + last->identifier() + ".size() );");

            file2.writeLine("return " + last->identifier() + ";");
        }
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
                        strContains(lenVar->original.suffix(), "*") ? "&" : "";
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

    void generateMemberBody() override {
        if (returnsArray) {
            generateArray();
        } else {
            file2.writeLine(returnType + " " + last->identifier() + ";");

            file2.writeLine(generatePFNcall());

            file2.writeLine("return " + last->identifier() + ";");
        }
    }
};

class MemberResolverCreateArray : public MemberResolver {
  protected:
    std::shared_ptr<VariableData> infoVar;
    std::shared_ptr<VariableData> lastVar;

  public:
    MemberResolverCreateArray(MemberContext &refCtx) : MemberResolver(refCtx) {
        for (auto &var : ctx.mdata.params) {
            if (strContains(var->type(), "CreateInfo")) {
                infoVar = var;
                break;
            }
        }
        if (!infoVar.get()) {
            std::cerr << "Error: MemberResolverAllocateArray: pCreateInfo not "
                         "found. ("
                      << ctx.protoName << ") " << ctx.className << std::endl;
        }

        lastVar = ctx.mdata.params[ctx.mdata.params.size() - 1];

        lastVar->convertToStdVector();
        lastVar->setIgnoreFlag(true);
        returnType = lastVar->fullType();

        dbgtag = "create array";
    }

    void generateMemberBody() override {
        std::string vectorSize = infoVar->identifier() + ".size()";
        file2.writeLine(lastVar->toString() + "(" + vectorSize + ");");

        file2.writeLine(generatePFNcall());

        file2.writeLine("return " + lastVar->identifier() + ";");
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

    void generateMemberBody() override {
        std::string vectorSize = lastVar->lenAttrib();

        file2.writeLine(lastVar->toString() + "(" + vectorSize + ");");

        file2.writeLine(generatePFNcall());

        file2.writeLine("return " + lastVar->identifier() + ";");
    }
};

class MemberResolverReturnProxy : public MemberResolver {
  protected:
    std::shared_ptr<VariableData> last;
    std::shared_ptr<VariableData> sizeVar;

  public:
    MemberResolverReturnProxy(MemberContext &refCtx) : MemberResolver(refCtx) {
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

    void generateMemberBody() override {
        file2.writeLine("T " + last->identifier() + ";");

        file2.writeLine(generatePFNcall()); // TODO missing &

        file2.writeLine("return " + last->identifier() + ";");
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

    void generateMemberBody() override {
        file2.writeLine("VULKAN_HPP_ASSERT( " + sizeVar->identifier() +
                        " % sizeof( T ) == 0 );");

        file2.writeLine(last->toString() + "( " + sizeVar->identifier() +
                        " / sizeof(T)"
                        " );");
        file2.writeLine(generatePFNcall());
        file2.writeLine("return " + last->identifier() + ";");
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

    void generateMemberBody() override {
        file2.writeLine(last->toString() + ";");
        file2.writeLine(lenVar->type() + " " + lenVar->identifier() + ";");

        if (ctx.pfnReturn == PFNReturnCategory::VK_RESULT) {
            file2.writeLine(assignToResult(""));
            std::string call = generatePFNcall();
            file2.writeLine("do {");
            file2.pushIndent();

            file2.writeLine(call);
            file2.writeLine("if (( result == Result::eSuccess ) && " +
                            lenVar->identifier() + ") {");
            file2.pushIndent();

            file2.writeLine(last->identifier() + ".resize(" +
                            lenVar->identifier() + ");");
            file2.writeLine(call);

            file2.writeLine("//VULKAN_HPP_ASSERT( " + lenVar->identifier() +
                            " <= " + last->identifier() + ".size() );");

            file2.popIndent();
            file2.writeLine("}");

            file2.popIndent();
            file2.writeLine("} while ( result == Result::eIncomplete );");

            file2.writeLine("if ( ( result == Result::eSuccess ) && ( " +
                            lenVar->identifier() + "< " + last->identifier() +
                            ".size() ) ) {");
            file2.pushIndent();

            file2.writeLine(last->identifier() + ".resize(" +
                            lenVar->identifier() + ");");

            file2.popIndent();
            file2.writeLine("}");

            file2.writeLine("return " + last->identifier() + ";");
        }
    }
};

class MemberResolverEnumerate : public MemberResolver {
  protected:
    std::shared_ptr<VariableData> last;
    std::shared_ptr<VariableData> lenVar;

  public:
    MemberResolverEnumerate(MemberContext &refCtx) : MemberResolver(refCtx) {
        dbgtag = "enumerate";
        last = *ctx.mdata.params.rbegin();

        lenVar = last->lengthAttribVar();

        last->convertToStdVector();
        last->setIgnoreFlag(true);

        returnType = last->fullType();
        lenVar->setAltPFN("&" + lenVar->original.identifier());
    }

    void generateMemberBody() override {
        bool returnsObject =
            vkHandles.find(last->original.type()) != vkHandles.end();

        std::string objvector = last->fullType();
        if (returnsObject) {
            last->setType(last->type() + "Base");
        }
        file2.writeLine(last->toString() + ";");
        file2.writeLine(lenVar->type() + " " + lenVar->identifier() + ";");

        if (ctx.pfnReturn == PFNReturnCategory::VK_RESULT) {
            file2.writeLine(assignToResult(""));
            std::string call = generatePFNcall();
            last->setAltPFN("nullptr");
            std::string call2 = generatePFNcall();
            file2.writeLine("do {");
            file2.pushIndent();

            file2.writeLine(call2);
            file2.writeLine("if (( result == Result::eSuccess ) && " +
                            lenVar->identifier() + ") {");
            file2.pushIndent();

            file2.writeLine(last->identifier() + ".resize(" +
                            lenVar->identifier() + ");");

            file2.writeLine(call);

            file2.writeLine("//VULKAN_HPP_ASSERT( " + lenVar->identifier() +
                            " <= " + last->identifier() + ".size() );");

            file2.popIndent();
            file2.writeLine("}");

            file2.popIndent();
            file2.writeLine("} while ( result == Result::eIncomplete );");

            file2.writeLine("if ( ( result == Result::eSuccess ) && ( " +
                            lenVar->identifier() + "< " + last->identifier() +
                            ".size() ) ) {");
            file2.pushIndent();

            file2.writeLine(last->identifier() + ".resize(" +
                            lenVar->identifier() + ");");

            file2.popIndent();
            file2.writeLine("}");
            if (returnsObject) {
                file2.writeLine(objvector + " out{" + last->identifier() +
                                ".begin(), " + last->identifier() + ".end()};");
                file2.writeLine("return out;");
            } else {
                file2.writeLine("return " + last->identifier() + ";");
            }
        }
    }
};

PFNReturnCategory evaluatePFNReturn(const std::string &type) {
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

bool getLastTwo(MemberContext &ctx,
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

std::vector<MemberResolver *> createMemberResolvers(MemberContext &ctx) {
    std::vector<MemberResolver *> resolvers;
    if (ctx.mdata.params.empty()) {
        std::cerr << " >>> Unhandled: no params" << ctx.mdata.name << ENDL;
        return resolvers;
    }

    VariableArray::reverse_iterator last =
        ctx.mdata.params.rbegin(); // last argument
    MemberNameCategory nameCategory = evalMemberNameCategory(ctx);

    if (ctx.pfnReturn == PFNReturnCategory::VK_OBJECT ||
        ctx.pfnReturn == PFNReturnCategory::OTHER) {
        MemberResolver *resolver = new MemberResolver(ctx);
        resolver->dbgtag = "PFN return";
        resolvers.push_back(resolver);
        return resolvers;
    }

    MemberResolver *resolver = nullptr;
    switch (nameCategory) {
    case MemberNameCategory::GET: {
        std::pair<VariableArray::reverse_iterator,
                  VariableArray::reverse_iterator>
            data;
        if (getLastTwo(ctx, data)) {
            if (isPointerToCArray(data.first->get()->identifier()) &&
                data.second->get()->identifier() ==
                    stripStartingP(data.first->get()->identifier()) +
                        "Size") { // refactor?

                resolvers.push_back(new MemberResolverReturnProxy(ctx));
                resolver = new MemberResolverReturnVectorOfProxy(ctx);
                resolver->dbgtag = "get";
                return resolvers;
            } else if (data.second->get()->identifier() ==
                       data.first->get()->identifier() + "Size") {
                if (isTypePointer(*data.second->get())) {
                    resolver = new MemberResolverReturnVectorData(ctx);
                } else {
                    resolver = new MemberResolverReturnProxy(ctx);
                }
            }
        }

        if (!resolver) { // fallback
            if (strContains(last->get()->suffix(), "*") &&
                !strContains(last->get()->prefix(), "const")) {
                resolver = new MemberResolverStruct(ctx);
            } else {
                resolver = new MemberResolver(ctx);
            }
        }
        resolver->dbgtag = "get";
    } break;
    case MemberNameCategory::GET_ARRAY:
        resolver = new MemberResolverGet(ctx);
        resolver->dbgtag = "get array";
        break;
    case MemberNameCategory::CREATE:
        resolver = new MemberResolverStruct(ctx);
        resolver->dbgtag = "create";
        break;
    case MemberNameCategory::CREATE_ARRAY:
        resolver = new MemberResolverCreateArray(ctx);
        resolver->dbgtag = "create array";
        break;
    case MemberNameCategory::ALLOCATE:
        resolver = new MemberResolverStruct(ctx);
        resolver->dbgtag = "allocate";
        break;
    case MemberNameCategory::ALLOCATE_ARRAY:
        resolver = new MemberResolverAllocateArray(ctx);
        resolver->dbgtag = "allocate array";
        break;
    case MemberNameCategory::ENUMERATE:
        resolver = new MemberResolverEnumerate(ctx);
        break;
    case MemberNameCategory::UNKNOWN:
        // std::cout << ">> HERE" << std::endl;
        if (strContains(last->get()->suffix(), "*") &&
            !strContains(last->get()->prefix(), "const")) {
            resolver = new MemberResolverStruct(ctx);
        } else {
            resolver = new MemberResolver(ctx);
        }
        break;
    }
    resolvers.push_back(resolver);
    return resolvers;
}

void generateClassMemberCpp(MemberContext &ctx) {
    std::string dbgProtoComment; // debug info
    std::vector<MemberResolver *> resolvers = createMemberResolvers(ctx);

    for (size_t i = 0; i < resolvers.size(); ++i) {
        MemberResolver *resolver = resolvers[i];
        resolver->generateDeclaration();
        resolver->generateDefinition();
        delete resolver;
    }
}

static void generateClassMembers(const std::string &className,
                                 const std::string &handle,
                                 const ClassData &data) {
    // extract member data from XMLElements
    std::string memberProcAddr = "vkGet" + data.getAddrSource + "ProcAddr";
    std::string memberCreate = "vkCreate" + className;

    // PFN function pointers
    for (const ClassMemberData &m : data.members) {
        withExtensionProtect(m.name, [&] {
            file.writeLine("PFN_" + m.name + " m_" + m.name + " = {};");
        });
    }

    std::string optInfo;
    if (!data.createMember.name.empty()) {
        optInfo = ", const " + className + "CreateInfo& createInfo";
    }

    file.popIndent();
    file.writeLine("public:");
    file.pushIndent();
    if (!data.getAddrMember.name.empty()) {
        file.writeLine("PFN_" + data.getAddrMember.name + " m_" +
                       data.getAddrMember.name + ";");

        // getProcAddr member
        file.writeLine("template<typename T>");
        file.writeLine(
            "inline T getProcAddr(const std::string_view &name) const");
        file.writeLine("{");
        file.pushIndent();
        file.writeLine("return std::bit_cast<T>(m_" +
                       data.getAddrMember.name + "(" + handle +
                       ", name.data()));");
        file.popIndent();
        file.writeLine("}");
    }

    std::string initParams = "const LibraryLoader &lib";
    std::string parentVar;
    if (!data.parent.empty()) {
        parentVar = data.parent;
        parentVar[0] = std::tolower(parentVar[0]);
        initParams += ", const " + data.parent + " &" + parentVar;
    }
    initParams += optInfo;
    file.writeLine("void init(" + initParams + ");");

    file2.writeLine("void " + className + "::init(" + initParams + ") {");
    file2.pushIndent();
    if (!data.getAddrMember.name.empty()) {
        if (data.getAddrSource == "Instance") {
            file2.writeLine("m_" + data.getAddrMember.name + " = lib." +
                            memberProcAddr + ";");
        } else {
            file2.writeLine("m_" + data.getAddrMember.name +
                            " = lib.getProcAddr<PFN_" +
                            data.getAddrMember.name + ">(\"" +
                            data.getAddrMember.name + "\");");
        }
    }
    if (!data.createMember.name.empty()) {
        std::string s = data.createMember.name;
        file2.writeLine("PFN_" + s + " m_" + s + " = lib.getProcAddr<PFN_" + s +
                        ">(\"" + s + "\");");

        MemberContext ctx{
            .className = className,
            .handle = handle,
            .protoName = std::regex_replace(toCppStyle(data.createMember.name),
                                            std::regex(className),
                                            ""), // prototype name (without vk)
            .pfnReturn = evaluatePFNReturn(data.createMember.type),
            .mdata = data.createMember};

        for (auto &m : ctx.mdata.params) {
            if (m->identifier() == "pCreateInfo") {
                m->convertToReturn();
                m->setIdentifier("createInfo");
            }
            if (m->original.type() == "VkAllocationCallbacks") {
                m->setAltPFN("nullptr");
            }
            if (m->type() == className) {
                m->convertToReturn();
            }
        }

        MemberResolverInit r(ctx);
        r.generateMemberBody();
    }

    std::string loadParamCall;
    if (data.getAddrMember.name.empty()) {
        loadParamCall = parentVar;
    }

    file2.writeLine("loadTable(" + loadParamCall + ");");

    file2.popIndent();
    file2.writeLine("}");

    // wrapper functions
    for (const ClassMemberData &m : data.members) {
        // debug
        static const std::vector<std::string> debugNames;

        if (!debugNames.empty()) {
            bool contains = false;
            for (const auto &s : debugNames) {
                if (strContains(m.name, s)) {
                    contains = true;
                    break;
                }
            }
            if (!contains) {
                continue;
            }
        }

        mname2 = m.name;
        withExtensionProtect(m.name, [&] {
            MemberContext ctx{.className = className,
                              .handle = handle,
                              .protoName = std::regex_replace(
                                  toCppStyle(m.name), std::regex(className),
                                  ""), // prototype name (without vk)
                              .pfnReturn = evaluatePFNReturn(m.type),
                              .mdata = m};

            generateClassMemberCpp(ctx);
        });
    }

    file.get() << ENDL;

    std::string loadParam;
    std::string loadSrc;
    if (data.getAddrMember.name.empty()) {
        loadSrc = parentVar;
        loadParam += "const " + data.parent + "& " + loadSrc;
        loadSrc += ".";
    }

    file.writeLine("void loadTable(" + loadParam + ");");

    file2.writeLine("void " + className + "::loadTable(" + loadParam + ")");
    file2.writeLine("{");
    file2.pushIndent();

    // function pointers initialization
    for (const ClassMemberData &m : data.members) {
        withExtensionProtect2(m.name, [&] {
            file2.writeLine("m_" + m.name + " = " + loadSrc + "getProcAddr<" +
                            "PFN_" + m.name + ">(\"" + m.name + "\");");
        });
    }
    file2.popIndent();
    file2.writeLine("}");
}

static void generateClass(const std::string &name) {
    withExtensionProtect(name, [&] {
        std::string className = toCppStyle(name, true);
        std::string classNameLower = className;
        classNameLower[0] = std::tolower(classNameLower[0]);
        std::string handle = "m_" + toCppStyle(name);

        file.writeLine("class " + className + " : public " + className +
                       "Base {");
        file.pushIndent();

        const auto &it = classMetaData.find(className);
        if (it != classMetaData.end()) {
            if (!it->second.members.empty()) {
                generateClassMembers(className, handle, it->second);
            }
        }

        file.popIndent();
        file.writeLine("public:");
        file.pushIndent();

        file.writeLine("VULKAN_HPP_CONSTEXPR         " + className +
                       "() = default;");
        file.writeLine("VULKAN_HPP_CONSTEXPR         " + className +
                       "( std::nullptr_t ) VULKAN_HPP_NOEXCEPT {}");

        file.writeLine("VULKAN_HPP_TYPESAFE_EXPLICIT " + className + "(Vk" +
                       className + " " + classNameLower +
                       ") VULKAN_HPP_NOEXCEPT {");
        file.writeLine("    " + handle + " = " + classNameLower + ";");
        file.writeLine("}");

        file.popIndent();
        file.writeLine("};");
    });
}

void parseCommands(XMLNode *node) {
    std::cout << "Parsing commands" << ENDL;

    // static constexpr std::array<std::string_view, 3> deviceObjects =
    // {"VkDevice", "VkQueue", "VkCommandBuffer"};
    static const std::vector<std::string> deviceObjects = {
        "VkDevice", "VkQueue", "VkCommandBuffer"};

    // command data is stored in XMLElement*
    std::vector<ClassMemberData> elementsDevice;
    std::vector<ClassMemberData> elementsInstance;
    std::vector<ClassMemberData> elementsPhysicalDevice;
    std::vector<ClassMemberData> elementsOther;

    classMetaData["Instance"] =
        ClassData{.getAddrSource = "Instance", .parent = ""};
    classMetaData["PhysicalDevice"] =
        ClassData{.getAddrSource = "Instance", .parent = "Instance"};
    classMetaData["Device"] =
        ClassData{.getAddrSource = "Device", .parent = "PhysicalDevice"};

    int c = 0;
    // iterate contents of <commands>, filter only <command> children
    XMLElement *commandElement = node->FirstChildElement();
    while (commandElement) {
        c++;
        // default destination is elementsOther
        std::vector<ClassMemberData> *target = &elementsOther;

        ClassMemberData command = parseClassMember(commandElement);

        if (command.params.size() > 0) {
            std::string first =
                command.params.at(0)
                    ->original.type(); // first argument of a command
            if (command.name == "vkGetDeviceProcAddr") {
                classMetaData["Device"].getAddrMember = command;
            } else if (command.name == "vkGetInstanceProcAddr") {
                classMetaData["Instance"].getAddrMember = command;
                // classMetaData["PhysicalDevice"].getAddrMember = command;
            } else if (command.name == "vkCreateDevice") {
                classMetaData["Device"].createMember = command;
            } else if (command.name == "vkCreateInstance") {
                classMetaData["Instance"].createMember = command;
            } else if (first ==
                       "VkPhysicalDevice") { // command is for physical device
                target = &classMetaData["PhysicalDevice"].members;
            } else if (isInContainter(deviceObjects,
                                      first)) { // command is for device
                target = &classMetaData["Device"].members;
            } else if (first == "VkInstance") { // command is for instance
                target = &classMetaData["Instance"].members;
            }
        }

        target->push_back(command);

        commandElement = commandElement->NextSiblingElement();
    }

    std::cout << "Parsing commands done" << ENDL;
}
