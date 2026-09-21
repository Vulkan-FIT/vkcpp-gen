// MIT License
// Copyright (c) 2021-2023  @guritchi
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "Generator.hpp"
#include "Registry.hpp"
#include "Format.hpp"

#ifndef USE_PCH
#include <filesystem>
#include <iostream>
#include <sstream>
#include <cassert>
#endif

static constexpr char const *CODE_PLATFORM_H{
    R"(#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

/*
***************************************************************************************************
*   Platform-specific directives and type declarations
***************************************************************************************************
*/

/* Platform-specific calling convention macros.
 *
 * Platforms should define these so that Vulkan clients call Vulkan commands
 * with the same calling conventions that the Vulkan implementation expects.
 *
 * VKAPI_ATTR - Placed before the return type in function declarations.
 *              Useful for C++11 and GCC/Clang-style function attribute syntax.
 * VKAPI_CALL - Placed after the return type in function declarations.
 *              Useful for MSVC-style calling convention syntax.
 * VKAPI_PTR  - Placed between the '(' and '*' in function pointer types.
 *
 * Function declaration:  VKAPI_ATTR void VKAPI_CALL vkCommand(void);
 * Function pointer type: typedef void (VKAPI_PTR *PFN_vkCommand)(void);
 */
#if defined(_WIN32)
// On Windows, Vulkan commands use the stdcall convention
#define VKAPI_ATTR
#define VKAPI_CALL __stdcall
#define VKAPI_PTR  VKAPI_CALL
#elif defined(__ANDROID__) && defined(__ARM_ARCH) && __ARM_ARCH < 7
#error "Vulkan is not supported for the 'armeabi' NDK ABI"
#elif defined(__ANDROID__) && defined(__ARM_ARCH) && __ARM_ARCH >= 7 && defined(__ARM_32BIT_STATE)
// On Android 32-bit ARM targets, Vulkan functions use the "hardfloat"
// calling convention, i.e. float parameters are passed in registers. This
// is true even if the rest of the application passes floats on the stack,
// as it does by default when compiling for the armeabi-v7a NDK ABI.
#define VKAPI_ATTR __attribute__((pcs("aapcs-vfp")))
#define VKAPI_CALL
#define VKAPI_PTR  VKAPI_ATTR
#else
// On other platforms, use the default calling convention
#define VKAPI_ATTR
#define VKAPI_CALL
#define VKAPI_PTR
#endif

#if !defined(VK_NO_STDDEF_H)
#include <stddef.h>
#endif // !defined(VK_NO_STDDEF_H)

#if !defined(VK_NO_STDINT_H)
#if defined(_MSC_VER) && (_MSC_VER < 1600)
    typedef signed   __int8  int8_t;
    typedef unsigned __int8  uint8_t;
    typedef signed   __int16 int16_t;
    typedef unsigned __int16 uint16_t;
    typedef signed   __int32 int32_t;
    typedef unsigned __int32 uint32_t;
    typedef signed   __int64 int64_t;
    typedef unsigned __int64 uint64_t;
#else
#include <stdint.h>
#endif
#endif // !defined(VK_NO_STDINT_H)

#ifdef __cplusplus
} // extern "C"
#endif // __cplusplus
)"
};

// // VK_VERSION_1_0 is a preprocessor guard. Do not pass it to API calls.
// #define VK_VERSION_1_0 1
static constexpr char const *RES_HEADER_C{
    R"(
#include "vk_platform.h"

#define VK_DEFINE_HANDLE(object) typedef struct object##_T* object;


#ifndef VK_USE_64_BIT_PTR_DEFINES
    #if defined(__LP64__) || defined(_WIN64) || (defined(__x86_64__) && !defined(__ILP32__) ) || defined(_M_X64) || defined(__ia64) || defined (_M_IA64) || defined(__aarch64__) || defined(__powerpc64__) || (defined(__riscv) && __riscv_xlen == 64)
        #define VK_USE_64_BIT_PTR_DEFINES 1
    #else
        #define VK_USE_64_BIT_PTR_DEFINES 0
    #endif
#endif


#ifndef VK_DEFINE_NON_DISPATCHABLE_HANDLE
    #if (VK_USE_64_BIT_PTR_DEFINES==1)
        #if (defined(__cplusplus) && (__cplusplus >= 201103L)) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 201103L))
            #define VK_NULL_HANDLE nullptr
        #else
            #define VK_NULL_HANDLE ((void*)0)
        #endif
    #else
        #define VK_NULL_HANDLE 0ULL
    #endif
#endif
#ifndef VK_NULL_HANDLE
    #define VK_NULL_HANDLE 0
#endif


#ifndef VK_DEFINE_NON_DISPATCHABLE_HANDLE
    #if (VK_USE_64_BIT_PTR_DEFINES==1)
        #define VK_DEFINE_NON_DISPATCHABLE_HANDLE(object) typedef struct object##_T *object;
    #else
        #define VK_DEFINE_NON_DISPATCHABLE_HANDLE(object) typedef uint64_t object;
    #endif
#endif

#define VK_MAKE_API_VERSION(variant, major, minor, patch) \
    ((((uint32_t)(variant)) << 29U) | (((uint32_t)(major)) << 22U) | (((uint32_t)(minor)) << 12U) | ((uint32_t)(patch)))

// DEPRECATED: This define has been removed. Specific version defines (e.g. VK_API_VERSION_1_0), or the VK_MAKE_VERSION macro, should be used instead.
//#define VK_API_VERSION VK_MAKE_API_VERSION(0, 1, 0, 0) // Patch version should always be set to 0

// Vulkan 1.0 version number
#define VK_API_VERSION_1_0 VK_MAKE_API_VERSION(0, 1, 0, 0)// Patch version should always be set to 0

// Version of this file
#define VK_HEADER_VERSION {0}

// Complete version of this file
#define VK_HEADER_VERSION_COMPLETE VK_MAKE_API_VERSION(0, 1, 3, VK_HEADER_VERSION)

// DEPRECATED: This define is deprecated. VK_MAKE_API_VERSION should be used instead.
#define VK_MAKE_VERSION(major, minor, patch) \
    ((((uint32_t)(major)) << 22U) | (((uint32_t)(minor)) << 12U) | ((uint32_t)(patch)))

// DEPRECATED: This define is deprecated. VK_API_VERSION_MAJOR should be used instead.
#define VK_VERSION_MAJOR(version) ((uint32_t)(version) >> 22U)

// DEPRECATED: This define is deprecated. VK_API_VERSION_MINOR should be used instead.
#define VK_VERSION_MINOR(version) (((uint32_t)(version) >> 12U) & 0x3FFU)

// DEPRECATED: This define is deprecated. VK_API_VERSION_PATCH should be used instead.
#define VK_VERSION_PATCH(version) ((uint32_t)(version) & 0xFFFU)

#define VK_API_VERSION_VARIANT(version) ((uint32_t)(version) >> 29U)
#define VK_API_VERSION_MAJOR(version) (((uint32_t)(version) >> 22U) & 0x7FU)
#define VK_API_VERSION_MINOR(version) (((uint32_t)(version) >> 12U) & 0x3FFU)
#define VK_API_VERSION_PATCH(version) ((uint32_t)(version) & 0xFFFU)
)"
};

static constexpr char const *RES_ERROR_CAT{ R"(
  class ErrorCategoryImpl : public std::error_category
  {{
  public:
    virtual const char * name() const VULKAN_HPP_NOEXCEPT override
    {{
      return VULKAN_HPP_NAMESPACE_STRING "::Result";
    }}
    virtual std::string message( int ev ) const override
    {{
/*
#  if defined( VULKAN_HPP_NO_TO_STRING )
      return std::to_string( ev );
#  else
      return {0}::to_string( static_cast<{0}::Result>( ev ) );
#  endif
*/
      return std::to_string( ev );
    }}
  }};
)" };

static constexpr char const *RES_ERRORS{ R"(
  class Error
  {{
  public:
    Error() VULKAN_HPP_NOEXCEPT                = default;
    Error( const Error & ) VULKAN_HPP_NOEXCEPT = default;
    virtual ~Error() VULKAN_HPP_NOEXCEPT       = default;

    virtual const char * what() const VULKAN_HPP_NOEXCEPT = 0;
  }};

  class LogicError
    : public Error
    , public std::logic_error
  {{
  public:
    explicit LogicError( const std::string & what ) : Error(), std::logic_error( what ) {{}}
    explicit LogicError( char const * what ) : Error(), std::logic_error( what ) {{}}

    virtual const char * what() const VULKAN_HPP_NOEXCEPT
    {{
      return std::logic_error::what();
    }}
  }};

  class SystemError
    : public Error
    , public std::system_error
  {{
  public:
    SystemError( std::error_code ec ) : Error(), std::system_error( ec ) {{}}
    SystemError( std::error_code ec, std::string const & what ) : Error(), std::system_error( ec, what ) {{}}
    SystemError( std::error_code ec, char const * what ) : Error(), std::system_error( ec, what ) {{}}
    SystemError( int ev, std::error_category const & ecat ) : Error(), std::system_error( ev, ecat ) {{}}
    SystemError( int ev, std::error_category const & ecat, std::string const & what ) : Error(), std::system_error( ev, ecat, what ) {{}}
    SystemError( int ev, std::error_category const & ecat, char const * what ) : Error(), std::system_error( ev, ecat, what ) {{}}

    virtual const char * what() const VULKAN_HPP_NOEXCEPT
    {{
      return std::system_error::what();
    }}
  }};

  VULKAN_HPP_INLINE const std::error_category & errorCategory() VULKAN_HPP_NOEXCEPT
  {{
    static ErrorCategoryImpl instance;
    return instance;
  }}

  VULKAN_HPP_INLINE std::error_code make_error_code( {0} e ) VULKAN_HPP_NOEXCEPT
  {{
    return std::error_code( {1}, errorCategory() );
  }}

  VULKAN_HPP_INLINE std::error_condition make_error_condition( {0} e ) VULKAN_HPP_NOEXCEPT
  {{
    return std::error_condition( {1}, errorCategory() );
  }}
)" };

static constexpr char const *RES_ERRORS_UNIFIED{ R"(
  class Error : public std::runtime_error
  {{
    {0}::Result m_result;
  public:
    explicit Error( {0}::Result result, const char* what ) : std::runtime_error( what ) {{}}

    {0}::Result result() const {{
      return m_result;
    }}
  }};
)" };

static constexpr char const *RES_UNIQUE_HANDLE{ R"(

  template <typename Type, typename Dispatch>
  class UniqueHandleTraits;

  template <typename Type, typename Dispatch>
  class UniqueHandle : public UniqueHandleTraits<Type, Dispatch>::deleter
  {
  private:
    using Deleter = typename UniqueHandleTraits<Type, Dispatch>::deleter;

  public:
    using element_type = Type;

    UniqueHandle() : Deleter(), m_value() {}

    explicit UniqueHandle( Type const & value, Deleter const & deleter = Deleter() ) VULKAN_HPP_NOEXCEPT
      : Deleter( deleter )
      , m_value( value )
    {
    }

    UniqueHandle( UniqueHandle const & ) = delete;

    UniqueHandle( UniqueHandle && other ) VULKAN_HPP_NOEXCEPT
      : Deleter( std::move( static_cast<Deleter &>( other ) ) )
      , m_value( other.release() )
    {
    }

    ~UniqueHandle() VULKAN_HPP_NOEXCEPT
    {
      if ( m_value )
      {
        this->destroy( m_value );
      }
    }

    UniqueHandle & operator=( UniqueHandle const & ) = delete;

    UniqueHandle & operator=( UniqueHandle && other ) VULKAN_HPP_NOEXCEPT
    {
      reset( other.release() );
      *static_cast<Deleter *>( this ) = std::move( static_cast<Deleter &>( other ) );
      return *this;
    }

    explicit operator bool() const VULKAN_HPP_NOEXCEPT
    {
      return m_value.operator bool();
    }

    Type const * operator->() const VULKAN_HPP_NOEXCEPT
    {
      return &m_value;
    }

    Type * operator->() VULKAN_HPP_NOEXCEPT
    {
      return &m_value;
    }

    Type const & operator*() const VULKAN_HPP_NOEXCEPT
    {
      return m_value;
    }

    Type & operator*() VULKAN_HPP_NOEXCEPT
    {
      return m_value;
    }

    const Type & get() const VULKAN_HPP_NOEXCEPT
    {
      return m_value;
    }

    Type & get() VULKAN_HPP_NOEXCEPT
    {
      return m_value;
    }

    void reset( Type const & value = Type() ) VULKAN_HPP_NOEXCEPT
    {
      if ( m_value != value )
      {
        if ( m_value )
        {
          this->destroy( m_value );
        }
        m_value = value;
      }
    }

    Type release() VULKAN_HPP_NOEXCEPT
    {
      Type value = m_value;
      m_value    = nullptr;
      return value;
    }

    void swap( UniqueHandle<Type, Dispatch> & rhs ) VULKAN_HPP_NOEXCEPT
    {
      std::swap( m_value, rhs.m_value );
      std::swap( static_cast<Deleter &>( *this ), static_cast<Deleter &>( rhs ) );
    }

  private:
    Type m_value;
  };
  /*
  template <typename UniqueType>
  VULKAN_HPP_INLINE std::vector<typename UniqueType::element_type> uniqueToRaw( std::vector<UniqueType> const & handles )
  {
    std::vector<typename UniqueType::element_type> newBuffer( handles.size() );
    std::transform( handles.begin(), handles.end(), newBuffer.begin(), []( UniqueType const & handle ) { return handle.get(); } );
    return newBuffer;
  }
  */
  template <typename Type, typename Dispatch>
  VULKAN_HPP_INLINE void swap( UniqueHandle<Type, Dispatch> & lhs, UniqueHandle<Type, Dispatch> & rhs ) VULKAN_HPP_NOEXCEPT
  {
    lhs.swap( rhs );
  }

)" };


static constexpr char const *RES_UNIQUE_HANDLE_EXP{ R"(

  template <typename Type>
  class UniqueHandleTraits;

  template <typename Type>
  class UniqueHandle : public UniqueHandleTraits<Type>::deleter
  {
  private:
    using Deleter = typename UniqueHandleTraits<Type>::deleter;

  public:
    using element_type = Type;

    UniqueHandle() : Deleter(), m_value() {}

    explicit UniqueHandle( Type const & value, Deleter const & deleter = Deleter() ) VULKAN_HPP_NOEXCEPT
      : Deleter( deleter )
      , m_value( value )
    {
    }

    template<typename... Args>
    explicit UniqueHandle( Deleter const & deleter, Args&&... args)
      : Deleter( deleter )
      , m_value( std::forward<Args>(args)... )
    {}

    UniqueHandle( UniqueHandle const & ) = delete;

    UniqueHandle( UniqueHandle && other ) VULKAN_HPP_NOEXCEPT
      : Deleter( std::move( static_cast<Deleter &>( other ) ) )
      , m_value( other.release() )
    {
    }

    ~UniqueHandle() VULKAN_HPP_NOEXCEPT
    {
      if ( m_value )
      {
        this->destroy( m_value );
      }
    }

    UniqueHandle & operator=( UniqueHandle const & ) = delete;

    UniqueHandle & operator=( UniqueHandle && other ) VULKAN_HPP_NOEXCEPT
    {
      reset( other.release() );
      *static_cast<Deleter *>( this ) = std::move( static_cast<Deleter &>( other ) );
      return *this;
    }

    explicit operator bool() const VULKAN_HPP_NOEXCEPT
    {
      return m_value.operator bool();
    }

    Type const * operator->() const VULKAN_HPP_NOEXCEPT
    {
      return &m_value;
    }

    Type * operator->() VULKAN_HPP_NOEXCEPT
    {
      return &m_value;
    }

    Type const & operator*() const VULKAN_HPP_NOEXCEPT
    {
      return m_value;
    }

    Type & operator*() VULKAN_HPP_NOEXCEPT
    {
      return m_value;
    }

    const Type & get() const VULKAN_HPP_NOEXCEPT
    {
      return m_value;
    }

    Type & get() VULKAN_HPP_NOEXCEPT
    {
      return m_value;
    }

    void reset( Type const & value = Type() ) VULKAN_HPP_NOEXCEPT
    {
      if ( m_value != value )
      {
        if ( m_value )
        {
          this->destroy( m_value );
        }
        m_value = value;
      }
    }

    Type release() VULKAN_HPP_NOEXCEPT
    {
      Type value = m_value;
      m_value    = nullptr;
      return value;
    }

    void swap( UniqueHandle<Type> & rhs ) VULKAN_HPP_NOEXCEPT
    {
      std::swap( m_value, rhs.m_value );
      std::swap( static_cast<Deleter &>( *this ), static_cast<Deleter &>( rhs ) );
    }

  private:
    Type m_value;
  };
  /*
  template <typename UniqueType>
  VULKAN_HPP_INLINE std::vector<typename UniqueType::element_type> uniqueToRaw( std::vector<UniqueType> const & handles )
  {
    std::vector<typename UniqueType::element_type> newBuffer( handles.size() );
    std::transform( handles.begin(), handles.end(), newBuffer.begin(), []( UniqueType const & handle ) { return handle.get(); } );
    return newBuffer;
  }
  */
  template <typename Type>
  VULKAN_HPP_INLINE void swap( UniqueHandle<Type> & lhs, UniqueHandle<Type> & rhs ) VULKAN_HPP_NOEXCEPT
  {
    lhs.swap( rhs );
  }

)" };

static constexpr char const *RES_RESULT_CHECK_CPP{ R"(
  VULKAN_HPP_INLINE void resultCheck( Result result, char const * message )
  {{
#ifdef VULKAN_HPP_NO_EXCEPTIONS
    ignore( result );  // just in case VULKAN_HPP_ASSERT_ON_RESULT is empty
    ignore( message );
    VULKAN_HPP_ASSERT_ON_RESULT( result == Result::eSuccess );
#else
    if ( result != Result::eSuccess ){0}
    {{
      detail::throwResultException( static_cast<VkResult>(result), message );
    }}
#endif
  }}

  VULKAN_HPP_INLINE void resultCheck( Result result, char const * message, std::initializer_list<Result> successCodes )
  {{
#ifdef VULKAN_HPP_NO_EXCEPTIONS
    ignore( result );  // just in case VULKAN_HPP_ASSERT_ON_RESULT is empty
    ignore( message );
    ignore( successCodes );  // just in case VULKAN_HPP_ASSERT_ON_RESULT is empty
    VULKAN_HPP_ASSERT_ON_RESULT( std::find( successCodes.begin(), successCodes.end(), result ) != successCodes.end() );
#else
    if ( std::find( successCodes.begin(), successCodes.end(), result ) == successCodes.end() ){0}
    {{
      detail::throwResultException( static_cast<VkResult>(result), message );
    }}
#endif
  }}

  VULKAN_HPP_INLINE void resultCheck( VkResult result, char const * message )
  {{
#ifdef VULKAN_HPP_NO_EXCEPTIONS
    ignore( result );  // just in case VULKAN_HPP_ASSERT_ON_RESULT is empty
    ignore( message );
    VULKAN_HPP_ASSERT_ON_RESULT( result == VK_SUCCESS );
#else
    if ( result != VK_SUCCESS ){0}
    {{
      detail::throwResultException( result, message );
    }}
#endif
  }}

  VULKAN_HPP_INLINE void resultCheck( VkResult result, char const * message, std::initializer_list<VkResult> successCodes )
  {{
#ifdef VULKAN_HPP_NO_EXCEPTIONS
    ignore( result );  // just in case VULKAN_HPP_ASSERT_ON_RESULT is empty
    ignore( message );
    ignore( successCodes );  // just in case VULKAN_HPP_ASSERT_ON_RESULT is empty
    VULKAN_HPP_ASSERT_ON_RESULT( std::find( successCodes.begin(), successCodes.end(), result ) != successCodes.end() );
#else
    if ( std::find( successCodes.begin(), successCodes.end(), result ) == successCodes.end() ){0}
    {{
      detail::throwResultException( result, message );
    }}
#endif
  }}
)" };

static constexpr char const *RES_RESULT_CHECK{ R"(
  VULKAN_HPP_INLINE void resultCheck( VkResult result, char const * message )
  {{
#ifdef VULKAN_HPP_NO_EXCEPTIONS
    ignore( result );  // just in case VULKAN_HPP_ASSERT_ON_RESULT is empty
    ignore( message );
    VULKAN_HPP_ASSERT_ON_RESULT( result == VK_SUCCESS );
#else
    if ( result != VK_SUCCESS ){0}
    {{
      detail::throwResultException( result, message );
    }}
#endif
  }}

  VULKAN_HPP_INLINE void resultCheck( VkResult result, char const * message, std::initializer_list<VkResult> successCodes )
  {{
#ifdef VULKAN_HPP_NO_EXCEPTIONS
    ignore( result );  // just in case VULKAN_HPP_ASSERT_ON_RESULT is empty
    ignore( message );
    ignore( successCodes );  // just in case VULKAN_HPP_ASSERT_ON_RESULT is empty
    VULKAN_HPP_ASSERT_ON_RESULT( std::find( successCodes.begin(), successCodes.end(), result ) != successCodes.end() );
#else
    for (const auto &r : successCodes) {{
        if (result == r) {{
            return;
        }}
    }}
    detail::throwResultException( result, message );
#endif
  }}
)" };

static constexpr char const *RES_ARRAY_PROXY{ R"(
template <typename T>
 class ArrayProxy
 {
 public:
   VULKAN_HPP_CONSTEXPR ArrayProxy() VULKAN_HPP_NOEXCEPT
     : m_count( 0 )
     , m_ptr( nullptr )
   {
   }

   VULKAN_HPP_CONSTEXPR ArrayProxy( std::nullptr_t ) VULKAN_HPP_NOEXCEPT
     : m_count( 0 )
     , m_ptr( nullptr )
   {
   }

   ArrayProxy( T const & value ) VULKAN_HPP_NOEXCEPT
     : m_count( 1 )
     , m_ptr( &value )
   {
   }

   ArrayProxy( uint32_t count, T const * ptr ) VULKAN_HPP_NOEXCEPT
     : m_count( count )
     , m_ptr( ptr )
   {
   }

   template <std::size_t C>
   ArrayProxy( T const ( &ptr )[C] ) VULKAN_HPP_NOEXCEPT
     : m_count( C )
     , m_ptr( ptr )
   {
   }

#  if __GNUC__ >= 9
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Winit-list-lifetime"
#  endif

   ArrayProxy( std::initializer_list<T> const & list ) VULKAN_HPP_NOEXCEPT
     : m_count( static_cast<uint32_t>( list.size() ) )
     , m_ptr( list.begin() )
   {
   }

   template <typename B = T, typename std::enable_if<std::is_const<B>::value, int>::type = 0>
   ArrayProxy( std::initializer_list<typename std::remove_const<T>::type> const & list ) VULKAN_HPP_NOEXCEPT
     : m_count( static_cast<uint32_t>( list.size() ) )
     , m_ptr( list.begin() )
   {
   }

#  if __GNUC__ >= 9
#    pragma GCC diagnostic pop
#  endif

   // Any type with a .data() return type implicitly convertible to T*, and a .size() return type implicitly
   // convertible to size_t. The const version can capture temporaries, with lifetime ending at end of statement.
   template <typename V,
             typename std::enable_if<std::is_convertible<decltype( std::declval<V>().data() ), T *>::value &&
                                     std::is_convertible<decltype( std::declval<V>().size() ), std::size_t>::value>::type * = nullptr>
   ArrayProxy( V const & v ) VULKAN_HPP_NOEXCEPT
     : m_count( static_cast<uint32_t>( v.size() ) )
     , m_ptr( v.data() )
   {
   }

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

   T const * data() const VULKAN_HPP_NOEXCEPT
   {
     return m_ptr;
   }

 private:
   uint32_t  m_count;
   T const * m_ptr;
 };

template <typename T>
 class ArrayProxyNoTemporaries
 {
 public:
   VULKAN_HPP_CONSTEXPR ArrayProxyNoTemporaries() VULKAN_HPP_NOEXCEPT
     : m_count( 0 )
     , m_ptr( nullptr )
   {
   }

   VULKAN_HPP_CONSTEXPR ArrayProxyNoTemporaries( std::nullptr_t ) VULKAN_HPP_NOEXCEPT
     : m_count( 0 )
     , m_ptr( nullptr )
   {
   }

   template <typename B = T, typename std::enable_if<std::is_convertible<B, T>::value && std::is_lvalue_reference<B>::value, int>::type = 0>
   ArrayProxyNoTemporaries( B && value ) VULKAN_HPP_NOEXCEPT
     : m_count( 1 )
     , m_ptr( &value )
   {
   }

   ArrayProxyNoTemporaries( uint32_t count, T * ptr ) VULKAN_HPP_NOEXCEPT
     : m_count( count )
     , m_ptr( ptr )
   {
   }

   template <std::size_t C>
   ArrayProxyNoTemporaries( T ( &ptr )[C] ) VULKAN_HPP_NOEXCEPT
     : m_count( C )
     , m_ptr( ptr )
   {
   }

   template <std::size_t C>
   ArrayProxyNoTemporaries( T ( &&ptr )[C] ) = delete;

   // Any l-value reference with a .data() return type implicitly convertible to T*, and a .size() return type implicitly convertible to size_t.
   template <typename V,
             typename std::enable_if<!std::is_convertible<decltype( std::declval<V>().begin() ), T *>::value &&
                                       std::is_convertible<decltype( std::declval<V>().data() ), T *>::value &&
                                       std::is_convertible<decltype( std::declval<V>().size() ), std::size_t>::value && std::is_lvalue_reference<V>::value,
                                     int>::type = 0>
   ArrayProxyNoTemporaries( V && v ) VULKAN_HPP_NOEXCEPT
     : m_count( static_cast<uint32_t>( v.size() ) )
     , m_ptr( v.data() )
   {
   }

   // Any l-value reference with a .begin() return type implicitly convertible to T*, and a .size() return type implicitly convertible to size_t.
   template <typename V,
             typename std::enable_if<std::is_convertible<decltype( std::declval<V>().begin() ), T *>::value &&
                                       std::is_convertible<decltype( std::declval<V>().size() ), std::size_t>::value && std::is_lvalue_reference<V>::value,
                                     int>::type = 0>
   ArrayProxyNoTemporaries( V && v ) VULKAN_HPP_NOEXCEPT
     : m_count( static_cast<uint32_t>( v.size() ) )
     , m_ptr( v.begin() )
   {
   }

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
 class StridedArrayProxy : protected ArrayProxy<T>
 {
 public:
   using ArrayProxy<T>::ArrayProxy;

   StridedArrayProxy( uint32_t count, T const * ptr, uint32_t stride ) VULKAN_HPP_NOEXCEPT
     : ArrayProxy<T>( count, ptr )
     , m_stride( stride )
   {
     VULKAN_HPP_ASSERT( sizeof( T ) <= stride );
   }

   using ArrayProxy<T>::begin;

   const T * end() const VULKAN_HPP_NOEXCEPT
   {
     return reinterpret_cast<T const *>( static_cast<uint8_t const *>( begin() ) + size() * m_stride );
   }

   using ArrayProxy<T>::front;

   const T & back() const VULKAN_HPP_NOEXCEPT
   {
     VULKAN_HPP_ASSERT( begin() && size() );
     return *reinterpret_cast<T const *>( static_cast<uint8_t const *>( begin() ) + ( size() - 1 ) * m_stride );
   }

   using ArrayProxy<T>::empty;
   using ArrayProxy<T>::size;
   using ArrayProxy<T>::data;

   uint32_t stride() const
   {
     return m_stride;
   }

 private:
   uint32_t m_stride = sizeof( T );
 };
)" };

static constexpr char const *RES_ARRAY_WRAPPER{ R"(
  template <typename T, size_t N>
  class ArrayWrapper1D : public std::array<T, N>
  {
  public:
    VULKAN_HPP_CONSTEXPR ArrayWrapper1D() VULKAN_HPP_NOEXCEPT : std::array<T, N>() {}

    VULKAN_HPP_CONSTEXPR ArrayWrapper1D( std::array<T, N> const & data ) VULKAN_HPP_NOEXCEPT : std::array<T, N>( data ) {}

#if ( VK_USE_64_BIT_PTR_DEFINES == 0 )
    // on 32 bit compiles, needs overloads on index type int to resolve ambiguities
    VULKAN_HPP_CONSTEXPR T const & operator[]( int index ) const VULKAN_HPP_NOEXCEPT
    {
      return std::array<T, N>::operator[]( index );
    }

    T & operator[]( int index ) VULKAN_HPP_NOEXCEPT
    {
      return std::array<T, N>::operator[]( index );
    }
#endif

    operator T const *() const VULKAN_HPP_NOEXCEPT
    {
      return this->data();
    }

    operator T *() VULKAN_HPP_NOEXCEPT
    {
      return this->data();
    }

    template <typename B = T, typename std::enable_if<std::is_same<B, char>::value, int>::type = 0>
    operator std::string() const
    {
      return std::string( this->data() );
    }

#if 17 <= VULKAN_HPP_CPP_VERSION
    template <typename B = T, typename std::enable_if<std::is_same<B, char>::value, int>::type = 0>
    operator std::string_view() const
    {
      return std::string_view( this->data() );
    }
#endif

#if defined( VULKAN_HPP_HAS_SPACESHIP_OPERATOR )
    template <typename B = T, typename std::enable_if<std::is_same<B, char>::value, int>::type = 0>
    std::strong_ordering operator<=>( ArrayWrapper1D<char, N> const & rhs ) const VULKAN_HPP_NOEXCEPT
    {
      return *static_cast<std::array<char, N> const *>( this ) <=> *static_cast<std::array<char, N> const *>( &rhs );
    }
#else
    template <typename B = T, typename std::enable_if<std::is_same<B, char>::value, int>::type = 0>
    bool operator<( ArrayWrapper1D<char, N> const & rhs ) const VULKAN_HPP_NOEXCEPT
    {
      return *static_cast<std::array<char, N> const *>( this ) < *static_cast<std::array<char, N> const *>( &rhs );
    }

    template <typename B = T, typename std::enable_if<std::is_same<B, char>::value, int>::type = 0>
    bool operator<=( ArrayWrapper1D<char, N> const & rhs ) const VULKAN_HPP_NOEXCEPT
    {
      return *static_cast<std::array<char, N> const *>( this ) <= *static_cast<std::array<char, N> const *>( &rhs );
    }

    template <typename B = T, typename std::enable_if<std::is_same<B, char>::value, int>::type = 0>
    bool operator>( ArrayWrapper1D<char, N> const & rhs ) const VULKAN_HPP_NOEXCEPT
    {
      return *static_cast<std::array<char, N> const *>( this ) > *static_cast<std::array<char, N> const *>( &rhs );
    }

    template <typename B = T, typename std::enable_if<std::is_same<B, char>::value, int>::type = 0>
    bool operator>=( ArrayWrapper1D<char, N> const & rhs ) const VULKAN_HPP_NOEXCEPT
    {
      return *static_cast<std::array<char, N> const *>( this ) >= *static_cast<std::array<char, N> const *>( &rhs );
    }
#endif

    template <typename B = T, typename std::enable_if<std::is_same<B, char>::value, int>::type = 0>
    bool operator==( ArrayWrapper1D<char, N> const & rhs ) const VULKAN_HPP_NOEXCEPT
    {
      return *static_cast<std::array<char, N> const *>( this ) == *static_cast<std::array<char, N> const *>( &rhs );
    }

    template <typename B = T, typename std::enable_if<std::is_same<B, char>::value, int>::type = 0>
    bool operator!=( ArrayWrapper1D<char, N> const & rhs ) const VULKAN_HPP_NOEXCEPT
    {
      return *static_cast<std::array<char, N> const *>( this ) != *static_cast<std::array<char, N> const *>( &rhs );
    }
  };

  // specialization of relational operators between std::string and arrays of chars
  template <size_t N>
  bool operator<( std::string const & lhs, ArrayWrapper1D<char, N> const & rhs ) VULKAN_HPP_NOEXCEPT
  {
    return lhs < rhs.data();
  }

  template <size_t N>
  bool operator<=( std::string const & lhs, ArrayWrapper1D<char, N> const & rhs ) VULKAN_HPP_NOEXCEPT
  {
    return lhs <= rhs.data();
  }

  template <size_t N>
  bool operator>( std::string const & lhs, ArrayWrapper1D<char, N> const & rhs ) VULKAN_HPP_NOEXCEPT
  {
    return lhs > rhs.data();
  }

  template <size_t N>
  bool operator>=( std::string const & lhs, ArrayWrapper1D<char, N> const & rhs ) VULKAN_HPP_NOEXCEPT
  {
    return lhs >= rhs.data();
  }

  template <size_t N>
  bool operator==( std::string const & lhs, ArrayWrapper1D<char, N> const & rhs ) VULKAN_HPP_NOEXCEPT
  {
    return lhs == rhs.data();
  }

  template <size_t N>
  bool operator!=( std::string const & lhs, ArrayWrapper1D<char, N> const & rhs ) VULKAN_HPP_NOEXCEPT
  {
    return lhs != rhs.data();
  }

  template <typename T, size_t N, size_t M>
  class ArrayWrapper2D : public std::array<ArrayWrapper1D<T, M>, N>
  {
  public:
    VULKAN_HPP_CONSTEXPR ArrayWrapper2D() VULKAN_HPP_NOEXCEPT : std::array<ArrayWrapper1D<T, M>, N>() {}

    VULKAN_HPP_CONSTEXPR ArrayWrapper2D( std::array<std::array<T, M>, N> const & data ) VULKAN_HPP_NOEXCEPT
      : std::array<ArrayWrapper1D<T, M>, N>( *reinterpret_cast<std::array<ArrayWrapper1D<T, M>, N> const *>( &data ) )
    {
    }
  };
)" };

static constexpr char const *RES_VECTOR_DETAIL{ R"(
namespace detail {
  template<typename T>
  class Iterator
  {
  public:
    using value_type = std::remove_cv<T>::type;
    using element_type = std::add_cv<T>::type;
    using pointer    = T*;
    using reference  = T&;
    using difference_type = std::ptrdiff_t;
    using iterator_category=std::random_access_iterator_tag;
    using iterator_concept=std::contiguous_iterator_tag;

    Iterator() : v(nullptr) {}
    Iterator(reference r) : v(&r) {}
    Iterator(pointer p) : v(p) {}

    reference                 operator*()             { return *v; }
    // std::add_const<reference> operator*()       const { return *v; }
    pointer                   operator->()  const          { return v; }
    // std::add_const<pointer>   operator->()      const { return v; }
    reference                 operator[](difference_type m) const { return *(v + m); }
    // std::add_const<reference> operator[](difference_type m) const { return *(v + m); }

    friend reference operator*(const Iterator& it) { return *(it.v); }

    friend Iterator operator+(difference_type diff, const Iterator& it) { return it + diff; }
    friend Iterator operator-(difference_type diff, const Iterator& it) { return it - diff; }

    Iterator& operator++()       { ++v; return *this; }
    Iterator& operator--()       { --v; return *this; }
    Iterator  operator++(int)    { Iterator it(*this); ++v; return it; }
    Iterator  operator--(int)    { Iterator it(*this); --v; return it; }

    Iterator& operator+=(difference_type  n)  { v += n; return *this; }
    Iterator& operator-=(difference_type  n)  { v -= n; return *this; }

    Iterator operator+(difference_type  n)   const { Iterator it(*this); return it += n;}
    Iterator operator-(difference_type  n)   const { Iterator it(*this); return it -= n;}

    difference_type operator-(const Iterator& it) const { return v - it.v; }

    bool operator<(Iterator const& it)  const { return v <  it.v; }
    bool operator<=(Iterator const& it) const { return v <= it.v; }
    bool operator>(Iterator const& it)  const { return v >  it.v; }
    bool operator>=(Iterator const& it) const { return v >= it.v; }
    bool operator!=(const Iterator &it) const { return v != it.v; }
    bool operator==(const Iterator &it) const { return v == it.v; }

  private:
    pointer v;
  };

#if VULKAN_HPP_CPP_VERSION < 20
  template<typename T>
  class VectorBase {
  protected:

    T *m_begin;
    T *m_end;
    size_t cap;

    VectorBase() = default;

    VectorBase(T *data, size_t cap) noexcept
      : m_begin(data), m_end(data), cap(cap)
    {}

#if __cpp_lib_allocate_at_least
    using allocation_result = std::allocation_result<T*>;
    static allocation_result allocate(size_t count) {
      return std::allocator<T>().allocate_at_least(count);
    }
#else
    struct allocation_result {
      T *ptr;
      size_t count;
    };
    static allocation_result allocate(size_t count) {
      return { std::allocator<T>().allocate(count), count };
    }
#endif

    void set_storage(const allocation_result &alloc, size_t size) noexcept {
      m_begin = alloc.ptr;
      m_end = alloc.ptr + size;
      cap = alloc.count;
    }

    void copy_items_from(const T* src) noexcept (std::is_nothrow_copy_assignable_v<T>) {
      for (auto* it = m_begin; it != m_end; ++it) {
        *it = *src;
        src++;
      }
    }

    void move_items_from(T *begin, T *end, T* dst) noexcept (std::is_nothrow_move_assignable_v<T>) {
      std::move(begin, end, dst);
    }

    void destroy_items(T *begin, T *end) noexcept(std::is_nothrow_destructible_v<T>) {
      for (auto* it = begin; it != end; ++it) {
        std::destroy_at(it);
      }
    }

    void clear_storage() noexcept(std::is_nothrow_destructible_v<T>) {
      destroy_items(m_begin, m_end);
      m_end = m_begin;
    }


  };
#endif

  struct Empty {
  };

} // namespace detail
)" };

static constexpr char const *RES_VECTOR_CPP20{ R"(

template<typename T, size_t N = 0>
class Vector {

  template<typename, size_t> friend class Vector;

  using value_type = T;
  using reference = value_type&;
  using const_reference = const value_type&;
  using iterator = detail::Iterator<T>;
  using const_iterator = detail::Iterator<T>;

  T *m_begin = {};
  T *m_end = {};
  size_t cap = 0;
#if defined(_MSC_VER) && !defined(__clang__)
  [[msvc::no_unique_address]]
#else
  [[no_unique_address]]
#endif
  std::conditional_t<N == 0, detail::Empty, T[N]> storage;

#if __cpp_lib_allocate_at_least
  using allocation_result = std::allocation_result<T*>;
  static allocation_result allocate(size_t count) {
    return std::allocator<T>().allocate_at_least(count);
  }
#else
  struct allocation_result {
    T *ptr;
    size_t count;
  };
  static allocation_result allocate(size_t count) {
    return { std::allocator<T>().allocate(count), count };
  }
#endif

  void set_storage(const allocation_result &alloc, size_t size) noexcept {
    m_begin = alloc.ptr;
    m_end = alloc.ptr + size;
    cap = alloc.count;
  }

  void destroy_items(T *begin, T *end) noexcept(std::is_nothrow_destructible_v<T>) {
    for (auto* it = begin; it != end; ++it) {
      std::destroy_at(it);
    }
  }

  void clear_storage() noexcept(std::is_nothrow_destructible_v<T>) {
    destroy_items(m_begin, m_end);
    m_end = m_begin;
  }

  void deallocate_storage() noexcept(std::is_nothrow_destructible_v<T>) {
    clear();
    if (this->m_begin && !is_inplace()) {
      std::allocator<T>().deallocate(this->m_begin, this->cap);
    }
    if constexpr (N == 0) {
      this->m_begin = nullptr;
      this->m_end   = nullptr;
    }
    else {
      this->m_begin = storage;
      this->m_end   = storage;
    }
    this->cap = N;
  }

  void reallocate(size_t count, size_t currentSize) {
    auto alloc = this->allocate(count);
    // if (this->m_begin) {
    if (currentSize != 0) {
      std::memcpy(alloc.ptr, this->m_begin, currentSize * sizeof(T));
      if (!is_inplace()) {
        std::allocator<T>().deallocate(this->m_begin, this->cap);
      }
    }
    this->set_storage(alloc, currentSize);
  }

  template<size_t M>
  void move_from(Vector<T, M> &v) {
    if constexpr (M != 0) {
      if (v.is_inplace()) {
        reserve(v.size());
        std::move(v.m_begin, v.m_end, this->m_begin);
        v.m_end = v.m_begin;
        return;
      }
    }
    std::swap(this->m_begin, v.m_begin);
    std::swap(this->m_end, v.m_end);
    std::swap(this->cap, v.cap);
  }

public:
  constexpr Vector() = default;

  constexpr explicit Vector(size_t s) {
    resize(s);
  }

  template<size_t X>
  constexpr Vector(const Vector<T, X> &v) {
    if (!v.empty()) {
        auto s = v.size();
        if (s > this->cap) {
          reallocate(s, s);
        }
        else {
          this->m_end = this->m_begin + s;
        }
        std::copy(v.m_begin, v.m_end, this->m_begin);
    }
  }

  template<size_t M>
  constexpr Vector(Vector<T, M> &&v) noexcept {
    move_from(v);
  }

  ~Vector() noexcept(std::is_nothrow_destructible_v<T>) {
    deallocate_storage();
  }


  template<size_t M>
  Vector& operator=(Vector<T, M> &v) {
    if (this != reinterpret_cast<void*>(&v)) {
      clear();
      auto s = v.size();
      if (s > this->cap) {
        reallocate(s, s);
      }
      else {
        this->m_end = this->m_begin + s;
      }
      std::copy(v.m_begin, v.m_end, this->m_begin);
    }
    return *this;
  }

  template<size_t M>
  Vector& operator=(Vector<T, M> &&v) {
    if (this != reinterpret_cast<void*>(&v)) {
      clear();
      move_from(v);
    }
    return *this;
  }

  void clear() noexcept(std::is_nothrow_destructible_v<T>) {
    this->clear_storage();
  }

  void reserve(size_t s) {
    if (s > this->cap) {
      reallocate(s, size());
    }
  }

  void resize(size_t s) {
    size_t cs = size();
    if (s < cs) {
      if constexpr (!std::is_trivially_destructible_v<T>) {
        auto *old = this->m_end;
        this->m_end = this->m_begin + s;
        this->destroy_items(this->m_end, old);
      }
      else {
        this->m_end = this->m_begin + s;
      }
    }
    else if (s > cs) {
      T* it;
      if (s > this->cap) {
        reallocate(s, cs);
        it = this->m_begin + cs;
      }
      else {
        it = this->m_end;
      }
      this->m_end = this->m_begin + s;
      for (; it != this->m_end; ++it) {
        std::construct_at(it);
      }
    }
  }

  [[nodiscard]] bool resize_noexcept(size_t s) noexcept {
    try {
      resize(s);
      return true;
    }
    catch (std::exception&) {
      return false;
    }
  }

  void resize_noinit(size_t s) requires(std::is_trivially_destructible_v<T>) {
    size_t cs = size();
    if (s < cs) {
      this->m_end = this->m_begin + s;
    }
    else if (s > cs) {
      if (s > this->cap) {
        reallocate(s, cs);
      }
      this->m_end = this->m_begin + s;
    }
  }

  [[nodiscard]] bool resize_noinit_noexcept(size_t s) requires(std::is_trivially_destructible_v<T>) {
    try {
      resize_noinit(s);
      return true;
    }
    catch (std::exception&) {
      return false;
    }
  }

  constexpr bool is_inplace() const noexcept {
    if constexpr (N == 0) {
      return false;
    }
    else {
      return this->m_begin == this->storage;
    }
  }

  size_t size() const noexcept {
    return this->m_end - this->m_begin;
  }

  size_t capacity() const noexcept {
    return this->cap;
  }

  constexpr size_t inplace_capacity() const {
    return N;
  }

  constexpr bool empty() const noexcept {
    return this->m_begin == this->m_end;
  }

  constexpr T *data() noexcept {
    return this->m_begin;
  }
  constexpr const T *data() const noexcept {
    return this->m_begin;
  }

  iterator begin() noexcept {
    return iterator{this->m_begin};
  }
  const_iterator begin() const noexcept {
    return const_iterator{this->m_begin};
  }

  iterator end() noexcept {
    return iterator{this->m_end};
  }
  const_iterator end() const noexcept {
    return const_iterator{this->m_end};
  }

  constexpr reference operator[](size_t n) noexcept {
    assert(n < size() && "vector[] index out of bounds");
    return this->m_begin[n];
  }
  constexpr const_reference operator[](size_t n) const noexcept {
    assert(n < size() && "vector[] index out of bounds");
    return this->m_begin[n];
  }

  constexpr reference at(size_t n) {
    if (n >= size())
      throw std::out_of_range("vector");
    return this->m_begin[n];
  }
  constexpr const_reference at(size_t n) const {
    if (n >= size())
      throw std::out_of_range("vector");
    return this->m_begin[n];
  }

  constexpr reference front() noexcept {
    assert(!empty() && "front() called on an empty vector");
    return *this->m_begin;
  }
  constexpr const_reference front() const noexcept {
    assert(!empty() && "front() called on an empty vector");
    return *this->m_begin;
  }

  constexpr reference back() noexcept {
    assert(!empty() && "back() called on an empty vector");
    return *(this->m_end - 1);
  }
  constexpr const_reference back() const noexcept {
    assert(!empty() && "back() called on an empty vector");
    return *(this->m_end - 1);
  }

};
)" };

static constexpr char const *RES_VECTOR{ R"(

template<typename T, size_t N = 0, bool s = N != 0>
class Vector;

template<typename T, size_t N>
class Vector<T, N, true> : protected detail::VectorBase<T> {

  using value_type = T;
  using reference = value_type&;
  using const_reference = const value_type&;
  using iterator = detail::Iterator<T>;
  using const_iterator = detail::Iterator<T>;


  T buffer[N];

  void deallocate_storage() noexcept(std::is_nothrow_destructible_v<T>) {
    clear();
    if (this->m_begin && this->m_begin != buffer) {
      std::allocator<T>().deallocate(this->m_begin, this->cap);
    }
    this->m_begin = buffer;
    this->m_end   = buffer;
    this->cap     = N;
  }

  void reallocate(size_t count, size_t size) {
    auto alloc = this->allocate(count);
    if (this->m_begin) {
      std::memcpy(alloc.ptr, this->m_begin, this->size() * sizeof(T));
      if (!is_inline()) { // TODO
        std::allocator<T>().deallocate(this->m_begin, this->cap);
      }
    }
    set_storage(alloc, size);
  }

  void reallocate(size_t count) {
    reallocate(count, count);
  }

  template<size_t X>
  void move_from(Vector<T, X> &&v) {
    if (v.is_inline()) {
      resize_uninitialized(v.size());
      move_items_from(std::forward<Vector<T, X>>(v));
      v.deallocate_storage();
    } else {
      std::swap(this->m_begin, v.m_begin);
      std::swap(this->m_end, v.m_end);
      std::swap(this->cap, v.cap);
    }
  }

public:
  constexpr Vector() = default;

  constexpr explicit Vector(size_t s) {
    resize(s);
  }

  template<size_t X>
  constexpr Vector(const Vector<T, X> &v) {
    if (!v.empty()) {
        reserve(v.size());
        this->m_end = this->m_begin + v.size();
        copy_items_from(v);
    }
  }

  template<size_t M>
  constexpr Vector(Vector<T, M> &&v) noexcept {
    move_from(std::forward<Vector<T, M>>(v));
  }

  ~Vector() noexcept(std::is_nothrow_destructible_v<T>) {
    deallocate_storage();
  }


  template<size_t X>
  Vector& operator=(Vector<T, X> &v) {
    if (this != &v) {
      clear();
      resize_uninitialized(v.size());
      copy_items_from(v);
    }
    return *this;
  }

  template<size_t X>
  Vector& operator=(Vector<T, X> &&v) {
    if (this != &v) {
      clear();
      move_from(std::forward<Vector<T, X>>(v));
    }
    return *this;
  }

  void clear() noexcept(std::is_nothrow_destructible_v<T>) {
    this->clear_storage();
  }

  void reserve(size_t s) {
    if (s > this->cap) {
      reallocate(s, size());
    }
  }

  void resize(size_t s) {
    size_t cs = size();
    if (s < cs) {
      if constexpr (!std::is_trivially_destructible_v<T>) {
        auto *old = this->m_end;
        this->m_end = this->m_begin + s;
        this->destroy_items(this->m_end, old);
      }
      else {
        this->m_end = this->m_begin + s;
      }
    }
    else if (s > cs) {
      T* it;
      if (s > this->cap) {
        reallocate(s);
        it = this->m_begin + cs;
      }
      else {
        it = this->m_end;
        this->m_end = this->m_begin + s;
      }
      for (; it != this->m_end; ++it) {
        std::construct_at(it);
      }
    }
  }

  [[nodiscard]] bool resize_noexcept(size_t s) noexcept {
    try {
      resize(s);
      return true;
    }
    catch (std::exception&) {
      return false;
    }
  }

  void resize_uninitialized(size_t s) requires(std::is_trivially_destructible_v<T>) {
    size_t cs = size();
    if (s < cs) {
      this->m_end = this->m_begin + s;
    }
    else if (s > cs) {
      if (s > this->cap) {
        reallocate(s);
      }
      else {
        this->m_end = this->m_begin + s;
      }
    }
  }

  constexpr bool is_inline() const noexcept {
    return this->m_begin == buffer;
  }

  size_t size() const noexcept {
    return this->m_end - this->m_begin;
  }

  size_t capacity() const noexcept {
    return this->cap;
  }

  constexpr size_t buffer_capacity() const {
    return N;
  }

  constexpr bool empty() const noexcept {
    return this->m_begin == this->m_end;
  }

  constexpr T *data() noexcept {
    return this->m_begin;
  }
  constexpr const T *data() const noexcept {
    return this->m_begin;
  }

  iterator begin() noexcept {
    return iterator{this->m_begin};
  }
  const_iterator begin() const noexcept {
    return const_iterator{this->m_begin};
  }

  iterator end() noexcept {
    return iterator{this->m_end};
  }
  const_iterator end() const noexcept {
    return const_iterator{this->m_end};
  }

  constexpr reference operator[](size_t n) noexcept {
    assert(n < size() && "vector[] index out of bounds");
    return this->m_begin[n];
  }
  constexpr const_reference operator[](size_t n) const noexcept {
    assert(n < size() && "vector[] index out of bounds");
    return this->m_begin[n];
  }

  constexpr reference at(size_t n) {
    if (n >= size())
      throw std::out_of_range("vector");
    return this->m_begin[n];
  }
  constexpr const_reference at(size_t n) const {
    if (n >= size())
      throw std::out_of_range("vector");
    return this->m_begin[n];
  }

  constexpr reference front() noexcept {
    assert(!empty() && "front() called on an empty vector");
    return *this->m_begin;
  }
  constexpr const_reference front() const noexcept {
    assert(!empty() && "front() called on an empty vector");
    return *this->m_begin;
  }

  constexpr reference back() noexcept {
    assert(!empty() && "back() called on an empty vector");
    return *(this->m_end - 1);
  }
  constexpr const_reference back() const noexcept {
    assert(!empty() && "back() called on an empty vector");
    return *(this->m_end - 1);
  }
};

template<typename T, size_t N>
class Vector<T, N, false> : protected detail::VectorBase<T> {

  using value_type = T;
  using reference = value_type&;
  using const_reference = const value_type&;
  using iterator = detail::Iterator<T>;
  using const_iterator = detail::Iterator<T>;

  void deallocate_storage() noexcept(std::is_nothrow_destructible_v<T>) {
    if (this->m_begin) {
      clear();
      std::allocator<T>().deallocate(this->m_begin, this->cap);
      this->m_begin = {};
      this->m_end   = {};
      this->cap     = 0;
    }
  }

  void reallocate(size_t count, size_t size) {
    auto alloc = this->allocate(count);
    if (this->m_begin) {
      std::memcpy(alloc.ptr, this->m_begin, this->size() * sizeof(T));
      std::allocator<T>().deallocate(this->m_begin, this->cap);
    }
    this->set_storage(alloc, size);
  }

  void reallocate(size_t count) {
    reallocate(count, count);
  }

  template<size_t X>
  void move_from(Vector<T, X> &&v) {
    if (v.is_inline()) {
      resize_uninitialized(v.size());
      move_items_from(std::forward<Vector<T, X>>(v));
      v.deallocate_storage();
    } else {
      std::swap(this->m_begin, v.m_begin);
      std::swap(this->m_end, v.m_end);
      std::swap(this->cap, v.cap);
    }
  }

  // void destroy_items(T *begin, T *end) noexcept(std::is_nothrow_destructible_v<T>) {
  //     for (auto* it = begin; it != end; ++it) {
  //       std::destroy_at(it);
  //     };
  // }

public:
  constexpr Vector() = default;

  constexpr explicit Vector(size_t s) : detail::VectorBase<T>() {
    resize(s);
  }

  template<size_t M>
  constexpr Vector(const Vector<T, M> &v) {
    if (!v.empty()) {
        reserve(v.size());
        this->m_end = this->m_begin + v.size();
        copy_items_from(v.m_begin);
    }
  }

  template<size_t X>
  constexpr Vector(Vector<T, X> &&v) noexcept {
    move_from(std::forward<Vector<T, X>>(v));
  }

  ~Vector() noexcept(std::is_nothrow_destructible_v<T>) {
    deallocate_storage();
  }

  template<size_t X>
  Vector& operator=(Vector<T, X> &v) {
    if (this != &v) {
      clear();
      resize_uninitialized(v.size());
      copy_items_from(v);
    }
    return *this;
  }

  template<size_t X>
  Vector& operator=(Vector<T, X> &&v) {
    if (this != &v) {
      clear();
      move_from(std::forward<Vector<T, X>>(v));
    }
    return *this;
  }





  void clear() noexcept(std::is_nothrow_destructible_v<T>) {
    this->clear_storage();
  }

  void reserve(size_t s) {
    if (s > this->cap) {
      reallocate(s, size());
    }
  }

  void resize(size_t s) {
    size_t cs = size();
    if (s < cs) {
      if constexpr (!std::is_trivially_destructible_v<T>) {
        auto *old = this->m_end;
        this->m_end = this->m_begin + s;
        destroy_items(this->m_end, old);
      }
      else {
        this->m_end = this->m_begin + s;
      }
    }
    else if (s > cs) {
      T* it;
      if (s > this->cap) {
        reallocate(s);
        it = this->m_begin + cs;
      }
      else {
        it = this->m_end;
        this->m_end = this->m_begin + s;
      }
      for (; it != this->m_end; ++it) {
        std::construct_at(it);
      }
    }
  }

  void resize_uninitialized(size_t s) requires(std::is_trivially_destructible_v<T>) {
    size_t cs = size();
    if (s < cs) {
      this->m_end = this->m_begin + s;
    }
    else if (s > cs) {
      if (s > this->cap) {
        reallocate(s);
      }
      else {
        this->m_end = this->m_begin + s;
      }
    }
  }

  constexpr bool is_inline() const noexcept {
    return false;
  }

  size_t size() const noexcept {
    return this->m_end - this->m_begin;
  }

  size_t capacity() const noexcept {
    return this->cap;
  }

  constexpr size_t buffer_capacity() const {
    return N;
  }

  constexpr bool empty() const noexcept {
    return this->m_begin == this->m_end;
  }

  constexpr T *data() noexcept {
    return this->m_begin;
  }
  constexpr const T *data() const noexcept {
    return this->m_begin;
  }

  iterator begin() noexcept {
    return iterator{this->m_begin};
  }
  const_iterator begin() const noexcept {
    return const_iterator{this->m_begin};
  }

  iterator end() noexcept {
    return iterator{this->m_end};
  }
  const_iterator end() const noexcept {
    return const_iterator{this->m_end};
  }

  constexpr reference operator[](size_t n) noexcept {
    assert(n < size() && "vector[] index out of bounds");
    return this->m_begin[n];
  }
  constexpr const_reference operator[](size_t n) const noexcept {
    assert(n < size() && "vector[] index out of bounds");
    return this->m_begin[n];
  }

  constexpr reference at(size_t n) {
    if (n >= size())
      throw std::out_of_range("vector");
    return this->m_begin[n];
  }
  constexpr const_reference at(size_t n) const {
    if (n >= size())
      throw std::out_of_range("vector");
    return this->m_begin[n];
  }

  constexpr reference front() noexcept {
    assert(!empty() && "front() called on an empty vector");
    return *this->m_begin;
  }
  constexpr const_reference front() const noexcept {
    assert(!empty() && "front() called on an empty vector");
    return *this->m_begin;
  }

  constexpr reference back() noexcept {
    assert(!empty() && "back() called on an empty vector");
    return *(this->m_end - 1);
  }
  constexpr const_reference back() const noexcept {
    assert(!empty() && "back() called on an empty vector");
    return *(this->m_end - 1);
  }
};
)" };

static constexpr char const *RES_BASE_TYPES{ R"(
  //==================
  //=== BASE TYPEs ===
  //==================

  using Bool32          = uint32_t;
  using DeviceAddress   = uint64_t;
  using DeviceSize      = uint64_t;
  using RemoteAddressNV = void *;
  using SampleMask      = uint32_t;
)" };

static constexpr char const *RES_FLAG_TRAITS{ R"(
  template <typename FlagBitsType>
  struct FlagTraits
  {
    static VULKAN_HPP_CONST_OR_CONSTEXPR bool isBitmask = false;
  };
)" };

static constexpr char const *RES_FLAGS_1{ R"(
  template <typename BitType>
  class Flags
  {
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
)" };

static constexpr char const *RES_FLAGS_2{ R"(
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
  };

#if !defined( VULKAN_HPP_HAS_SPACESHIP_OPERATOR )
  // relational operators only needed for pre C++20
  template <typename BitType>
  VULKAN_HPP_CONSTEXPR bool operator<( BitType bit, Flags<BitType> const & flags ) VULKAN_HPP_NOEXCEPT
  {
    return flags.operator>( bit );
  }

  template <typename BitType>
  VULKAN_HPP_CONSTEXPR bool operator<=( BitType bit, Flags<BitType> const & flags ) VULKAN_HPP_NOEXCEPT
  {
    return flags.operator>=( bit );
  }

  template <typename BitType>
  VULKAN_HPP_CONSTEXPR bool operator>( BitType bit, Flags<BitType> const & flags ) VULKAN_HPP_NOEXCEPT
  {
    return flags.operator<( bit );
  }

  template <typename BitType>
  VULKAN_HPP_CONSTEXPR bool operator>=( BitType bit, Flags<BitType> const & flags ) VULKAN_HPP_NOEXCEPT
  {
    return flags.operator<=( bit );
  }

  template <typename BitType>
  VULKAN_HPP_CONSTEXPR bool operator==( BitType bit, Flags<BitType> const & flags ) VULKAN_HPP_NOEXCEPT
  {
    return flags.operator==( bit );
  }

  template <typename BitType>
  VULKAN_HPP_CONSTEXPR bool operator!=( BitType bit, Flags<BitType> const & flags ) VULKAN_HPP_NOEXCEPT
  {
    return flags.operator!=( bit );
  }
#endif

  // bitwise operators
  template <typename BitType>
  VULKAN_HPP_CONSTEXPR Flags<BitType> operator&( BitType bit, Flags<BitType> const & flags ) VULKAN_HPP_NOEXCEPT
  {
    return flags.operator&( bit );
  }

  template <typename BitType>
  VULKAN_HPP_CONSTEXPR Flags<BitType> operator|( BitType bit, Flags<BitType> const & flags ) VULKAN_HPP_NOEXCEPT
  {
    return flags.operator|( bit );
  }

  template <typename BitType>
  VULKAN_HPP_CONSTEXPR Flags<BitType> operator^( BitType bit, Flags<BitType> const & flags ) VULKAN_HPP_NOEXCEPT
  {
    return flags.operator^( bit );
  }

)" };

static constexpr char const *RES_OPTIONAL{ R"(
  template <typename RefType>
  class Optional {
  public:
    Optional( RefType & reference ) VULKAN_HPP_NOEXCEPT
    {
      m_ptr = &reference;
    }
    Optional( RefType * ptr ) VULKAN_HPP_NOEXCEPT
    {
      m_ptr = ptr;
    }
    Optional( std::nullptr_t ) VULKAN_HPP_NOEXCEPT
    {
      m_ptr = nullptr;
    }

    operator RefType *() const VULKAN_HPP_NOEXCEPT
    {
      return m_ptr;
    }
    RefType const * operator->() const VULKAN_HPP_NOEXCEPT
    {
      return m_ptr;
    }
    explicit operator bool() const VULKAN_HPP_NOEXCEPT
    {
      return !!m_ptr;
    }

  private:
    RefType * m_ptr;
  };
)" };

static constexpr char const *RES_RAII{ R"(
    template <class T, class U = T>
    VULKAN_HPP_CONSTEXPR_14 VULKAN_HPP_INLINE T exchange( T & obj, U && newValue ) {
#  if ( 14 <= VULKAN_HPP_CPP_VERSION )
      return std::exchange<T>( obj, std::forward<U>( newValue ) );
#  else
      T oldValue = std::move( obj );
      obj        = std::forward<U>( newValue );
      return oldValue;
#  endif
    }
)" };

// static constexpr char const *RES_{R"()"};


namespace vkgen
{



    class FunctionGenerator
    {
      protected:
        using ArgVar = std::variant<Argument, const VariableData*>;
        struct ArgInit {
            std::string dst;
            std::string src;
        };

        const Generator &gen;
        std::vector<ArgVar> arguments;
        std::vector<ArgInit> inits;

        std::string getTemplate() const;

        void generatePrefix(std::string &output, bool declaration, bool isInline = false);

        void generateSuffix(std::string &output, bool declaration);

        void generateArguments(std::string &output, bool declaration);

        void generateArgument(std::string &output, const Argument &arg, bool declaration);

        void generateArgument(std::string &output, const VariableData *var, bool declaration);

        void generatePrototype(std::string &output, bool declaration, bool isInline = false);

        // TODO
        std::string generate(bool declaration, bool isInline = false);

      public:
        explicit FunctionGenerator(const Generator &gen) noexcept : gen(gen) {}

        FunctionGenerator(const Generator &gen, const std::string &type, const std::string &name) noexcept
          : gen(gen), type(type), name(name)
        {}

        std::string generate();

        std::string generate(GuardedOutputFuncs &impl);

        Argument& add(const std::string &type, const std::string &id, const std::string &assignment = "") {
            return get<Argument>(arguments.emplace_back(Argument{type, id, assignment}));
        }

        void addInit(const std::string &dst, const std::string &src) {
            inits.emplace_back(dst, src);
        }

        std::string      type;
        std::string      name;
        std::string      code;
        std::string      className;
        std::string      additonalTemplate;
        std::string      indent = "    ";
        Protect          optionalProtect;
        const GenericType *base = nullptr;
        bool          allowInline          = true;
        bool          specifierNodiscard   = false;
        bool          specifierInline      = {};
        bool          specifierExplicit    = {};
        bool          specifierNoexcept    = {};
        bool          specifierConst       = {};
        bool          specifierConstexpr   = {};
        bool          specifierConstexpr14 = {};
    };

    // TODO rename
    void Generator::genOptional(OutputBuffer &output, const GenericType &type, const std::function<void(OutputBuffer &)> &function) const {
        if (!type.canGenerate()) {
            return;
        }
        const auto &protect = type.getProtect();
        if (!protect.empty()) {
            output << "#if defined(" << protect << ")\n";
        }
        function(output);
        if (!protect.empty()) {
            output << "#endif // " << protect << "\n";
        }
    }

    void Generator::genPlatform(OutputBuffer &output, const GenericType &type, const std::function<void(OutputBuffer &)> &function) {
        genOptional(output, type, function);
    }

    void Generator::gen(OutputBuffer &output, const Define &define, const std::function<void(OutputBuffer &)> &function) const {
        if (define.state == Define::DISABLED) {
            return;
        }
        output << "#if ";
        if (define.type == Define::IF_NOT) {
            output << "!";
        }
        output << "defined( " << define.define << " )\n";

        function(output);

        output << "#endif // " << define.define << "\n";
    }

    std::string Generator::genNamespaceMacro(const Macro &m) {
        std::string output    = genMacro(m);
        const bool  stringify = true;
        if (m.usesDefine || stringify) {
            output += vkgen::format("#define {0}_STRING  VULKAN_HPP_STRINGIFY({1})\n", m.define, m.value);
        } else {
            output += vkgen::format("#define {0}_STRING  \"{1}\"\n", m.define, m.value);
        }
        return output;
    }

    void Generator::generateMacros(OutputBuffer &output) {
        output << R"(
#if defined( _MSVC_LANG )
#  define VULKAN_HPP_CPLUSPLUS _MSVC_LANG
#else
#  define VULKAN_HPP_CPLUSPLUS __cplusplus
#endif
)";
        if (cfg.gen.cppStd == 20) {
            output << R"(
#if 202002L < VULKAN_HPP_CPLUSPLUS
#  define VULKAN_HPP_CPP_VERSION 23
#elif 201703L < VULKAN_HPP_CPLUSPLUS
#  define VULKAN_HPP_CPP_VERSION 20
#else
#  error "vulkan.hpp needs at least c++ standard version 20"
#endif
)";
        } else if (cfg.gen.cppStd == 17) {
            output << R"(
#if 202002L < VULKAN_HPP_CPLUSPLUS
#  define VULKAN_HPP_CPP_VERSION 23
#elif 201703L < VULKAN_HPP_CPLUSPLUS
#  define VULKAN_HPP_CPP_VERSION 20
#elif 201402L < VULKAN_HPP_CPLUSPLUS
#  define VULKAN_HPP_CPP_VERSION 17
#else
#  error "vulkan.hpp needs at least c++ standard version 17"
#endif
)";
        } else {
            output << R"(
#if 202002L < VULKAN_HPP_CPLUSPLUS
#  define VULKAN_HPP_CPP_VERSION 23
#elif 201703L < VULKAN_HPP_CPLUSPLUS
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
)";
        }
        output << R"(
#if defined( VULKAN_HPP_DISABLE_ENHANCED_MODE )
#  if !defined( VULKAN_HPP_NO_SMART_HANDLE )
#    define VULKAN_HPP_NO_SMART_HANDLE
#  endif
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

#if defined( __clang__ )
#include <cassert>
#else
#include <assert.h>
#endif

#if !defined( VULKAN_HPP_ASSERT )
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

#if !defined( __has_include )
#  define __has_include( x ) false
#endif

)";
        if (cfg.gen.spaceshipOperator && !cfg.gen.globalMode) {
            output << R"(
//#if ( 201907 <= __cpp_lib_three_way_comparison ) && __has_include( <compare> ) && !defined( VULKAN_HPP_NO_SPACESHIP_OPERATOR )
//#  define VULKAN_HPP_HAS_SPACESHIP_OPERATOR
//#endif
#ifndef VULKAN_HPP_NO_SPACESHIP_OPERATOR
#  if __has_include( <compare> )
#    include <compare>
#    if ( 201907L <= __cpp_lib_three_way_comparison )
#      define VULKAN_HPP_HAS_SPACESHIP_OPERATOR
#    endif
#  endif
#endif
)";
        }

        if (cfg.gen.branchHint) {
            output << R"(
#if !defined( VULKAN_HPP_LIKELY )
#  if defined( __clang__ )
#    define VULKAN_HPP_LIKELY [[likely]]
#  else
#    define VULKAN_HPP_LIKELY
#  endif
#endif

#if !defined( VULKAN_HPP_UNLIKELY )
#  if defined( __clang__ )
#    define VULKAN_HPP_UNLIKELY [[unlikely]]
#  else
#    define VULKAN_HPP_UNLIKELY
#  endif
#endif
)";
        }

        output << R"(
#if ( 201803 <= __cpp_lib_span )
#  define VULKAN_HPP_SUPPORT_SPAN
#endif

#ifndef VK_USE_64_BIT_PTR_DEFINES
#  if defined( __LP64__ ) || defined( _WIN64 ) || ( defined( __x86_64__ ) && !defined( __ILP32__ ) ) || defined( _M_X64 ) || defined( __ia64 ) || \
    defined( _M_IA64 ) || defined( __aarch64__ ) || defined( __powerpc64__ ) || ( defined( __riscv ) && __riscv_xlen == 64 )
#    define VK_USE_64_BIT_PTR_DEFINES 1
#  else
#    define VK_USE_64_BIT_PTR_DEFINES 0
#  endif
#endif

// 32-bit vulkan is not typesafe for non-dispatchable handles, so don't allow copy constructors on this platform by default.
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

#if !defined( VULKAN_HPP_CONSTEXPR_INLINE )
#  if 201606L <= __cpp_inline_variables
#    define VULKAN_HPP_CONSTEXPR_INLINE VULKAN_HPP_CONSTEXPR inline
#  else
#    define VULKAN_HPP_CONSTEXPR_INLINE VULKAN_HPP_CONSTEXPR
#  endif
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

#if defined( VULKAN_HPP_NO_DEFAULT_DISPATCHER )
#  define VULKAN_HPP_DEFAULT_ARGUMENT_ASSIGNMENT
#  define VULKAN_HPP_DEFAULT_ARGUMENT_NULLPTR_ASSIGNMENT
#  define VULKAN_HPP_DEFAULT_DISPATCHER_ASSIGNMENT
#else
#  define VULKAN_HPP_DEFAULT_ARGUMENT_ASSIGNMENT         = {}
#  define VULKAN_HPP_DEFAULT_ARGUMENT_NULLPTR_ASSIGNMENT = nullptr
#  define VULKAN_HPP_DEFAULT_DISPATCHER_ASSIGNMENT       = VULKAN_HPP_DEFAULT_DISPATCHER
#endif

#ifndef VULKAN_HPP_DEFAULT_ALLOCATOR_ASSIGNMENT
#  define VULKAN_HPP_DEFAULT_ALLOCATOR_ASSIGNMENT        VULKAN_HPP_DEFAULT_ARGUMENT_NULLPTR_ASSIGNMENT
#endif

)";

        output << R"(
#define VULKAN_HPP_STRINGIFY2( text ) #text
#define VULKAN_HPP_STRINGIFY( text )  VULKAN_HPP_STRINGIFY2( text )
)";
        output << genNamespaceMacro(cfg.macro.mNamespace); // TODO move

        if (cfg.gen.raii.interop) {
            output << R"(
#ifdef VULKAN_HPP_EXPERIMENTAL_INTEROP
#   undef VULKAN_HPP_EXPERIMENTAL_NO_INTEROP
#else
#   define VULKAN_HPP_EXPERIMENTAL_NO_INTEROP
#endif

#ifdef VULKAN_HPP_EXPERIMENTAL_NO_INDIRECT_CMDS
#   undef VULKAN_HPP_EXPERIMENTAL_NO_RAII_INDIRECT
#   define VULKAN_HPP_EXPERIMENTAL_NO_RAII_INDIRECT_SUB
#else
#   undef VULKAN_HPP_EXPERIMENTAL_NO_RAII_INDIRECT_SUB
#   define VULKAN_HPP_EXPERIMENTAL_NO_RAII_INDIRECT
#endif

)";
        }

        output << "\n";
    }

    std::string Generator::generateHeader() {
        std::string output;

        output += "#include \"vulkan.h\"\n";
        output += "#include \"vulkan_hpp_macros.hpp\"\n";

        output += vkgen::format(R"(
static_assert(VK_HEADER_VERSION == {0}, "Wrong VK_HEADER_VERSION!");
)",
                                headerVersion);
        if (cfg.gen.importStdMacro) {
            output += R"(
#ifdef USE_IMPORT_STD
import std;
#else
)";
        }

        if (cfg.gen.cppStd >= 20) {
            output += R"(
#include <bit>   // std::bit_cast
)";
        }

        if (!cfg.gen.globalMode) {
            output += R"(
#include <algorithm>
)";
        } else {
            output += R"(
#include <type_traits>
)";
        }
        if (!cfg.gen.globalMode) {
            output += R"(
#include <array>   // ArrayWrapperND
)";
        }
        output += R"(
#include <cstring> // strcmp, std::memcpy
)";
        output += R"(
#include <string>  // std::string

/*
#if 17 <= VULKAN_HPP_CPP_VERSION
#  include <string_view>  // std::string_view
#endif
*/

#if !defined( VULKAN_HPP_DISABLE_ENHANCED_MODE )
#  include <tuple>   // std::tie
)";
        if (!cfg.gen.globalMode) {
            output += "#  include <vector>  // std::vector\n";
        }
        output += R"(
#endif

#if !defined( VULKAN_HPP_NO_EXCEPTIONS )
#  include <system_error>  // std::is_error_code_enum
#endif

#if defined( VULKAN_HPP_SUPPORT_SPAN )
#  include <span>
#endif
)";

        if (cfg.gen.globalMode) {
            output += R"(
#  include <cstdio>   // std::snprintf
)";
        } else if (cfg.gen.expApi) {
            output += R"(
#ifdef VULKAN_HPP_EXPERIMENTAL_HEX
#  include <cstdio>   // std::snprintf
#elif __cpp_lib_format
#  include <format>   // std::format
#else
#  include <sstream>  // std::stringstream
#endif
)";
        } else {
            output += R"(
#if __cpp_lib_format
#  include <format>   // std::format
#else
#  include <sstream>  // std::stringstream
#endif
)";
        }

        if (cfg.gen.importStdMacro) {
            output += R"(
#endif
)";
        }

        output += "\n";
        return output;
    }

    void Generator::generateFlags(vkgen::OutputBuffer &output) const {
        if (cfg.gen.altFlagTraits) {
            output << R"(
  template <typename BitType>
  class Flags
  {
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
      return Flags<BitType>( m_mask ^ static_cast<MaskType>(BitType::allFlags) );
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
  };

  #if !defined( VULKAN_HPP_HAS_SPACESHIP_OPERATOR )
  // relational operators only needed for pre C++20
  template <typename BitType>
  VULKAN_HPP_CONSTEXPR bool operator<( BitType bit, Flags<BitType> const & flags ) VULKAN_HPP_NOEXCEPT
  {
    return flags.operator>( bit );
  }

  template <typename BitType>
  VULKAN_HPP_CONSTEXPR bool operator<=( BitType bit, Flags<BitType> const & flags ) VULKAN_HPP_NOEXCEPT
  {
    return flags.operator>=( bit );
  }

  template <typename BitType>
  VULKAN_HPP_CONSTEXPR bool operator>( BitType bit, Flags<BitType> const & flags ) VULKAN_HPP_NOEXCEPT
  {
    return flags.operator<( bit );
  }

  template <typename BitType>
  VULKAN_HPP_CONSTEXPR bool operator>=( BitType bit, Flags<BitType> const & flags ) VULKAN_HPP_NOEXCEPT
  {
    return flags.operator<=( bit );
  }

  template <typename BitType>
  VULKAN_HPP_CONSTEXPR bool operator==( BitType bit, Flags<BitType> const & flags ) VULKAN_HPP_NOEXCEPT
  {
    return flags.operator==( bit );
  }

  template <typename BitType>
  VULKAN_HPP_CONSTEXPR bool operator!=( BitType bit, Flags<BitType> const & flags ) VULKAN_HPP_NOEXCEPT
  {
    return flags.operator!=( bit );
  }
  #endif

  // bitwise operators
  template <typename BitType>
  VULKAN_HPP_CONSTEXPR Flags<BitType> operator&( BitType bit, Flags<BitType> const & flags ) VULKAN_HPP_NOEXCEPT
  {
    return flags.operator&( bit );
  }

  template <typename BitType>
  VULKAN_HPP_CONSTEXPR Flags<BitType> operator|( BitType bit, Flags<BitType> const & flags ) VULKAN_HPP_NOEXCEPT
  {
    return flags.operator|( bit );
  }

  template <typename BitType>
  VULKAN_HPP_CONSTEXPR Flags<BitType> operator^( BitType bit, Flags<BitType> const & flags ) VULKAN_HPP_NOEXCEPT
  {
    return flags.operator^( bit );
  }


  // bitwise operators on BitType
  template <typename BitType>
  VULKAN_HPP_INLINE VULKAN_HPP_CONSTEXPR Flags<BitType> operator&( BitType lhs, BitType rhs ) VULKAN_HPP_NOEXCEPT
  {
    return Flags<BitType>( lhs ) & rhs;
  }
  template <typename BitType>
  VULKAN_HPP_INLINE VULKAN_HPP_CONSTEXPR Flags<BitType> operator~( BitType bit ) VULKAN_HPP_NOEXCEPT
  {
    return ~( Flags<BitType>( bit ) );
  }

  template <typename BitType>
  VULKAN_HPP_INLINE VULKAN_HPP_CONSTEXPR Flags<BitType> operator|( BitType lhs, BitType rhs ) VULKAN_HPP_NOEXCEPT
  {
    return Flags<BitType>( lhs ) | rhs;
  }

  template <typename BitType>
  VULKAN_HPP_INLINE VULKAN_HPP_CONSTEXPR Flags<BitType> operator^( BitType lhs, BitType rhs ) VULKAN_HPP_NOEXCEPT
  {
    return Flags<BitType>( lhs ) ^ rhs;
  }
)";
            return;
        }


        output << expIfndef("VULKAN_HPP_EXPERIMENTAL_NO_FLAG_TRAITS");
        output << RES_FLAG_TRAITS;
        output << expEndif("VULKAN_HPP_EXPERIMENTAL_NO_FLAG_TRAITS");
        output << RES_FLAGS_1;
        output << expIfndef("VULKAN_HPP_EXPERIMENTAL_NO_FLAG_TRAITS");
        output << R"(
    VULKAN_HPP_CONSTEXPR Flags<BitType> operator~() const VULKAN_HPP_NOEXCEPT
    {
      return Flags<BitType>( m_mask ^ FlagTraits<BitType>::allFlags.m_mask );
    }
)";
        output << expEndif("VULKAN_HPP_EXPERIMENTAL_NO_FLAG_TRAITS");
        output << RES_FLAGS_2;
        output << expIfndef("VULKAN_HPP_EXPERIMENTAL_NO_FLAG_TRAITS");
        output << R"(
  // bitwise operators on BitType
  template <typename BitType, typename std::enable_if<FlagTraits<BitType>::isBitmask, bool>::type = true>
  VULKAN_HPP_INLINE VULKAN_HPP_CONSTEXPR Flags<BitType> operator&( BitType lhs, BitType rhs ) VULKAN_HPP_NOEXCEPT
  {
    return Flags<BitType>( lhs ) & rhs;
  }
  template <typename BitType, typename std::enable_if<FlagTraits<BitType>::isBitmask, bool>::type = true>
  VULKAN_HPP_INLINE VULKAN_HPP_CONSTEXPR Flags<BitType> operator~( BitType bit ) VULKAN_HPP_NOEXCEPT
  {
    return ~( Flags<BitType>( bit ) );
  }
)";

        if (cfg.gen.expApi) {
            output << R"(
#ifndef VULKAN_HPP_EXPERIMENTAL_NO_FLAG_TRAITS
  template <typename BitType, typename std::enable_if<FlagTraits<BitType>::isBitmask, bool>::type = true>
#else
  template <typename BitType>
#endif
  VULKAN_HPP_INLINE VULKAN_HPP_CONSTEXPR Flags<BitType> operator|( BitType lhs, BitType rhs ) VULKAN_HPP_NOEXCEPT
  {
    return Flags<BitType>( lhs ) | rhs;
  }
#ifndef VULKAN_HPP_EXPERIMENTAL_NO_FLAG_TRAITS
  template <typename BitType, typename std::enable_if<FlagTraits<BitType>::isBitmask, bool>::type = true>
#else
  template <typename BitType>
#endif
  VULKAN_HPP_INLINE VULKAN_HPP_CONSTEXPR Flags<BitType> operator^( BitType lhs, BitType rhs ) VULKAN_HPP_NOEXCEPT
  {
    return Flags<BitType>( lhs ) ^ rhs;
  }
)";
        } else {
            output << R"(
  template <typename BitType, typename std::enable_if<FlagTraits<BitType>::isBitmask, bool>::type = true>
  VULKAN_HPP_INLINE VULKAN_HPP_CONSTEXPR Flags<BitType> operator|( BitType lhs, BitType rhs ) VULKAN_HPP_NOEXCEPT
  {
    return Flags<BitType>( lhs ) | rhs;
  }

  template <typename BitType, typename std::enable_if<FlagTraits<BitType>::isBitmask, bool>::type = true>
  VULKAN_HPP_INLINE VULKAN_HPP_CONSTEXPR Flags<BitType> operator^( BitType lhs, BitType rhs ) VULKAN_HPP_NOEXCEPT
  {
    return Flags<BitType>( lhs ) ^ rhs;
  }
)";
        }

        output << expEndif("VULKAN_HPP_EXPERIMENTAL_NO_FLAG_TRAITS");
    }

    std::string FunctionGenerator::getTemplate() const {
        return additonalTemplate;
    }

    void FunctionGenerator::generatePrefix(std::string &output, bool declaration, bool isInline) {
        const auto &cfg = gen.config();
        if (specifierInline) {
            if (isInline || !cfg.gen.cppFiles) {
                output += cfg.macro.mInline.get() + " ";
            }
        }
        const auto add = [&](bool enabled, const std::string_view value) {
            if (enabled) {
                output += value;
                output += ' ';
            }
        };
        add(specifierExplicit && declaration, gen.m_explicit);
        add(specifierConstexpr, gen.m_constexpr);
        add(specifierConstexpr14, gen.m_constexpr14);
    }

    void FunctionGenerator::generateSuffix(std::string &output, bool declaration) {
        const auto add = [&](bool enabled, const std::string_view value) {
            if (enabled) {
                output += ' ';
                output += value;
            }
        };
        add(specifierConst, "const");
        add(specifierNoexcept, gen.m_noexcept);
    }

    void FunctionGenerator::generateArgument(std::string &output, const Argument &arg, bool declaration) {
        output += arg.type;
        output += ' ';
        output += arg.id;
        if (declaration) {
            output += arg.assignment;
        }
    }

    void FunctionGenerator::generateArgument(std::string &output, const VariableData *var, bool declaration) {
        output += "/*V*/";
    }

    void FunctionGenerator::generateArguments(std::string &output, bool declaration) {
        for (const auto &a : arguments) {
            std::visit(
              [&](auto &&arg) {
                  generateArgument(output, arg, declaration);
                  output += ", ";
              },
              a);
        }
        if (output.ends_with(", ")) {
            output.erase(output.end() - 2);
        }
    }

    void FunctionGenerator::generatePrototype(std::string &output, bool declaration, bool isInline) {
        generatePrefix(output, declaration, isInline);
        std::string templ = getTemplate();
        if (!templ.empty()) {
            output += "template<";
            output += templ;
            output += ">\n";
            output += indent;
        }
        output += type;
        if (!type.empty()) {
            output += ' ';
        }
        if (!declaration && !className.empty()) {
            output += className;
            output += "::";
        }
        output += name;
        output += '(';
        generateArguments(output, declaration);
        output += ")";
        generateSuffix(output, declaration);
    }

    std::string FunctionGenerator::generate(bool declaration, bool isInline) {
        std::string output = indent;
        generatePrototype(output, declaration, isInline);
        output += '\n';
        if (!inits.empty()) {
            output += indent;
            output += "  : ";
            for (size_t i = 0; i < inits.size(); ++i) {
                output += inits[i].dst;
                output += '(';
                output += inits[i].src;
                output += ')';
                if (i != inits.size() - 1) {
                    output += ',';
                }
                output += '\n';
                output += indent;
                output += "    ";
            }
            strStripSuffix(output, "    ");
        }
        output += indent;
        output += "{\n";
        output += code;
        output += indent;
        output += "}\n\n";
        return output;
    }

    std::string FunctionGenerator::generate() {
        return generate(true, true);
    }

    std::string FunctionGenerator::generate(GuardedOutputFuncs &impl) {
        const bool hasTemplate = !getTemplate().empty();
        bool       genInline   = allowInline;
        if (gen.config().gen.cppFiles && (!hasTemplate || !allowInline)) {
            genInline = false;
        }
        if (genInline) {
            return generate();
        }
        std::string output = indent;
        generatePrototype(output, true);
        output += ";\n\n";

        std::vector<Protect> protects;
        if (!optionalProtect.first.empty()) {
            protects.emplace_back(optionalProtect);
        }
        if (base) {
            const auto &p = base->getProtect();
            if (!p.empty()) {
                protects.emplace_back(p, true);
            }
        }

        std::string def = generate(false);
        if (hasTemplate) {
            impl.templ.get(protects) << std::move(def);
        } else {
            impl.def.get(protects) << std::move(def);
        }
        return output;
    }

    class UniqueBaseGenerator
    {
        const Generator &gen;
        bool             exp;

      public:
        bool        alloc;
        bool        dispatch;
        bool        owner      = true;
        bool        destroy    = true;
        bool        destroyRef = false;
        bool        pool       = false;
        std::string name;
        std::string templ;
        std::string specialization;
        std::string destroyType;

        UniqueBaseGenerator(const Generator &gen) noexcept : gen(gen), exp(gen.config().gen.expApi), alloc(gen.config().gen.allocatorParam) {
            dispatch = !exp;
        }

        void generate(OutputBuffer &output) {
            OutputClass out{ .name = this->name + specialization };
            std::string ownerType = exp ? "OwnerType*" : "OwnerType";

            out.sPublic << "    " << name << "() = default;\n";

            ArgumentBuilder args(false);
            if (owner) {
                args.append(exp ? "OwnerType&" : "OwnerType", " owner", "", "m_owner", exp);
                out.sPrivate << "    " << ownerType << " m_owner = {};\n";
            }
            if (pool) {
                args.append("PoolType", " pool", "", "m_pool");
                out.sPrivate << "    PoolType m_pool = {};\n";
            }
            if (alloc) {
                args.append("Optional<const AllocationCallbacks>", " allocationCallbacks", " VULKAN_HPP_DEFAULT_ALLOCATOR_ASSIGNMENT", "m_allocationCallbacks");
                out.sPrivate << "    Optional<const AllocationCallbacks> m_allocationCallbacks = nullptr;\n";
            }
            if (dispatch) {
                args.append("Dispatch const &", "dispatch", " VULKAN_HPP_DEFAULT_DISPATCHER_ASSIGNMENT", "m_dispatch", true);
                out.sPrivate << "    Dispatch const *                    m_dispatch            = nullptr;\n";
            }

            if (args.size() > 0) {
                out.sPublic << "    " << name << "(" << args.string() << ") VULKAN_HPP_NOEXCEPT\n";
                out.sPublic << "     " << args.initializer() << "\n";
                out.sPublic << "    {\n";
                out.sPublic << "    }\n";
            }

            if (owner) {
                out.sPublic << vkgen::format(R"(
    {0} getOwner() const VULKAN_HPP_NOEXCEPT
    {{
      return m_owner;
    }}
)",
                                             ownerType);
            }
            if (alloc) {
                out.sPublic << vkgen::format(R"(
    Optional<const AllocationCallbacks> getAllocator() const VULKAN_HPP_NOEXCEPT
    {{
      return m_allocationCallbacks;
    }}
)");
            }
            if (dispatch) {
                out.sPublic << vkgen::format(R"(
    Dispatch const & getDispatch() const VULKAN_HPP_NOEXCEPT
    {{
      return *m_dispatch;
    }}
)");
            }
            if (destroy) {
                std::string assert;
                std::string code = "      ";
                if (owner) {
                    assert += "m_owner";
                    code += "m_owner";
                    code += exp ? "->" : ".";
                } else {
                    code += "t.";
                }

                if (dispatch) {
                    if (!assert.empty()) {
                        assert += " && ";
                    }
                    assert += "m_dispatch";
                }
                if (!assert.empty()) {
                    assert = "VULKAN_HPP_ASSERT ( " + assert + " );\n";
                }

                code += destroyType;
                code += "(";
                if (pool) {
                    code += "m_pool";
                }
                if (owner) {
                    if (pool) {
                        code += ", ";
                    }
                    code += "t";
                }
                if (alloc) {
                    if (owner) {
                        code += ", ";
                    }
                    code += "m_allocationCallbacks";
                }
                if (dispatch) {
                    if (owner || alloc) {
                        code += ", ";
                    }
                    code += "*m_dispatch";
                }
                code += ");\n";
                out.sProtected << vkgen::format(R"(
    template <typename T>
    void destroy( {2} t ) VULKAN_HPP_NOEXCEPT
    {{
      {0}{1}
    }}
)",
                                                assert,
                                                code,
                                                destroyRef ? "const T&" : "T");
            }

            std::string t = templ;

            if (pool) {
                if (!t.empty()) {
                    t += ", ";
                }
                t += "typename PoolType";
            }
            if (dispatch) {
                if (!t.empty()) {
                    t += ", ";
                }
                t += "typename Dispatch";
            }
            output << "  template <" << t << ">\n";
            output << std::move(out);
            output << "\n";
        }
    };

    void Generator::generateMainFile(GenOutput &files) {
        auto &output = files.addFile("");

        output << generateHeader();

        generateArrays(files, output);
        generateVector(files, output);

        output << beginNamespace();

        output << RES_OPTIONAL;

        if (!cfg.gen.globalMode) {
            gen(output, cfg.gen.smartHandles, [&](auto &output) { generateUniqueHandlesBase(output); });
        }

        if (!cfg.gen.globalMode) {
            generateDispatch(output);
            output << RES_BASE_TYPES;
        }

        if (cfg.gen.globalMode) {
            output << R"(
  VULKAN_HPP_INLINE std::string toHexString( uint32_t value )
  {
    std::string str;
    str.resize(6);
    int n = std::snprintf(str.data(), str.size(), "%x", value);
    VULKAN_HPP_ASSERT( n > 0 );
    return str;
  }
)";

        } else if (cfg.gen.expApi) {
            output << R"(
  VULKAN_HPP_INLINE std::string toHexString( uint32_t value )
  {
#ifdef VULKAN_HPP_EXPERIMENTAL_HEX
    std::string str;
    str.resize(6);
    int n = std::snprintf(str.data(), str.size(), "%x", value);
    VULKAN_HPP_ASSERT( n > 0 );
    return str;
#elif __cpp_lib_format
    return std::format( "{:x}", value );
#else
    std::stringstream stream;
    stream << std::hex << value;
    return stream.str();
#endif
  }
)";
        } else {
            output << R"(
  VULKAN_HPP_INLINE std::string toHexString( uint32_t value )
  {
#if __cpp_lib_format
    return std::format( "{:x}", value );
#else
    std::stringstream stream;
    stream << std::hex << value;
    return stream.str();
#endif
  }
)";
        }

        output << endNamespace();
        if (!cfg.gen.onlyC) {
            output << "#include \"vulkan_enums.hpp\"\n";
            output << R"(#if !defined( VULKAN_HPP_NO_TO_STRING )
#  include "vulkan_to_string.hpp"
#endif
)";
        }
        output << beginNamespace();
        generateErrorClasses(output);
        output << "\n";
        generateResultValue(output);

        generateApiConstants(output);

        output << R"(
  //=========================
  //=== CONSTEXPR CALLEEs ===
  //=========================
  template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
  VULKAN_HPP_CONSTEXPR uint32_t apiVersionMajor( T const version )
  {
    return ( ( ( uint32_t )( version ) >> 22U ) & 0x7FU );
  }
  template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
  VULKAN_HPP_CONSTEXPR uint32_t apiVersionMinor( T const version )
  {
    return ( ( ( uint32_t )( version ) >> 12U ) & 0x3FFU );
  }
  template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
  VULKAN_HPP_CONSTEXPR uint32_t apiVersionPatch( T const version )
  {
    return ( ( uint32_t )(version)&0xFFFU );
  }
  template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
  VULKAN_HPP_CONSTEXPR uint32_t apiVersionVariant( T const version )
  {
    return ( ( uint32_t )( version ) >> 29U );
  }
  template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
  VULKAN_HPP_CONSTEXPR uint32_t makeApiVersion( T const variant, T const major, T const minor, T const patch )
  {
    return ( ( ( ( uint32_t )( variant ) ) << 29U ) | ( ( ( uint32_t )( major ) ) << 22U ) | ( ( ( uint32_t )( minor ) ) << 12U ) | ( ( uint32_t )( patch ) ) );
  }
  template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
  VULKAN_HPP_DEPRECATED( "This define is deprecated. VK_MAKE_API_VERSION should be used instead." )
  VULKAN_HPP_CONSTEXPR uint32_t makeVersion( T const major, T const minor, T const patch )
  {
    return ( ( ( ( uint32_t )( major ) ) << 22U ) | ( ( ( uint32_t )( minor ) ) << 12U ) | ( ( uint32_t )( patch ) ) );
  }
  template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
  VULKAN_HPP_DEPRECATED( "This define is deprecated. VK_API_VERSION_MAJOR should be used instead." )
  VULKAN_HPP_CONSTEXPR uint32_t versionMajor( T const version )
  {
    return ( ( uint32_t )( version ) >> 22U );
  }
  template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
  VULKAN_HPP_DEPRECATED( "This define is deprecated. VK_API_VERSION_MINOR should be used instead." )
  VULKAN_HPP_CONSTEXPR uint32_t versionMinor( T const version )
  {
    return ( ( ( uint32_t )( version ) >> 12U ) & 0x3FFU );
  }
  template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
  VULKAN_HPP_DEPRECATED( "This define is deprecated. VK_API_VERSION_PATCH should be used instead." )
  VULKAN_HPP_CONSTEXPR uint32_t versionPatch( T const version )
  {
    return ( ( uint32_t )(version)&0xFFFU );
  }

  //=========================
  //=== CONSTEXPR CALLERs ===
  //=========================
  VULKAN_HPP_CONSTEXPR_INLINE auto ApiVersion            = makeApiVersion( 0, 1, 0, 0 );
  VULKAN_HPP_CONSTEXPR_INLINE auto ApiVersion10          = makeApiVersion( 0, 1, 0, 0 );
  VULKAN_HPP_CONSTEXPR_INLINE auto ApiVersion11          = makeApiVersion( 0, 1, 1, 0 );
  VULKAN_HPP_CONSTEXPR_INLINE auto ApiVersion12          = makeApiVersion( 0, 1, 2, 0 );
  VULKAN_HPP_CONSTEXPR_INLINE auto ApiVersion13          = makeApiVersion( 0, 1, 3, 0 );
  VULKAN_HPP_CONSTEXPR_INLINE auto HeaderVersionComplete = makeApiVersion( 0, 1, 3, VK_HEADER_VERSION );
)";

        output << endNamespace();

        if (cfg.gen.globalMode) {
            output << "#include \"vulkan_types.hpp\"\n";
        } else {
            output << "#include \"vulkan_handles.hpp\"\n";
            output << "#include \"vulkan_structs.hpp\"\n";
        }
        output << "#include \"vulkan_context.hpp\"\n";
        if (cfg.gen.globalMode) {
            output << "\n";
            output << beginNamespace();
            output << "  extern " + m_ns + "::" + loader.name + " " + strFirstLower(loader.name) + ";\n";
            for (const Handle &t : topLevelHandles) {
                output << "  extern " + m_ns + "::" + t.name + " " + strFirstLower(t.name) + ";\n";
            }
            output << "\n";
            for (Handle &t : topLevelHandles) {
                for (auto &cmd : t.ctorCmds) {
                    output << "  // " + cmd.name.original + "\n";
                    /* in progress
                    GuardedOutput decl;
                    MemberGeneratorExperimental g{ *this, cmd, decl, outputFuncs, true };
                    g.generate();
                    std::stringstream str;
                    decl.write(str);
                    output += std::move(str.str());
                    */
                }
            }
            output << endNamespace();
            output << "\n";
        }

        if (!cfg.gen.onlyC) {
            output << "#ifndef VULKAN_HPP_NO_STRUCT_CHAIN\n";
            generateStructChain(files, output);
            output << "#endif // VULKAN_HPP_NO_STRUCT_CHAIN\n";
        }

        output << "#include \"vulkan_funcs.hpp\"\n";

        output << "\n";

#ifdef INST
        output += Inst::mainFileEnd();
#endif
    }

    static void createPath(const std::filesystem::path &path) {
        if (std::filesystem::exists(path)) {
            return;
        }
        std::error_code ec;
        std::filesystem::create_directories(path, ec);
        if (ec) {
            throw std::runtime_error{ "Can't create directory " + path.string() + ": " + ec.message() };
        }
    }

    void Generator::generateModuleEnums(OutputBuffer &output) {
        gen(output, cfg.gen.handleTemplates, [&](auto &output) {
            output << vkgen::format(R"(
  //=============
  //=== ENUMs ===
  //=============
  using {0}::CppType;
)",
                                    m_ns);
        });

        GuardedOutput out;

        std::unordered_set<std::string> generated;
        std::array<Protect, 1>          protect;
        protect[0].second = true;
        for (const auto &e : enums) {

            const auto *t = find(e.name.original);
            if (t && !t->canGenerate()) {
                continue;
            }

            protect[0].first = e.getProtect();
            auto &output     = out.get(protect);
            if (generated.contains(e.name)) {
                // std::cout << ">> skip enum: " << e.name << std::endl;
                // output += std::move("  // skip: " + e.name + "\n");
                // continue;
            } else {
                output << "  using " << m_ns << "::" << e.name + ";\n";
                std::string name = e.name;
                auto        tag  = strRemoveTag(name);
                if (name.ends_with("FlagBits")) {
                    name = std::regex_replace(e.name, std::regex("FlagBits"), "Flags");
                    output << "  using " << m_ns + "::" << name << ";\n";
                }
            }

            generated.insert(e.name);
        }

        output << std::move(out);
    }

    void Generator::generateModuleHandles(OutputBuffer &output) {
        output << R"(
  //===============
  //=== HANDLEs ===
  //===============
)";

        if (cfg.gen.expApi) {
            output << "using " << m_ns + "::" << loader.name << ";\n";
        }
        for (const Handle &e : handles.ordered) {
            genOptional(output, e, [&](auto &output) { output << "  using " << m_ns << "::" << e.name << ";\n"; });
        }

        gen(output, cfg.gen.smartHandles, [&](auto &output) {
            output << R"(
  //======================
  //=== UNIQUE HANDLEs ===
  //======================
#ifndef  VULKAN_HPP_NO_SMART_HANDLE
)";

            for (const Handle &e : handles.ordered) {
                if (e.uniqueVariant()) {
                    genOptional(output, e, [&](auto &output) { output << "  using " << m_ns << "::Unique" << e.name << ";\n"; });
                }
            }

            output << "#endif // VULKAN_HPP_NO_SMART_HANDLE\n";
        });
    }

    void Generator::generateModuleStructs(OutputBuffer &output) {
        output << R"(
  //===============
  //=== STRUCTs ===
  //===============
)";
        GuardedOutput out;

        std::array<Protect, 1> protect;
        protect[0].second = true;
        for (const auto &e : structs) {
            protect[0].first = e.getProtect();
            auto &output     = out.get(protect);
            output << "  using " << m_ns << "::" << e.name << ";\n";
        }

        output << std::move(out);
    }

    void Generator::generateModules(GenOutput &main, std::filesystem::path path) {
        GenOutput outModule{ "vulkan", ".cppm", path };
        auto     &module_output = outModule.addFile("", ".cppm");

        module_output << "module;\n\n";

        module_output << "#include \"" << main.getFilename("") << "\"\n";
        if (cfg.gen.raii.enabled) {
            module_output << "#include \"" << main.getFilename("_raii") << "\"\n";
        }

        module_output << "\nexport module vulkan;\n\n";

        module_output << "  // VULKAN CORE\n";
        module_output << "export {\n";

        module_output << R"(
  //=============
  //=== ENUMs ===
  //=============
)";

        for (const auto &e : enums) {
            // if (e.version.empty()) {
            //     continue;
            // }
            const auto *t = find(e.name.original);
            if (t && !t->canGenerate()) {
                continue;
            }

            const auto *base = reinterpret_cast<const GenericType *>(&e);
            if (t) {
                base = reinterpret_cast<const GenericType *>(t);
            }

            genOptional(module_output, *base, [&](auto &output) {
                std::string name = e.name.original;
                if (e.members.empty()) {
                    name = std::regex_replace(name, std::regex("FlagBits"), "Flags");
                }
                output << "  using ::" << name << ";";
                output << "\n";
            });
        }

        module_output << R"(
  //===============
  //=== STRUCTs ===
  //===============
)";

        for (const auto &e : this->structs) {
            genOptional(module_output, e, [&](auto &output) { output << "  using ::" << e.name.original << ";\n"; });
        }

        module_output << R"(
  //===============
  //=== HANDLEs ===
  //===============
)";

        for (const auto &e : this->handles) {
            genOptional(module_output, e, [&](auto &output) { output << "  using ::" << e.name.original << ";\n"; });
        }

        module_output << "}\n";

        module_output << "export " << beginNamespace();

        module_output << vkgen::format(R"(
  //=====================================
  //=== HARDCODED TYPEs AND FUNCTIONs ===
  //=====================================
  using {0}::ArrayWrapper1D;
  using {0}::ArrayWrapper2D;
  // using {0}::DispatchLoaderBase;
  using {0}::Flags;
  using {0}::FlagTraits;

#if !defined( VK_NO_PROTOTYPES )
  // using {0}::DispatchLoaderStatic;
#endif /*VK_NO_PROTOTYPES*/

  using {0}::operator&;
  using {0}::operator|;
  using {0}::operator^;
  using {0}::operator~;
  // using VULKAN_HPP_DEFAULT_DISPATCHER_TYPE;

#if !defined( VULKAN_HPP_DISABLE_ENHANCED_MODE )
  using {0}::ArrayProxy;
  using {0}::ArrayProxyNoTemporaries;
  using {0}::Optional;
  // using {0}::StridedArrayProxy;
  using {0}::StructureChain;
  // using {0}::UniqueHandle;
#endif /*VULKAN_HPP_DISABLE_ENHANCED_MODE*/

#if !defined( VULKAN_HPP_NO_SMART_HANDLE )
  // using {0}::ObjectDestroy;
  // using {0}::ObjectFree;
  // using {0}::ObjectRelease;
  // using {0}::PoolFree;
#endif /*VULKAN_HPP_NO_SMART_HANDLE*/

  //==================
  //=== BASE TYPEs ===
  //==================
  using {0}::Bool32;
  using {0}::DeviceAddress;
  using {0}::DeviceSize;
  using {0}::RemoteAddressNV;
  using {0}::SampleMask;

)",
                                       m_ns);

        generateModuleEnums(module_output);

        module_output << vkgen::format(R"(
  //=========================
  //=== Index Type Traits ===
  //=========================
  //using {0}::IndexTypeValue;

  //======================
  //=== ENUM to_string ===
  //======================
#if !defined( VULKAN_HPP_NO_TO_STRING )
  using {0}::to_string;
  using {0}::toHexString;
#endif /*VULKAN_HPP_NO_TO_STRING*/

  //=============================
  //=== EXCEPTIONs AND ERRORs ===
  //=============================
#if !defined( VULKAN_HPP_NO_EXCEPTIONS )
)",
                                       m_ns);
        if (cfg.gen.unifiedException) {
            module_output << vkgen::format(R"(
#  ifdef VULKAN_HPP_UNIFIED_EXCEPTION
  using {0}::Error;
#  else
)",
                                           m_ns);
        }

        std::set<std::string>        errors;
        std::set<const ErrorClass *> platformErrors;
        for (const auto &e : errorClasses) {
            if (e.value.getProtect().empty()) {
                errors.insert(e.name);
            } else {
                platformErrors.insert(&e);
            }
        }
        errors.insert("make_error_code");
        errors.insert("make_error_condition");
        errors.insert("Error");
        errors.insert("LogicError");
        errors.insert("SystemError");
        for (const auto &e : errors) {
            module_output << "  using " << m_ns << "::" << e << ";\n";
        }
        for (const auto &e : platformErrors) {
            genOptional(module_output, e->value, [&](auto &output) { output << "  using " << m_ns << "::" << e->name << ";\n"; });
        }
        if (cfg.gen.unifiedException) {
            module_output << "#  endif // VULKAN_HPP_UNIFIED_EXCEPTION\n";
        }

        module_output << vkgen::format(R"(#endif /*VULKAN_HPP_NO_EXCEPTIONS*/

  using {0}::createResultValueType;
  using {0}::ignore;
  using {0}::resultCheck;
  using {0}::ResultValue;
  using {0}::ResultValueType;

  //=========================================
  //=== CONSTEXPR CONSTANTs AND FUNCTIONs ===
  //=========================================
)",
                                       m_ns);

        for (const auto &a : apiConstants) {
            genOptional(module_output, a, [&](auto &output) { output << "  using " << m_ns << "::" << a.name << ";\n"; });
        }

        module_output << vkgen::format(R"(
  //========================
  //=== CONSTEXPR VALUEs ===
  //========================
  using {0}::HeaderVersion;

  //=========================
  //=== CONSTEXPR CALLEEs ===
  //=========================
  using {0}::apiVersionMajor;
  using {0}::apiVersionMinor;
  using {0}::apiVersionPatch;
  using {0}::apiVersionVariant;
  using {0}::makeApiVersion;
  using {0}::makeVersion;
  using {0}::versionMajor;
  using {0}::versionMinor;
  using {0}::versionPatch;

  //==========================
  //=== CONSTEXPR CALLERSs ===
  //==========================
  using {0}::ApiVersion;
  using {0}::ApiVersion10;
  using {0}::ApiVersion11;
  using {0}::ApiVersion12;
  using {0}::ApiVersion13;
  using {0}::HeaderVersionComplete;

)",
                                       m_ns);
        // === STRUCTs ===
        generateModuleStructs(module_output);

        // === HANDLEs ===
        generateModuleHandles(module_output);

        if (cfg.gen.raii.enabled) {
            module_output << "  namespace VULKAN_HPP_RAII_NAMESPACE {\n";

            module_output << R"(
  //======================
  //=== RAII HARDCODED ===
  //======================
)";
            module_output << vkgen::format(R"(
    using {0}::{1};
  // using {0}::{1}Dispatcher;
  using {0}::DeviceDispatcher;
  using {0}::InstanceDispatcher;
  using {0}::exchange;
  )",
                                           m_ns_raii,
                                           loader.name);

            module_output << R"(
  //====================
  //=== RAII HANDLEs ===
  //====================
)";

            for (const Handle &e : this->handles.ordered) {
                genOptional(module_output, e, [&](auto &output) { output << "  using " << m_ns_raii << "::" << e.name << ";\n"; });
            }

            module_output << "  } // VULKAN_HPP_RAII_NAMESPACE\n";
        }

        module_output << endNamespace();

        outModule.writeFiles(*this);
    }

    template <typename T>
    static void generateForwardDeclarations(vkgen::OutputBuffer &output, const std::vector<T> &items, vkgen::Generator &gen) {
        output << gen.beginNamespace();
        for (const vkgen::GenericType &i : items) {
            output += gen.genOptional(i, [&](auto &output) { output += "  " + i.metaTypeDeclaration() + " " + i.name + ";\n"; });
        }
        output << gen.endNamespace();
    }

    void Generator::wrapNamespace(OutputBuffer &output, std::function<void(OutputBuffer &)> func) {
        output << beginNamespace();
        func(output);
        output << endNamespace();
    }

    void Generator::generateForwardHandles(OutputBuffer &output) {
        output << beginNamespace();
        for (const auto &e : handles.ordered) {
            generateClassDecl(output, e);
        }
        output << endNamespace();
    }

    class CCodeGenerator
    {
        Generator                      &gen;
        OutputBuffer                   &output;
        std::unordered_set<std::string> generated;
        bool                            dbg = true;

        template <typename T>
        void generate(const T &elem, bool isExtension = false) {
            const auto &type = elem.name.original;
            if (generated.find(type) != generated.end()) {
                std::cout << "Skip generated: " << type << "\n";
                return;
            }
            generated.insert(type);
            generateElement(elem);
        }

      public:
        CCodeGenerator(Generator &gen, OutputBuffer &output) : gen(gen), output(output) {}

        std::string extDebug(const GenericType &type) {
            std::string out = "// ";
            out += type.metaTypeString();
            out += "[";
            out += std::to_string(type.id);
            out += "] ";

            auto *ext = type.getExtension();
            if (ext) {
                out += " ext: ";
                out += ext->name.original;
            }
            out += " ";
            out += type.name.original;
            out += "\n";
            return out;
        }

        void generateDeclaration(const GenericType &type) {
            output << type.metaTypeDeclaration() << " " << type.name.original << ";\n";
        }

        void generateDeclaration(const Feature &feature) {
            for (const Enum &t : feature.enums) {
                generateDeclaration(t);
            }
            for (const Struct &t : feature.structs) {
                generateDeclaration(t);
            }
        }

        void generate(const Platform &platfrom) {
            const auto &protect = platfrom.protect;
            output << "// PLATFORM " << platfrom.name << "\n";
            output << "#ifdef " << protect << "\n";
            for (const auto &i : platfrom.includes) {
                output << i << "\n";
            }
            for (const Extension &extension : platfrom.extensions) {
                generate(extension);
            }
            output << "#endif // " << protect << "\n";
            output << "\n";
        }

        void generate(const Extension &extension) {
            const auto &name = extension.name.original;
            if (!extension.comment.empty()) {
                output << "// " << extension.comment << "\n";
            }
            output << "// " << name << " is a preprocessor guard. Do not pass it to API calls.\n";
            output << "#define " << name << " 1\n";

            for (const auto &i : extension.includes) {
                output << i << "\n";
            }
            for (const auto &c : extension.constants) {
                output << c;
            }
            output << "\n";
            const Feature &contents = extension;
            generate(contents, true);
            output << "\n";
        }

        void generate(const Feature &feature, bool isExtension) {
            output << "\n";
            output << "// " << feature.name.original << " is a preprocessor guard. Do not pass it to API calls.\n";
            output << "#define " << feature.name.original << " 1\n";
            for (const Snippet &t : feature.defines) {
                continue;
                generate(t, isExtension);
            }
            for (const Snippet &t : feature.baseTypes) {
                generate(t, isExtension);
            }
            for (const Handle &t : feature.handles) {
                generate(t, isExtension);
            }
            for (const Enum &t : feature.enums) {
                generate(t, isExtension);
            }
            for (const Struct &s : feature.forwardStructs) {
                const auto &name = s.name.original;
                output << "typedef struct " << name << " " << name << ";\n\n";
            }
            for (const FuncPointer &t : feature.funcPointers) {
                if (t.inStruct) {
                    generate(t, isExtension);
                }
            }
            for (const Struct &t : feature.structs) {
                generate(t, isExtension);
            }
            for (const FuncPointer &t : feature.funcPointers) {
                if (!t.inStruct) {
                    generate(t, isExtension);
                }
            }
            for (const Command &t : feature.commands) {
                generate(t, isExtension);
            }
            output << "\n";
        }

        void generateElement(const Command &elem) {
            const auto &name = elem.name.original;
            const auto &type = elem.aliasParent? reinterpret_cast<const Command*>(elem.aliasParent)->type : elem.type;
            const auto &params = elem.aliasParent? reinterpret_cast<const Command*>(elem.aliasParent)->params : elem.params;
            output << extDebug(elem);
            output << "typedef " << type << " (VKAPI_PTR *PFN_" << name << ")(";
            auto       it  = params.begin();
            const auto end = params.end();
            while (it != end) {
                const VariableData &p = *it;

                output << p.originalFullType();
                output << " ";
                output << p.identifier();
                output << p.optionalArraySuffix();

                it++;
                bool last = it == end;
                if (!last) {
                    output << ", ";
                }
            }
            output << ");";
            if (elem.aliasParent) {
                output << " // alias";
            }
            output << "\n\n";
        }

        void generateElement(const Handle &type) {
            output << extDebug(type);
            if (type.aliasParent) {
                const auto &name = type.name.original;
                output << "typedef " << type.aliasParent->name.original << " " << name << ";";
                output << " // alias";
                output << "\n";
                return;
            }
            output << type.code;
            output << "\n";
        }

        void generateElement(const Struct &elem) {
            output << extDebug(elem);
            const auto &name = elem.name.original;
            if (elem.aliasParent) {
                output << "typedef " << elem.aliasParent->name.original << " " << name << ";";
                output << " // alias";
                output << "\n";
                return;
            }

            const auto &type = elem.isUnion() ? "union" : "struct";
            output << "typedef " << type << " " << name << " {\n";
            for (const auto &m : elem.members) {
                output << "    " << m->originalFullType() << "    " << m->identifier() << m->optionalArraySuffix() << m->getNameSuffix() << ";\n";
            }
            output << "} " << name << ";\n";
            output << "\n";
        }

        template <typename T>
        void generate(const std::vector<T> &elements, const std::function<void(OutputBuffer &, const T &, bool)> &func) {
            auto       it  = elements.begin();
            const auto end = elements.end();
            while (it != end) {
                const auto &m = *it;
                it++;
                const bool last = it == end;
                // TODO
                gen.genOptional(output, m, [&](auto &output) { func(output, m, last); });
            }
        }

        void generateElement(const Enum &type) {
            const auto &name = type.name.original;
            output << extDebug(type);

            if (type.aliasParent) {
                output << "typedef " << type.aliasParent->name.original << " " << name << ";\n";
                if (type.isBitmask()) {
                    output << "typedef " << reinterpret_cast<Enum*>(type.aliasParent)->bitmask.original << " " << type.bitmask.original << ";\n";
                }
                return;
            }

            if (type.type == "VkFlags") {
                output << "typedef enum " << name << " {\n";

                generate<EnumValue>(type.members, [&](auto &output, const EnumValue &m, bool last) {
                    output << "    " << m.name.original << " = " << m.value;
                    if (!last) {
                        output << ",";
                    }
                    output << "\n";
                });
                output << "} " << name << ";\n";

            } else {
                output << "typedef " << type.type << " " << name << ";\n";

                generate<EnumValue>(type.members, [&](auto &output, const EnumValue &m, bool last) {
                    output << "static const " << name << " " << m.name.original << " = " << m.value << ";\n";
                });
            }

            if (type.isBitmask()) {
                output << "typedef " << type.type << " " << type.bitmask.original << ";\n";
            }

            output << "\n";
        }

        void generateElement(const Snippet &type) {
            output << type.code;
            output << "\n\n";
        }
    };

    static void generateTypeDebug(GenericType &type, OutputBuffer &output) {
        output << std::move(type.getVersionDebug()) << "\n";
    }

    static void generateFeatureDebgug(const Feature &f, OutputBuffer &output) {
        for (const auto &t : f.baseTypes) {
            generateTypeDebug(t, output);
        }
        for (const auto &t : f.handles) {
            generateTypeDebug(t, output);
        }
        for (const auto &t : f.enums) {
            generateTypeDebug(t, output);
        }
        for (const auto &t : f.structs) {
            generateTypeDebug(t, output);
        }
        for (const auto &t : f.funcPointers) {
            generateTypeDebug(t, output);
        }
        for (const auto &t : f.commands) {
            generateTypeDebug(t, output);
        }
    }

    static void printDebugExtensionDepends(const Extension *ext, int indent, OutputBuffer &output) {
        if (!ext->versiondepends.empty()) {
            for (int i = 0; i < indent; ++i) {
                output << " ";
            }
            output << "// -> " << ext->versiondepends << "\n";
        }

        for (const auto *dep : ext->depends) {
            for (int i = 0; i < indent; ++i) {
                output << " ";
            }
            output << "// -> " << dep->name << "\n";
            printDebugExtensionDepends(dep, indent + 2, output);
        }
    }

    void Generator::generateCore(OutputBuffer &output) {

        CCodeGenerator cgen{ *this, output };
        output << vkgen::format(RES_HEADER_C, headerVersion);

        for (const auto &c : apiConstants) {
            output << "#define " << c.name.original << "  " << c.value << "\n";
        }

        for (const Feature &feature : features.vulkan) {
            output << "// feature: " << feature.name.original << "\n";
            cgen.generate(feature, false);
        }
        for (const Extension &extension : extensions.vulkan) {
            if (!extension.platform) {
                output << "// extension: " << extension.name.original << "\n";
                cgen.generate(extension);
            }
        }
        for (const Platform &platform : platforms.vulkan) {
            cgen.generate(platform);
        }

    }

    void Generator::generateApiVideo(std::filesystem::path path) {
        if (!video) {
            return;
        }
        GenOutput files{ "", ".h", path };
        files.cguard = true;
        for (const auto &e : video->extensions) {
            if (e.comment.starts_with("protect with ")) {
                const auto guard = e.comment.substr(14);
            }
            auto          &file = files.addFile(e.name);
            CCodeGenerator cgen{ *this, file };
            cgen.generate(e);
        }
        files.writeFiles(*this);
    }

    void Generator::generateApiC(std::filesystem::path path) {
        GenOutput vkfiles{ "vk", ".h", path };
        vkfiles.cguard = true;
        auto &platform = vkfiles.addFile("_platform");
        platform << CODE_PLATFORM_H;

        vkfiles.writeFiles(*this);

        GenOutput files{ "vulkan", ".h", path };
        files.cguard = true;
        auto &main   = files.addFile("");

        generateCore(main);

        files.writeFiles(*this);
    }

    void Generator::generateApiCpp(std::filesystem::path path) {

        GenOutput out{ "vulkan", ".hpp", path };

        auto &enums           = out.addFile("_enums");
        auto &enums_forward   = out.addFile("_enums_forward");
        auto &to_string       = out.addFile("_to_string");
        auto &handles         = out.addFile("_handles");
        auto &smart_handles   = out.addFile("_smart");
        auto &forward         = out.addFile("_forward");
        auto &handles_forward = out.addFile("_handles_forward");
        auto &structs_forward = out.addFile("_structs_forward");
        auto &structs         = out.addFile("_structs");
        auto &types           = out.addFile("_types");
        auto &macros = out.addFile("_hpp_macros");

        generateMacros(macros);

        generateEnums(enums, enums_forward);

        generateForwardHandles(handles_forward);

        structs_forward << beginNamespace();
        for (const Struct &e : this->structs.vulkan) {
            generateStructDecl(structs_forward, e);
        }
        structs_forward << endNamespace();

        forward << "#include \"" << out.getFilename("_structs_forward") << "\"\n";
        forward << "#include \"" << out.getFilename("_handles_forward") <<"\"\n";

        types << "#include \"vulkan.h\"\n";
        types << "#include \"vulkan_hpp_macros.hpp\"\n";
        types << beginNamespace();
        types << RES_BASE_TYPES;
        types << "\n";
        types << endNamespace();
        if (!cfg.gen.onlyC) {
            types << "#include \"vulkan_enums.hpp\"\n";
        }
        types << "#include \"vulkan_handles.hpp\"\n";
        if (!cfg.gen.onlyC) {
            types << "#include \"vulkan_structs.hpp\"\n";
        }

        generateHandles(handles, smart_handles, out);

        generateStructs(structs);

        generateMainFile(out);

        if (cfg.gen.globalMode) {
            auto &global = out.addFile("_global");
            global << "#include \"vulkan.hpp\"\n";
            global << beginNamespace();
            global << "  " << loader.name << " " << strFirstLower(loader.name) << ";\n";
            for (const Handle &t : topLevelHandles) {
                global << "  " << t.name << " " << strFirstLower(t.name) << ";\n";
            }

            global << endNamespace();

            {
                auto &output = out.addFile("_enum_string", ".h");

                output << "#include \"vulkan.hpp\"\n";
                output << "#include <string>\n";
                output << "\n";
                for (const Enum &e : this->enums.ordered) {
                    genOptional(output, e, [&](auto &output) {
                        if (e.aliasParent) {
                            output << "  " << m_inline << " std::string string_" << e.aliasParent->name.original << "(" << e.name.original << " value) {\n";
                            output << "    return string_" << e.name.original << "(value);\n";
                            output << "  }\n";
                            return;
                        }

                        output << "  " << m_inline << " std::string string_" << e.name.original << "(" << e.name.original << " value)";
                        output << "  {\n";
                        if (e.isBitmask()) {
                            if (e.members.empty()) {
                                output << "     return \"{}\";\n";
                            } else {
                                output << R"(
    if ( !(bool)value )
      return "{}";
    std::string result;
)";
                                for (const auto &m : e.members) {
                                    if (m.isAlias) {
                                        continue;
                                    }
                                    genOptional(output, m, [&](auto &output) {
                                        output << "    if (value & " << m.name.original << ")\n";
                                        output << "       result += \"" << m.name.original << " | \";\n";
                                    });
                                }
                                output << "    return \"{ \" + result.substr( 0, result.size() - 3 ) + \" }\";\n";
                            }
                        } else {
                            output << "    switch (value) {\n";
                            if (e.members.empty()) {
                                output << "      // no values\n";
                            }
                            for (const auto &m : e.members) {
                                if (m.isAlias) {
                                    continue;
                                }
                                genOptional(
                                  output, m, [&](auto &output) { output << "      case " << m.name.original << ": return \"" << m.name.original << "\";\n"; });
                            }
                            output << "      default: return \"invalid ( \" + vk::toHexString(value)  + \" )\";\n";
                            output << "    }\n";
                        }
                        output << "  }\n";
                    });
                }
            }

            {
                auto &output = out.addFile("_struct_string", ".h");
                output << "#include \"vulkan_enum_string.h\"\n";
                output << beginNamespace();

                generate<Struct>(output, this->structs.ordered, [&](auto &output, auto &item) {
                    output << "  " << m_inline << " std::string to_string(const " << item.name.original << " &value)";
                    output << "  {\n";
                    output << "    std::string result;\n";
                    output << "    result += \"" << item.name.original << " {\\n\";\n";
                    for (const auto &m : item.members) {
                        output << "    result.append(\"\\n  " << m->identifier() << ": \").append(";
                        if (m->isPointer() || m->isArray() || m->hasArrayLength() || m->original.type().starts_with("PFN") || m->isHandle()) {
                            // output += "std::hex << value." + m->identifier() + " << std::dec << '\\n';\n";
                            // output += "\"Pointer\"";
                            output << "std::to_string(reinterpret_cast<uintptr_t>(value." << m->identifier() << "))";
                        } else if (m->isUnion()) {
                            output << "\"Union: " << m->original.type() << "\"";
                        } else if (m->isStruct()) {
                            output << "to_string(value." << m->identifier() << ")";
                        } else if (m->isEnum()) {
                            std::string type = std::regex_replace(m->original.type(), std::regex("FlagBits"), "Flags");
                            output << "string_" << type << "(value." << m->identifier() << ")";
                        } else {
                            output << "std::to_string(value." << m->identifier() << ")";
                        }
                        output << ");\n";
                    }
                    output << "    result += \"\\n}\\n\";\n";
                    output << "    return result;\n";
                    output << "  }\n";
                });

                output << endNamespace();
            }

            /*
            if (cfg.gen.staticObject) {
                auto &output = out.addFile("_global_interface", ".hpp");
                output << beginNamespace();
                Handle  empty{ *this };
                empty.name = String("GlobalInterface");
                output << "  class GlobalInterface {\n";
                output << "  public:\n";

                output << "    GlobalInterface() = default;\n";
                output << "    GlobalInterface(const vk::Device &) " << m_noexcept << " {}\n\n";
                // std::cout << outputFuncs.decl.toString();
                // GuardedOutputFuncs funcs;
                GuardedOutput members;
                for (auto &h : topLevelHandles) {
                    for (auto &m : h.get().members) {
                        ClassCommand    d{ this, &empty, *m.src };
                        MemberGenerator g{ *this, d, members, outputFuncs };
                        g.ctx.removeSuperclassVar = true;
                        g.ctx.globalPass = true;

                        g.generate();
                    }
                }
//                auto s = funcs.toString();
//                std::cout << funcs2.decl.toString();
//                std::cout << funcs2.def.toString();
                // outputFuncs << std::move(funcs.decl);
                // output << std::move(funcs.decl);
                output << std::move(members);
                output << "  };\n";

                // output << std::move(funcs.def);
                output << endNamespace();
            }
            */

            /*
            for (const Enum &e: this->enums.ordered) {
                to_stream += "// " + e.name.original + "\n";
                for (const auto &m : e.members) {
                    to_stream += "  // " + m.name.original + "    " + m.name + "\n";

                }
                to_stream += "\n";
            }
            */
            // to_stream += endNamespace();
        }

        if (cfg.gen.raii.enabled) {
            auto &raii         = out.addFile("_raii");
            auto &raii_forward = out.addFile("_raii_forward");
            auto &raii_funcs   = out.addFile("_raii_funcs");

            generateRAII(raii, raii_forward, out);
            generateFuncsRAII(raii_funcs);
        }

        {
            auto &context = out.addFile("_context");
            generateContext(context);
        }

        auto &funcs      = out.addFile("_funcs");
        auto &funcs_impl = out.addFile("_funcs_impl");

        if (cfg.gen.globalMode) {
            auto &impl = out.addFile("_impl", ".cpp");
            impl << "#include \"vulkan.hpp\"\n";
            impl << "#undef VULKAN_HPP_INLINE\n";
            impl << "#define VULKAN_HPP_INLINE\n";
            impl << "#include \"vulkan_funcs_impl.hpp\"\n\n";
            impl << "#include \"vulkan_to_string_impl.hpp\"\n\n";
            impl << "#include \"vulkan_global.hpp\"\n\n";
        }

        const auto appendFuncs = [&](OutputBuffer &output, GuardedOutputFuncs &funcs) {
            funcs_impl << std::move(funcs.def);

            output << std::move(funcs.decl);
            output << std::move(funcs.templ);
            output << std::move(funcs.platform);
        };

        const auto genFuncs = [&](OutputBuffer &output, bool useC) {
            GuardedOutputFuncs funcs;
            generateGlobalFuncs(funcs, useC);
            appendFuncs(output, funcs);
        };

        if (cfg.gen.globalMode) {
            funcs << R"(
#if ( __cpp_lib_span >= 201803L )
#  include <span>
#endif
)";
            funcs << "#include \"vulkan_context.hpp\"\n";
            funcs << "#include \"vulkan_array.hpp\"\n";
            funcs << "#include \"vulkan_types.hpp\"\n";
        }

        if (!cfg.gen.onlyC) {
            funcs << "#include \"vulkan_enums.hpp\"\n";
            funcs << "#include \"vulkan_structs.hpp\"\n";
        }

        funcs << beginNamespace();
        funcs_impl << beginNamespace();

        funcs << R"(
  void resultCheck( Result result, char const * message );

  void resultCheck( Result result, char const * message, std::initializer_list<Result> successCodes );

  void resultCheck( VkResult result, char const * message );

  void resultCheck( VkResult result, char const * message, std::initializer_list<VkResult> successCodes );
)";

        funcs_impl << vkgen::format(RES_RESULT_CHECK_CPP, cfg.gen.branchHint ? "VULKAN_HPP_UNLIKELY" : "");

        if (cfg.gen.globalMode) {
            funcs << vkgen::format(R"(
  void loadLib();

  void initInstance(const {0}::InstanceCreateInfo& createInfo);

  {0}::Result initInstance_noThrow(const {0}::InstanceCreateInfo& createInfo);

  void initInstance(const {0}::InstanceCreateInfo& createInfo, bool& vulkan10enforced);

  void initDevice({0}::PhysicalDevice physicalDevice, const {0}::DeviceCreateInfo& createInfo);

  {0}::Result initDevice_noThrow({0}::PhysicalDevice physicalDevice, const {0}::DeviceCreateInfo& createInfo);

  bool isExtensionSupported(std::span<{0}::ExtensionProperties> extensionList, const char* extensionName) noexcept;
)",
                                   m_ns);

            funcs_impl << vkgen::format(R"(
  VULKAN_HPP_INLINE void loadLib() {{
    if (!{0}::context.load()) {{
      throw std::runtime_error(VULKAN_HPP_NAMESPACE_STRING "::loadLib");
    }}
  }}

  VULKAN_HPP_INLINE void initInstance(const {0}::InstanceCreateInfo& createInfo) {{
    Result result = {0}::instance.init({0}::context, createInfo);
    {0}::resultCheck(static_cast<VkResult>(result), VULKAN_HPP_NAMESPACE_STRING "::initInstance");
  }}

  VULKAN_HPP_INLINE {0}::Result initInstance_noThrow(const {0}::InstanceCreateInfo& createInfo) {{
    return {0}::instance.init({0}::context, createInfo);
  }}

  VULKAN_HPP_INLINE void initInstance(const {0}::InstanceCreateInfo& createInfo, bool& vulkan10enforced) {{
    Result result = {0}::initInstance_noThrow(createInfo);

    if(result == Result::eErrorIncompatibleDriver && createInfo.pApplicationInfo &&
       createInfo.pApplicationInfo->apiVersion != vk::ApiVersion10)
    {{
      // replace requested Vulkan version by 1.0 to avoid eErrorIncompatibleDriver error
      ApplicationInfo appInfo2(*createInfo.pApplicationInfo);
      appInfo2.apiVersion = ApiVersion10;
      InstanceCreateInfo createInfo2(createInfo);
      createInfo2.pApplicationInfo = &appInfo2;

      // create instance (second attempt)
      result = {0}::initInstance_noThrow(createInfo2);
      vulkan10enforced = true;
    }}
    else {{
      vulkan10enforced = false;
    }}

    {0}::resultCheck(static_cast<VkResult>(result), VULKAN_HPP_NAMESPACE_STRING "::initInstance");
  }}

  VULKAN_HPP_INLINE void initDevice({0}::PhysicalDevice physicalDevice, const {0}::DeviceCreateInfo& createInfo) {{
    Result result = {0}::device.init({0}::instance, physicalDevice, createInfo);
    {0}::resultCheck(static_cast<VkResult>(result), VULKAN_HPP_NAMESPACE_STRING "::initDevice");
  }}

  VULKAN_HPP_INLINE {0}::Result initDevice_noThrow({0}::PhysicalDevice physicalDevice, const {0}::DeviceCreateInfo& createInfo) {{
    return {0}::device.init({0}::instance, physicalDevice, createInfo);
  }}

  VULKAN_HPP_INLINE bool isExtensionSupported(const std::span<{0}::ExtensionProperties> extensionList, const char* extensionName) noexcept
  {{
    for(const ExtensionProperties &extension : extensionList)
      if(strcmp(extension.extensionName, extensionName) == 0)
        return true;
    return false;
  }}
)",
                                        m_ns);
        }

        appendFuncs(funcs, outputFuncs);
        if (cfg.gen.globalFunctions) {
            genFuncs(funcs, cfg.gen.onlyC);
        }

        funcs << endNamespace();
        funcs << R"(
#ifndef VULKAN_USE_CPP
#include "vulkan_funcs_impl.hpp"
#endif // VULKAN_USE_CPP
)";

        funcs_impl << endNamespace();

        to_string << beginNamespace();
        if (cfg.gen.globalMode) {
            auto &impl = out.addFile("_to_string_impl");
            impl << "#include <string>\n";
            impl << beginNamespace();
            impl << std::move(outputToStringDef);
            impl << endNamespace();

            to_string << std::move(outputToStringDecl);
        } else {
            to_string << std::move(outputToStringDef);
        }
        to_string << endNamespace();

        if (cfg.gen.cppModules) {
            generateModules(out, path);
        }

        out.writeFiles(*this);
    }

    void Generator::generateEnumStr(const Enum &data, OutputBuffer &output) {

        GuardedOutput                   members;
        GuardedOutput                   to_string;
        std::unordered_set<std::string> generated;
        std::unordered_set<std::string> generatedCase;

        if (data.aliasParent) {
            output << "  using " << data.name << " = " << data.aliasParent->name << ";\n";
            auto parent = reinterpret_cast<Enum*>(data.aliasParent);
            if (parent->isBitmask()) {
                output << "  using " << data.bitmask << " = " << parent->bitmask << ";\n";
            }
            // TODO to_string
            return;
        }


        for (const auto &m : data.members) {
            if (generated.find(m.name) != generated.end()) {
                continue;
            }
            generated.insert(m.name);

            members.add(m, [&](auto &output) {
                output << "    " << m.name << " = " << m.value;
                if (cfg.dbg.methodTags) {
                    output << ", // " << m.name.original << "\n";
                } else {
                    output << ",\n";
                }
            });

            if (!m.isAlias) {
                if (generatedCase.find(m.value) != generatedCase.end()) {
                    continue;
                }
                generatedCase.insert(m.value);
                to_string.add(m, [&](auto &output) {
                    std::string value = m.name;
                    strStripPrefix(value, "e");
                    output << "      case " << data.name;
                    output << "::" << m.name;
                    output << ": return \"" << value << "\";\n";
                });
            }
        }

        output << "//  [" << std::to_string(data.id) << "]\n";

        output << "  enum class " << data.name;
        if (data.isBitmask()) {
            output << " : " << data.bitmask.original;
        }
        output << " {\n";
        output << members.toString();
        if (cfg.gen.altFlagTraits&& data.isBitmask()) {
            output << "    allFlags = ";
            output << std::move(generateAllFlagsValue(data));
            output << ",\n";
        }
        output << "\n  };\n";

        std::string str;
        if (data.isBitmask()) {
            genFlagTraits(data, data.name, output, str);
        }
        /*
        FunctionGenerator fun(*this, "std::string", "to_string");
        fun.indent = "  ";
        fun.base = &data;
        fun.optionalProtect = Protect{"VULKAN_HPP_NO_TO_STRING", false};
        fun.allowInline = true;
        fun.specifierInline = true;
        fun.add(data.name, "value");
        */

        if (data.isBitmask()) {
            // fun.code = str;
        } else {
            str = to_string.toString();
            if (str.empty()) {
                str = "    return \"\\\"(void)\\\"\";\n";
            } else {
                str += "      default: return \"invalid ( \" + " + m_ns + "::toHexString(static_cast<uint32_t>(value))  + \" )\";";

                str = vkgen::format(R"(
    switch (value) {{
{0}
    }}
)",
                                    str);
            }
        }

        std::string proto = "std::string to_string(";
        proto += data.isBitmask()? data.bitmask : data.name;
        proto += " value)";

        // to_string_output += fun.generate(outputFuncs);
        if (cfg.gen.globalMode) {
            outputToStringDecl.add(data, [&](auto &output) {
                output << "  ";
                output << proto;
                output << ";\n\n";
            });
        }

        outputToStringDef.add(data, [&](auto &output) {
            output << "  ";
            output << cfg.macro.mInline.define;
            output << " ";
            output << proto;
            output << " {\n";
            output << str;
            output << "  }\n";
        });
    }

    void Generator::generateEnum(const Enum &data, OutputBuffer &output, OutputBuffer &output_forward) {

        genOptional(output, data, [&](auto &output) { generateEnumStr(data, output); });

        genOptional(output_forward, data, [&](auto &output) {
            output << "  enum class " << data.name;
            if (data.isBitmask()) {
                output << " : " << data.name.original;
            }
            output << ";\n";
        });
    }

    void Generator::generateStructToString()
    {

    }

    std::string Generator::generateToStringInclude() const {
        std::string output;
        output += "#include \"vulkan_enums.hpp\"\n";
        return output;
    }

    void Generator::generateEnums(OutputBuffer &output, OutputBuffer &output_forward) {
        if (verbose) {
            std::cout << "gen enums " << '\n';
        }

        output << "#include <type_traits>\n";
        output << "#include \"vulkan_hpp_macros.hpp\"\n";
        output << "\n";

        output << beginNamespace();
        output_forward << beginNamespace();

        generateFlags(output);

        if (!cfg.gen.globalMode) {
            gen(output, cfg.gen.handleTemplates, [&](auto &output) {
                output << R"(
  template <typename EnumType, EnumType value>
  struct CppType
  {};
)";
            });
        }

        std::unordered_set<std::string> generated;
        for (const Enum &e : enums.vulkan) {
            if (generated.contains(e.name)) {
                continue;
            }
            generateEnum(e, output, output_forward);
            generated.insert(e.name);
        }

        output << endNamespace();
        output_forward << endNamespace();

        if (verbose) {
            std::cout << "gen enums done" << '\n';
        }
    }

    std::string Generator::generateAllFlagsValue(const Enum &data) {
        std::string output;

        std::map<std::string, uint64_t> values;

        for (const auto &m : data.members) {
            if (m.isAlias) {
                continue;
            }

            const auto &p = m.getProtect();
            values[std::string(p)] |= m.numericValue;
        }

        if (values.empty()) {
            output += cfg.gen.altFlagTraits ? "0" : "{}";
        } else {
            std::string name = std::regex_replace(data.name, std::regex("FlagBits"), "Flags");
            if (!cfg.gen.altFlagTraits) {
                output += "static_cast<";
                output += name;
                output += ">(\n          ";
            }
            bool first      = true;
            bool hasProtect = false;
            for (const auto &v : values) {
                const auto &p = v.first;
                hasProtect    = !p.empty();
                if (hasProtect) {
                    if (cfg.gen.altFlagTraits && first) {
                        output += "\n";
                    }
                    output += "#if defined(" + p + ")";
                    if (first) {
                        output += "\n";
                    }
                }
                if (first) {
                    first = false;
                } else {
                    output += "\n        | ";
                }
                output += EnumValue::toHex(v.second, data.is64bit());
                output += "\n";
                if (hasProtect) {
                    output += "#endif // " + p + "\n";
                }
            }
            if (!cfg.gen.altFlagTraits) {
                output += "    )";
            } else if (!hasProtect) {
                strStripSuffix(output, "\n");
            }
        }

        return output;
    }

    void Generator::genFlagTraits(const Enum &data, std::string inherit, OutputBuffer &output, std::string &to_string_code) {
        std::string name = std::regex_replace(data.name, std::regex("FlagBits"), "Flags");

        OutputBuffer str;

        std::map<std::string, std::string> temp;
        std::map<std::string, uint64_t>    values;

        for (size_t i = 0; i < data.members.size(); ++i) {
            const auto &m = data.members[i];
            if (m.isAlias) {
                continue;
            }

            const auto  &p   = m.getProtect();
            std::string &dst = temp[std::string(p)];
            if (!dst.empty()) {
                dst += "\n        | ";
            }
            dst += inherit + "::" + m.name + " // " + std::string(m.value) + ", " + std::to_string(m.numericValue);

            values[std::string(p)] |= m.numericValue;

            genOptional(str, m, [&](auto &output) {
                std::string value = m.name;
                strStripPrefix(value, "e");
                output << vkgen::format(R"(
    if (value & {0}::{1})
      result += "{2} | ";
)",
                                        inherit,
                                        m.name,
                                        value);
            });
        }


            output << vkgen::format(R"(  using {0} = Flags<{1}>;

)",
                                    data.bitmask,
                                    data.name);

            if (!cfg.gen.altFlagTraits) {
                output << expIfndef("VULKAN_HPP_EXPERIMENTAL_NO_FLAG_TRAITS");
                output << vkgen::format(R"(
  template <>
  struct FlagTraits<{0}> {{
)",
                                        inherit);
                if (data.isBitmask()) {
                    output << "    static VULKAN_HPP_CONST_OR_CONSTEXPR bool             isBitmask = true;\n";
                }
                output << "    static VULKAN_HPP_CONST_OR_CONSTEXPR " << name << " allFlags = ";
                output << std::move(generateAllFlagsValue(data));
                output << ";\n  };\n";
                output << expEndif("VULKAN_HPP_EXPERIMENTAL_NO_FLAG_TRAITS");
            }

        if (str.size() == 0) {
            to_string_code = "    return \"{}\";\n";
        } else {
            std::stringstream temp;
            temp << str;  // TODO

            to_string_code = vkgen::format(R"(
    if ( !(bool)value )
      return "{{}}";
    std::string result;
{0}
    return "{{ " + result.substr( 0, result.size() - 3 ) + " }}";
)",
                                           temp.str());
        }
    }

    void Generator::generateDispatch(OutputBuffer &output) {
        output << generateDispatchLoaderBase();
        output << "#if !defined( VK_NO_PROTOTYPES )\n";
        generateDispatchLoaderStatic(output);
        output << "#endif // VK_NO_PROTOTYPES\n";
        output << R"(
  class DispatchLoaderDynamic;
#if !defined( VULKAN_HPP_DISPATCH_LOADER_DYNAMIC )
#  if defined( VK_NO_PROTOTYPES )
#    define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#  else
#    define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 0
#  endif
#endif

#if !defined( VULKAN_HPP_STORAGE_API )
#  if defined( VULKAN_HPP_STORAGE_SHARED )
#    if defined( _MSC_VER )
#      if defined( VULKAN_HPP_STORAGE_SHARED_EXPORT )
#        define VULKAN_HPP_STORAGE_API __declspec( dllexport )
#      else
#        define VULKAN_HPP_STORAGE_API __declspec( dllimport )
#      endif
#    elif defined( __clang__ ) || defined( __GNUC__ )
#      if defined( VULKAN_HPP_STORAGE_SHARED_EXPORT )
#        define VULKAN_HPP_STORAGE_API __attribute__( ( visibility( "default" ) ) )
#      else
#        define VULKAN_HPP_STORAGE_API
#      endif
#    else
#      define VULKAN_HPP_STORAGE_API
#      pragma warning Unknown import / export semantics
#    endif
#  else
#    define VULKAN_HPP_STORAGE_API
#  endif
#endif
)";

        output << vkgen::format(R"(
#if !defined( VULKAN_HPP_DEFAULT_DISPATCHER )
#  if VULKAN_HPP_DISPATCH_LOADER_DYNAMIC == 1
#    define VULKAN_HPP_DEFAULT_DISPATCHER ::{0}::defaultDispatchLoaderDynamic
#    define VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE                     \
      namespace {0}                                                        \
      {{                                                                            \
        VULKAN_HPP_STORAGE_API DispatchLoaderDynamic defaultDispatchLoaderDynamic; \
      }}
  extern VULKAN_HPP_STORAGE_API DispatchLoaderDynamic defaultDispatchLoaderDynamic;
#  else
  static inline ::{0}::DispatchLoaderStatic & getDispatchLoaderStatic()
  {{
    static ::{0}::DispatchLoaderStatic dls;
    return dls;
  }}
#    define VULKAN_HPP_DEFAULT_DISPATCHER ::{0}::getDispatchLoaderStatic()
#    define VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE
#  endif
#endif

#if !defined( VULKAN_HPP_DEFAULT_DISPATCHER_TYPE )
#  if VULKAN_HPP_DISPATCH_LOADER_DYNAMIC == 1
#    define VULKAN_HPP_DEFAULT_DISPATCHER_TYPE ::{0}::DispatchLoaderDynamic
#  else
#    define VULKAN_HPP_DEFAULT_DISPATCHER_TYPE ::{0}::DispatchLoaderStatic
#  endif
#endif
)",
                                m_ns);
    }

    void Generator::generateApiConstants(OutputBuffer &output) {
        output << R"(
    //===========================
    //=== CONSTEXPR CONSTANTs ===
    //===========================
)";

        for (const auto &a : apiConstants) {
            genOptional(
              output, a, [&](auto &output) { output << "    VULKAN_HPP_CONSTEXPR_INLINE " << a.type << " " << a.name << " = " << a.name.original << ";\n"; });
        }

        output << R"(
    //========================
    //=== CONSTEXPR VALUEs ===
    //========================
    VULKAN_HPP_CONSTEXPR_INLINE uint32_t HeaderVersion = VK_HEADER_VERSION;
)";
    }

    void Generator::generateResultValue(OutputBuffer &output) {
        output << R"(
  template <typename T>
  void ignore( T const & ) VULKAN_HPP_NOEXCEPT
  {
  }

  template <typename T>
  struct ResultValue
  {
)";

        if (!cfg.gen.globalMode) {
            output << R"(
#ifdef VULKAN_HPP_HAS_NOEXCEPT
    ResultValue( Result r, T & v ) VULKAN_HPP_NOEXCEPT( VULKAN_HPP_NOEXCEPT( T( v ) ) )
#else
    ResultValue( Result r, T & v )
#endif
      : result( r ), value( v )
    {
    }

#ifdef VULKAN_HPP_HAS_NOEXCEPT
    ResultValue( Result r, T && v ) VULKAN_HPP_NOEXCEPT( VULKAN_HPP_NOEXCEPT( T( std::move( v ) ) ) )
#else
    ResultValue( Result r, T && v )
#endif
      : result( r ), value( std::move( v ) )
    {
    }

    Result result;
    T      value;

    operator std::tuple<Result &, T &>() VULKAN_HPP_NOEXCEPT
    {
      return std::tuple<Result &, T &>( result, value );
    }
  };
)";

        } else {
            output << R"(
#ifdef VULKAN_HPP_HAS_NOEXCEPT
    ResultValue( VkResult r, T & v ) VULKAN_HPP_NOEXCEPT( VULKAN_HPP_NOEXCEPT( T( v ) ) )
#else
    ResultValue( VkResult r, T & v )
#endif
      : result( r ), value( v )
    {
    }

#ifdef VULKAN_HPP_HAS_NOEXCEPT
    ResultValue( VkResult r, T && v ) VULKAN_HPP_NOEXCEPT( VULKAN_HPP_NOEXCEPT( T( std::move( v ) ) ) )
#else
    ResultValue( VkResult r, T && v )
#endif
      : result( r ), value( std::move( v ) )
    {
    }

    VkResult result;
    T      value;

    operator std::tuple<VkResult &, T &>() VULKAN_HPP_NOEXCEPT
    {
      return std::tuple<VkResult &, T &>( result, value );
    }
};
)";
        }

        output << R"(
/*
#if !defined( VULKAN_HPP_NO_SMART_HANDLE )
  template <typename Type, typename Dispatch>
  struct ResultValue<UniqueHandle<Type, Dispatch>>
  {
#  ifdef VULKAN_HPP_HAS_NOEXCEPT
    ResultValue( Result r, UniqueHandle<Type, Dispatch> && v ) VULKAN_HPP_NOEXCEPT
#  else
    ResultValue( Result r, UniqueHandle<Type, Dispatch> && v )
#  endif
      : result( r )
      , value( std::move( v ) )
    {
    }

    std::tuple<Result, UniqueHandle<Type, Dispatch>> asTuple()
    {
      return std::make_tuple( result, std::move( value ) );
    }

    Result                       result;
    UniqueHandle<Type, Dispatch> value;
  };

  template <typename Type, typename Dispatch>
  struct ResultValue<std::vector<UniqueHandle<Type, Dispatch>>>
  {
#  ifdef VULKAN_HPP_HAS_NOEXCEPT
    ResultValue( Result r, std::vector<UniqueHandle<Type, Dispatch>> && v ) VULKAN_HPP_NOEXCEPT
#  else
    ResultValue( Result r, std::vector<UniqueHandle<Type, Dispatch>> && v )
#  endif
      : result( r )
      , value( std::move( v ) )
    {
    }

    std::tuple<Result, std::vector<UniqueHandle<Type, Dispatch>>> asTuple()
    {
      return std::make_tuple( result, std::move( value ) );
    }

    Result                                    result;
    std::vector<UniqueHandle<Type, Dispatch>> value;
  };
#endif
*/

  template <typename T>
  struct ResultValueType
  {
#ifdef VULKAN_HPP_NO_EXCEPTIONS
    typedef ResultValue<T> type;
#else
    typedef T    type;
#endif
  };
)";
        if (!cfg.gen.globalMode) {
            output << R"(
  template <>
  struct ResultValueType<void>
  {
#ifdef VULKAN_HPP_NO_EXCEPTIONS
    typedef Result type;
#else
    typedef void type;
#endif
  };

  VULKAN_HPP_INLINE typename ResultValueType<void>::type createResultValueType( Result result )
  {
#ifdef VULKAN_HPP_NO_EXCEPTIONS
    return result;
#else
    ignore( result );
#endif
  }

  template <typename T>
  VULKAN_HPP_INLINE typename ResultValueType<T>::type createResultValueType( Result result, T & data )
  {
#ifdef VULKAN_HPP_NO_EXCEPTIONS
    return ResultValue<T>( result, data );
#else
    ignore( result );
    return data;
#endif
  }

  template <typename T>
  VULKAN_HPP_INLINE typename ResultValueType<T>::type createResultValueType( Result result, T && data )
  {
#ifdef VULKAN_HPP_NO_EXCEPTIONS
    return ResultValue<T>( result, std::move( data ) );
#else
    ignore( result );
    return std::move( data );
#endif
  }

VULKAN_HPP_INLINE typename ResultValueType<void>::type createResultValueType( VkResult result )
  {
#ifdef VULKAN_HPP_NO_EXCEPTIONS
    return static_cast<Result>(result);
#else
    ignore( result );
#endif
  }

  template <typename T>
  VULKAN_HPP_INLINE typename ResultValueType<T>::type createResultValueType( VkResult result, T & data )
  {
#ifdef VULKAN_HPP_NO_EXCEPTIONS
    return ResultValue<T>( static_cast<Result>(result), data );
#else
    ignore( result );
    return data;
#endif
  }

  template <typename T>
  VULKAN_HPP_INLINE typename ResultValueType<T>::type createResultValueType( VkResult result, T && data )
  {
#ifdef VULKAN_HPP_NO_EXCEPTIONS
    return ResultValue<T>( static_cast<Result>(result), std::move( data ) );
#else
    ignore( result );
    return std::move( data );
#endif
  }
)";
        } else {
            output << R"(
  template <>
  struct ResultValueType<void>
  {
#ifdef VULKAN_HPP_NO_EXCEPTIONS
    typedef VkResult type;
#else
    typedef void type;
#endif
  };

  VULKAN_HPP_INLINE typename ResultValueType<void>::type createResultValueType( VkResult result )
  {
#ifdef VULKAN_HPP_NO_EXCEPTIONS
    return result;
#else
    ignore( result );
#endif
  }

  template <typename T>
  VULKAN_HPP_INLINE typename ResultValueType<T>::type createResultValueType( VkResult result, T & data )
  {
#ifdef VULKAN_HPP_NO_EXCEPTIONS
    return ResultValue<T>( result, data );
#else
    ignore( result );
    return data;
#endif
  }

  template <typename T>
  VULKAN_HPP_INLINE typename ResultValueType<T>::type createResultValueType( VkResult result, T && data )
  {
#ifdef VULKAN_HPP_NO_EXCEPTIONS
    return ResultValue<T>( result, std::move( data ) );
#else
    ignore( result );
    return std::move( data );
#endif
  }
)";
        }
    }

    void Generator::generateErrorClasses(OutputBuffer &output) {
        OutputBuffer caseCode;

        output << vkgen::format(RES_ERROR_CAT, m_ns);
        output << "#ifdef VULKAN_HPP_UNIFIED_EXCEPTION\n";
        output << vkgen::format(RES_ERRORS_UNIFIED, m_ns);
        output << "#else\n";
        output << vkgen::format(RES_ERRORS, cfg.gen.globalMode ? "VkResult" : "Result", cfg.gen.globalMode ? "e" : "static_cast<int>( e )");

        for (const auto &e : errorClasses) {;

            std::string value = cfg.gen.globalMode ? e.value.name.original : "Result::" + e.value.name;

            genOptional(output, e.value, [&](auto &output) {
                output << vkgen::format(R"(
  class {0} : public SystemError
  {{
  public:
    {0}( std::string const & message ) : SystemError( make_error_code( {1} ), message ) {{}}
    {0}( char const * message ) : SystemError( make_error_code( {1} ), message ) {{}}
  }};
)",
                                        e.name,
                                        value);
            });

            genOptional(caseCode, e.value, [&](auto &output) { output << "        case " << value << ": throw " << e.name << "(message);\n"; });
        }

        output << "#endif // VULKAN_HPP_UNIFIED_EXCEPTION\n";

        output << "  namespace detail {\n";
        output << "    [[noreturn]] void " << m_inline << " throwResultException(VkResult result, char const *message) {\n";
        if (cfg.gen.unifiedException) {
            output << R"(
#ifdef VULKAN_HPP_UNIFIED_EXCEPTION
      throw Error( result, message );
#else
)";
        }
        output << "      switch (result) {\n";
        output << std::move(caseCode);
        output << "        default: throw SystemError( make_error_code( result ) );\n";
        output << "      }\n";
        if (cfg.gen.unifiedException) {
            output << "#endif // VULKAN_HPP_UNIFIED_EXCEPTION\n";
        }
        output << "    }\n";
        output << "    [[noreturn]] void " << m_inline << " throwResultException(" << m_ns << "::Result result, char const *message) {\n";
        output << "      throwResultException(static_cast<VkResult>(result), message);\n";
        output << "    }\n";
        output << "  } // namespace detail\n";
    }

    std::string Generator::generateDispatchLoaderBase() {
        std::string output;
        output += R"(
  class DispatchLoaderBase
  {
  public:
    DispatchLoaderBase() = default;
    DispatchLoaderBase( std::nullptr_t )
#if !defined( NDEBUG )
      : m_valid( false )
#endif
    {
    }

#if !defined( NDEBUG )
    size_t getVkHeaderVersion() const
    {
      VULKAN_HPP_ASSERT( m_valid );
      return vkHeaderVersion;
    }

  private:
    size_t vkHeaderVersion = VK_HEADER_VERSION;
    bool   m_valid         = true;
#endif
  };

)";
        return output;
    }

    void Generator::generateDispatchLoaderStatic(OutputBuffer &output) {
        output << "//#if !defined( VK_NO_PROTOTYPES )\n";
        output << "  class DispatchLoaderStatic : public DispatchLoaderBase {\n";
        output << "  public:\n";

        Handle empty{ *this };
        for (auto &command : commands) {
            genOptional(output, command, [&](auto &output) {
                ClassCommand                       d{ this, &empty, command };
                MemberContext                      ctx{ .ns = Namespace::VK, .disableDispatch = true, .disableAllocatorRemoval = true };
                MemberResolverStaticDispatch const r{ *this, d, ctx };
                // TODO
                output << r.temporary();
            });
        }

        output << "  };\n";
        output << "//#endif\n";
    }

    void Generator::generateUnit(const std::string_view file, GenOutput &out, OutputBuffer &parent, OutputBuffer &&code) {
        if (!cfg.gen.splitFiles) {
            parent << beginNamespace();
            parent << std::move(code);
            parent << endNamespace();
        } else {
            out.writeFile(*this, file, code);
            parent << "#include \"";
            parent << file;
            parent << "\"\n";
        }
    }

    void Generator::generateStructDecl(OutputBuffer &output, const Struct &d) const {
        genOptional(output, d, [&](auto &output) {
            if (d.aliasParent) {
                output << "// alias:\n";
                output << "  using " << d.name << " = " << d.aliasParent->name << ";\n";
                return;
            }
            output << (d.isStruct() ? "  struct " : "  union ");
            output << d.name << ";\n";
        });
    }

    void Generator::generateClassDecl(OutputBuffer &output, const Handle &data, const std::string &name) const {
        genOptional(output, data, [&](auto &output) { output << "  class " << name << ";\n"; });
    }

    void Generator::generateClassDecl(OutputBuffer &output, const Handle &data) const {
        generateClassDecl(output, data, data.name);
    }

    std::string Generator::generateForwardInclude(GenOutput &out) const {
        std::string output;
        if (!cfg.gen.globalMode) {
            output += "#include \"" + out.getFilename("_forward") + "\"\n";
            output += "#include \"vulkan_structs_forward.hpp\"\n";
        }
        output += "#include \"vulkan_handles_forward.hpp\"\n";

        if (cfg.gen.raii.interop) {
            output += "#include \"" + out.getFilename("_raii_forward") + "\"\n";
        }

        return output;
    }

    void Generator::generateGlobalFuncs(vkgen::GuardedOutputFuncs &output, bool useC) {
        if (cfg.gen.globalMode) {
            const Handle  empty{ *this };

            for (const Command &s : staticCommands) {
                output.def << "  // static cmd: " << s.name.original << "\n";
            }

            for (Command &c : commands.ordered) {

                bool m = false;
                for (const Command &s : staticCommands) {
                    if (c.name.original == s.name.original) {
                        m = true;
                        break;
                    }
                }
                if (m) {
                    // continue;
                }
                if (c.name == "createInstance") {
                    continue;
                }
                if (c.name == "createDevice") {
                    continue;
                }
                if (c.name == "enumerateInstanceVersion") {
                    continue;
                }

                ClassCommand d{ this, &empty, c };

                MemberGenerator g{ *this, d, output, true };
                g.ctx.globalModeStatic = true;
                g.ctx.globalUseCAPI    = useC;
                g.generate();
            }

            for (Handle &e : handles.ordered) {
                if (e.uniqueVariant() && e.isSubclass) {
                    for (auto &c : e.ctorCmds) {
                        if (!c.name.starts_with("create") || !c.name.starts_with("allocate")) {
                            continue;
                        }
                        output.def << "  // " << c.name.original << "\n";
                        MemberContext ctx{ .ns = Namespace::VK };
                            ctx.disableDispatch = true;
                            ctx.globalModeStatic = true;
                            ctx.exp             = true;
                        MemberResolverCreateUnique resolver{ *this, c, ctx };
                        resolver.generate(output.decl, output);

                    }
                    for (auto &c : e.vectorCmds) {
                        if (!c.name.starts_with("create") || !c.name.starts_with("allocate")) {
                            continue;
                        }
                        MemberContext ctx{ .ns = Namespace::VK };
                        ctx.disableDispatch = true;
                        ctx.globalModeStatic = true;
                        ctx.exp             = true;
                        ctx.returnSingle = true;
                        MemberResolverCreateUnique resolver{ *this, c, ctx };
                        resolver.generate(output.decl, output);
                    }
                }
            }
        }
    }

    void Generator::generateHandles(OutputBuffer &output, OutputBuffer &output_smart, GenOutput &out) {
        if (verbose) {
            std::cout << "gen handles " << '\n';
        }

        output << "#include <cstddef>\n";
        output << "#include \"vulkan_hpp_macros.hpp\"\n";
        output << "#include \"vulkan_structs_forward.hpp\"\n";
        if (cfg.gen.globalMode) {
            output << "#include \"vulkan_context.hpp\"\n";
        }

        output << generateForwardInclude(out);

        output << beginNamespace();
        if (cfg.gen.globalMode) {
            output << "  struct InstanceCreateInfo;\n";
            output << "  struct DeviceCreateInfo;\n";
        }

        output << "#ifndef VULKAN_HPP_NO_STRUCT_CHAIN\n";
        output << R"(
  template <typename... ChainElements>
  class StructureChain;
)";
        output << "#endif // VULKAN_HPP_NO_STRUCT_CHAIN\n";

        gen(output, cfg.gen.handleTemplates, [&](auto &output) {
            output << R"(
  template <typename Type>
  struct isVulkanHandleType
  {
    static VULKAN_HPP_CONST_OR_CONSTEXPR bool value = false;
  };
)";
        });

        if (cfg.gen.smartHandles->enabled()) {
            generateUniqueHandles(output_smart);

            output << "#ifndef  VULKAN_HPP_NO_SMART_HANDLE\n";
            output << "#include \"" + out.getFilename("_smart") << "\"\n";
            output << "#endif // VULKAN_HPP_NO_SMART_HANDLE\n";
        }

        if (!cfg.gen.expApi && !cfg.gen.globalMode) {
            // static commands
            const Handle  empty{ *this };
            GuardedOutput decl;
            for (auto &c : staticCommands) {
                ClassCommand    d{ this, &empty, c };
                MemberGenerator g{ *this, d, decl, outputFuncs, true };
                g.generate();
            }
            output << std::move(decl);
        }

        for (Handle &h : handles) {
            if (cfg.gen.onlyC && h.isSubclass) {
                continue;
            }
            if (cfg.gen.globalMode || cfg.gen.expApi) {
                genPlatform(output, h, [&](auto &output) { generateClassWithPFN(output, h); });
            } else {
                genPlatform(output, h, [&](auto &output) { generateClass(output, h, false); });
            }
        }

        output << endNamespace();

        if (verbose) {
            std::cout << "gen handles done" << '\n';
        }
    }

    static std::string getDeleter(const Handle &h, const std::string_view parent, const std::string &dispatch) {
        if (h.name == "CommandBuffer") {
            return "PoolFree<" + std::string(parent) + ", CommandPool" + dispatch + ">";
        }
        if (h.name == "DescriptorSet") {
            return "PoolFree<" + std::string(parent) + ", DescriptorPool" + dispatch + ">";
        }
        std::string output;
        switch (h.creationCat) {
            case Handle::CreationCategory::ALLOCATE: output += "ObjectFree<"; break;
            case Handle::CreationCategory::CREATE: output += "ObjectDestroy<"; break;
            default: break;
        }
        output += parent;
        output += dispatch;
        output += ">";
        return output;
    }

    void Generator::generateUniqueHandlesBase(OutputBuffer &output) {
        if (cfg.gen.expApi) {
            output << RES_UNIQUE_HANDLE_EXP;
        } else {
            output << RES_UNIQUE_HANDLE;
        }

        UniqueBaseGenerator g(*this);
        if (cfg.gen.globalMode) {
            g.owner = false;
        }
        g.name        = "ObjectDestroy";
        g.templ       = "typename OwnerType";
        g.destroyType = "destroy";
        output << "  struct AllocationCallbacks;\n\n";
        g.generate(output);

        g.templ.clear();
        g.specialization = "<NoParent";
        g.specialization += (g.dispatch ? ", Dispatch>" : ">");
        if (!cfg.gen.globalMode) {
            g.owner = false;
        }
        g.destroyRef = true;
        output << "  class NoParent;\n\n";
        g.generate(output);

        g.name        = "ObjectFree";
        g.templ       = "typename OwnerType";
        g.destroyType = "free";
        g.specialization.clear();
        if (!cfg.gen.globalMode) {
            g.owner = true;
        }
        g.destroyRef = false;
        g.generate(output);

        g.name        = "ObjectRelease";
        g.destroyType = "release";
        g.alloc       = false;
        g.generate(output);

        g.name        = "PoolFree";
        g.destroyType = "free";
        g.pool        = true;
        g.generate(output);
    }

    void Generator::generateUniqueHandles(OutputBuffer &output) {
        if (cfg.gen.globalMode) {
            generateUniqueHandlesBase(output);
        }

        for (Handle const &e : handles.ordered) {
            if (e.uniqueVariant()) {
                if (cfg.gen.globalMode && !e.isSubclass) {
                    continue;
                }
                genOptional(output, e, [&](auto &output) {
                    std::string templ;
                    std::string templType;
                    if (!cfg.gen.expApi) {
                        output << "  template <typename Dispatch>";
                        templ     = ", Dispatch";
                        templType = ", VULKAN_HPP_DEFAULT_DISPATCHER_TYPE";
                    } else {
                        output << "  template <>";
                    }

                    std::string_view parent = e.isSubclass ? std::string_view(e.superclass) : "NoParent";
                    output << vkgen::format(R"(
  class UniqueHandleTraits<{0}{1}>
  {{
  public:
    using deleter = {3};
  }};

  using Unique{0} = UniqueHandle<{0}{2}>;

)",
                                            e.name,
                                            templ,
                                            templType,
                                            getDeleter(e, parent, templ));
                });
            }
        }
    }

    std::string Generator::generateStructsInclude() const {
        std::string out = "#include <cstring>  // strcmp\n";
        return out;
    }

    void Generator::generateStructs(OutputBuffer &output, bool exp) {
        output << "#include \"vulkan_hpp_macros.hpp\"\n";
        if (cfg.gen.globalMode) {
            output << "#include \"vulkan_enums.hpp\"\n";
        }
        output << "#include \"vulkan_structs_forward.hpp\"\n";
        if (!cfg.gen.globalMode) {
            output << R"(
#ifndef VULKAN_HPP_NO_STRUCT_COMPARE
#if defined( VULKAN_HPP_HAS_SPACESHIP_OPERATOR )
#  include <compare>
#endif
#endif
)";
            if (!exp) {
                output << "\n";
                if (cfg.gen.importStdMacro) {
                    output << R"(
#include <string.h> // TODO
#ifndef USE_IMPORT_STD
#  ifndef VULKAN_HPP_NO_STRUCT_COMPARE
#    include <cstring>  // strcmp
#  endif // VULKAN_HPP_NO_STRUCT_COMPARE
#endif
)";
                } else {
                    gen(output, cfg.gen.structCompare, [&](auto &output) { output << generateStructsInclude(); });
                }
                output << "\n";
            }
        }
        output << beginNamespace();
        for (const Struct &e : structs.vulkan) {
            genPlatform(output, e, [&](auto &output) { generateStruct(output, e, exp); });
        }

        output << endNamespace();
    }

    void Generator::generateArrays(GenOutput &files, OutputBuffer &parent) {
        OutputBuffer output;
        if (cfg.gen.globalMode) {
            output << "#include \"vulkan.h\"\n";
            output << "#include \"vulkan_hpp_macros.hpp\"\n";
        }
        output << beginNamespace();
        output << RES_ARRAY_PROXY;
        // PROXY TEMPORARIES
        if (!cfg.gen.globalMode) {
            output << RES_ARRAY_WRAPPER;
        }
        output << endNamespace();
        generateUnit("vulkan_array.hpp", files, parent, std::move(output));
    }

    void Generator::generateVector(GenOutput &files, OutputBuffer &parent) {
        if (!cfg.gen.functionsVecAndArray) {
            return;
        }
        OutputBuffer output;
        if (cfg.gen.globalMode) {
            output << R"(
#include <memory>
#include <stdexcept>
#include <type_traits>
#include "vulkan_hpp_macros.hpp"
)";
        }
        output << beginNamespace();
        output << RES_VECTOR_DETAIL;
        output << "#if VULKAN_HPP_CPP_VERSION >= 20\n";
        output << RES_VECTOR_CPP20;
        output << "#else\n";
        output << "  #error \"Vector below c++20 is work in progress\"\n";
        output << RES_VECTOR;
        output << "#endif\n";
        output << endNamespace();
        generateUnit("vulkan_vector.hpp", files, parent, std::move(output));
    }

    void Generator::generateStructChain(GenOutput &files, OutputBuffer &parent) {
        OutputBuffer output;

        output << beginNamespaceStandalone();

        output << R"(

template <typename X, typename Y>
  struct StructExtends
  {
    enum
    {
      value = false
    };
  };

  template <typename Type, class...>
  struct IsPartOfStructureChain
  {
    static const bool valid = false;
  };

  template <typename Type, typename Head, typename... Tail>
  struct IsPartOfStructureChain<Type, Head, Tail...>
  {
    static const bool valid = std::is_same<Type, Head>::value || IsPartOfStructureChain<Type, Tail...>::valid;
  };

  template <size_t Index, typename T, typename... ChainElements>
  struct StructureChainContains
  {
    static const bool value = std::is_same<T, typename std::tuple_element<Index, std::tuple<ChainElements...>>::type>::value ||
                              StructureChainContains<Index - 1, T, ChainElements...>::value;
  };

  template <typename T, typename... ChainElements>
  struct StructureChainContains<0, T, ChainElements...>
  {
    static const bool value = std::is_same<T, typename std::tuple_element<0, std::tuple<ChainElements...>>::type>::value;
  };

  template <size_t Index, typename... ChainElements>
  struct StructureChainValidation
  {
    using TestType          = typename std::tuple_element<Index, std::tuple<ChainElements...>>::type;
    static const bool valid = StructExtends<TestType, typename std::tuple_element<0, std::tuple<ChainElements...>>::type>::value &&
                              ( /*TestType::allowDuplicate ||*/ !StructureChainContains<Index - 1, TestType, ChainElements...>::value ) &&
                              StructureChainValidation<Index - 1, ChainElements...>::valid;
  };

  template <typename... ChainElements>
  struct StructureChainValidation<0, ChainElements...>
  {
    static const bool valid = true;
  };

  template <typename... ChainElements>
  class StructureChain : public std::tuple<ChainElements...>
  {
  public:
    StructureChain() VULKAN_HPP_NOEXCEPT
    {
      static_assert( StructureChainValidation<sizeof...( ChainElements ) - 1, ChainElements...>::valid, "The structure chain is not valid!" );
)";

        output << R"(
      link<sizeof...( ChainElements ) - 1>();
    }

    StructureChain( StructureChain const & rhs ) VULKAN_HPP_NOEXCEPT : std::tuple<ChainElements...>( rhs )
    {
      static_assert( StructureChainValidation<sizeof...( ChainElements ) - 1, ChainElements...>::valid, "The structure chain is not valid!" );
      link( &std::get<0>( *this ),
            &std::get<0>( rhs ),
            reinterpret_cast<VkBaseOutStructure *>( &std::get<0>( *this ) ),
            reinterpret_cast<VkBaseInStructure const *>( &std::get<0>( rhs ) ) );
    }

    StructureChain( StructureChain && rhs ) VULKAN_HPP_NOEXCEPT : std::tuple<ChainElements...>( std::forward<std::tuple<ChainElements...>>( rhs ) )
    {
      static_assert( StructureChainValidation<sizeof...( ChainElements ) - 1, ChainElements...>::valid, "The structure chain is not valid!" );
      link( &std::get<0>( *this ),
            &std::get<0>( rhs ),
            reinterpret_cast<VkBaseOutStructure *>( &std::get<0>( *this ) ),
            reinterpret_cast<VkBaseInStructure const *>( &std::get<0>( rhs ) ) );
    }

    StructureChain( ChainElements const &... elems ) VULKAN_HPP_NOEXCEPT : std::tuple<ChainElements...>( elems... )
    {
      static_assert( StructureChainValidation<sizeof...( ChainElements ) - 1, ChainElements...>::valid, "The structure chain is not valid!" );
      link<sizeof...( ChainElements ) - 1>();
    }

    StructureChain & operator=( StructureChain const & rhs ) VULKAN_HPP_NOEXCEPT
    {
      std::tuple<ChainElements...>::operator=( rhs );
      link( &std::get<0>( *this ),
            &std::get<0>( rhs ),
            reinterpret_cast<VkBaseOutStructure *>( &std::get<0>( *this ) ),
            reinterpret_cast<VkBaseInStructure const *>( &std::get<0>( rhs ) ) );
      return *this;
    }

    // StructureChain & operator=( StructureChain && rhs ) = delete;

    template <typename T = typename std::tuple_element<0, std::tuple<ChainElements...>>::type, size_t Which = 0>
    T & get() VULKAN_HPP_NOEXCEPT
    {
      return std::get<ChainElementIndex<0, T, Which, void, ChainElements...>::value>( static_cast<std::tuple<ChainElements...> &>( *this ) );
    }

    template <typename T = typename std::tuple_element<0, std::tuple<ChainElements...>>::type, size_t Which = 0>
    T const & get() const VULKAN_HPP_NOEXCEPT
    {
      return std::get<ChainElementIndex<0, T, Which, void, ChainElements...>::value>( static_cast<std::tuple<ChainElements...> const &>( *this ) );
    }

    template <typename T0, typename T1, typename... Ts>
    std::tuple<T0 &, T1 &, Ts &...> get() VULKAN_HPP_NOEXCEPT
    {
      return std::tie( get<T0>(), get<T1>(), get<Ts>()... );
    }

    template <typename T0, typename T1, typename... Ts>
    std::tuple<T0 const &, T1 const &, Ts const &...> get() const VULKAN_HPP_NOEXCEPT
    {
      return std::tie( get<T0>(), get<T1>(), get<Ts>()... );
    }

    template <typename ClassType, size_t Which = 0>
    typename std::enable_if<std::is_same<ClassType, typename std::tuple_element<0, std::tuple<ChainElements...>>::type>::value && ( Which == 0 ), bool>::type
      isLinked() const VULKAN_HPP_NOEXCEPT
    {
      return true;
    }

    template <typename ClassType, size_t Which = 0>
    typename std::enable_if<!std::is_same<ClassType, typename std::tuple_element<0, std::tuple<ChainElements...>>::type>::value || ( Which != 0 ), bool>::type
      isLinked() const VULKAN_HPP_NOEXCEPT
    {
      static_assert( IsPartOfStructureChain<ClassType, ChainElements...>::valid, "Can't unlink Structure that's not part of this StructureChain!" );
      return isLinked( reinterpret_cast<VkBaseInStructure const *>( &get<ClassType, Which>() ) );
    }

    template <typename ClassType, size_t Which = 0>
    typename std::enable_if<!std::is_same<ClassType, typename std::tuple_element<0, std::tuple<ChainElements...>>::type>::value || ( Which != 0 ), void>::type
      relink() VULKAN_HPP_NOEXCEPT
    {
      static_assert( IsPartOfStructureChain<ClassType, ChainElements...>::valid, "Can't relink Structure that's not part of this StructureChain!" );
      auto pNext = reinterpret_cast<VkBaseInStructure *>( &get<ClassType, Which>() );
      VULKAN_HPP_ASSERT( !isLinked( pNext ) );
      auto & headElement = std::get<0>( static_cast<std::tuple<ChainElements...> &>( *this ) );
      pNext->pNext       = reinterpret_cast<VkBaseInStructure const *>( headElement.pNext );
      headElement.pNext  = pNext;
    }

    template <typename ClassType, size_t Which = 0>
    typename std::enable_if<!std::is_same<ClassType, typename std::tuple_element<0, std::tuple<ChainElements...>>::type>::value || ( Which != 0 ), void>::type
      unlink() VULKAN_HPP_NOEXCEPT
    {
      static_assert( IsPartOfStructureChain<ClassType, ChainElements...>::valid, "Can't unlink Structure that's not part of this StructureChain!" );
      unlink( reinterpret_cast<VkBaseOutStructure const *>( &get<ClassType, Which>() ) );
    }

  private:
    template <int Index, typename T, int Which, typename, class First, class... Types>
    struct ChainElementIndex : ChainElementIndex<Index + 1, T, Which, void, Types...>
    {
    };

    template <int Index, typename T, int Which, class First, class... Types>
    struct ChainElementIndex<Index, T, Which, typename std::enable_if<!std::is_same<T, First>::value, void>::type, First, Types...>
      : ChainElementIndex<Index + 1, T, Which, void, Types...>
    {
    };

    template <int Index, typename T, int Which, class First, class... Types>
    struct ChainElementIndex<Index, T, Which, typename std::enable_if<std::is_same<T, First>::value, void>::type, First, Types...>
      : ChainElementIndex<Index + 1, T, Which - 1, void, Types...>
    {
    };

    template <int Index, typename T, class First, class... Types>
    struct ChainElementIndex<Index, T, 0, typename std::enable_if<std::is_same<T, First>::value, void>::type, First, Types...>
      : std::integral_constant<int, Index>
    {
    };

    bool isLinked( VkBaseInStructure const * pNext ) const VULKAN_HPP_NOEXCEPT
    {
      VkBaseInStructure const * elementPtr =
        reinterpret_cast<VkBaseInStructure const *>( &std::get<0>( static_cast<std::tuple<ChainElements...> const &>( *this ) ) );
      while ( elementPtr )
      {
        if ( elementPtr->pNext == pNext )
        {
          return true;
        }
        elementPtr = elementPtr->pNext;
      }
      return false;
    }
)";

        output << R"(
    template <size_t Index>
    typename std::enable_if<Index != 0, void>::type link() VULKAN_HPP_NOEXCEPT
    {
      auto & x = std::get<Index - 1>( static_cast<std::tuple<ChainElements...> &>( *this ) );
      x.pNext  = &std::get<Index>( static_cast<std::tuple<ChainElements...> &>( *this ) );
      link<Index - 1>();
    }

    template <size_t Index>
    typename std::enable_if<Index == 0, void>::type link() VULKAN_HPP_NOEXCEPT
    {
    }

    void link( void * dstBase, void const * srcBase, VkBaseOutStructure * dst, VkBaseInStructure const * src )
    {
      while ( src->pNext )
      {
        std::ptrdiff_t offset = reinterpret_cast<char const *>( src->pNext ) - reinterpret_cast<char const *>( srcBase );
        dst->pNext            = reinterpret_cast<VkBaseOutStructure *>( reinterpret_cast<char *>( dstBase ) + offset );
        dst                   = dst->pNext;
        src                   = src->pNext;
      }
      dst->pNext = nullptr;
    }

    void unlink( VkBaseOutStructure const * pNext ) VULKAN_HPP_NOEXCEPT
    {
      VkBaseOutStructure * elementPtr = reinterpret_cast<VkBaseOutStructure *>( &std::get<0>( static_cast<std::tuple<ChainElements...> &>( *this ) ) );
      while ( elementPtr && ( elementPtr->pNext != pNext ) )
      {
        elementPtr = elementPtr->pNext;
      }
      if ( elementPtr )
      {
        elementPtr->pNext = pNext->pNext;
      }
      else
      {
        VULKAN_HPP_ASSERT( false );  // fires, if the ClassType member has already been unlinked !
      }
    }
  };
)";

        generateStructChains(output);
        output << endNamespaceStandalone();

        if (cfg.gen.globalMode) {
            output << R"(
namespace std {
  template<typename... ChainElements>
  class tuple_size<vk::StructureChain<ChainElements...>>:public std::integral_constant<std::size_t, sizeof...(ChainElements)>{};

  template<std::size_t I, typename... ChainElements>
  class tuple_element<I,vk::StructureChain<ChainElements...>>:public tuple_element<I, std::tuple<ChainElements...>>{};
}
)";
        }

        generateUnit("vulkan_struct_chains.hpp", files, parent, std::move(output));
    }

    void Generator::generateStructChains(vkgen::OutputBuffer &output) {
        GuardedOutput out;

        for (const Struct &s : structs.ordered) {
            if (s.extends.empty()) {
                continue;
            }

            out.add(s, [&](auto &output) {
                for (const auto &e : s.extends) {
                    if (!e->canGenerate()) {
                        continue;
                    }
                    genOptional(output, *e, [&](auto &output) {
                        output << "  template <>\n";
                        output << "  struct StructExtends<";
                        output << e->name << ", " << s.name;
                        output << R"(>
 {
   enum
   {
     value = true
   };
 };
)";
                    });
                }
            });
        }
        output << "#ifndef VULKAN_HPP_NO_STRUCT_EXTENDS\n";
        output << out.toString();
        output << "#endif // VULKAN_HPP_NO_STRUCT_EXTENDS\n";
    }

    bool Generator::generateStructConstructor(OutputBuffer &output, const Struct &data, bool transform) {
        bool hasProxy = false;

        FunctionGenerator fun(*this, "", data.name);
        fun.className          = data.name;
        fun.base               = &data;
        fun.optionalProtect    = Protect{ "VULKAN_HPP_NO_STRUCT_CONSTRUCTORS", false };
        fun.allowInline        = true;
        fun.specifierConstexpr = !transform;

        VariableData *pNext = nullptr;

        for (const auto &p : data.members) {
            std::string id   = p->identifier() + "_";
            std::string type = p->fullType(*this);

            bool const toProxy = transform && p->hasLengthVar() && !p->hasArrayLength();
            if (p->hasLengthVar() && !p->hasArrayLength()) {
                hasProxy = true;
            }

            if (p->type() == "StructureType") {
                if (data.name != "BaseOutStructure" && data.name != "BaseInStructure") {
                    continue;
                }
                fun.add(type, id, " = " + m_ns + "::StructureType::eApplicationInfo");
            } else if (p->identifier() == "pNext") {
                pNext = p.get();
            } else {
                if (toProxy) {
                    VariableData var(*p);
                    var.removeLastAsterisk();
                    if (var.type() == "void" && !var.isPointer()) {
                        var.setType("T");
                        fun.additonalTemplate = "typename T";
                    }
                    fun.add(vkgen::format("{0}::ArrayProxyNoTemporaries<{1}> const &", m_ns, var.fullType(*this)), id);
                } else {
                    fun.add(type, id, transform ? "" : " = {}");
                }
            }

            std::string const lhs = p->identifier();
            if (toProxy) {
                fun.addInit(lhs, id + ".data()");
            } else {
                std::stringstream rhs;
                const auto &vars = p->getArrayVars();
                if (!vars.empty() && transform) {
                    rhs << "static_cast<uint32_t>(";
                    for (size_t i = 0; i < vars.size(); ++i) {
                        const auto        &v  = vars[i];
                        const std::string &id = v->identifier();
                        if (i != vars.size() - 1) {
                            rhs << " !" << id << "_" << ".empty()? " << id << "_.size() :\n";
                        } else {
                            rhs << id << "_.size()";
                        }
                        if (v->type() == "void" && !v->isPointer()) {
                            rhs << " * sizeof(T)";
                        }
                    }
                    rhs << ")";
                }
                else {
                    rhs << id;
                }
                fun.addInit(lhs, rhs.str());
            }
        }
        if (pNext) {
            fun.add(pNext->fullType(*this), pNext->identifier() + "_", " = nullptr");
        }

        output << fun.generate(outputFuncs);

        return hasProxy;
    }

    void Generator::generateStruct(OutputBuffer &output, const Struct &data, bool exp) {
        bool genSetters          = data.hasStructType() && !data.returnedonly;
        bool genSettersProxy     = genSetters;
        bool genCompareOperators = data.isStruct();
        bool genConstructors     = true;

        if (cfg.gen.globalMode) {
            genSetters          = false;
            genSettersProxy     = false;
            genCompareOperators = false;
            genConstructors     = false;
        }

        std::string structureType;

        if (data.aliasParent) {
            output << "// TODO alias\n";
            output << "  using " << data.name << " = " << data.aliasParent->name << ";\n";
            return;
        }

        if (!data.structTypeValue.empty()) {
            structureType = "StructureType::" + data.structTypeValue;
        }

        if (!cfg.gen.globalMode) {
            for (const auto &m : data.members) {
                if (data.isStruct()) {
                    const auto &type = m->original.type();
                    const auto &s    = structs.find(type);
                    if (s) {
                        if (s->isUnion()) {
                            genCompareOperators = false;
                        }
                    }
                }
                if (m->hasArrayLength()) {
                    m->setSpecialType(VariableData::TYPE_ARRAY);
                }
            }
        }

        output << "// [" << std::to_string(data.id) << "]\n";
        output << "  " << data.metaTypeDeclaration() << " " << data.name << " {\n";

        output << "    using NativeType = " << data.name.original << ";\n";
        if (data.isStruct()) {
            if (!structureType.empty()) {
                output <<
                  "    static const bool                               "
                  "   allowDuplicate = false;\n";
                output << "    static VULKAN_HPP_CONST_OR_CONSTEXPR " << m_ns << "::StructureType structureType = " << structureType << ";\n";
            }
        }

        // constructor
        if (genConstructors) {
            if (data.isStruct()) {
                gen(output, cfg.gen.structConstructors, [&](auto &output) {
                    bool const hasProxy = generateStructConstructor(output, data, false);

                    if (hasProxy) {
                        output << "#  if !defined( VULKAN_HPP_DISABLE_ENHANCED_MODE )\n";
                        generateStructConstructor(output, data, true);
                        output << "#  endif // VULKAN_HPP_DISABLE_ENHANCED_MODE \n";
                    }

                    // copy constuctor
                    output << vkgen::format(R"(
{1} {0}( {0} const & rhs ) {2} = default;

)",
                                            data.name,
                                            m_constexpr14,
                                            m_noexcept);
                    {
                        // C style copy constuctor
                        FunctionGenerator fun(*this, "", data.name);
                        fun.className         = data.name;
                        fun.optionalProtect   = Protect{ "VULKAN_HPP_NO_STRUCT_CONSTRUCTORS", false };
                        fun.specifierNoexcept = true;
                        fun.add("Vk" + data.name + " const &", "rhs");
                        fun.addInit(data.name, "*reinterpret_cast<" + data.name + " const *>( &rhs )");
                        output << fun.generate(outputFuncs);
                    }
                });
            } else {
                gen(output, cfg.gen.unionConstructors, [&](auto &output) {
                    std::map<std::string, uint8_t> types;
                    for (const auto &m : data.members) {
                        auto it = types.find(m->type());
                        if (it != types.end()) {
                            it->second++;
                            continue;
                        }
                        types.emplace(m->type(), 1);
                    }

                    bool first = true;
                    for (const auto &m : data.members) {
                        if (m->original.type() == "VkBool32") {
                            continue;
                        }
                        const auto &type = m->type();
                        std::string id   = m->identifier();

                        auto it = types.find(type);
                        // multiple same types, generate only once
                        if (it != types.end()) {
                            if (it->second == 0) {
                                continue;
                            }
                            if (it->second > 1) {
                                it->second = 0;
                                id         = strFirstLower(type);
                            }
                        }

                        VariableData var = *m;
                        var.setIdentifier(id + "_");
                        id = m->identifier();

                        std::string arg = var.toString(*this);
                        std::string assignment;
                        if (first) {
                            assignment = " = {}";
                            first      = false;
                        }
                        output << vkgen::format(R"(
{5} {0}({1}{2}) : {3}( {4} ) {{}}
            )",
                                                data.name,
                                                arg,
                                                assignment,
                                                id,
                                                var.identifier(),
                                                m_constexpr14);
                    }
                });
            }
        }

        if (genSetters || genSettersProxy) {
            const auto &define = data.isStruct() ? cfg.gen.structSetters : cfg.gen.unionSetters;
            gen(output, define, [&](auto &output) {
                if (genSetters) {
                    for (const auto &m : data.members) {
                        if (m->type() == "StructureType") {
                            continue;
                        }

                        const auto &id = m->identifier();

                        FunctionGenerator fun(*this, data.name + "&", "set" + strFirstUpper(id));
                        fun.className            = data.name;
                        fun.base                 = &data;
                        fun.optionalProtect      = Protect{ define.data.define, false };
                        fun.specifierNoexcept    = true;
                        fun.specifierConstexpr14 = true;

                        fun.add(m->fullType(*this), m->identifier() + "_");
                        fun.code = vkgen::format(R"(
      {0} = {0}_;
      return *this;
)",
                                                 id);

                        output << fun.generate(outputFuncs);
                    }
                }

                bool hasArrayMember = false;
                // arrayproxy setters
                if (genSettersProxy) {
                    for (const auto &m : data.members) {
                        if (m->hasLengthVar()) {
                            hasArrayMember   = true;
                            auto        var  = VariableData(*m);
                            std::string name = var.identifier();
                            // name conversion
                            if (name.size() >= 3 && name.starts_with("pp") && (bool)std::isupper(name[2])) {
                                name = name.substr(1);
                            } else if (name.size() >= 2 && name.starts_with("p") && (bool)std::isupper(name[1])) {
                                name = name.substr(1);
                            }
                            name = strFirstUpper(name);

                            var.setIdentifier(m->identifier() + "_");
                            var.removeLastAsterisk();

                            FunctionGenerator fun(*this, data.name + "&", "set" + name);
                            fun.className         = data.name;
                            fun.base              = &data;
                            fun.optionalProtect   = Protect{ define.data.define, false };
                            fun.specifierNoexcept = true;

                            std::string const id = m->identifier();
                            std::string       modif;
                            if (var.original.type() == "void" && !var.original.isConstSuffix()) {
                                // output += "    template <typename DataType>\n";
                                fun.additonalTemplate = "typename DataType";
                                var.setType("DataType");
                                modif = " * sizeof(DataType)";
                            }

                            fun.add("ArrayProxyNoTemporaries<" + var.fullType(*this) + "> const &", var.identifier());

                            fun.code = vkgen::format(R"(
  {1} = static_cast<uint32_t>({0}.size(){3});
  {2} = {0}.data();
  return *this;
)",
                                                     var.identifier(),
                                                     m->getLengthVar()->identifier(),
                                                     m->identifier(),
                                                     modif);

                            output << fun.generate(outputFuncs);
                        }
                    }
                }
            });
        }

        if (data.isStruct()) {
            output << vkgen::format(R"(
    {0} & operator=({0} const &rhs) VULKAN_HPP_NOEXCEPT = default;

    {0} & operator=({1} const &rhs) VULKAN_HPP_NOEXCEPT {{
      *this = *reinterpret_cast<{2}::{0} const *>(&rhs);
      return *this;
    }}
)",
                                    data.name,
                                    data.name.original,
                                    m_ns);
        }

        output << vkgen::format(R"(

    operator {1} const &() const VULKAN_HPP_NOEXCEPT {{
      return *reinterpret_cast<const {1} *>(this);
    }}

    explicit operator {1}&() VULKAN_HPP_NOEXCEPT {{
      return *reinterpret_cast<{1} *>(this);
    }}

)",
                                data.name,
                                data.name.original);

        if (!cfg.gen.globalMode) {
            gen(output, cfg.gen.structReflect, [&](auto &output) {
                ArgumentBuilder types{ false };
                ArgumentBuilder tie{ false };
                for (const auto &m : data.members) {
                    types.append(m->fullType(*this), "");
                    tie.append("", m->identifier());
                }

                std::string type = vkgen::format(R"(
#  if 14 <= VULKAN_HPP_CPP_VERSION
    auto
#  else
    std::tuple<{0}>
#  endif
)",
                                                 types.string());

                FunctionGenerator fun(*this, type, "reflect");
                fun.className         = data.name;
                fun.base              = &data;
                fun.optionalProtect   = Protect{ "VULKAN_HPP_USE_REFLECT", true };
                fun.specifierNoexcept = true;
                fun.specifierConst    = true;
                fun.code              = "      return std::tie(" + tie.string() + ");\n";

                output << fun.generate(outputFuncs);
            });
        }

        std::string comp;
        for (const auto &m : data.members) {
            const std::string &id = m->identifier();
            comp += "( " + id + " == rhs." + id +
                    ")"
                    " && ";
        }
        strStripSuffix(comp, " && ");

        if (genCompareOperators) {
            // hardcoded types from https://github.com/KhronosGroup/Vulkan-Hpp/blob/main/VulkanHppGenerator.cpp
            static const std::set<std::string> simpleTypes = { "char",   "double",  "DWORD",    "float",    "HANDLE",   "HINSTANCE", "HMONITOR",
                                                               "HWND",   "int",     "int8_t",   "int16_t",  "int32_t",  "int64_t",   "LPCWSTR",
                                                               "size_t", "uint8_t", "uint16_t", "uint32_t", "uint64_t", "void" };

            std::string prefix;
            std::string compareMembers;
            std::string spaceshipMembers;
            bool        nonDefaultCompare = false;
            std::string ordering;
            if (data.containsFloatingPoints) {
                ordering = "std::partial_ordering";
            } else {
                ordering = "std::strong_ordering";
            }

            for (const auto &m : data.members) {
                const auto &id   = m->identifier();
                const auto &type = m->original.type();
                if (m->getNamespace() != Namespace::VK && simpleTypes.find(type) == simpleTypes.end() && enums.find(type)) {
                    nonDefaultCompare = true;

                    compareMembers += prefix + vkgen::format("( memcmp( &{0}, &rhs.{0}, sizeof( {1} ) ) == 0 )", id, type);

                    spaceshipMembers += "      if ( auto cmp = memcmp( &" + id + ", &rhs." + id + ", sizeof( " + type + " ) ); cmp != 0 )\n";
                    spaceshipMembers += "        return ( cmp < 0 ) ? " + ordering + "::less : " + ordering + "::greater;\n";
                } else if (type == "char" && !m->getLenAttrib().empty()) {
                    nonDefaultCompare = true;

                    if (m->lenExpressions.size() == 1) {
                        compareMembers += prefix + vkgen::format("( ( {0} == rhs.{0} ) || ( strcmp( {0}, rhs.{0} ) == 0 ) )", id, type);

                        spaceshipMembers += "     if ( " + id + " != rhs." + id + " )\n";
                        spaceshipMembers += "        if ( auto cmp = strcmp( " + id + ", rhs." + id + " ); cmp != 0 )\n";
                        spaceshipMembers += "          return ( cmp < 0 ) ? " + ordering + "::less : " + ordering + "::greater;\n";
                    } else {
                        compareMembers += prefix + vkgen::format("std::equal( {1}, {1} + {0}, rhs.{1}", m->lenExpressions[0], id);
                        compareMembers += ", []( char const * left, char const * right ) { return ( left == right ) || ( strcmp( left, right ) == 0 ); } )";

                        constexpr auto spaceshipMemberTemplate =
                          R"(      for ( size_t i = 0; i < {0}; ++i )
      {{
        if ( {1}[i] != rhs.{1}[i] )
          if ( auto cmp = strcmp( {1}[i], rhs.{1}[i] ); cmp != 0 )
            return cmp < 0 ? {2}::less : {2}::greater;
      }}
)";
                        spaceshipMembers += vkgen::format(spaceshipMemberTemplate, m->lenExpressions[0], id, ordering);
                    }
                } else {
                    compareMembers += prefix + "( " + id + " == rhs." + id + " )";
                    spaceshipMembers += "      if ( auto cmp = " + id + " <=> rhs." + id + "; cmp != 0 ) return cmp;\n";
                }
                prefix = "\n          && ";
            }

            std::string compareBody, spaceshipOperator;
            const bool  useSpaceship = cfg.gen.spaceshipOperator && !containsFuncPointer(data);
            if (nonDefaultCompare) {
                compareBody = "      return " + compareMembers + ";";
                if (useSpaceship) {
                    constexpr auto spaceshipOperatorTemplate =
                      R"(    {2} operator<=>( {0} const & rhs ) const VULKAN_HPP_NOEXCEPT /*X*/
    {{
{1}
      return {2}::equivalent;
    }}
)";

                    spaceshipOperator = vkgen::format(spaceshipOperatorTemplate, data.name, spaceshipMembers, ordering);
                }
            } else {
                constexpr auto compareBodyTemplate =
                  R"(#if defined( VULKAN_HPP_USE_REFLECT )
      return this->reflect() == rhs.reflect();
#else
      return {0};
#endif // VULKAN_HPP_USE_REFLECT
)";
                compareBody = vkgen::format(compareBodyTemplate, compareMembers);

                if (useSpaceship) {
                    spaceshipOperator = "    auto operator<=>( " + data.name + " const & ) const = default;";
                }
            }

            gen(output, cfg.gen.structCompare, [&](auto &output) {
                if (!spaceshipOperator.empty()) {
                    output << "#  if defined( VULKAN_HPP_HAS_SPACESHIP_OPERATOR )\n";

                    if (nonDefaultCompare) {
                        FunctionGenerator fun(*this, ordering, "operator<=>");
                        fun.className         = data.name;
                        fun.base              = &data;
                        fun.optionalProtect   = Protect{ "VULKAN_HPP_USE_SPACESHIP_OPERATOR", true };  // workaround
                        fun.specifierNoexcept = true;
                        fun.specifierConst    = true;
                        fun.code              = vkgen::format(R"(
{0}
      return {1}::equivalent;
)",
                                                 spaceshipMembers,
                                                 ordering);

                        fun.add(data.name + " const &", "rhs");
                        output << fun.generate(outputFuncs);
                    } else {
                        output << spaceshipOperator + "\n";
                    }

                    output << "#  else\n";
                }

                {
                    FunctionGenerator fun(*this, "bool", "operator==");
                    fun.className         = data.name;
                    fun.base              = &data;
                    fun.optionalProtect   = Protect{ "VULKAN_HPP_HAS_SPACESHIP_OPERATOR", false };
                    fun.specifierNoexcept = true;
                    fun.specifierConst    = true;

                    fun.add(data.name + " const &", "rhs");
                    fun.code = compareBody;
                    output << fun.generate(outputFuncs);
                }
                {
                    FunctionGenerator fun(*this, "bool", "operator!=");
                    fun.className         = data.name;
                    fun.base              = &data;
                    fun.optionalProtect   = Protect{ "VULKAN_HPP_HAS_SPACESHIP_OPERATOR", false };
                    fun.specifierNoexcept = true;
                    fun.specifierConst    = true;

                    fun.add(data.name + " const &", "rhs");
                    fun.code = "      return !operator==( rhs );\n";
                    output << fun.generate(outputFuncs);
                }

                if (!spaceshipOperator.empty()) {
                    output << "#  endif\n";
                }
            });
        }

        bool       cmock  = cfg.gen.globalMode;
        for (const auto &m : data.members) {
            if (data.isStruct()) {
                std::string assignment = "{}";
                const auto &type       = m->original.type();
                if (type == "VkStructureType") {
                    assignment = structureType.empty() ? "StructureType::eApplicationInfo" : structureType;
                }

                m->setAssignment(" = " + assignment);

                if ((cmock && ((!m->isStructOrUnion() && !m->isEnum() && !m->isHandle()) || m->hasArrayLength()))) {
                    output << "    " << m->fullType(*this) << "    " << m->identifier() << m->optionalArraySuffix() << m->getNameSuffix();
                    if (!assignment.empty()) {
                        output << " = " << assignment;
                    }
                    output << ";\n";
                } else {
                    output << "    " << m->toStructStringWithAssignment(*this, false) << ";\n";
                }

            } else {
                if ((cmock && (!m->isStruct() && !m->isEnum() || m->hasArrayLength()))) {
                    output << "    " << m->fullType(*this) << "    " << m->identifier() << m->optionalArraySuffix() << m->getNameSuffix() << ";\n";
                } else {
                    output << "    " << m->toStructString(*this, false) << ";\n";
                }
            }
        }

        output << "  };\n\n";

        if (data.isStruct() && !structureType.empty() && !cfg.gen.globalMode) {
            gen(output, cfg.gen.handleTemplates, [&](auto &output) {
                output << vkgen::format(R"(
  template <>
  struct CppType<StructureType, {0}> {{
    using Type = {1};
  }};
)",
                                        structureType,
                                        data.name);
            });
        }

    }

    std::string Generator::generateIncludeRAII(GenOutput &out) const {
        std::string output;
        if (cfg.gen.importStdMacro) {
            output += R"(
#ifndef USE_IMPORT_STD
)";
        }

        output += R"(
#include <memory>
#include <vector>
#include <utility>  // std::exchange, std::forward
)";

        if (cfg.gen.importStdMacro) {
            output += R"(
#endif
)";
        }

        output += vkgen::format(R"(
#include "{0}"
#include "{1}"
)",
                                out.getFilename(""),
                                out.getFilename("_raii_forward"));

        return output;
    }

    void Generator::generateDestroyMember(const Handle &h, GuardedOutput &decl, GuardedOutputFuncs &def) {
        if (h.dtorCmd) {
            MemberContext ctx{ .ns = Namespace::VK };
            if (cfg.gen.globalMode) {
                ctx.disableDispatch = true;
                ctx.removeSuperclassVar = true;
                ctx.exp             = true;
            }
            ClassCommand          d(this, &h, *h.dtorCmd);
            MemberResolverDestroy resolver{ *this, d, ctx };
            resolver.generate(decl, def);
        }
    }

    void Generator::generateClassWithPFN(OutputBuffer &output, Handle &h) {
        OutputClass out {
            .name = h.name
        };
        const auto       &name     = out.name;
        const std::string dispatch = name + "Dispatcher";
        const auto       &handle   = h.vkhandle.identifier();
        const bool vkhpp = cfg.gen.globalMode;

        generateClassTypeInfo(h, output, out);

        output << "// " << h.superclass << " ";
        if (h.parent) {
            output << h.parent->name << " ";
        }
        output << "\n";

        output << "  class " << name << " {\n";
        output << "  protected:\n";
        output << "    Vk" << name << " " << handle << " = {};\n";
        if (vkhpp && !h.isSubclass) {
            output << "    const " << h.superclass << "* m_parent = {};\n";
            output << "    " << dispatch << " m_dispatcher = {};\n";
        }
        output << "  public:\n";

        output << out.sPublic.toString();
        output << "    explicit " + name + "(std::nullptr_t) VULKAN_HPP_NOEXCEPT {}\n";

        GuardedOutput ctors;
        GuardedOutput dtor;
        GuardedOutputFuncs members;

        if (!h.isSubclass) {
            for (auto &d : h.ctorCmds) {
                MemberContext ctx{ .ns = Namespace::VK };
                ctx.insertSuperclassVar = true;
                ctx.disableDispatch = true;
                ctx.exp             = true;
                MemberResolverCtor resolver{ *this, d, ctx };
                if (!resolver.hasDependencies) {
                    std::cout << "ctor skipped: class " << h.name << ", p: " << '\n';
                    continue;
                }
                resolver.generate(ctors, outputFuncs);

                MemberResolverInit init{ *this, d, ctx };
                init.generate(ctors, outputFuncs);
            }

            // generateDestroyMember(h, dtor, outputFuncs);
        }


        if (!h.isSubclass) {
            for (auto &d : h.members) {
                 MemberGenerator g{ *this, d, members.decl, outputFuncs };
                 g.generate();
            }
        }

        {
            std::stringstream temp;
            ctors.write(temp);  // TODO
            output << temp.str();
        }

        output << "    " << name << "() = default;\n";
        if (h.isSubclass) {
            output << "    " << name << "(Vk" << name << " handle)\n";
            output << "      : " << handle << "(handle)\n";
            output << "      {}\n";
        }
        else {
            output << "    " << name << "(" << name << " const&) = delete;\n";

            if (!vkhpp) {
                output << "    " << name << "(" << name << "&& rhs) " << m_noexcept << "\n";
                output << "      : " << handle << "(rhs." << handle;
                output << "),\n        m_dispatcher(rhs.m_dispatcher)\n";
                output << "      {}\n";
            }
            output << "    " << name << "& operator=(" << name << " const &) = delete;\n";
            if (!vkhpp) {
                output << "    " << name << "& operator=(" << name << "&& rhs) " << m_noexcept << "\n";
                output << "    {\n";
                output << "      " << handle << " = rhs." << handle << ";\n";
                output << "      m_dispatcher = rhs.m_dispatcher;\n";
                output << "      return *this;\n";
                output << "    }\n";
            }
        }

        {
            std::stringstream temp;
            dtor.write(temp);
            output << temp.str();
        }

        output << "    operator Vk" << name << "() const {\n";
        output << "      return " << handle << ";\n";
        output << "    }\n";

        output << "    Vk" << name << " const &operator*() const " << m_noexcept << " {\n";
        output << "      return " << handle << ";\n";
        output << "    }\n";

        if (vkhpp && !h.isSubclass) {
            output << "    " << dispatch << " const * getDispatcher() const " << m_noexcept << " {\n";
            output << "      return &m_dispatcher;\n";
            output << "    }\n";

            output << "    " << h.superclass << " const * get" << h.superclass << "() const " << m_noexcept << " {\n";
            output << "      return m_parent;\n";
            output << "    }\n";
        }

        output << members.decl.toString();

        output << "  };\n";
    }

    void Generator::generateContext(OutputBuffer &output) {
        if (cfg.gen.integrateVma) {
            output << "#include <vma/vk_mem_alloc.h>\n";
        }
        if (cfg.gen.globalMode) {

            if (cfg.gen.cppStd >= 20) {
                output << "#include <bit>\n";
            }
            if (cfg.gen.functionsVecAndArray) {
                output << "#include \"vulkan_vector.hpp\"\n";
            }
        }
        output << "#include \"vulkan_handles_forward.hpp\"\n";

        output << R"(
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
)";

        output << beginNamespace();
        generateDispatchRAII(output);

        OutputClass out {
            .name = loader.name
        };

        const std::string dispatcher = loader.name + "Dispatcher";

        out.sProtected << "    LIBHANDLE lib = {};\n";
        std::string dispatchCall = "m_dispatcher";
        if (!cfg.gen.globalMode && cfg.gen.dispatchTableAsUnique) {
            out.sProtected << "    std::unique_ptr<" + dispatcher + "> m_dispatcher;\n";
            dispatchCall += "->";
        } else {
            out.sProtected << "    " << dispatcher << " m_dispatcher;\n";
            dispatchCall += '.';
        }

        out.sPublic << R"(
#ifdef _WIN32
    static constexpr char const* defaultLibpath = "vulkan-1.dll";
#else
    static constexpr char const* defaultLibpath = "libvulkan.so.1";
#endif
)";
        out.sPublic << vkgen::format(R"(
    {0}() = default;

    ~{0}() {{
      unload();
    }}
    /*
    {0}(const char *libpath) {{
      load(libpath);
    }}
    */
    {3}{1} const* getDispatcher(){4}
    {{
      return &{2}m_dispatcher;
    }}
)",
                                     loader.name,
                                     dispatcher,
                                     cfg.gen.dispatchTableAsUnique ? "*" : "",
                                     "", // cfg.gen.globalMode? "static " : "",
                                     " const" // cfg.gen.globalMode? "" : " const"
        );

        // VULKAN_HPP_ASSERT( m_dispatcher->getVkHeaderVersion() == VK_HEADER_VERSION );
        out.sPublic << vkgen::format(R"(
    VULKAN_HPP_INLINE PFN_vkVoidFunction getProcAddr(const char* name) const {{
      return {1}vkGetInstanceProcAddr(nullptr, name);
    }}

    template<typename T>
    VULKAN_HPP_INLINE T getProcAddr(const char *name) const {{
      return {0}<T>({1}vkGetInstanceProcAddr(nullptr, name));
    }}

    VULKAN_HPP_NODISCARD bool load(const char *libpath) VULKAN_HPP_NOEXCEPT {{

#ifdef _WIN32
      lib = LoadLibraryA(libpath);
#else
      lib = dlopen(libpath, RTLD_NOW);
#endif
      if (!lib) {{
        return false;
      }}

#ifdef _WIN32
      PFN_vkGetInstanceProcAddr getInstanceProcAddr = {0}<PFN_vkGetInstanceProcAddr>(GetProcAddress(lib, "vkGetInstanceProcAddr"));
#else
      PFN_vkGetInstanceProcAddr getInstanceProcAddr = {0}<PFN_vkGetInstanceProcAddr>(dlsym(lib, "vkGetInstanceProcAddr"));
#endif
      if (!getInstanceProcAddr) {{
        return false;
      }}
)",
            m_cast,
            dispatchCall);
        if (cfg.gen.dispatchTableAsUnique) {
            out.sPublic << "      m_dispatcher.reset( new " << dispatcher << "( getInstanceProcAddr ) );\n";
        } else {
            out.sPublic << "      m_dispatcher.load( getInstanceProcAddr );\n";
        }
        out.sPublic << R"(
      return true;
    }

    VULKAN_HPP_NODISCARD bool load() VULKAN_HPP_NOEXCEPT {
      return load(defaultLibpath);
    }

    void unload() {
      if (lib) {
#ifdef _WIN32
        FreeLibrary(lib);
#else
        dlclose(lib);
#endif
        lib = nullptr;
      }
    }

    VULKAN_HPP_INLINE bool isLoaded() const {
        return lib;
    }

)";

        auto &funcs = (cfg.gen.expApi || cfg.gen.globalMode) ? outputFuncs : outputFuncsRAII;
        if (cfg.gen.globalMode) {
            funcs.decl << R"(
    uint32_t enumerateInstanceVersion();

)";
            funcs.def << vkgen::format(R"(
    {0} uint32_t enumerateInstanceVersion() {{
      const auto &pfn = vk::context.getDispatcher()->vkEnumerateInstanceVersion;
      if (pfn == nullptr) {{
        return VK_API_VERSION_1_0;
      }}
      uint32_t version;
      auto result = pfn(&version);
      resultCheck(result,
                VULKAN_HPP_NAMESPACE_STRING "::enumerateInstanceVersion");
      return version;
    }}

)",
                                       cfg.macro.mInline.define);
        }
        else {
            out.sPublic << R"(
    uint32_t enumerateInstanceVersion() const;

)";
            funcs.def << vkgen::format(R"(
    uint32_t Context::enumerateInstanceVersion() const noexcept {{
      if ({0}vkEnumerateInstanceVersion == nullptr) {{
        return VK_API_VERSION_1_0;
      }}
      uint32_t version;
      auto result = {0}vkEnumerateInstanceVersion(&version);
      resultCheck(result,
                VULKAN_HPP_NAMESPACE_STRING "::enumerateInstanceVersion");
      return version;
    }}

)",
                                       dispatchCall);
        }

        // if (!cfg.gen.globalMode) {
            for (auto &m : loader.members) {
                if (!cfg.gen.globalMode && m.name == "createInstance") {
                    continue;
                }
                if (m.name == "enumerateInstanceVersion") {
                    continue;
                }
                MemberContext ctx{ .ns = Namespace::RAII };
                if (m.src->nameCat == Command::NameCategory::CREATE) {
                    ctx.insertClassVar = true;
                }
                ctx.globalUseCAPI = cfg.gen.onlyC;
                generateClassMember(m, ctx, out, funcs);
            }
        // }

        output << R"(
#ifdef _WIN32
#  define LIBHANDLE HINSTANCE
#else
#  define LIBHANDLE void*
#endif
)";

        output << std::move(out);
        output << "  ";

        output << endNamespace();
    }

    void Generator::generateExperimentalRAII(OutputBuffer &output, GenOutput &out) {
        output << genNamespaceMacro(cfg.macro.mNamespaceRAII);
        output << generateIncludeRAII(out);

        output << beginNamespace();
        output << "  " << beginNamespaceRAII(true);

        output << RES_RAII;

        generateClassesRAII(output, true);

        output << "  " + endNamespaceRAII();
        output << endNamespace();
    }

    void Generator::generateRAII(OutputBuffer &output, OutputBuffer &output_forward, GenOutput &out) {
        output_forward << genNamespaceMacro(cfg.macro.mNamespaceRAII);

        output_forward << beginNamespace();
        output_forward << "  " << beginNamespaceRAII(true);

        output << generateIncludeRAII(out);

        output << "#include \"vulkan_context.hpp\"\n";

        output << beginNamespace();
        output << "  " << beginNamespaceRAII();

        output << RES_RAII;

        for (Handle &e : handles.ordered) {
            generateClassDecl(output_forward, e);
        }

        output << "  using " << m_ns << "::" << loader.name << ";\n";
        for (const Handle &h : topLevelHandles) {
            output << "  using " << m_ns << "::" << h.name << "Dispatcher;\n";
        }

        generateClassesRAII(output);

        output_forward << "  " << endNamespaceRAII();
        output_forward << endNamespace();

        output << "  " << endNamespaceRAII();
        output << endNamespace();

        output << "#include \"" << out.getFilename("_raii_funcs") << "\"\n";
    }

    void Generator::generateFuncsRAII(OutputBuffer &output) {
        output << beginNamespace();
        output << "  " << beginNamespaceRAII();
        output << std::move(outputFuncsRAII.def);
        output << "  " << endNamespaceRAII();
        output << endNamespace();
    }

    static void initVmaMap(std::unordered_map<std::string, std::pair<const char *, bool>> &vma) {
        vma.emplace("vkGetInstanceProcAddr", std::make_pair(nullptr, false));
        vma.emplace("vkGetDeviceProcAddr", std::make_pair(nullptr, false));
        vma.emplace("vkGetPhysicalDeviceProperties", std::make_pair(nullptr, false));
        vma.emplace("vkGetPhysicalDeviceMemoryProperties", std::make_pair(nullptr, false));
        vma.emplace("vkAllocateMemory", std::make_pair(nullptr, false));
        vma.emplace("vkFreeMemory", std::make_pair(nullptr, false));
        vma.emplace("vkMapMemory", std::make_pair(nullptr, false));
        vma.emplace("vkUnmapMemory", std::make_pair(nullptr, false));
        vma.emplace("vkFlushMappedMemoryRanges", std::make_pair(nullptr, false));
        vma.emplace("vkInvalidateMappedMemoryRanges", std::make_pair(nullptr, false));
        vma.emplace("vkBindBufferMemory", std::make_pair(nullptr, false));
        vma.emplace("vkBindImageMemory", std::make_pair(nullptr, false));
        vma.emplace("vkGetBufferMemoryRequirements", std::make_pair(nullptr, false));
        vma.emplace("vkGetImageMemoryRequirements", std::make_pair(nullptr, false));
        vma.emplace("vkCreateBuffer", std::make_pair(nullptr, false));
        vma.emplace("vkDestroyBuffer", std::make_pair(nullptr, false));
        vma.emplace("vkCreateImage", std::make_pair(nullptr, false));
        vma.emplace("vkDestroyImage", std::make_pair(nullptr, false));
        vma.emplace("vkCmdCopyBuffer", std::make_pair(nullptr, false));
        vma.emplace("vkGetBufferMemoryRequirements2KHR", std::make_pair("VMA_DEDICATED_ALLOCATION || VMA_VULKAN_VERSION >= 1001000", false));
        vma.emplace("vkGetImageMemoryRequirements2KHR", std::make_pair("VMA_DEDICATED_ALLOCATION || VMA_VULKAN_VERSION >= 1001000", false));
        vma.emplace("vkBindBufferMemory2KHR", std::make_pair("VMA_BIND_MEMORY2 || VMA_VULKAN_VERSION >= 1001000", false));
        vma.emplace("vkBindImageMemory2KHR", std::make_pair("VMA_BIND_MEMORY2 || VMA_VULKAN_VERSION >= 1001000", false));
        vma.emplace("vkGetPhysicalDeviceMemoryProperties2KHR", std::make_pair("VMA_MEMORY_BUDGET || VMA_VULKAN_VERSION >= 1001000", false));
        vma.emplace("vkGetDeviceBufferMemoryRequirements", std::make_pair("VMA_KHR_MAINTENANCE4 || VMA_VULKAN_VERSION >= 1003000", false));
        vma.emplace("vkGetDeviceImageMemoryRequirements", std::make_pair("VMA_KHR_MAINTENANCE4 || VMA_VULKAN_VERSION >= 1003000", false));
    }

    class DispatchGenerator
    {
        const Generator &gen;
        const Handle    &h;
        std::string      getAddr;
        std::string      handle;
        OutputBuffer      init;
        OutputBuffer      init2;
        bool             isContext;

        void generateContextMembers(bool useVma, OutputClass &out, OutputBuffer &output) {
            std::unordered_map<std::string, std::pair<const char *, bool>> vma;

            if (useVma) {
                out.inherits += "public VmaVulkanFunctions";
                initVmaMap(vma);

                for (auto *d : h.filteredMembers) {
                    const std::string &name = d->name.original;
                    auto               it   = vma.find(name);
                    if (it != vma.end()) {
                        it->second.second = true;
                    }
                }

                for (auto &v : vma) {
                    if (!v.second.second) {
                        init << vkgen::format("      {0} = PFN_{0}(vkGetDeviceProcAddr(device, \"{0}\"));\n", v.first);
                    } else {
                        init << "/*" << v.first << "*/\n";
                    }
                }
            }

            // PFN function pointers
            for (auto *d : h.filteredMembers) {
                const std::string &name = d->name.original;

                const char *vmaGuard = nullptr;
                bool        isVma    = false;
                if (useVma) {
                    auto it = vma.find(name);
                    if (it != vma.end()) {
                        isVma    = !it->second.first;
                        vmaGuard = it->second.first;
                    }
                }

                if (!isVma) {
                    gen.genOptional(out.sPublic.get(), *d, [&](auto &output) {
                        if (vmaGuard) {
                            output << "#if !(" << vmaGuard << ")\n";
                        }
                        output << vkgen::format("    PFN_{0} {0} = {{}};", name);
                        if (d->src->aliasParent) {
                            output << " // alias";
                        }
                        output << "\n";
                        if (vmaGuard) {
                            output << "#endif\n";
                        }
                    });
                }

                gen.genOptional(init,
                  *d, [&](auto &output) {
                      output << vkgen::format("      {0} = PFN_{0}( {1}({2}, \"{0}\") );\n", name, getAddr, handle);
                });

            }
        }

      public:
        DispatchGenerator(const Generator &gen, const Handle &h, bool isContext = false) : gen(gen), h(h), isContext(isContext) {}

        void generate(OutputBuffer &output) {
            init.clear();
            init2.clear();
            OutputClass out {
                .name = h.name + "Dispatcher"
            };
            const auto &name = out.name;

            std::string src;
            if (isContext) {
                getAddr = "vkGetInstanceProcAddr";
                handle  = "NULL";
            } else if (h.getAddrCmd.has_value()) {
                getAddr = h.getAddrCmd->name.original;
                handle  = strFirstLower(h.name);
                src     = h.name.original + " " + handle;
            }

            const bool useVma = h.name == "Device" && gen.config().gen.integrateVma;

            generateContextMembers(useVma, out, output);

            if (!useVma) {
                out.sPublic << "    PFN_" << getAddr << " " << getAddr << " = {};\n";
            }
            out.sPublic << "\n";

            out.sPublic << "    " << name << "() = default;\n";

            if (h.name == "Instance") {
                std::string name = "vkGetDeviceProcAddr";
                out.sPublic << "    PFN_" << name << " " << name << " = {};\n";
                init << vkgen::format("      {0} = PFN_{0}( {1}({2}, \"{0}\") );\n", name, getAddr, handle);
            }

            std::string addrInit = "      " + getAddr + " = getProcAddr;\n";
            {
                std::string args = "PFN_" + getAddr + " getProcAddr";
                std::string loadArgs = "getProcAddr";
                if (!isContext) {
                    args     += ", " + src;
                    loadArgs += ", " + handle;
                }
                out.sPublic << vkgen::format(R"(
    {0}({1}) {{
      load({2});
    }}
)",
                                             name,
                                             args,
                                             loadArgs);

                if (!isContext) {
                    std::string dispatchArgs = "const " + h.superclass + "Dispatcher &source, " + src;
                    std::string dispatchLoadArgs = "source.vkGet" + h.name + "ProcAddr, " + handle;

                    out.sPublic << vkgen::format(R"(
    {0}({1}) {{
      load({2});
    }}

    void load({1}) {{
      load({2});
    }}
)",
                    name,
                    dispatchArgs,
                    dispatchLoadArgs );
                }

                out.sPublic << "    void load(" + args + ") {\n";
                out.sPublic << std::move(addrInit);
                out.sPublic << std::move(init);
                out.sPublic << std::move(init2);
                out.sPublic << "    }\n";
            }


            if (useVma) {
                out.sPublic << R"(
    VULKAN_HPP_NODISCARD const VmaVulkanFunctions* vma() const {
      return this;
    }
)";
            }

            output << std::move(out);
        }
    };

    void Generator::generateDispatchRAII(OutputBuffer &output) {
        auto &instance = findHandle("VkInstance");
        auto &device   = findHandle("VkDevice");

        DispatchGenerator(*this, loader, true).generate(output);
        DispatchGenerator(*this, instance).generate(output);
        DispatchGenerator(*this, device).generate(output);

        output << "\nstruct Dispatch : public InstanceDispatcher, DeviceDispatcher {};\n";
    }

    void Generator::evalCommand(Command &cmd) const {
        std::string const name = cmd.name;
        std::string const tag  = strWithoutTag(name);
        cmd.nameCat            = evalNameCategory(name);
    }

    Command::NameCategory Generator::evalNameCategory(const std::string &name) {
        using enum Command::NameCategory;
        if (name.starts_with("get")) {
            return GET;
        }
        if (name.starts_with("allocate")) {
            return ALLOCATE;
        }
        if (name.starts_with("acquire")) {
            return ACQUIRE;
        }
        if (name.starts_with("create")) {
            return CREATE;
        }
        if (name.starts_with("enumerate")) {
            return ENUMERATE;
        }
        if (name.starts_with("write")) {
            return WRITE;
        }
        if (name.starts_with("destroy")) {
            return DESTROY;
        }
        if (name.starts_with("free")) {
            return FREE;
        }
        return UNKNOWN;
    }

    void Generator::generateClassMember(ClassCommand &m, MemberContext &ctx, OutputClass &out, GuardedOutputFuncs &outFuncs, bool inlineFuncs) {
        MemberGenerator g{ *this, m, out.sFuncs, outFuncs };
        if (ctx.globalUseCAPI) {
            g.ctx.globalUseCAPI = true;
        }
        g.generate();
    }

    void Generator::generateClassMembers(const Handle &data, OutputClass &out, GuardedOutputFuncs &outFuncs, Namespace ns, bool inlineFuncs) {
        std::string output;
        if (ns == Namespace::RAII) {
            const auto        &className = data.name;
            const auto        &handle    = data.vkhandle.identifier();
            const std::string &ldr       = loader.name;

            const auto  &superclass = findHandle("Vk" + data.superclass);
            VariableData super(superclass.name);
            super.setConst(true);

            std::string call;
            if (data.dtorCmd) {
                MemberContext                 ctx{ .ns = ns };
                ClassCommand                  d(this, &data, *data.dtorCmd);
                MemberResolverClearRAII const r{ *this, d, ctx };
                call = r.temporary(handle);
            }

            std::string clear;
            std::string swap;
            data.foreachVars(VariableData::Flags::CLASS_VAR_RAII, [&](const VariableData &v) {
                clear += "      " + v.identifier() + " = nullptr;\n";
                swap += "      std::swap( " + v.identifier() + ", rhs." + v.identifier() + " );\n";
            });

            output += vkgen::format(R"(
    inline void {0}::clear() VULKAN_HPP_NOEXCEPT {{
{1}{2}
    }}

    inline void {0}::swap({4}::{0} &rhs) VULKAN_HPP_NOEXCEPT {{
{3}
    }}
)",
                                    className,
                                    call,
                                    clear,
                                    swap,
                                    m_ns_raii);
        }

        if (!output.empty()) {
            genOptional(outputFuncsRAII.def.get(), data, [&](auto &out) { out << output; });
        }

        // wrapper functions
        for (ClassCommand &m : const_cast<Handle &>(data).members) {
            if (ns == Namespace::VK && m.src->isIndirect()) {
                continue;
            }
            MemberContext ctx{ .ns = ns };
            generateClassMember(m, ctx, out, outFuncs, inlineFuncs);
        }
    }

    void Generator::generateClassConstructors(const Handle &data, OutputClass &out) {
        const std::string &superclass = data.superclass;

        out.sPublic << vkgen::format(R"(
    VULKAN_HPP_CONSTEXPR {0}() = default;
)",
                                     data.name);

//        std::string body;
//        if (!dispatchInit.empty()) {
//            body = ", m_dispatcher(" + dispatchInit + ")";
//        }

        out.sPublic << vkgen::format(R"(
    VULKAN_HPP_CONSTEXPR {0}(std::nullptr_t) VULKAN_HPP_NOEXCEPT {{}}
)",
                                     data.name,
                                     strFirstLower(data.name));

        if (!data.isSubclass && cfg.gen.globalMode) {
            out.sPublic << vkgen::format(R"(
    VULKAN_HPP_TYPESAFE_EXPLICIT {0}(Vk{0} {1}) VULKAN_HPP_NOEXCEPT {{
      {2} = {1};
    }}
)",
                                         data.name,
                                         strFirstLower(data.name),
                                         data.vkhandle.identifier());
        }
        else {
            out.sPublic << vkgen::format(R"(
    VULKAN_HPP_TYPESAFE_EXPLICIT {0}(Vk{0} {1}) VULKAN_HPP_NOEXCEPT  : {2}({1}) {{}}
)",
                                         data.name,
                                         strFirstLower(data.name),
                                         data.vkhandle.identifier());
        }

        if (!cfg.gen.globalMode) {
            const auto &superclass = data.superclass;

            for (auto &m : const_cast<Handle &>(data).vectorCmds) {

                if (m.name.original == "vkCreateSharedSwapchainsKHR") {
                    continue;
                }

                MemberContext ctx{ .ns = Namespace::VK };
                ctx.insertSuperclassVar = true;
                ctx.disableDispatch     = true;
                ctx.exp                 = true;
                ctx.returnSingle = true;
                MemberResolverCtor resolver{ *this, m, ctx };

                resolver.generate(out.sPublic, outputFuncs);
            }

            for (auto &m : const_cast<Handle &>(data).ctorCmds) {

                if (cfg.dbg.methodTags) {
                    out.sPublic << "    /* exp ctor: " << m.name.original << " */\n";
                }
                const auto genCtor = [&](ClassCommand &d) {
                    MemberContext ctx{ .ns = Namespace::VK };
                    ctx.insertSuperclassVar = true;
                    ctx.disableDispatch     = true;
                    ctx.exp                 = true;
                    MemberResolverCtor resolver{ *this, d, ctx };

                    if (!resolver.hasDependencies) {
                        // std::cout << "ctor skipped: class " << data.name << ", s: " << superclass << '\n';
                        return;
                    }

                    resolver.generate(out.sPublic, outputFuncs);
                };

                genCtor(m);
            }
        }

    }

    void Generator::generateClassConstructorsRAII(const Handle &data, OutputClass &out) {
        static constexpr Namespace ns = Namespace::RAII;

        const auto        &superclass = data.superclass;
        const std::string &owner      = data.ownerhandle;

        const auto genCtor = [&](ClassCommand &d, auto &parent, bool insert = false) {
            MemberContext ctx{ .ns = ns };
            ctx.insertSuperclassVar = insert;
            MemberResolverCtor resolver{ *this, d, ctx };

            if (!resolver.hasDependencies) {
                // std::cout << "ctor skipped: class " << data.name << ", p: " <<
                // parent->type() << ", s: " << superclass << '\n';
                return;
            }

            resolver.generate(out.sPublic, outputFuncsRAII);
        };

        for (auto &m : const_cast<Handle &>(data).ctorCmds) {
            //        if (cfg.dbg.methodTags) {
            //            out.sPublic += "    /*ctor: " + m.name.original + " */\n";
            //        }

            const auto &parent = *m.src->_params.begin()->get();
            if (!data.isSubclass && parent.original.type() != superclass.original) {
                genCtor(m, parent, true);
            }

            if (parent.isHandle()) {
                genCtor(m, parent);
            }
        }

        {
            if (cfg.dbg.methodTags) {
                out.sPublic << "    /*handle constructor*/\n";
            }

            std::string        parent = strFirstLower(superclass);
            std::string        handle = strFirstLower(data.name);
            InitializerBuilder init("        ");

            init.append(data.vkhandle.identifier(), handle);
            if (data.ownerRaii) {
                init.append(data.ownerRaii->identifier(), "&" + parent);
            }

            std::string argDecl;
            std::string argDef;
            std::string dispatcherInit;
            if (!data.isSubclass) {
                if ((cfg.gen.raii.staticInstancePFN && data.name == "Instance") /*|| !cfg.gen.dispatchTableAsUnique*/) {
                    dispatcherInit = "    m_dispatcher = " + data.name;
                    dispatcherInit += "Dispatcher( " + parent;
                    dispatcherInit += ".getDispatcher()->vkGet" + data.name + "ProcAddr, " + handle + " );\n";
                } else if ((cfg.gen.raii.staticDevicePFN && data.name == "Device") /*|| !cfg.gen.dispatchTableAsUnique*/) {
                    dispatcherInit = "    m_dispatcher = " + data.name;
                    dispatcherInit += "Dispatcher( " + parent;
                    dispatcherInit += ".getDispatcher()->vkGet" + data.name + "ProcAddr, " + handle + " );\n";
                } else {
                    dispatcherInit = "    m_dispatcher.reset( new " + data.name;
                    dispatcherInit += "Dispatcher( " + parent;
                    dispatcherInit += ".getDispatcher()->vkGet" + data.name + "ProcAddr, " + handle + " ) );\n";
                }
            }
            if (data.secondOwner) {
                std::string id = strFirstLower(data.secondOwner->type());
                argDecl += vkgen::format(", Vk{0} {1}", data.secondOwner->type(), id);
                argDef += vkgen::format(", Vk{0} {1}", data.secondOwner->type(), id);
                init.append(data.secondOwner->identifier(), id);
            }
            if (cfg.gen.allocatorParam && data.creationCat != Handle::CreationCategory::NONE) {
                argDecl += ", " + m_ns + "::Optional<const " + m_ns + "::AllocationCallbacks> allocator = nullptr";
                argDef += ", " + m_ns + "::Optional<const " + m_ns + "::AllocationCallbacks> allocator";
                init.append(cvars.raiiAllocator.identifier(), "static_cast<const " + m_ns + "::AllocationCallbacks *>( allocator )");
            }
            out.sPublic << vkgen::format(R"(
    VULKAN_HPP_INLINE {0}( {5}::{1} const & {2},
      Vk{0} {3}{4} );
)",
                                         data.name,
                                         superclass,
                                         parent,
                                         handle,
                                         argDecl,
                                         m_ns_raii);

            outputFuncsRAII.def.add(
              data,
              [&](auto &output) {
                  output << vkgen::format(
                    "  {0}::{0}( {6}::{1} const & {2}, Vk{0} {3}{4} ){5}\n", data.name, superclass, parent, handle, argDef, init.string(), m_ns_raii);
                  output << "  {\n";
                  output << dispatcherInit;
                  output << "  }\n";
              });
        }

    }

    void Generator::generateUniqueClassStr(OutputBuffer &output, const Handle &data, bool inlineFuncs) {
        if (!data.dtorCmd) {
            std::cerr << "class has no destructor info!" << '\n';
            return;
        }

        bool const         isSubclass = data.isSubclass;
        const std::string &base       = data.name;
        const std::string &handle     = data.vkhandle.identifier();
        const std::string &superclass = data.superclass;
        OutputClass     out{
            .name = "Unique" + base
        };
        const auto &className  = out.name;

        ClassCommand             d{ this, &data, *data.dtorCmd };
        MemberContext            ctx{ .ns = Namespace::VK, .inUnique = true };
        MemberResolverUniqueCtor r{ *this, d, ctx };

        bool hasAllocation = false;
        for (VariableData &p : r.cmd->params) {
            p.setIgnoreFlag(true);
            p.setIgnoreProto(true);
            if (p.original.type() == "VkAllocationCallbacks") {
                hasAllocation = true;
            }
        }

        std::string       destroyCall;
        std::string const destroyMethod = data.creationCat == Handle::CreationCategory::CREATE ? "destroy" : "free";

        if (data.ownerUnique) {
            destroyCall = data.ownerUnique->identifier() + "." + destroyMethod + "(";
            if (data.secondOwner) {
                destroyCall += data.secondOwner->identifier() + ", ";
            }
            destroyCall += "static_cast<" + base + ">(*this)";
            if (cfg.gen.allocatorParam && hasAllocation) {
                destroyCall += ", " + cvars.uniqueAllocator.identifier();
            }
            if (cfg.gen.dispatchParam) {
                destroyCall += ", *" + cvars.uniqueDispatch.identifier();
            }
            destroyCall += ");";
        } else {
            destroyCall = base + "::" + destroyMethod + "(";
            std::string args;
            if (cfg.gen.allocatorParam && hasAllocation) {
                args += cvars.uniqueAllocator.identifier();
            }
            if (cfg.gen.dispatchParam) {
                if (!args.empty()) {
                    args += ", ";
                }
                args += "*" + cvars.uniqueDispatch.identifier();
            }
            destroyCall += args;
            destroyCall += ");";
        }

        auto &var = r.addVar(r.cmd->params.begin());
        var.setFullType("", base, " const &");
        var.setIdentifier("value");

        ctx.generateInline = true;
        for (VariableData &p : r.cmd->params) {
            if (p.original.type() == data.name.original) {
                p.setIgnoreFlag(true);
                p.setIgnoreProto(true);
            } else {
                p.setIgnoreFlag(false);
                p.setIgnoreProto(false);
            }
            if (p.isHandle()) {
                p.setConst(true);
                p.convertToReference();
            }
        }

        std::string const pass;
        out.inherits = "public " + base;

        InitializerBuilder copyCtor("        ");
        copyCtor.append(base, "other.release()");

        std::string assignemntOp;
        data.foreachVars(VariableData::Flags::CLASS_VAR_UNIQUE, [&](const VariableData &v) {
            //        if (v.type() == data.name) {
            //            return;
            //        }
            out.sPrivate << "    " << v.toClassVar(*this);
            assignemntOp += "\n      " + v.identifier() + " = std::move(other." + v.identifier() + ");";
            copyCtor.append(v.identifier(), "std::move(other." + v.identifier() + ")");
        });

        out.sPublic << "    " << className << "() = default;\n";

        r.generate(out.sPublic, outputFuncs);

        out.sPublic << vkgen::format(R"(
    {0}({0} const &) = delete;

    {0}({0} && other) VULKAN_HPP_NOEXCEPT{2}
    {{
    }}

    ~{0}() VULKAN_HPP_NOEXCEPT {{
      if ({1}) {{
        this->destroy();
      }}
    }}

    {0}& operator=({0} const&) = delete;

)",
                                     className,
                                     handle,
                                     copyCtor.string());

        out.sPublic << vkgen::format(R"(
    {0}& operator=({0} && other) VULKAN_HPP_NOEXCEPT {{
      reset(other.release());{1}
      return *this;
    }}
)",
                                     className,
                                     assignemntOp);

        out.sPublic << vkgen::format(R"(

    explicit operator bool() const VULKAN_HPP_NOEXCEPT {{
      return {1}::operator bool();
    }}

    {1} const * operator->() const VULKAN_HPP_NOEXCEPT {{
      return this;
    }}

    {1} * operator->() VULKAN_HPP_NOEXCEPT {{
      return this;
    }}

    {1} const & operator*() const VULKAN_HPP_NOEXCEPT {{
      return *this;
    }}

    {1} & operator*() VULKAN_HPP_NOEXCEPT {{
      return *this;
    }}

    const {1}& get() const VULKAN_HPP_NOEXCEPT {{
      return *this;
    }}

    {1}& get() VULKAN_HPP_NOEXCEPT {{
      return *this;
    }}

    void reset({1} const &value = {1}()) {{
      if ({2} != static_cast<Vk{1}>(value) ) {{
        if ({2}) {{
          {3}
        }}
        {2} = value;
      }}
    }}

    {1} release() VULKAN_HPP_NOEXCEPT {{
      {1} value = *this;
      {2} = nullptr;
      return value;
    }}

    void destroy() {{
      {3}
      {2} = nullptr;
    }}

    void swap({0} &rhs) VULKAN_HPP_NOEXCEPT {{
      std::swap(*this, rhs);
    }}

)",
                                     className,
                                     base,
                                     handle,
                                     destroyCall);

        output << std::move(out);

        output << vkgen::format(R"(
  VULKAN_HPP_INLINE void swap({0} &lhs, {0} &rhs) VULKAN_HPP_NOEXCEPT {{
    lhs.swap(rhs);
  }}

)",
                                className);
    }

    void Generator::generateUniqueClass(OutputBuffer &output, const Handle &data) {
        genPlatform(output, data, [&](auto &output) { generateUniqueClassStr(output, data, false); });
    }

    void Generator::generateClassTypeInfo(const Handle &h, OutputBuffer &output, OutputClass &out) {
        std::string debugReportValue;
        bool unknown = false;
        {
            auto en = enums.find("VkDebugReportObjectTypeEXT");
            if (en) {
                auto it = en->find("e" + out.name);
                if (it) {
                    debugReportValue = cfg.gen.globalMode? it->name.original : it->name;
                }
            }
            if (debugReportValue.empty()) {
                unknown = true;
                // TODO cpp
                debugReportValue = cfg.gen.globalMode? "VK_DEBUG_REPORT_OBJECT_TYPE_UNKNOWN_EXT" : "eUnknown";
            }
        }

        out.sPublic << vkgen::format(R"(
    using CType      = Vk{0};
    using NativeType = Vk{0};
)",
                                     out.name);


        if (!cfg.gen.globalMode) {
            out.sPublic << vkgen::format(R"(
    static {3} {2}::ObjectType objectType =
      {2}::ObjectType::{0};
    static {3} {2}::DebugReportObjectTypeEXT debugReportObjectType =
      {2}::DebugReportObjectTypeEXT::{1};
)",
            h.objType,
            debugReportValue,
            m_ns,
            cfg.gen.expandMacros? "constexpr" : "VULKAN_HPP_CONST_OR_CONSTEXPR");
        }
        else {
            out.sPublic << vkgen::format(R"(
    static {0} VkObjectType objectType = {1};
    static {0} VkDebugReportObjectTypeEXT debugReportObjectType = {2};
)",
                cfg.gen.expandMacros? "constexpr" : "VULKAN_HPP_CONST_OR_CONSTEXPR",
                h.objType.original,
                debugReportValue
            );
        }

        if (!cfg.gen.globalMode) {
            gen(output, cfg.gen.handleTemplates, [&](auto &output) {
                output << vkgen::format(R"(
  template <>
  struct CppType<{0}::ObjectType, {0}::ObjectType::e{1}>
  {{
    using Type = {0}::{1};
  }};

)",
                                        m_ns,
                                        out.name);

                if (!unknown) {
                    output << vkgen::format(R"(
  template <>
  struct CppType<{0}::DebugReportObjectTypeEXT,
                 {0}::DebugReportObjectTypeEXT::{2}>
  {{
    using Type = {0}::{1};
  }};

)",
                                            m_ns,
                                            out.name,
                                            debugReportValue);
                }

                output << vkgen::format(R"(
  template <>
  struct isVulkanHandleType<{0}::{1}>
  {{
    static VULKAN_HPP_CONST_OR_CONSTEXPR bool value = true;
  }};
)",
                                        m_ns,
                                        out.name);
            });
        }

    }

    void Generator::generateClass(OutputBuffer &output, const Handle &data, bool inlineFuncs, bool noFuncs) {
        OutputClass out {
            .name = data.name
        };
        const auto &className      = out.name;
        const std::string &classNameLower = strFirstLower(className);
        const std::string &handle         = data.vkhandle.identifier();
        // const std::string &superclass     = data.superclass;

        // std::cout << "Gen class: " << className << '\n';
        // std::cout << "  ->superclass: " << superclass << '\n';

        generateClassTypeInfo(data, output, out);

        generateClassConstructors(data, out);

        generateDestroyMember(data, out.sPublic, outputFuncs);

        out.sProtected << "    ";
        out.sProtected << data.name.original << " " << handle;
        if (data.isSubclass || !cfg.gen.globalMode) {
            out.sProtected << " = {}";
        }
        out.sProtected << ";\n";

        out.sPublic << vkgen::format(R"(
    operator Vk{0}() const {{
      return {2};
    }}

    explicit operator bool() const VULKAN_HPP_NOEXCEPT {{
      return {2} != VK_NULL_HANDLE;
    }}

    bool operator!() const VULKAN_HPP_NOEXCEPT {{
      return {2} == VK_NULL_HANDLE;
    }}

#if defined( VULKAN_HPP_TYPESAFE_CONVERSION )
    {0} & operator=( Vk{0} {1} ) VULKAN_HPP_NOEXCEPT
    {{
      {2} = {1};
      return *this;
    }}
#endif

    {0} & operator=( std::nullptr_t ) VULKAN_HPP_NOEXCEPT
    {{
      {2} = {{}};
      return *this;
    }}

{3}
#   if defined( VULKAN_HPP_HAS_SPACESHIP_OPERATOR )
    auto operator<=>( {0} const & ) const = default;
#  else
    bool operator==( {0} const & rhs ) const VULKAN_HPP_NOEXCEPT
    {{
      return {2} == rhs.{2};
    }}

    bool operator!=( {0} const & rhs ) const VULKAN_HPP_NOEXCEPT
    {{
      return {2} != rhs.{2};
    }}

    bool operator<( {0} const & rhs ) const VULKAN_HPP_NOEXCEPT
    {{
      return {2} < rhs.{2};
    }}
#  endif
{4}
)",
                                     className,
                                     classNameLower,
                                     handle,
                                     expIfndef("VULKAN_HPP_EXPERIMENTAL_NO_CLASS_COMPARE"),
                                     expEndif("VULKAN_HPP_EXPERIMENTAL_NO_CLASS_COMPARE"));

        noFuncs = cfg.gen.globalMode;
        if (!noFuncs) {
            if (cfg.gen.expApi || cfg.gen.globalMode) {
                for (ClassCommand &m : const_cast<Handle &>(data).members) {
                    MemberGenerator g{ *this, m, out.sPublic, outputFuncs };
                    g.generate();
                }
            } else {
                generateClassMembers(data, out, outputFuncs, Namespace::VK, inlineFuncs);
            }
        }

        output << std::move(out);
    }

    void Generator::generateClassRAII(OutputBuffer &output, const Handle &data, bool asUnique) {
        std::string className = data.name;
        if (asUnique) {
            className += "Unique";
        }
        OutputClass out {
            .name = className
        };

        const std::string &classNameLower = strFirstLower(className);
        const std::string &handle         = data.vkhandle.identifier();
        const auto        &superclass     = data.superclass;
        const std::string &owner          = data.ownerhandle;
        const bool         exp            = cfg.gen.expApi;

        {
            std::string debugReportValue = "Unknown";
            auto        en               = enums.find("VkDebugReportObjectTypeEXT");
            if (en && en->containsValue("e" + className)) {
                debugReportValue = className;
            }

            out.sPublic << vkgen::format(R"(
    using CType      = Vk{1};

    static VULKAN_HPP_CONST_OR_CONSTEXPR {0}::ObjectType objectType =
      {0}::ObjectType::e{1};
    static VULKAN_HPP_CONST_OR_CONSTEXPR {0}::DebugReportObjectTypeEXT debugReportObjectType =
      {0}::DebugReportObjectTypeEXT::e{2};

)",
                                         m_ns,
                                         className,
                                         debugReportValue);
        }

        generateClassConstructorsRAII(data, out);

        InitializerBuilder init("        ");
        std::string        assign = "\n";

        data.foreachVars(VariableData::Flags::CLASS_VAR_RAII, [&](const VariableData &v) {
            if (v.identifier() == "m_dispatcher") {
                if (exp) {
                    return;
                }
                init.append(v.identifier(), vkgen::format("rhs.{0}.release()", v.identifier()));
                assign += vkgen::format("        {0}.reset( rhs.{0}.release() );\n", v.identifier());
            } else {
                init.append(v.identifier(), vkgen::format("{0}::exchange(rhs.{1}, {{}})", m_ns_raii, v.identifier()));
                assign += vkgen::format("        {1} = {0}::exchange(rhs.{1}, {{}});\n", m_ns_raii, v.identifier());
            }
        });

        out.sPublic << "    explicit " << data.name << "(std::nullptr_t) VULKAN_HPP_NOEXCEPT {}\n";

        out.sPublic << vkgen::format(R"(
    ~{0}() {{
        clear();
    }}

    {0}() = default;
    {0}({0} const&) = delete;
    {0}({0}&& rhs) VULKAN_HPP_NOEXCEPT {1}
    {{}}
    {0}& operator=({0} const &) = delete;
    {0}& operator=({0}&& rhs) VULKAN_HPP_NOEXCEPT {{
        if ( this != &rhs ) {{
            clear();
        }}{2}
        return *this;
    }}
    )",
                                     data.name,
                                     init.string(),
                                     assign);

        std::string release;

        data.foreachVars(VariableData::Flags::CLASS_VAR_RAII, [&](const VariableData &v) {
            if (exp && v.identifier() == "m_dispatcher") {
                return;
            }
            out.sPrivate << "    " << v.toClassVar(*this);
            if (v.identifier() != handle) {
                release += "      " + v.identifier() + " = nullptr;\n";
            }
        });

        out.sPublic << vkgen::format(R"(
    {0}::{2} const &operator*() const VULKAN_HPP_NOEXCEPT {{
        return {3};
    }}
    void clear() VULKAN_HPP_NOEXCEPT;
    void swap({1}::{2} &) VULKAN_HPP_NOEXCEPT;
)",
                                     m_ns,
                                     m_ns_raii,
                                     className,
                                     handle
                                    );

        std::string releaseType = (cfg.gen.expApi && !data.isSubclass) ? "Vk" + className : m_ns + "::" + className;
        out.sPublic << vkgen::format(R"(

    {0} release()
    {{
{2}      return {4}::{5}{{{1}::exchange( {3}, nullptr )}};
    }}
)",
                                     releaseType,
                                     m_ns_raii,
                                     release,
                                     handle,
                                     m_ns,
                                     className);

        if (!exp) {
            outputFuncsRAII.def.add(
              data,
              [&](auto &output) {
                  std::string dispatchSrc;
                  std::string type = data.name;
                  if (cfg.gen.raii.staticInstancePFN && data.name.original == "VkInstance") {
                      dispatchSrc = "      return &" + m_ns_raii + "::Instance::m_dispatcher;\n";
                      out.sPublic << "    inline static " << type << "Dispatcher m_dispatcher;\n";
                  } else if (cfg.gen.raii.staticDevicePFN && data.name.original == "VkDevice") {
                      dispatchSrc = "      return &" + m_ns_raii + "::Device::m_dispatcher;\n";
                      out.sPublic << "    inline static " << type << "Dispatcher m_dispatcher;\n";
                  } else if (data.name.original == "VkInstance" || data.name.original == "VkDevice") {
                      dispatchSrc = "      return &*m_dispatcher;\n";
                  } else {
                      type = superclass;
                      if (data.ownerRaii) {
                          dispatchSrc = "      return " + data.ownerRaii->identifier() + "->getDispatcher();\n";
                      }
                  }

                  std::string spec;
                  if (!cfg.gen.cppModules) {
                      spec = cfg.macro.mInline.get() + " ";
                  }
                  out.sPublic << vkgen::format(R"(
    {2}{0}::{1}Dispatcher const * getDispatcher() const;
    )",
                                               m_ns_raii,
                                               type,
                                               spec);

                  output << vkgen::format(R"(
    {4}{0}::{1}Dispatcher const * {2}::getDispatcher() const
    {{
      //VULKAN_HPP_ASSERT( m_dispatcher->getVkHeaderVersion() == VK_HEADER_VERSION );
{3}
    }}
    )",
                                          m_ns_raii,
                                          type,
                                          data.name,
                                          dispatchSrc,
                                          spec);
              });
        }

        if (data.ownerRaii) {
            out.sPublic << vkgen::format(R"(
    VULKAN_HPP_INLINE {0}::{1} const & get{1}() const
    {{
      return *{2};
    }}
)",
                                         m_ns_raii,
                                         data.ownerRaii->type(),
                                         data.ownerRaii->identifier());
        }

        if (!exp) {
            // generateClassMembers(data, out, outputFuncsRAII, Namespace::RAII);
        }

        output << std::move(out);

        if (!exp && !data.vectorCmds.empty()) {
            OutputClass out {
                .name = className + "s"
            };
            const auto &name = out.name;

            out.inherits += vkgen::format("public std::vector<{0}::{1}>", m_ns_raii, className);

            int passed = 0;
            for (const auto &m : data.vectorCmds) {
                if (m.src->_params.empty()) {
                    std::cerr << "RAII vector constructor: no params" << '\n';
                    continue;
                }

                MemberContext ctx{ .ns = Namespace::RAII };
                const auto   &parent = *m.src->_params.begin()->get();
                if (parent.original.type() != superclass.original) {
                    ctx.insertSuperclassVar = true;
                }

                MemberResolverVectorCtor r{ *this, const_cast<ClassCommand &>(m), ctx };
                if (!r.hasDependencies) {
                    std::cout << "vector ctor skipped: class " << data.name << ", p: " << parent.type() << ", s: " << superclass << '\n';
                    continue;
                }
                r.generate(out.sPublic, outputFuncsRAII);
                passed++;
            }

            if (passed > 0) {
                out.sPublic << vkgen::format(R"(
    {0}( std::nullptr_t ) {{}}

    {0}()                          = delete;
    {0}( {0} const & )             = delete;
    {0}( {0} && rhs )              = default;
    {0} & operator=( {0} const & ) = delete;
    {0} & operator=( {0} && rhs )  = default;
)",
                                             name);

                output << std::move(out);
            } else {
                std::cout << "no suitable constructors for class: " << data.name << '\n';
            }
        }
    }

    void Generator::generateClassesRAII(OutputBuffer &output, bool exp) {
        for (const Handle &h : handles.ordered) {
            genOptional(output, h, [&](auto &output) { generateClassRAII(output, h, exp); });
        }
    }

    std::string Generator::genMacro(const Macro &m) {
        std::string out;
        out += vkgen::format(R"(
#if !defined( {0} )
#  define {0} {1}
#endif
)",
                             m.define,
                             m.value);
        return out;
    }

    std::string Generator::beginNamespace(bool noExport) const {
        return beginNamespace(cfg.macro.mNamespace, noExport);
    }

    std::string Generator::beginNamespaceRAII(bool noExport) const {
        return beginNamespace(cfg.macro.mNamespaceRAII, noExport);
    }

    std::string Generator::beginNamespace(const Macro &ns, bool noExport) const {
        std::string output;
        return output + "namespace " + ns.getDefine() + " {\n";
    }

    std::string Generator::beginNamespaceStandalone() const {
        return cfg.gen.splitFiles? beginNamespace(cfg.macro.mNamespace) : "";
    }

    std::string Generator::endNamespace() const {
        return endNamespace(cfg.macro.mNamespace);
    }

    std::string Generator::endNamespaceStandalone() const {
        return cfg.gen.splitFiles? endNamespace(cfg.macro.mNamespace) : "";
    }


    std::string Generator::endNamespaceRAII() const {
        return endNamespace(cfg.macro.mNamespaceRAII);
    }

    std::string Generator::endNamespace(const Macro &ns) const {
        return "}  // namespace " + ns.getDefine() + "\n";
    }

    Generator::Generator()
      : VulkanRegistry(*this)
      , cvars{ ClassVariables{ .raiiAllocator        = VariableData{ VariableDataInfo{ .prefix     = "const ",
                                                                                       .vktype     = "VkAllocationCallbacks",
                                                                                       .suffix     = " *",
                                                                                       .identifier = "m_allocator",
                                                                                       .assigment  = " = {}",
                                                                                       .ns         = Namespace::VK,
                                                                                       .flag       = VariableData::Flags::CLASS_VAR_RAII } },
                               .raiiInstanceDispatch = VariableData{ VariableDataInfo{ .stdtype    = "std::unique_ptr<InstanceDispatcher>",
                                                                                       .identifier = "m_dispatcher",
                                                                                       .ns         = Namespace::NONE,
                                                                                       .flag       = VariableData::Flags::CLASS_VAR_RAII } },
                               .raiiDeviceDispatch   = VariableData{ VariableDataInfo{ .stdtype    = "std::unique_ptr<DeviceDispatcher>",
                                                                                       .identifier = "m_dispatcher",
                                                                                       .ns         = Namespace::NONE,
                                                                                       .flag       = VariableData::Flags::CLASS_VAR_RAII } },
                               .uniqueAllocator      = VariableData{ VariableDataInfo{ .prefix     = "const ",
                                                                                       .vktype     = "VkAllocationCallbacks",
                                                                                       .suffix     = " *",
                                                                                       .identifier = "m_allocator",
                                                                                       .assigment  = " = {}",
                                                                                       .ns         = Namespace::VK,
                                                                                       .flag       = VariableData::Flags::CLASS_VAR_UNIQUE } },
                               .uniqueDispatch       = VariableData{ VariableDataInfo{ .prefix      = "const ",
                                                                                       .vktype      = "VkDispatch",
                                                                                       .suffix      = " *",
                                                                                       .identifier  = "m_dispatch",
                                                                                       .assigment   = " = {}",
                                                                                       .ns          = Namespace::NONE,
                                                                                       .flag        = VariableData::Flags::CLASS_VAR_UNIQUE,
                                                                                       .specialType = VariableData::TYPE_DISPATCH } } } } {
        unload();

    }

    std::string_view Generator::getNamespace(Namespace ns) const {
        using enum vkgen::Namespace;
        switch (ns) {
            case VK: return m_ns;
            case RAII: return m_ns_raii;
            case STD: return "std";
            default: return "";
        }
    }

    void Generator::resetConfig() {
        cfg.reset();
    }

    void Generator::loadConfigPreset() {
        resetConfig();
    }

    void Generator::setOutputPath(const std::string &path) {
        outputFilePath = path;
        // if (isOuputPathValid()) {
        //     std::string filename = std::filesystem::path(path).filename().string();
        //     filename             = camelToSnake(filename);
        // }
    }

    bool Generator::load(const std::string &xmlPath) {
        auto start   = std::chrono::system_clock::now();
        auto result  = VulkanRegistry::load(*this, xmlPath);
        if (result) {
            auto end     = std::chrono::system_clock::now();
            auto elapsed = std::chrono::duration<double>(end - start);
            std::cout << "registry loaded in " << elapsed.count() << "s\n";
        }
        return result;
    }

    void Generator::generate() {
        const bool expand = cfg.gen.expandMacros;
        const auto getMacro = [&](const Macro &m) {
            return expand? m.value : m.get();
        };

        m_ns      = getMacro(cfg.macro.mNamespace);
        m_ns_raii = m_ns + "::" + getMacro(cfg.macro.mNamespaceRAII);
        m_constexpr   = getMacro(cfg.macro.mConstexpr);
        m_constexpr14 = getMacro(cfg.macro.mConstexpr14);
        m_inline = getMacro(cfg.macro.mInline);
        m_explicit = getMacro(cfg.macro.mExplicit);
        m_nodiscard = expand? "[[nodiscard]]" : "VULKAN_HPP_NODISCARD";
        m_noexcept = getMacro(cfg.macro.mNoexcept);

        if (cfg.gen.cppStd >= 20) {
            m_cast        = "std::bit_cast";
        } else {
            m_cast        = "reinterpret_cast";
        }

        cfg.gen.expApi = cfg.gen.globalMode;

        auto start = std::chrono::system_clock::now();

        std::string p = outputFilePath;
        if (outputFilePath.empty()) {
            p = ".";
        }

        std::filesystem::path const path = std::filesystem::absolute(p + "\\");
        const auto vulkanPath = std::filesystem::absolute(p + "\\vulkan\\");
        const auto videoPath = std::filesystem::absolute(p + "\\vk_video\\");

        std::cout << "generating to: " << path << '\n';
        createPath(path);
        createPath(vulkanPath);
        createPath(videoPath);

        // TODO check existing files?

        outputToStringDecl.clear();
        outputToStringDef.clear();
        outputFuncs.clear();
        outputFuncsRAII.clear();

        const auto &contextClassName = cfg.gen.contextClassName.data;
        if (contextClassName.empty()) {
            throw std::runtime_error{ "Context class name is not valid" };
        }
        loader.name          = String(contextClassName);
        loader.name.original = "Vk" + contextClassName;
        loader.prepare(*this);
        for (auto &h : handles) {
            h.prepare(*this);
        }

        cvars.uniqueDispatch.setType(cfg.macro.mDispatchType.get());

        generateApiVideo(videoPath);
        generateApiC(vulkanPath);
        generateApiCpp(vulkanPath);

        auto end     = std::chrono::system_clock::now();
        auto elapsed = std::chrono::duration<double>(end - start);
        std::cout << "generated in " << elapsed.count() << "s" << std::endl;
    }

    void Generator::saveConfigFile(const std::string &filename, bool verbose) {
        if (!isLoaded()) {
            return;
        }
        cfg.save(*this, filename, verbose);
    }

    void Generator::loadConfigFile(const std::string &filename) {
        if (!isLoaded()) {
            throw std::runtime_error("Can't load config: registry is not loaded");
        }
        cfg.load(*this, filename);
    }

} // namespace vkgen