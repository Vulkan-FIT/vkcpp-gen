#include "Generator.hpp"

#include <fstream>
#include "Instrumentation.h"

static constexpr char const *RES_HEADER{
    R"(
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

#if !defined( VULKAN_HPP_ASSERT_ON_RESULT )
#  define VULKAN_HPP_ASSERT_ON_RESULT VULKAN_HPP_ASSERT
#endif

#if !defined( VULKAN_HPP_STATIC_ASSERT )
#  define VULKAN_HPP_STATIC_ASSERT static_assert
#endif

static_assert(VK_HEADER_VERSION == {0}, "Wrong VK_HEADER_VERSION!");

// 32-bit vulkan is not typesafe for handles, so don't allow copy constructors on this platform by default.
// To enable this feature on 32-bit platforms please define VULKAN_HPP_TYPESAFE_CONVERSION
#if ( VK_USE_64_BIT_PTR_DEFINES == 1 )
#  if !defined( VULKAN_HPP_TYPESAFE_CONVERSION )
#    define VULKAN_HPP_TYPESAFE_CONVERSION
#  endif
#endif

#if !defined( __has_include )
#  define __has_include( x ) false
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
)"};

static constexpr char const *RES_ERRORS{R"(
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
    SystemError( int ev, std::error_category const & ecat, std::string const & what ) : Error(), std::system_error( ev, ecat, what ) {}
    SystemError( int ev, std::error_category const & ecat, char const * what ) : Error(), std::system_error( ev, ecat, what ) {}

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
  }
)"};

static constexpr char const *RES_RESULT_VALUE{R"(
  template <typename T>
  void ignore( T const & ) VULKAN_HPP_NOEXCEPT
  {
  }

  template <typename T>
  struct ResultValue
  {
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

  VULKAN_HPP_INLINE void resultCheck( Result result, char const * message )
  {
#ifdef VULKAN_HPP_NO_EXCEPTIONS
    ignore( result );  // just in case VULKAN_HPP_ASSERT_ON_RESULT is empty
    ignore( message );
    VULKAN_HPP_ASSERT_ON_RESULT( result == Result::eSuccess );
#else
    if ( result != Result::eSuccess )
    {
      throwResultException( result, message );
    }
#endif
  }

  VULKAN_HPP_INLINE void resultCheck( Result result, char const * message, std::initializer_list<Result> successCodes )
  {
#ifdef VULKAN_HPP_NO_EXCEPTIONS
    ignore( result );  // just in case VULKAN_HPP_ASSERT_ON_RESULT is empty
    ignore( message );
    ignore( successCodes );  // just in case VULKAN_HPP_ASSERT_ON_RESULT is empty
    VULKAN_HPP_ASSERT_ON_RESULT( std::find( successCodes.begin(), successCodes.end(), result ) != successCodes.end() );
#else
    if ( std::find( successCodes.begin(), successCodes.end(), result ) == successCodes.end() )
    {
      throwResultException( result, message );
    }
#endif
  }
)"};

static constexpr char const *RES_ARRAY_PROXY{R"(
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

static constexpr char const *RES_ARRAY_WRAPPER{R"(
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
)"};

static constexpr char const *RES_BASE_TYPES{R"(
  //==================
  //=== BASE TYPEs ===
  //==================

  using Bool32          = uint32_t;
  using DeviceAddress   = uint64_t;
  using DeviceSize      = uint64_t;
  using RemoteAddressNV = void *;
  using SampleMask      = uint32_t;
)"};

static constexpr char const *RES_FLAGS{R"(
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
};
)"};

static constexpr char const *RES_OPTIONAL{R"(
  template <typename RefType>
  class Optional {
  public:
    Optional( RefType & reference ) {NOEXCEPT}
    {
      m_ptr = &reference;
    }
    Optional( RefType * ptr ) {NOEXCEPT}
    {
      m_ptr = ptr;
    }
    Optional( std::nullptr_t ) {NOEXCEPT}
    {
      m_ptr = nullptr;
    }

    operator RefType *() const {NOEXCEPT}
    {
      return m_ptr;
    }
    RefType const * operator->() const {NOEXCEPT}
    {
      return m_ptr;
    }
    explicit operator bool() const {NOEXCEPT}
    {
      return !!m_ptr;
    }

  private:
    RefType * m_ptr;
  };
)"};

static constexpr char const *RES_RAII{R"(
    template <class T, class U = T>
    VULKAN_HPP_CONSTEXPR_14 {INLINE} T exchange( T & obj, U && newValue ) {
#  if ( 14 <= VULKAN_HPP_CPP_VERSION )
      return std::exchange<T>( obj, std::forward<U>( newValue ) );
#  else
      T oldValue = std::move( obj );
      obj        = std::forward<U>( newValue );
      return oldValue;
#  endif
    }
)"};

// static constexpr char const *RES_FLAGS{R"()"};


static std::string
regex_replace(const std::string &input, const std::regex &regex,
              std::function<std::string(std::smatch const &match)> format) {

    std::string output;
    std::sregex_iterator it(input.begin(), input.end(), regex);
    static const std::sregex_iterator end = std::sregex_iterator{};
    while (it != end) {
        auto temp = it;
        output += temp->prefix();
        output += format(*temp);
        it++;
        if (it == end) {
            output += temp->suffix();
            break;
        }
    }
    return output;
}

void Generator::bindVars(Variables &vars) const {
    const auto findVar = [&](const std::string &id) {
        for (auto &p : vars) {
            if (p->original.identifier() == id) {
                return p;
            }
        }
        std::cerr << "can't find param (" + id + ")" << std::endl;
        return std::make_shared<VariableData>(*this,
                                              VariableData::TYPE_INVALID);
    };

    for (auto &p : vars) {
        p->getArrayVars().clear();
//        if (!p->getArrayVars().empty()) {
//            std::cerr << "not clean" << std::endl;
//        }
    }

    for (auto &p : vars) {
        const std::string &len = p->getLenAttribIdentifier();
        if (!len.empty() && p->getAltlenAttrib().empty()) {
            const auto &var = findVar(len);
            if (!var->isInvalid()) {
                p->bindLengthVar(var);
                var->bindArrayVar(p);
            }
        }
    }
}

bool Generator::isStructOrUnion(const std::string &name) const {
    return structs.contains(name);
    // return structs.find(name) != structs.end();
}

void Generator::parsePlatforms(XMLNode *node) {
    if (verbose)
        std::cout << "Parsing platforms" << std::endl;
    // iterate contents of <platforms>, filter only <platform> children
    for (XMLElement *platform : Elements(node) | ValueFilter("platform")) {
        const char *name = platform->Attribute("name");
        const char *protect = platform->Attribute("protect");
        if (name && protect) {
            platforms.add(PlatformData{name, protect, defaultWhitelistOption});
        }
    }
    if (verbose)
        std::cout << "Parsing platforms done" << std::endl;
}

void Generator::parseFeature(XMLNode *node) {
    if (verbose)
        std::cout << "Parsing feature" << std::endl;
    const char *name = node->ToElement()->Attribute("name");
    if (name) {
        for (XMLElement *require : Elements(node) | ValueFilter("require")) {
            for (XMLElement &entry : Elements(require)) {
                const std::string_view value = entry.Value();
                if (value == "enum") {
                    parseEnumExtend(entry, nullptr);
                }
            }
        }
    }
    if (verbose)
        std::cout << "Parsing feature done" << std::endl;
}

void Generator::parseExtensions(XMLNode *node) {
    if (verbose)
        std::cout << "Parsing extensions" << std::endl;

    // iterate contents of <extensions>, filter only <extension> children
    for (XMLElement *extension : Elements(node) | ValueFilter("extension")) {
        const auto name = getRequiredAttrib(extension, "name");

        bool supported = true;
        if (getAttrib(extension, "supported") == "disabled") {
            supported = false;
        }

        // std::cout << "Extension: " << name << std::endl;

//        const char *protect = nullptr;
//        ExtensionData *ext = nullptr;
        if (supported) {
            PlatformData* platform = nullptr;
            const auto platformAttrib = getAttrib(extension, "platform");
            if (platformAttrib.has_value()) {
                auto it = platforms.find(platformAttrib.value());
                if (it != platforms.end()) {
                    platform = &(*it);
             //       protect = platform->protect.data();
                } else {
                    std::cerr << "Warn: Unknown platform in extensions: "
                              << platformAttrib.value() << std::endl;
                }
            }

            bool enabled = supported && defaultWhitelistOption;
            // std::cout << "add ext: " << name << std::endl;
            extensions.add(ExtensionData{std::string{name}, platform, supported, enabled});
//            std::cout << "ext: " << ext << std::endl;
//            std::cout << "> n: " << ext->name.original << std::endl;
        }
    }


    for (XMLElement *extension : Elements(node) | ValueFilter("extension")) {
        const auto name = getRequiredAttrib(extension, "name");

        ExtensionData *ext = nullptr;
        auto it = extensions.find(name);
        if (it != extensions.end()) {
            ext = &(*it);
        }

        // iterate contents of <extension>, filter only <require> children
        for (XMLElement *require :
             Elements(extension) | ValueFilter("require")) {
            // iterate contents of <require>
            // std::cout << "  <require>" << std::endl;
            for (XMLElement &entry : Elements(require)) {
                const std::string_view value = entry.Value();
                // std::cout << "    <" << value << ">" << std::endl;

                if (value == "command") {
                    const char *name = entry.Attribute("name");
                    if (!name) {
                        std::cerr
                            << "Error: extension bind: command has no name"
                            << std::endl;
                    }
                    auto command = commands.find(name);
                    if (command != commands.end()) {
                        if (!ext) {
                            command->setUnsuppored();
                        }
                        else {
                            if (!isInContainter(ext->commands, command.base())) {
                               ext->commands.push_back(command.base());
                            }
                            command->ext = ext;
                        }
                    } else {
                        std::cerr
                            << "Error: extension bind: can't find command: "
                            << name << std::endl;
                    }

                } else if (value == "type" /*&& ext && !ext->protect.empty()*/) {
                    const char *nameAttrib = entry.Attribute("name");
                    if (!nameAttrib) {
                        std::cerr << "Error: extension bind: type has no name"
                                  << std::endl;
                        continue;
                    }
                    std::string name = nameAttrib;
                    if (!name.starts_with("Vk")) {
                        continue;
                    }

                    auto type = findType(name);
                    if (!type) {
                        name = std::regex_replace(name, std::regex("Flags"), "FlagBits");
                        type = findType(name);
                    }
                    if (type) {
                        if (!ext) {
                            type->setUnsuppored();
                        }
                        else {
                            if (!isInContainter(ext->types, type)) {
                                ext->types.push_back(type);
                            }
                            type->ext = ext;
                        }
                    }
                    else {
                        if (!ext)
                            std::cerr << "type not found in extension: " << name << std::endl;
                    }
                } else if (value == "enum" && ext) {
                    parseEnumExtend(entry, ext);
                }
            }
        }
    }
    if (verbose)
        std::cout << "Parsing extensions done" << std::endl;
}

void Generator::parseTags(XMLNode *node) {
    if (verbose)
        std::cout << "Parsing tags" << std::endl;
    // iterate contents of <tags>, filter only <tag> children
    for (XMLElement *tag : Elements(node) | ValueFilter("tag")) {
        const char *name = tag->Attribute("name");
        if (name) {
            tags.emplace(name);
        }
    }
    if (verbose)
        std::cout << "Parsing tags done" << std::endl;
}

std::string Generator::genWithProtect(const std::string &code, const std::string &protect) const {
    std::string output;
    if (code.empty()) {
        return output;
    }
    if (!protect.empty()) {
        output += "#if defined(" + protect + ")\n";
    }
    output += code;
    if (!protect.empty()) {
        output += "#endif //" + protect + "\n";
    }
    return output;
}

std::pair<std::string, std::string> Generator::genCodeAndProtect(const BaseType &type, std::function<void (std::string &)> function, bool bypass) const {
    if (!type.canGenerate() && !bypass) {
        return std::make_pair("", "");
    }
    std::string output;
    std::string protect = std::string{type.getProtect()};
    function(output);
    return std::make_pair(output, protect);
}

std::string
Generator::genOptional(const BaseType &type,
                       std::function<void(std::string &)> function, bool bypass) const {    

    auto [code, protect] = genCodeAndProtect(type, function, bypass);
    return genWithProtect(code, protect);
}

std::string Generator::strRemoveTag(std::string &str) const {
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

std::string Generator::strWithoutTag(const std::string &str) const {
    std::string out = str;
    for (const std::string &tag : tags) {
        if (out.ends_with(tag)) {
            out.erase(out.size() - tag.size());
            break;
        }
    }
    return out;
}

std::pair<std::string, std::string>
Generator::snakeToCamelPair(std::string str) const {
    std::string suffix = strRemoveTag(str);
    std::string out = convertSnakeToCamel(str);

    out = std::regex_replace(out, std::regex("bit"), "Bit");
    out = std::regex_replace(out, std::regex("Rgba10x6"), "Rgba10X6");
    out = std::regex_replace(out, std::regex("1d"), "1D");
    out = std::regex_replace(out, std::regex("2d"), "2D");
    out = std::regex_replace(out, std::regex("3d"), "3D");
    if (out.size() >= 2) {
        for (int i = 0; i < out.size() - 1; i++) {
            char c = out[i];
            bool cond = c == 'r' || c == 'g' || c == 'b' || c == 'a';
            if (cond && std::isdigit(out[i + 1])) {
                out[i] = std::toupper(c);
            }
        }
    }

    return std::make_pair(out, suffix);
}

std::string Generator::snakeToCamel(const std::string &str) const {
    const auto &p = snakeToCamelPair(str);
    return p.first + p.second;
}

std::string Generator::enumConvertCamel(const std::string &enumName,
                                        std::string value,
                                        bool isBitmask) const {

    std::string enumSnake = enumName;
    std::string tag = strRemoveTag(enumSnake);
    if (!tag.empty()) {
        tag = "_" + tag;
    }
    enumSnake = camelToSnake(enumSnake);

    strStripPrefix(value, "VK_");

    const auto &tokens = split(enumSnake, "_");

    for (auto it = tokens.begin(); it != tokens.end(); it++) {
        std::string token = *it + "_";
        if (!value.starts_with(token)) {
            break;
        }
        value.erase(0, token.size());
    }

    if (value.ends_with(tag)) {
        value.erase(value.size() - tag.size());
    }

    for (auto it = tokens.rbegin(); it != tokens.rend(); it++) {
        std::string token = "_" + *it;
        if (!value.ends_with(token)) {
            break;
        }
        value.erase(value.size() - token.size());
    }

    value = strFirstUpper(snakeToCamel(value));
    if (isBitmask) {
        strStripSuffix(value, "Bit");
    }
    return "e" + value;
}

std::string Generator::genNamespaceMacro(const Macro &m) {
    std::string output = genMacro(m);
    if (m.usesDefine) {
        output += format("#define {0}_STRING  VULKAN_HPP_STRINGIFY({1})\n",
                         m.define, m.value);
    } else {
        output += format("#define {0}_STRING  \"{1}\"\n", m.define, m.value);
    }
    return output;
}

std::string Generator::generateHeader() {
    std::string output;
    if (cfg.gen.cppModules) {
        output += "module;\n";
    }

    output += format(R"(
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
)");

    output += "#include <vulkan/vulkan.h>\n";

    if (cfg.gen.cppModules) {
        output += format(R"(

#if !defined( VULKAN_HPP_ENABLE_DYNAMIC_LOADER_TOOL )
#  define VULKAN_HPP_ENABLE_DYNAMIC_LOADER_TOOL 1
#endif
#if VULKAN_HPP_ENABLE_DYNAMIC_LOADER_TOOL == 1
#  if defined( __unix__ ) || defined( __APPLE__ ) || defined( __QNXNTO__ ) || defined( __Fuchsia__ )
#    include <dlfcn.h>
#  endif
#endif
)"          );
    }

    output += format(R"(
#define VULKAN_HPP_STRINGIFY2( text ) #text
#define VULKAN_HPP_STRINGIFY( text )  VULKAN_HPP_STRINGIFY2( text )
)"      );
    output += genNamespaceMacro(cfg.macro.mNamespace);    

    if (cfg.gen.cppModules) {
        output += format("export module {NAMESPACE};\n");

        output += format(RES_HEADER, headerVersion);      

        output += format(R"(
import <string>;
import <vector>;
// import <sstream>;
import <system_error>;
import <utility>;

#if !defined( VULKAN_HPP_ASSERT )
import <cassert>;
#  define VULKAN_HPP_ASSERT assert
#endif

#if 17 <= VULKAN_HPP_CPP_VERSION
import <string_view>;
#endif

#if defined( VULKAN_HPP_DISABLE_ENHANCED_MODE )
#  if !defined( VULKAN_HPP_NO_SMART_HANDLE )
#    define VULKAN_HPP_NO_SMART_HANDLE
#  endif
#else
import <memory>;
import <vector>;
#endif

#if ( 201711 <= __cpp_impl_three_way_comparison ) && __has_include( <compare> ) && !defined( VULKAN_HPP_NO_SPACESHIP_OPERATOR )
#  define VULKAN_HPP_HAS_SPACESHIP_OPERATOR
#endif
#if defined( VULKAN_HPP_HAS_SPACESHIP_OPERATOR )
import <compare>;
#endif
/*
#if ( 201803 <= __cpp_lib_span )
#  define VULKAN_HPP_SUPPORT_SPAN
import <span>;
#endif
*/
#if VULKAN_HPP_ENABLE_DYNAMIC_LOADER_TOOL == 1
#  if defined( _WIN32 )
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

)"      );
    }
    else {


        output += format(RES_HEADER, headerVersion);
        output += format(R"(
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

#if !defined( VULKAN_HPP_ASSERT )
#  include <cassert>
#  define VULKAN_HPP_ASSERT assert
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

)"      );
    }

    output += "\n";
    return output;
}

void Generator::generateFiles(std::filesystem::path path) {

    std::string prefix = "vulkan20";
    std::string ext = ".hpp";
    if (cfg.gen.cppModules) {
        ext = ".ixx";
    }

    platformOutput.clear();
    for (const auto &p : platforms) {
        if (p.protect.empty()) {
            std::cerr << ">>> platform with no protect" <<  std::endl;
            continue;
        }
        platformOutput.segments.emplace(std::string{p.protect}, FileOutput("_" + p.name));
    }

    const auto getFileNameProtect = [&](const std::string &name){
        std::string out;
        for (const auto & c : name) {
            if (c == '.') {
                out += '_';
            }
            else {
                out += std::toupper(c);
            }
        }
        return out;
    };

    const auto gen = [&](std::string suffix, std::string protect,
                         const std::string &content,
                         Namespace ns = Namespace::VK) {
        std::string filename = prefix + suffix + ext;
        std::string output;
        if (protect.empty()) {
            protect = getFileNameProtect(filename);
        }
        if (!cfg.gen.cppModules) {
            output += "#ifndef " + protect + "\n";
            output += "#define " + protect + "\n";
        }
        if (ns == Namespace::NONE) {
            output += content;
        } else {
            output += beginNamespace(ns);
            output += content;
            output += endNamespace(ns);
        }
        if (!cfg.gen.cppModules) {
            output += "#endif\n";
        }

        std::ofstream file;

        auto p = path.replace_filename(filename);
        file.open(p, std::ios::out | std::ios::trunc);
        if (!file.is_open()) {
            throw std::runtime_error("Can't open file: " + outputFilePath);
        }

        file << output;

        file.flush();
        file.close();

        std::cout << "Generated: " << p << std::endl;
    };

    GenOutput out = {
        .enums = generateEnums(),
        .handles = generateHandles(),
        .structs = generateStructs(),
        .raii = generateRAII()
    };

    auto main = generateMainFile(out);

    if (!cfg.gen.cppModules) {
        gen("_enums", "VULKAN20_ENUMS_HPP", out.enums);
        gen("_handles", "VULKAN20_HANDLES_HPP", out.handles);
        gen("_structs", "VULKAN20_STRUCTS_HPP", out.structs);
        gen("_funcs", "VULKAN20_FUNCS_HPP", outputFuncs.get());
        gen("_raii", "VULKAN20_RAII_HPP", out.raii, Namespace::NONE);
        gen("_raii_funcs", "VULKAN20_RAII_FUNCS_HPP", outputFuncsRAII.get(), Namespace::RAII);

        for (const auto &k : platformOutput.segments) {
            const std::string &suffix = k.second.filename;
            const std::string &content = k.second.content;
            if (content.empty()) {
                continue;
            }
            gen(suffix, "", content, Namespace::VK);
        }

    }

    gen("", "VULKAN20_HPP", main, Namespace::NONE);

//    if (!cfg.gen.cppModules) {
//        ext = ".cpp";
//        gen("", "VULKAN20_HPP", generateModuleImpl(), Namespace::NONE);
//    }
}

std::string Generator::generateMainFile(GenOutput &g) {
    std::string output;
    output += generateHeader();

    output += beginNamespace(Namespace::VK);

    output += format(RES_ARRAY_PROXY);
    // PROXY TEMPORARIES
    output += format(RES_ARRAY_WRAPPER);
    output += format(RES_FLAGS);
    output += format(RES_OPTIONAL);
    // STRUCTURE CHAINS
    // UNIQUE HANDLE
    if (!cfg.gen.cppModules) {
        output += generateDispatch();
    }
    output += format(RES_BASE_TYPES);

    if (!cfg.gen.cppModules) {
        output += endNamespace(Namespace::VK);
        output += "#include \"vulkan20_enums.hpp\"\n";
    }
    else {
        output += g.enums;
    }

    output += generateErrorClasses();
    output += "\n";
    output += format(RES_RESULT_VALUE);

    if (!cfg.gen.cppModules) {
        output += endNamespace(Namespace::VK);
        output += "#include \"vulkan20_handles.hpp\"\n";
        output += "#include \"vulkan20_structs.hpp\"\n";
        output += "#include \"vulkan20_funcs.hpp\"\n";
        // platforms
        for (const auto &k : platformOutput.segments) {
            const std::string &protect = k.first;
            const std::string &suffix = k.second.filename;
            const std::string &content = k.second.content;
            if (content.empty()) {
                continue;
            }
            output += "#if defined( " + protect + " )\n";
            output += "#include \"vulkan20_" + suffix + ".hpp\"\n";
            output += "#endif\n";
        }

        // out += beginNamespace(Namespace::VK);
        // STRUCT EXTENDS
        // DYNAMIC LOADER
        // out += endNamespace(Namespace::VK);

#ifdef INST
        output += Inst::mainFileEnd();
#endif

    }
    else {
        output += g.handles;
        output += g.structs;
        output += outputFuncs.get();
        output += endNamespace(Namespace::VK);

        output += g.raii;
        output += beginNamespace(Namespace::RAII);
        output += outputFuncsRAII.get();
        output += endNamespace(Namespace::RAII);
    }
    return output;
}

std::string Generator::generateModuleImpl() {
    std::string out;
    // TODO decide if use at all
    return out;
}

Generator::Variables
Generator::parseStructMembers(XMLElement *node, std::string &structType,
                              std::string &structTypeValue) {
    Variables members;
    // iterate contents of <type>, filter only <member> children
    for (XMLElement *member : Elements(node) | ValueFilter("member")) {

        XMLVariableParser parser{member, *this}; // parse <member> element

        std::string type = parser.type();
        std::string name = parser.identifier();

        if (const char *values = member->ToElement()->Attribute("values")) {
            std::string value = enumConvertCamel(type, values);
            parser.setAssignment(" = " + type + "::" + value);
            if (parser.original.type() ==
                "VkStructureType") { // save sType information for structType
                structType = type;
                structTypeValue = value;
            }
        }

        members.push_back(std::make_shared<VariableData>(parser));
    }
    bindVars(members);
    return members;
}

void Generator::parseEnumExtend(XMLElement &node, ExtensionData *ext) {
    const char *extends = node.Attribute("extends");
    // const char *bitpos = node.Attribute("bitpos");
    const char *value = node.Attribute("name");
    const char *alias = node.Attribute("alias");

    if (extends && value) {
        bool dbg = false;
        if (std::string{extends} == "VkImageCreateFlagBits") {
            // dbg = true;
        }

        auto it = enums.find(extends, dbg);
        if (it != enums.end()) {
            std::string cpp = enumConvertCamel(it->name, value,
                                               it->isBitmask);
            if (!it->containsValue(cpp)) {
                String v{cpp};
                v.original = value;
                EnumValue data{v, alias ? true : false};
                data.ext = ext;
                it->members.push_back(data);
            }

        } else {
            std::cerr << "err: Cant find enum: " << extends << std::endl;
        }
    }
}

std::string Generator::generateEnumStr(const EnumData &data) {
    std::string output;
    const std::string &name = data.name;

    UnorderedOutput<> members{*this};
    UnorderedOutput<> to_string{*this};

    for (const auto &m : data.members) {
        members.add(m, [&](std::string &output) {
            output += "    " + m.name + " = " + m.name.original + ",\n";
        });
        if (!m.isAlias) {
            to_string.add(m, [&](std::string &output) {
                std::string value = m.name;
                strStripPrefix(value, "e");
                output += "      case " + name + "::" + m.name +
                          ": return \"" + value + "\";\n";
            });
        }
    }
    output += "  enum class " + name;
    if (data.isBitmask) {
        std::string base = std::regex_replace(data.name.original, std::regex("FlagBits"), "Flags");
        output += " : " + base;
    }

    output += " {\n";
    // strStripSuffix(output, ",\n");
    output += members.get();
    output += "\n  };\n";

    for (const auto &a : data.aliases) {
        output += "  using " + a + " = " + name + ";\n";
    }

    std::string str = to_string.get();
    if (str.empty()) {
        output += format(R"(
{INLINE} std::string to_string({0} value) {
return {1};
}
)",
                         name, "\"(void)\"");
    } else {
        str += format(R"(
  default: return "invalid (" + {NAMESPACE}::toHexString(static_cast<uint32_t>(value)) + {0};)", "\" )\"");

        output += format(R"(
{INLINE} std::string to_string({0} value) {
switch (value) {
{1}
}
}
)",
                         name, str);
    }

    if (data.isBitmask) {
        output += genFlagTraits(data, name);
    }
    return output;
}

std::string Generator::generateEnum(const EnumData &data) {
    auto p = data.getProtect();
    if (!p.empty()) {
        // std::cout << "enum with p: " << p << std::endl;
        platformOutput.add(data, [&](std::string &output) {
            output += generateEnumStr(data);
        });
        return "";
    }
    else {
        return genOptional(data, [&](std::string &output) {
            output += generateEnumStr(data);
        });
    }
}

std::string Generator::generateEnums() {
    std::string output;    

    output += format(R"(
  template <typename EnumType, EnumType value>
  struct CppType
  {};

  template <typename Type>
  struct isVulkanHandleType
  {
  static VULKAN_HPP_CONST_OR_CONSTEXPR bool value = false;
  };

)");

    if (cfg.gen.cppModules) {
        output += format(R"(
  {INLINE} std::string toHexString(uint32_t value) {
      return "TODO";
  }

)");
    }
    else {
    output += format(R"(
  {INLINE} std::string toHexString(uint32_t value) {
      std::stringstream stream;
      stream << std::hex << value;
      return stream.str();
  }

)");
    }

    std::unordered_set<std::string> generated;
    for (const auto &e : enums) {        
        if (generated.contains(e.name)) {
            continue;
        }
//        output += "  // " + e.first + ", " + e.second.name.original +
//         (e.second.isBitmask ? "  Bitmask" : "") +"\n";
        output += generateEnum(e);
        generated.insert(e.name);
    }
    return output;
}

std::string Generator::genFlagTraits(const EnumData &data,
                                     std::string inherit) {
    std::string output;

    std::string name =
        std::regex_replace(data.name, std::regex("FlagBits"), "Flags");

    std::string flags = "";
    std::string str;

    for (size_t i = 0; i < data.members.size(); ++i) {
        const auto &m = data.members[i];
        if (m.isAlias) {
            continue;
        }
        flags += genOptional(m, [&](std::string &output) {
            output += "        | VkFlags(" + inherit + "::" + m.name + ")\n";
        });
        str += genOptional(m, [&](std::string &output) {
            std::string value = m.name;
            strStripPrefix(value, "e");
            output += format(R"(
    if (value & {0}::{1})
      result += "{2} | ";
)",
                             inherit, m.name, value);
        });
    }
    strStripPrefix(flags, "        | ");
    if (flags.empty()) {
        flags += "0\n";
    }

    output += format(R"(
  using {0} = Flags<{1}>;
)",
                     name, inherit);

    if (str.empty()) {
        output += format(R"(
  {INLINE} std::string to_string({0} value) {
    return "{}";
  }
)",
                         name, inherit, str);
    } else {
        output += format(R"(
  {INLINE} std::string to_string({0} value) {
    if ( !value )
      return "{}";

    std::string result;
    {2}

    return "{ " + result.substr( 0, result.size() - 3 ) + " }";
  }
)",
                         name, inherit, str);
    }

    output += format(R"(

  template <>
  struct FlagTraits<{1}> {
    enum : VkFlags {
      allFlags = {2}
    };
  };

)",
                     name, inherit, flags);

    if (data.members.empty()) {
        return output;
    }

    output += format(R"(
  {INLINE} {CONSTEXPR} {0} operator|({1} bit0, {1} bit1) {NOEXCEPT} {
    return {0}( bit0 ) | bit1;
  }

  {INLINE} {CONSTEXPR} {0} operator&({1} bit0, {1} bit1) {NOEXCEPT} {
    return {0}( bit0 ) & bit1;
  }

  {INLINE} {CONSTEXPR} {0} operator^({1} bit0, {1} bit1) {NOEXCEPT} {
    return {0}( bit0 ) ^ bit1;
  }

  {INLINE} {CONSTEXPR} {0} operator~({1} bits) {NOEXCEPT} {
    return ~( {0}( bits ) );
  }

)",
                     name, inherit);

    return output;
}

std::string Generator::generateDispatch() {
    std::string output;
    output += generateDispatchLoaderBase();
    output += generateDispatchLoaderStatic();
    output += format(R"(
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

#if !defined( VULKAN_HPP_DEFAULT_DISPATCHER )
#  if VULKAN_HPP_DISPATCH_LOADER_DYNAMIC == 1
#    define VULKAN_HPP_DEFAULT_DISPATCHER ::{NAMESPACE}::defaultDispatchLoaderDynamic
#    define VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE                     \
      namespace {NAMESPACE}                                                        \
      {                                                                            \
        VULKAN_HPP_STORAGE_API DispatchLoaderDynamic defaultDispatchLoaderDynamic; \
      }
  extern VULKAN_HPP_STORAGE_API DispatchLoaderDynamic defaultDispatchLoaderDynamic;
#  else
  static inline ::{NAMESPACE}::DispatchLoaderStatic & getDispatchLoaderStatic()
  {
    static ::{NAMESPACE}::DispatchLoaderStatic dls;
    return dls;
  }
#    define VULKAN_HPP_DEFAULT_DISPATCHER ::{NAMESPACE}::getDispatchLoaderStatic()
#    define VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE
#  endif
#endif

#if !defined( VULKAN_HPP_DEFAULT_DISPATCHER_TYPE )
#  if VULKAN_HPP_DISPATCH_LOADER_DYNAMIC == 1
#    define VULKAN_HPP_DEFAULT_DISPATCHER_TYPE ::{NAMESPACE}::DispatchLoaderDynamic
#  else
#    define VULKAN_HPP_DEFAULT_DISPATCHER_TYPE ::{NAMESPACE}::DispatchLoaderStatic
#  endif
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
)");
    return output;
}

std::string Generator::generateErrorClasses() {
    std::string output;
    //    output += "#ifndef VULKAN_HPP_NO_EXCEPTIONS\n";
    //    output += beginNamespace(Namespace::STD);
    //    output += format(R"(
    //  template <>
    //  struct is_error_code_enum<VULKAN_HPP_NAMESPACE::Result> : public
    //  true_type
    //  {
    //  };
    //)");
    //    output += endNamespace(Namespace::STD);
    //    output += "#endif\n";
    if (!cfg.gen.cppModules) {
        output += beginNamespace(Namespace::VK);
    }
    output += format(RES_ERRORS);

    std::string str;

    for (const auto &e : errorClasses) {
        std::string name = e->name;
        strStripPrefix(name, "eError");
        name += "Error";

        output += genOptional(*e, [&](std::string &output) {
            output += this->format(R"(
  class {0} : public SystemError
  {
  public:
    {0}( std::string const & message ) : SystemError( make_error_code( Result::{1} ), message ) {}
    {0}( char const * message ) : SystemError( make_error_code( Result::{1} ), message ) {}
  };
)",
                                   name, e->name);
        });

        str += genOptional(*e, [&](std::string &output) {
            output += "        case Result::" + e->name + ": throw " + name +
                      "(message);\n";
        });
    }

    if (!cfg.gen.cppModules) {
        output += "  namespace {\n";
    }
    output += format(R"(
    [[noreturn]] void throwResultException({NAMESPACE}::Result result, char const *message) {
      switch (result) {
{0}
        default: throw SystemError( make_error_code( result ) );
      }
    }
)",
    str);

    if (!cfg.gen.cppModules) {
        output += "  }  // namespace\n";
    }

    return output;
}

std::string Generator::generateDispatchLoaderBase() {
    std::string output;
    output += format(R"(
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

)",
                     "");
    dispatchLoaderBaseGenerated = true;
    return output;
}

std::string Generator::generateDispatchLoaderStatic() {
    std::string output;
    output += "//#if !defined( VK_NO_PROTOTYPES )\n";
    output += "  class DispatchLoaderStatic : public DispatchLoaderBase {\n";
    output += "  public:\n";

    HandleData empty{""};
    const auto genCommand = [&](const CommandData &c){
        output += genOptional(c, [&](std::string &output) {
            ClassCommandData d{this, &empty, c};
            MemberContext ctx{d, Namespace::VK};

            std::string protoArgs = ctx.createProtoArguments(true);
            std::string args = ctx.createPassArguments(true);
            std::string name = ctx.name.original;
            std::string proto = ctx.type + " " + name + "(" + protoArgs + ")";
            std::string call;
            if (ctx.pfnReturn != PFNReturnCategory::VOID) {
                call += "return ";
            }
            call += "::" + name + "(" + args + ");";

            output += format(R"(
    {0} const {NOEXCEPT} {
      {1}
    }
)",
                             proto, call);
        });
    };

    if (cfg.gen.vulkanCommands) {
        for (const auto &command : commands) {
            genCommand(command);
        }
    }
    else {
        for (const auto &command : staticCommands) {
            genCommand(command);
        }
    }

    output += "  };\n";
    output += "//#endif\n";
    return output;
}

void Generator::parseTypes(XMLNode *node) {
    if (verbose)
        std::cout << "Parsing declarations" << std::endl;

    std::vector<std::pair<std::string_view, std::string_view>> handleBuffer;
    std::vector<std::pair<std::string_view, std::string_view>> aliasedTypes;

    // iterate contents of <types>, filter only <type> children
    for (XMLElement *type : Elements(node) | ValueFilter("type")) {
        const char *cat = type->Attribute("category");
        if (!cat) {
            continue;
        }
        const char *name = type->Attribute("name");

        if (strcmp(cat, "enum") == 0) {
            if (name) {
                const char *alias = type->Attribute("alias");
                if (alias) {
                    auto it = enums.find(alias);
                    if (it == enums.end()) {
                        std::cerr
                            << "parse alias enum: can't find target: " << alias
                            << std::endl;
                    } else {
                        String n{name, true};
                        it->aliases.push_back(n);
                    }
                } else {
                    auto it = enums.find(name);
                    if (it == enums.end()) {
                        enums.add(EnumData{name});
                    }
                    else {
                        //std::cout << "add enum dupe: " << name << std::endl;
                    }
                }
            }
        } else if (strcmp(cat, "bitmask") == 0) {

            const char *nameAttrib = type->Attribute("name");
            const char *aliasAttrib = type->Attribute("alias");
            const char *reqAttrib = type->Attribute("requires");            

            std::string temp;
            if (reqAttrib) {
                nameAttrib = reqAttrib;
            }
            else if (!aliasAttrib) {
                XMLElement *nameElem = type->FirstChildElement("name");
                if (!nameElem) {
                    std::cerr << "Error: bitmap has no name" << std::endl;
                    continue;
                }
                nameAttrib = nameElem->GetText();
                temp = std::regex_replace(nameAttrib, std::regex("Flags"), "FlagBits");
                nameAttrib = temp.c_str();
            }
            if (!nameAttrib) {
                std::cerr << "Error: bitmap alias has no name" << std::endl;
                continue;
            }

            String name(nameAttrib, true);

//            std::cout << "parse bitmask. name: " << nameAttrib << " (" << name << ", " << name.original << ")";
//            if (aliasAttrib) {
//                std::cout << " alias: " << aliasAttrib;
//            }
//            if (reqAttrib) {
//                std::cout << " req: " << reqAttrib;
//            }
//            std::cout << std::endl;

            if (aliasAttrib) {
                std::string alias = std::regex_replace(aliasAttrib, std::regex("Flags"),
                                          "FlagBits");

                auto it = enums.find(alias);
                if (it == enums.end()) {
                    std::cerr << "Error: parse alias enum: can't find target "
                              << alias << " (" << nameAttrib << ")"
                              << std::endl;
                } else {
                    it->aliases.push_back(name);
                }
            } else {

                EnumData data{nameAttrib};
                data.isBitmask = true;
                enums.add(data);

            }

        } else if (strcmp(cat, "handle") == 0) {
            XMLElement *nameElem = type->FirstChildElement("name");
            if (nameElem) {
                const char *nameAttrib = nameElem->GetText();
                if (!nameAttrib || strlen(nameAttrib) == 0) {
                    std::cerr << "Missing name in handle node" << std::endl;
                    continue;
                }
                std::string name{nameAttrib};
                const char *parent = type->Attribute("parent");
                const char *alias = type->Attribute("alias");
                bool isSubclass = name != "VkInstance" && name != "VkDevice";
                HandleData d(name, isSubclass);

                if (alias) {
                    d.alias = alias;
                }
                if (parent) {
                    handleBuffer.push_back(std::make_pair(nameAttrib, parent));
                }
                //handles.emplace(nameAttrib, d);
                handles.add(d);
            }
        } else if (strcmp(cat, "struct") == 0 || strcmp(cat, "union") == 0) {
            if (name) {
                const char *alias = type->Attribute("alias");
                if (alias) {
                    aliasedTypes.push_back(std::make_pair(name, alias));
                } else {
                    StructData data;
                    auto returnedonly = getAttrib(type, "returnedonly");
                    if (returnedonly == "true") {
                        data.returnedonly = true;
                    }
                    data.name = String(name, true);
                    data.type = (strcmp(cat, "struct") == 0)
                                    ? StructData::VK_STRUCT
                                    : StructData::VK_UNION;
                    std::string structType{}, structTypeValue{}; // placeholders
                    data.members =
                        parseStructMembers(type, structType, structTypeValue);
                    data.structTypeValue = structTypeValue;

                    // auto p = &structs.emplace(name, data).first->second;
                    // structs.add(data);
                    structs.items.push_back(data);
                }
            }
        } else if (strcmp(cat, "define") == 0) {
            XMLDefineParser parser{type, *this};
            if (parser.name == "VK_HEADER_VERSION") {
                headerVersion = parser.value;
            }
        }
    }

    if (headerVersion.empty()) {
        throw std::runtime_error("header version not found.");
    }

    for (auto &a : aliasedTypes) {
        const auto &it = structs.find(a.second.data());
        if (it == structs.end()) {
            std::cout << "Error: Type has no alias target: " << a.second << " ("
                      << a.first << ")" << std::endl;
        } else {
            it->aliases.push_back(String(a.first.data(), true));
        }
    }

    std::unordered_set<std::string> visited;
    std::unordered_set<std::string> current;

    auto it = structs.begin();

    std::function<void(decltype(it) pos)> check;

    const auto findStruct = [&](decltype(it) pos, const std::string &name) {
        for (; pos != structs.end(); pos++) {
            if (pos->name == name) {
                break;
            }
        }
        return pos;
    };

    const auto reorder = [&](decltype(it) pos, std::string type) {
        if (current.contains(type)) {
            std::cout << "Error: cyclic dependency: ";
            for (const auto &c : current)
                std::cout << "  " << c << std::endl;
            std::cout << std::endl;
            throw std::runtime_error("can't reorder structs");
        }
        std::string t = pos->name;
        current.insert(t);
        auto dep = findStruct(pos, type);
//        std::cout << "Moving " << (*dep)->name << " before " << (*pos)->name
//                  << std::endl;
        auto d = *dep;
        structs.items.erase(dep);
        dep = structs.items.insert(pos, d);
        check(dep);
        current.erase(t);
    };

    check = [&](decltype(it) pos) {
        const auto &t = pos->name;
        visited.insert(t);
        for (const auto &m : pos->members) {
            if (!m->isPointer() && isStructOrUnion(m->original.type())) {        
                const auto &type = m->type();
                if (!visited.contains(type)) {
                    reorder(pos, type);
                }
            }
        }
    };

    if (verbose)
        std::cout << "Processing types dependencies" << std::endl;
    for (; it != structs.end(); ++it) {
        check(it);
    }
    if (verbose)
        std::cout << "Processing types dependencies done" << std::endl;

    for (auto &h : handleBuffer) {
        const auto &t = handles.find(h.first.data());
        const auto &p = handles.find(h.second.data());
        if (t != handles.end() && p != handles.end()) {
            t->parent = &(*p);
        }
    }

    for (auto &h : handles) {
        // initialize superclass information
        auto superclass = getHandleSuperclass(h);
        h.superclass = superclass;
        // std::cout << "Handle: " << h.second.name << ", super: " << superclass
        // << std::endl;
        if (h.isSubclass) {
            h.ownerhandle = "m_" + strFirstLower(superclass);
        }
    }

//    for (auto &h : handles) {
//        if (h.first != h.second.name.original) {
//            std::cerr << "Error: handle intergity check. " << h.first << " vs "
//                      << h.second.name.original << std::endl;
//        }
//    }

//    std::cout << "Enums {" << std::endl;
//    for (auto &e : enums) {
//        std::cout << "  @" << e.name.original << std::endl;
//    }
//    std::cout << "}" << std::endl;
    if (verbose)
        std::cout << "Parsing declarations done" << std::endl;
}

void Generator::parseEnums(XMLNode *node) {

    const char *typeptr = node->ToElement()->Attribute("type");
    if (!typeptr) {
        return;
    }

    std::string_view type{typeptr};

    bool isBitmask = type == "bitmask";

    if (isBitmask || type == "enum") {

        const char *name = node->ToElement()->Attribute("name");
        if (!name) {
            std::cerr << "Can't get name of enum" << std::endl;
            return;
        }

        auto it = enums.find(name);
        if (it == enums.end()) {
            std::cerr << "cant find " << name << "in enums" << std::endl;
            return;
        }

        auto &en = *it;
        en.isBitmask = isBitmask; // maybe unnecessary

        std::vector<std::string> aliased;

        for (XMLElement *e : Elements(node) | ValueFilter("enum")) {
            const char *value = e->Attribute("name");
            if (value) {
                const char *alias = e->Attribute("alias");
                if (alias) {
                    const char *comment = e->Attribute("comment");
                    if (!comment) {
                        aliased.push_back(value);
                    }
                    continue;
                }
                std::string cpp = enumConvertCamel(en.name, value, isBitmask);
                String v{cpp};
                v.original = value;

                en.members.emplace_back(v);
            }
        }

        for (const auto &a : aliased) {
            std::string cpp = enumConvertCamel(en.name, a, isBitmask);
            if (!en.containsValue(cpp)) {
                String v{cpp};
                v.original = a;
                en.members.emplace_back(v, true);
            }
        }

    }
}

std::string Generator::generateStructDecl(const StructData &d) {
    return genOptional(d, [&](std::string &output) {
        std::string cppname = strStripVk(d.name.original); // TODO

        if (d.type == StructData::VK_STRUCT) {
            output += "  struct " + cppname + ";\n";
        } else {
            output += "  union " + cppname + ";\n";
        }

        for (auto &a : d.aliases) {
            output += "  using " + strStripVk(a) + " = " + cppname + ";\n";
        }
    });
}

std::string Generator::generateClassDecl(const HandleData &data,
                                         bool allowUnique) {
    return genOptional(data, [&](std::string &output) {
        output += "  class " + data.name + ";\n";
        if (allowUnique && data.uniqueVariant()) {
            output += "  class Unique" + data.name + ";\n";
        }
    });
}

std::string Generator::generateClassString(const std::string &className,
                                           const GenOutputClass &from) {
    std::string output = "  class " + className;
    if (!from.inherits.empty()) {
        output += " : " + from.inherits;
    }
    output += " {\n";

    const auto addSection = [&](const std::string &visibility,
                                const std::string &s) {
        if (!s.empty()) {
            output += "  " + visibility + ":\n" + s;
        }
    };

    addSection("public", from.sPublic.get());
    addSection("private", from.sPrivate.get());
    addSection("protected", from.sProtected.get());
    output += "  };\n";
    return output;
}

std::string Generator::generateHandles() {
    std::string output;
    for (auto &e : handles) {
        output += generateClassDecl(e, true);
    }
    for (auto &e : structs) {
        output += generateStructDecl(e);
    }

    if (!cfg.gen.cppModules) {
        HandleData empty{""};
        GenOutputClass out{*this};
        for (auto &c : staticCommands) {
            ClassCommandData d{this, &empty, c};
            MemberContext ctx{d, Namespace::VK};
            ctx.isStatic = true;
            generateClassMember(ctx, out, outputFuncs);
        }
        output += out.sPublic.get();
    }

    for (auto &e : handles) {
        output += generateClass(e, outputFuncs);
    }
    if (!cfg.gen.cppModules) {
        if (cfg.gen.smartHandles) {
            for (auto &e : handles) {
                if (e.uniqueVariant()) {
                    output += generateUniqueClass(e, outputFuncs);
                }
            }
        }
    }

    return output;
}

std::string Generator::generateStructs() {
    UnorderedOutput output{*this};
    // std::string output;
    for (auto &e : structs) {
        // output += generateStruct(e);
        auto prot = e.getProtect();
        if (!prot.empty()) {
            platformOutput.add(e, [&](std::string &output){
                output += generateStruct(e);
            });
        }
        else {
            output.add(e, [&](std::string &output){
                output += generateStruct(e);
            });
        }
    }
    return output.get();
}

std::string Generator::generateStruct(const StructData &data) {
    std::string output;
    //return genOptional(data, [&](std::string &output) {
        // std::cout << "gen struct: " << data.name << std::endl;
        std::string members;
        std::string funcs;
        std::string structureType;
        if (!data.structTypeValue.empty()) {
            structureType = "StructureType::" + data.structTypeValue;
        }

        std::vector<VariableData*> ctor;
        for (const auto &m : data.members) {
            if (data.type == StructData::VK_STRUCT) {            
                std::string assignment = "{}";
                if (m->original.type() == "VkStructureType") {
                    assignment = structureType.empty()
                                     ?
                                     /*cfg.macro.mNamespace.get() + "::"*/
                                     "StructureType::eApplicationInfo"
                                     : structureType;
                }
                else if (cfg.gen.structConstructor) {
                    ctor.push_back(&*m);
                }
                m->setAssignment(" = " + assignment);
                members += "    " + m->toStructStringWithAssignment() + ";\n";
            } else {
                members += "    " + m->toStructString() + ";\n";
            }
            if (m->hasArrayLength()) {
                m->setSpecialType(VariableData::TYPE_ARRAY);
            }
        }
        // setters
        if (!data.returnedonly && cfg.gen.structSetters) {
            for (const auto &m : data.members) {
                if (m->type() == "StructureType") {
                    continue;
                }
                auto var = std::make_shared<VariableData>(*m.get());
                var->setIdentifier(m->identifier() + "_");

                std::string id = m->identifier();
                std::string name = strFirstUpper(id);
                std::string arg = var->toString();
                funcs += format(R"(
    {CONSTEXPR_14} {0}& set{1}({2}) {NOEXCEPT} {
      {3} = {4};
      return *this;
    }
            )"          , data.name, name, arg, id, var->identifier() );
            }
        }

        bool hasArrayMember = false;
        // arrayproxy setters
        if (!data.returnedonly && cfg.gen.structSettersProxy) {
            for (const auto &m : data.members) {
                if (m->hasLengthVar()) {
                    hasArrayMember = true;
                    auto var = std::make_shared<VariableData>(*m.get());
                    std::string name = var->identifier();
                    // name conversion
                    if (name.size() >= 3 && name.starts_with("pp") &&
                        std::isupper(name[2])) {
                        name = name.substr(1);
                    } else if (name.size() >= 2 && name.starts_with("p") &&
                               std::isupper(name[1])) {
                        name = name.substr(1);
                    }
                    name = strFirstUpper(name);

                    var->setIdentifier(m->identifier() + "_");
                    var->removeLastAsterisk();

                    std::string id = m->identifier();
                    std::string modif;
                    if (var->original.type() == "void" && !var->original.isConstSuffix()) {
                        funcs += "    template <typename DataType>\n";
                        var->setType("DataType");
                        modif = " * sizeof(DataType)";
                    }                                       

                    std::string arg = "ArrayProxyNoTemporaries<" + var->fullType() + "> const &" + var->identifier();
                    funcs += format(R"(
    {0}& set{1}({2}) {NOEXCEPT} {
      {4} = static_cast<uint32_t>({3}.size(){6});
      {5} = {3}.data();
      return *this;
    }
                )"          , data.name, name, arg, var->identifier(), var->getLengthVar()->identifier(), m->identifier(), modif);
                }
            }
        }
        // not used anymore
        //  if (cfg.gen.structNoinit) {
        //        std::string alt = "Noinit";
        //        output += "    struct " + alt + " {\n";
        //        for (const auto &l : lines) {
        //            output += "      " + l.text;
        //            if (!l.assignment.empty()) {
        //                output += ";";
        //            }
        //            output += "\n";
        //        }
        //        output += "      " + alt + "() = default;\n";
        //        output += "      operator " + name +
        //                  " const&() const { return *std::bit_cast<const " +
        //                  name +
        //                  "*>(this); }\n";
        //        output += "      operator " + name + " &() { return
        //        *std::bit_cast<" +
        //                  name + "*>(this); }\n";
        //        output += "    };\n";
        //    }
        output += "  " + data.getType() + " " + data.name + " {\n";
        output += "    using NativeType = " + data.name.original + ";\n";

        if (data.type == StructData::VK_STRUCT) {

            if (!structureType.empty()) {
                output += "    static const bool                               "
                          "   allowDuplicate = false;\n";
                output += "    static VULKAN_HPP_CONST_OR_CONSTEXPR "
                          "StructureType structureType = " +
                          structureType + ";\n";
            }
            // constructor
            if (cfg.gen.structConstructor) {
                HandleData cls{""};

                MemberContext ctx{*this, &cls, data, Namespace::VK, true};
                ctx.disableDispatch = true;
                MemberResolverStructCtor resolver{ctx, false};
                output += "\n";
                output += resolver.generateDefinition(true, true);

                // alternate constructor
                if (cfg.gen.structSettersProxy && hasArrayMember) {
                    MemberContext ctx{*this, &cls, data, Namespace::VK, true};
                    ctx.disableDispatch = true;
                    MemberResolverStructCtor resolver{ctx, true};
                    output += "\n";
                    output += resolver.generateDefinition(true, true);
                }
            }


            output += format(R"(
    {0} & operator=({0} const &rhs) {NOEXCEPT} = default;

    {0} & operator=({1} const &rhs) {NOEXCEPT} {
      *this = *reinterpret_cast<{NAMESPACE}::{0} const *>(&rhs);
      return *this;
    }
)",
                             data.name, data.name.original);
        }

        output += funcs;

        output += format(R"(
    operator {0}*() { return this; }

    explicit operator {1} const &() const {NOEXCEPT} {
      return *reinterpret_cast<const {1} *>(this);
    }

    explicit operator {1}&() {NOEXCEPT} {
      return *reinterpret_cast<{1} *>(this);
    }

)",
                         data.name, data.name.original);

        // reflect
        if (cfg.gen.structReflect) {
            ArgumentBuilder types{false};
            ArgumentBuilder tie{false};
            for (const auto &m : data.members) {
                types.append(m->fullType(), "");
                tie.append("", m->identifier());
            }
            //std::string ret = "std::tuple<" + types.string() + ">";
            output += format(R"(
#if defined( VULKAN_HPP_USE_REFLECT )
#  if 14 <= VULKAN_HPP_CPP_VERSION
    auto
#  else
    std::tuple<{0}>
#  endif
      reflect() const {NOEXCEPT}
    {
      return std::tie({1});
    }
#endif
)",         types.string(), tie.string());
        }

        std::string comp;
        for (const auto &m : data.members) {
            const std::string &id = m->identifier();
            comp += "( " + id + " == rhs." + id + ")" " && ";
        }
        strStripSuffix(comp, " && ");

        output += format(R"(
#if defined( VULKAN_HPP_HAS_SPACESHIP_OPERATOR )
    auto operator<=>( {0} const & ) const = default;
#else
    bool operator==( {0} const & rhs ) const {NOEXCEPT}
    {
#  if defined( VULKAN_HPP_USE_REFLECT )
      return this->reflect() == rhs.reflect();
#  else
      return {1};
#  endif
    }

    bool operator!=({0} const &rhs) const {NOEXCEPT} {
        return !operator==( rhs );
    }
#endif
)",                     data.name, comp);

        output += members;
        output += "  };\n";

        if (data.type == StructData::VK_STRUCT && !structureType.empty()) {
            output += format(R"(
  template <>
  struct CppType<StructureType, {0}> {
    using Type = {1};
  };
)",
                             structureType, data.name);
        }

        for (const auto &a : data.aliases) {
            output += "  using " + a + " = " + data.name + ";\n";
        }
    //});
    return output;
}

std::string Generator::generateRAII() {
    std::string output;

    output += genNamespaceMacro(cfg.macro.mNamespaceRAII);
//    if (cfg.gen.cppModules) {
//        output += format("export module {NAMESPACE_RAII};\n");
//    }
    output += beginNamespace(Namespace::RAII);
    output += "  using namespace " + cfg.macro.mNamespace->get() + ";\n";

    output += format(RES_RAII);

    // outputRAII += "  class " + loader.name + ";\n";
    for (auto &e : handles) {
        output += generateClassDecl(e, false);
    }
    output += generateLoader();

    for (auto &e : handles) {        
        output += generateClassRAII(e, outputFuncsRAII);
    }    
    output += endNamespace(Namespace::RAII);
    if (!cfg.gen.cppModules) {
        output += "#include \"vulkan20_raii_funcs.hpp\"\n";
    }
    return output;
}

void Generator::evalCommand(CommandData &cmd) const {
    std::string name = cmd.name;
    std::string tag = strWithoutTag(name);
    cmd.nameCat = evalNameCategory(name);
}

Generator::MemberNameCategory
Generator::evalNameCategory(const std::string &name) {
    if (name.starts_with("get")) {
        return MemberNameCategory::GET;
    }
    if (name.starts_with("allocate")) {
        return MemberNameCategory::ALLOCATE;
    }
    if (name.starts_with("acquire")) {
        return MemberNameCategory::ACQUIRE;
    }
    if (name.starts_with("create")) {
        return MemberNameCategory::CREATE;
    }
    if (name.starts_with("enumerate")) {
        return MemberNameCategory::ENUMERATE;
    }
    if (name.starts_with("write")) {
        return MemberNameCategory::WRITE;
    }
    if (name.starts_with("destroy")) {
        return MemberNameCategory::DESTROY;
    }
    if (name.starts_with("free")) {
        return MemberNameCategory::FREE;
    }
    return MemberNameCategory::UNKNOWN;
}

template <typename T>
void Generator::createOverload(
    MemberContext &ctx, const std::string &name,
    std::vector<std::unique_ptr<MemberResolver>> &secondary) {

    const auto getType = [](const MemberContext &ctx) {
        const auto &vars = ctx.getFilteredProtoVars();
        if (vars.empty()) {
            return std::string();
        }
        return vars.begin()->get()->original.type();
    };

    MemberContext c{ctx};
    c.name = name;
    std::string type = getType(c);
    if (type.empty() || !isHandle(type)) {
        // std::cout << ">>> drop: " << c.name.original << std::endl;
        return;
    }

    // std::cout << "Checking destroy for " << type << std::endl;
    for (const auto &g : ctx.cls->generated) {
        if (g.name == c.name) {
            std::string gtype = getType(g);
            if (type == gtype) {
                // std::cout << "  >> conflict here: " << gtype << std::endl;
                return;
            }
        }
    }
    secondary.push_back(std::make_unique<T>(c));
    secondary.rbegin()->get()->dbgtag = "overload";
}

void Generator::generateClassMember(MemberContext &ctx, GenOutputClass &out,
                                    UnorderedOutput<> &funcs, bool inlineFuncs) {
    if (ctx.ns == Namespace::VK && ctx.isRaiiOnly()) {
        return;
    }
    std::vector<std::unique_ptr<MemberResolver>> secondary;

    bool createPassOverload = true; // ctx.ns == Namespace::VK;
    const auto getPrimaryResolver = [&]() {
        std::unique_ptr<MemberResolver> resolver;
        const auto &last = ctx.getLastVar(); // last argument
        bool uniqueVariant = false;
        if (last->isHandle()) {
            const auto &handle = findHandle(last->original.type());
            uniqueVariant = handle.uniqueVariant();

            if (last->isArrayOut() && handle.vectorVariant &&
                ctx.ns == Namespace::RAII) {

                MemberContext c{ctx};
                const auto &parent = c.params.begin()->get();
                if (parent->isHandle()) {
                    const auto &handle = findHandle(parent->original.type());
                    if (handle.isSubclass) {
                        const auto &superclass = handle.superclass;
                        if (/*parent->original.type() != superclass.original &&
                             */
                            superclass.original !=
                            ctx.cls->superclass.original) {
                            std::shared_ptr<VariableData> var =
                                std::make_shared<VariableData>(*this,
                                                               superclass);                            
                            var->setConst(true);
                            c.params.insert(c.params.begin(), var);
                        }
                    }
                }

                for (const auto &p : c.params) {
                    if (p->isHandle()) {
                        p->toRAII();
                    }
                }

                resolver = std::make_unique<MemberResolverVectorRAII>(c);
                return resolver;
            }
        }

        if (ctx.pfnReturn == PFNReturnCategory::OTHER) {
            resolver = std::make_unique<MemberResolver>(ctx);
            resolver->dbgtag = "PFN return";
            return resolver;
        }

        switch (ctx.nameCat) {
        case MemberNameCategory::GET:
        case MemberNameCategory::WRITE: {
            if (ctx.containsPointerVariable()) {
                auto &&var = ctx.getLastPointerVar();
                if (var->isHandle()) {
                    createPassOverload = true;
                }
                resolver = std::make_unique<MemberResolverGet>(ctx);
                return resolver;
            }
            break;
        }
        case MemberNameCategory::ALLOCATE:
        case MemberNameCategory::CREATE:
            createPassOverload = true;
            if (ctx.ns == Namespace::VK) {
                if (uniqueVariant && !last->isArray() && cfg.gen.smartHandles) {
                    secondary.push_back(
                        std::make_unique<MemberResolverCreateUnique>(ctx));
                }
            }
            resolver = std::make_unique<MemberResolverCreate>(ctx);
            return resolver;
        case MemberNameCategory::ENUMERATE:
            resolver = std::make_unique<MemberResolverEnumerate>(ctx);
            return resolver;
        case MemberNameCategory::DESTROY:
            createOverload<MemberResolver>(ctx, "destroy", secondary);
            break;
        case MemberNameCategory::FREE:
            createOverload<MemberResolver>(ctx, "free", secondary);
            break;
        default:
            break;
        }

        if (last->isPointer() && !last->isConst() && !last->isArrayIn()) {
            resolver = std::make_unique<MemberResolverGet>(ctx);
            resolver->dbgtag = "get (default)";
            return resolver;
        }
        return std::make_unique<MemberResolver>(ctx);
    };

    const auto &resolver = getPrimaryResolver();

    const auto gen = [&](MemberResolverBase &r) {        
        bool separate = false;
        if (inlineFuncs) {
            ctx.generateInline = true;
        }
        else {
            separate = ctx.ns == Namespace::VK;
        }
        r.generate(out.sPublic, funcs, platformOutput, separate);
    };

    if (createPassOverload) {
        if (ctx.canGenerate()) {
            MemberResolverPass passResolver{ctx};
            passResolver.dbgtag = "pass";

            bool same = resolver->compareSignature(passResolver);

//            if (same) {
//                out.sPublic += "/*\n";
//                funcs += "/*\n";
//            }
            if (!same) {
                gen(passResolver);
            }
//            if (same) {
//                out.sPublic += "*/\n";
//                funcs += "*/\n";
//            }
        };
    }

    gen(*resolver);

    for (const auto &resolver : secondary) {
        gen(*resolver);
        //resolver->generate(out.sPublic, funcs);
    }
}

void Generator::generateClassMembers(HandleData &data, GenOutputClass &out,
                                     UnorderedOutput<> &funcs, Namespace ns, bool inlineFuncs) {
    std::string output;
    if (ns == Namespace::RAII) {
        const auto &className = data.name;
        const auto &handle = data.vkhandle;
        const std::string &ldr = loader.name;

        const auto &superclass = findHandle("Vk" + data.superclass);
        VariableData super(*this, superclass.name);
        super.setConst(true);

        if (data.getAddrCmd.has_value()) {
            const std::string &getAddr = data.getAddrCmd->name.original;
            out.sProtected +=
                "    PFN_" + getAddr + " m_" + getAddr + " = {};\n";

            // getProcAddr member
            out.sPublic += format(R"(
    template<typename T>
    inline T getProcAddr(const std::string_view &name) const {
      return std::bit_cast<T>(m_{0}({1}, name.data()));
    }
)",
                                  getAddr, handle);
        }

        if (data.hasPFNs()) {

            // PFN function pointers
            for (auto d : data.filteredMembers) {
                const ClassCommandData &m = *d;
                out.sProtected.add(*m.src, [&](std::string &output) {
                    const std::string &name = m.name.original;
                    output += format("    PFN_{0} m_{0} = {};\n", name);
                });
            }

            std::string declParams = super.toString();
            std::string defParams = declParams;

            if (super.type() == ldr) {
                super.setAssignment(" = libLoader");
                declParams = super.toStringWithAssignment();
            }

            out.sProtected += "  void loadPFNs(" + declParams + ");\n";

            output +=
                "  inline void " + className + "::loadPFNs(" + defParams + ") {\n";

            if (data.getAddrCmd.has_value()) {
                const std::string &getAddr = data.getAddrCmd->name.original;
                output += "    m_" + getAddr + " = " + super.identifier();
                if (super.type() == ldr) {
                    output += ".getInstanceProcAddr();\n";
                } else {
                    output += ".getProcAddr<PFN_" + getAddr + ">(\"" + getAddr +
                              "\");\n";
                }
            }

            std::string loadSrc;
            if (data.getAddrCmd->name.empty()) {
                loadSrc = super.identifier() + ".";
            }
            // function pointers initialization
            for (const ClassCommandData &m : data.members) {
                output += genOptional(*m.src, [&](std::string &output) {
                    const std::string &name = m.name.original;
                    output += format(
                        "    m_{0} = {1}getProcAddr<PFN_{0}>(\"{0}\");\n", name,
                        loadSrc);
                });
            }
            output += "  }\n";
        }

        std::string call;
        if (!data.dtorCmds.empty()) {
            std::string n = data.dtorCmds[0]->name;
            ClassCommandData d(this, &data, *data.dtorCmds[0]);
            MemberContext ctx{d, ns};
            call = "if (" + handle + ") {\n";
            auto parent = ctx.params.begin()->get();
            if (parent->isHandle() &&
                parent->original.type() != data.name.original) {
                parent->setIgnorePFN(true);
            }
            auto alloc = ctx.params.rbegin()->get();
            if (alloc->original.type() == "VkAllocationCallbacks") {
                alloc->setAltPFN("nullptr");
            }
            if (data.ownerhandle.empty()) {
                call += "      m_" + data.dtorCmds[0]->name.original + "(" +
                        ctx.createPFNArguments() + ");\n";
            } else {
                call += "      " + data.ownerhandle + "->" +
                        data.dtorCmds[0]->name + "(" +
                        ctx.createPassArguments(true) + ");\n"; // TODO config alloc var
            }
            call += "    }\n    ";
        }

        std::string clear;
        if (data.hasPFNs()) {
            clear = "\n";
            for (const auto &m : data.members) {
                clear += genOptional(*m.src, [&](std::string &output){
                    output += "    m_" + m.name.original + " = nullptr;\n";
                });
            }
        }

        if (data.ownerhandle.empty()) {
            output += format(R"(
  inline void {0}::clear() {NOEXCEPT} {
    {2}{1} = nullptr;{3}
  }

  inline void {0}::swap({NAMESPACE_RAII}::{0} &rhs) {NOEXCEPT} {
    std::swap({1}, rhs.{1});
  }
)",
                             className, handle, call, clear);
        } else {
            output += format(R"(
  inline void {0}::clear() {NOEXCEPT} {
    {3}{2} = nullptr;
    {1} = nullptr;{4}
  }

  inline void {0}::swap({NAMESPACE_RAII}::{0} &rhs) {NOEXCEPT} {
    std::swap({2}, rhs.{2});
    std::swap({1}, rhs.{1});
  }
)",
                             className, handle, data.ownerhandle, call, clear);
        }
    }

    if (!output.empty()) {
        funcs += genOptional(data, [&](std::string &out) { out += output; });
    }

    if (ns == Namespace::VK && !cfg.gen.vulkanCommands) {
        return;
    }
    // wrapper functions
    for (const ClassCommandData &m : data.members) {
        MemberContext ctx{m, ns};
        generateClassMember(ctx, out, funcs, inlineFuncs);
    }
}

void Generator::generateClassConstructors(const HandleData &data,
                                          GenOutputClass &out) {
    const std::string &superclass = data.superclass;

    out.sPublic += format(R"(
    {CONSTEXPR} {0}() = default;
    {CONSTEXPR} {0}(std::nullptr_t) {NOEXCEPT} {}

    {EXPLICIT} {0}(Vk{0} {1}) {NOEXCEPT}  : {2}({1}) {}
)",
                          data.name, strFirstLower(data.name), data.vkhandle);
}

void Generator::generateClassConstructorsRAII(const HandleData &data,
                                              GenOutputClass &out,
                                              UnorderedOutput<> &funcs) {
    static constexpr Namespace ns = Namespace::RAII;

    const auto &superclass = data.superclass;
    const std::string &owner = data.ownerhandle;

    const auto genCtor = [&](MemberContext &ctx, auto &parent) {
        MemberResolverCtor resolver{ctx};        
        bool check = resolver.checkMethod();
        if (!check) {
            // std::cout << "ctor skipped: class " << data.name << ", p: " <<
            // parent->type() << ", s: " << superclass << std::endl;
        } else {            
            resolver.generate(out.sPublic, funcs, platformOutput);
        }
    };

    for (auto &m : data.ctorCmds) {
        const auto &parent = m.src->params.begin()->get();        
        if (parent->original.type() != superclass.original) {
            MemberContext ctx{m, ns, true};

            std::shared_ptr<VariableData> var =
                std::make_shared<VariableData>(*this, superclass);
            var->setConst(true);            
            ctx.params.insert(ctx.params.begin(), var);
            // std::cout << "ctor conflict: class " << data.name << ", p: " <<
            // parent->type() << ", s: " << superclass << std::endl;

            genCtor(ctx, parent);

            if (parent->isHandle()) {
                const auto &handle = findHandle(parent->original.type());
                if (handle.superclass.original != superclass.original) {
                    std::cerr << "ctor: impossible combination" << std::endl;
                    continue;
                }
            }
        }

        MemberContext ctx{m, ns, true};
        genCtor(ctx, parent);
    }

    InitializerBuilder init("        ");
    std::string assign = "\n";

    init.append(data.vkhandle, format("{NAMESPACE_RAII}::exchange(rhs.{0}, {})", data.vkhandle));
    assign += format("        {0} = {NAMESPACE_RAII}::exchange(rhs.{0}, {});\n",  data.vkhandle);

    if (!data.ownerhandle.empty()) {
        init.append(data.ownerhandle, format("{NAMESPACE_RAII}::exchange(rhs.{0}, {})", data.ownerhandle));
        assign += format("        {0} = {NAMESPACE_RAII}::exchange(rhs.{0}, {});\n",  data.ownerhandle);
    }

    if (data.getAddrCmd.has_value()) {
        const std::string &id = "m_" + data.getAddrCmd->name.original;
        init.append(id, format("{NAMESPACE_RAII}::exchange(rhs.{0}, {})", id));
        assign += format("        {0} = {NAMESPACE_RAII}::exchange(rhs.{0}, {});\n",  id);
    }

    if (data.hasPFNs()) {
        for (const auto &m : data.members) {
            init.appendRaw(genOptional(*m.src, [&](std::string &output) {
                std::string name = "m_" + m.name.original;
                output += format("        , {0}({NAMESPACE_RAII}::exchange(rhs.{0}, {}))\n", name);
            }));

            assign += genOptional(*m.src, [&](std::string &output) {
                std::string name = "m_" + m.name.original;
                output += format("        {0} = {NAMESPACE_RAII}::exchange(rhs.{0}, {});\n", name);
            });

        }
    }

    out.sPublic += format(R"(
    {0}(std::nullptr_t) {NOEXCEPT} {}
    ~{0}() {
        clear();
    }

    {0}() = delete;
    {0}({0} const&) = delete;
    {0}({0}&& rhs) {NOEXCEPT}{1} {
    }
    {0}& operator=({0} const &) = delete;
    {0}& operator=({0}&& rhs) {NOEXCEPT} {
        if ( this != &rhs ) {
            clear();
        }{2}
        return *this;
    }
    )",
                          data.name, init.string(), assign);

    std::string loadCall;
    if (data.hasPFNs()) {
        loadCall = "\n      //loadPFNs(" + owner + ");\n    ";
    }

    if (!owner.empty()) {
        out.sPublic +=
            format(R"(
    inline {EXPLICIT} {0}(const {2} &{3}, Vk{0} {1}) {NOEXCEPT} : {5}(&{3}), {4}({1}){{6}}
)",
                   data.name, strFirstLower(data.name), superclass,
                   strFirstLower(superclass), data.vkhandle, owner, loadCall);
    } else {
        out.sPublic +=
            format(R"(
    {EXPLICIT} {0}(const {2} &{3}, Vk{0} {1}) {NOEXCEPT} : {4}({1}) {{5}}
)",
                   data.name, strFirstLower(data.name), superclass,
                   strFirstLower(superclass), data.vkhandle, loadCall);
    }
}

std::string Generator::generateUniqueClassStr(HandleData &data,
                                           UnorderedOutput<> &funcs, bool inlineFuncs) {
    std::string output;
    if (data.dtorCmds.empty()) {
        std::cout << "class has no destructor info!" << std::endl;
        return "";
    }

    const CommandData &cmd = *data.dtorCmds[0];
    ClassCommandData d{this, &data, cmd};
    MemberContext ctx{d, Namespace::VK};
    ctx.inUnique = true;

    GenOutputClass out{*this};
    const std::string &base = data.name;
    const std::string &className = "Unique" + base;
    const std::string &handle = data.vkhandle;
    const std::string &superclass = data.superclass;
    bool isSubclass = data.isSubclass;

    std::string pass;

    out.inherits = "public " + base;

    std::string destroyCall = getDispatchCall("m_dispatch->");
    destroyCall += ctx.name.original + "(" + ctx.createPFNArguments() + ");";

    InitializerBuilder copyCtor("        ");
    copyCtor.append(base, "other.release()");
    for (const auto &v : data.uniqueVars) {
        out.sPrivate += "    " + v.toString() + " = {};\n";
        copyCtor.append(v.identifier(), "std::move(other." + v.identifier() + ")");
    }

    out.sPublic += "    " + className + "() = default;\n";

    VariableData var(*this);
    var.setFullType("", base, " const &");
    var.setIdentifier("value");
    ctx.constructor = true;
    ctx.generateInline = true;
    ctx.params.insert(ctx.params.begin(), std::make_shared<VariableData>(var));
    MemberResolverUniqueCtor r{ctx};
    //out.sPublic += "/*\n";
    r.generate(out.sPublic, funcs, platformOutput);
    //out.sPublic += "*/\n";

    out.sPublic += format(R"(
{0}({0} const &) = delete;

{0}({0} && other) {NOEXCEPT}{2}
{
}

~{0}() {NOEXCEPT} {
  if ({1}) {
    this->destroy();
  }
}

{0}& operator=({0} const&) = delete;

)",
                      className, handle, copyCtor.string());

    std::string assignemntOp;
    for (const auto &v : data.uniqueVars) {
        assignemntOp += "\n      " + v.identifier() + " = std::move(other." + v.identifier() + ");";
    }

    out.sPublic += format(R"(
{0}& operator=({0} && other) {NOEXCEPT} {
  reset(other.release());{1}
  return *this;
}
)",
                      className, assignemntOp);

    out.sPublic += format(R"(

explicit operator bool() const {NOEXCEPT} {
  return {1}::operator bool();
}

{1} const * operator->() const {NOEXCEPT} {
  return this;
}

{1} * operator->() {NOEXCEPT} {
  return this;
}

{1} const & operator*() const {NOEXCEPT} {
  return *this;
}

{1} & operator*() {NOEXCEPT} {
  return *this;
}

const {1}& get() const {NOEXCEPT} {
  return *this;
}

{1}& get() {NOEXCEPT} {
  return *this;
}

void reset({1} const &value = {1}()) {
  if ({2} != static_cast<Vk{1}>(value) ) {
    if ({2}) {
      {3}
    }
    {2} = value;
  }
}

{1} release() {NOEXCEPT} {
  {1} value = *this;
  {2} = nullptr;
  return value;
}

void destroy() {
  {3}
  {2} = nullptr;
}

void swap({0} &rhs) {NOEXCEPT} {
  std::swap(*this, rhs);
}

)",
                      className, base, handle, destroyCall);

    output += generateClassString(className, out);

    output += format(R"(
{INLINE} void swap({0} &lhs, {0} &rhs) {NOEXCEPT} {
lhs.swap(rhs);
}

)",
                     className);
    return output;
}

std::string Generator::generateUniqueClass(HandleData &data,
                                           UnorderedOutput<> &funcs) {    
    std::string output;
    if (!data.getProtect().empty()) {
        // std::cout << "class " << data.name << " to " << data.getProtect() << std::endl;
        platformOutput.add(data, [&](std::string &output) {
            output += generateUniqueClassStr(data, funcs, true);
        });
    }
    else {
        output += genOptional(data, [&](std::string &output) {
            output += generateUniqueClassStr(data, funcs, false);
        });
    }
    return output;
}

String Generator::getHandleSuperclass(const HandleData &data) {
    if (!data.parent) {
        return loader.name;
    }
    if (data.name.original == "VkSwapchainKHR") {
        return findHandle("VkDevice").name;
    }
    auto *it = data.parent;
    while (it->parent) {
        if (it->name.original == "VkInstance" || it->name.original == "VkDevice") {
            break;
        }
        it = it->parent;
    }
    return it->name;
}

std::string Generator::generateClassStr(HandleData data, UnorderedOutput<> &funcs, bool inlineFuncs) {
    std::string output;
    GenOutputClass out{*this};

    const std::string &className = data.name;
    const std::string &classNameLower = strFirstLower(className);
    const std::string &handle = data.vkhandle;
    const std::string &superclass = data.superclass;

    // std::cout << "Gen class: " << className << std::endl;
    //std::cout << "  ->superclass: " << superclass << std::endl;

    std::string debugReportValue = "Unknown";
    auto en = enums.find("VkDebugReportObjectTypeEXT");
    if (en != enums.end() && en->containsValue("e" + className)) {
        debugReportValue = className;
    }

    out.sPublic += format(R"(
using CType      = Vk{0};
using NativeType = Vk{0};

static VULKAN_HPP_CONST_OR_CONSTEXPR {NAMESPACE}::ObjectType objectType =
  {NAMESPACE}::ObjectType::e{0};
static VULKAN_HPP_CONST_OR_CONSTEXPR {NAMESPACE}::DebugReportObjectTypeEXT debugReportObjectType =
  {NAMESPACE}::DebugReportObjectTypeEXT::e{1};

)",
                          className, debugReportValue);

    generateClassConstructors(data, out);

    out.sProtected += "    " + data.name.original + " " + handle + " = {};\n";

    out.sPublic += format(R"(
operator Vk{0}() const {
  return {2};
}

explicit operator bool() const {NOEXCEPT} {
  return {2} != VK_NULL_HANDLE;
}

bool operator!() const {NOEXCEPT} {
  return {2} == VK_NULL_HANDLE;
}

#if defined( VULKAN_HPP_TYPESAFE_CONVERSION )
{0} & operator=( Vk{0} {1} ) VULKAN_HPP_NOEXCEPT
{
  {2} = {1};
  return *this;
}
#endif

{0} & operator=( std::nullptr_t ) VULKAN_HPP_NOEXCEPT
{
  {2} = {};
  return *this;
}

#if defined( VULKAN_HPP_HAS_SPACESHIP_OPERATOR )
auto operator<=>( {0} const & ) const = default;
#else
bool operator==( {0} const & rhs ) const VULKAN_HPP_NOEXCEPT
{
  return {2} == rhs.{2};
}

bool operator!=( {0} const & rhs ) const VULKAN_HPP_NOEXCEPT
{
  return {2} != rhs.{2};
}

bool operator<( {0} const & rhs ) const VULKAN_HPP_NOEXCEPT
{
  return {2} < rhs.{2};
}
#endif
)",
                          className, classNameLower, handle);
    if (!cfg.gen.cppModules) {
        generateClassMembers(data, out, funcs, Namespace::VK, inlineFuncs);
    }

    output += generateClassString(className, out);

    output += format(R"(
template <>
struct CppType<{NAMESPACE}::ObjectType, {NAMESPACE}::ObjectType::e{0}>
{
using Type = {NAMESPACE}::{0};
};

)",
                     className, debugReportValue);

    if (debugReportValue != "Unknown") {
        output += format(R"(
template <>
struct CppType<{NAMESPACE}::DebugReportObjectTypeEXT,
             {NAMESPACE}::DebugReportObjectTypeEXT::e{1}>
{
using Type = {NAMESPACE}::{0};
};

)",
                         className, debugReportValue);
    }

    output += format(R"(
template <>
struct isVulkanHandleType<{NAMESPACE}::{0}>
{
static VULKAN_HPP_CONST_OR_CONSTEXPR bool value = true;
};

)",
                     className);

    return output;
}

std::string Generator::generateClass(HandleData data, UnorderedOutput<> &funcs) {
    std::string output;
    if (data.canGenerate()) {
        std::string str;
        for (const auto &s : data.subscribers) {
            str += " " + s->name;
        }
        std::cout << " gen class " << data.name << " " << str << std::endl;
    }

    if (!data.getProtect().empty()) {
        platformOutput.add(data, [&](std::string &output) {
            output += generateClassStr(data, funcs, true);
        });
    }
    else {
        output += genOptional(data, [&](std::string &output) {
            output += generateClassStr(data, funcs, false);
        });
    }
    return output;
}

std::string Generator::generateClassRAII(HandleData data, UnorderedOutput<> &funcs) {
    std::string output;
    output += genOptional(data, [&](std::string &output) {
        GenOutputClass out{*this};

        const std::string &className = data.name;
        const std::string &classNameLower = strFirstLower(className);
        const std::string &handle = data.vkhandle;
        const auto &superclass = data.superclass;
        const std::string &owner = data.ownerhandle;

        std::string debugReportValue = "Unknown";
        auto en = enums.find("VkDebugReportObjectTypeEXT");
        if (en != enums.end() && en->containsValue("e" + className)) {
            debugReportValue = className;
        }

        out.sPublic += format(R"(
    using CType      = Vk{0};
    using NativeType = Vk{0};

    static VULKAN_HPP_CONST_OR_CONSTEXPR {NAMESPACE}::ObjectType objectType =
      {NAMESPACE}::ObjectType::e{0};
    static VULKAN_HPP_CONST_OR_CONSTEXPR {NAMESPACE}::DebugReportObjectTypeEXT debugReportObjectType =
      {NAMESPACE}::DebugReportObjectTypeEXT::e{1};

)",
                              className, debugReportValue);

        generateClassConstructorsRAII(data, out, funcs);

        if (!owner.empty()) {
            out.sPrivate +=
                format("    {0} const * {1} = {};\n", superclass, owner);

            out.sPublic += format(R"(
    {0} const * get{0}() const {
      return {1};
    }
)",
                                  superclass, owner);
        }
        out.sPrivate +=
            format("    {NAMESPACE}::{0} {1} = {};\n", className, handle);

        out.sPublic += format(R"(

    {NAMESPACE}::{0} const &operator*() const {NOEXCEPT} {
        return {1};
    }

    void clear() {NOEXCEPT};
    void swap({NAMESPACE_RAII}::{0} &) {NOEXCEPT};
)",
                              className, handle);

        generateClassMembers(data, out, funcs, Namespace::RAII);

        output += generateClassString(className, out);

        if (!data.vectorCmds.empty()) {

            GenOutputClass out{*this};
            std::string name = className + "s";

            out.inherits +=
                format("public std::vector<{NAMESPACE_RAII}::{0}>", className);

            HandleData cls = data;
            cls.name = name;

            for (auto &m : data.vectorCmds) {

                MemberContext ctx{m, Namespace::RAII, true};
                ctx.cls = &cls;
                const auto &parent = ctx.params.begin()->get();

                if (parent->original.type() != superclass.original) {

                    std::shared_ptr<VariableData> var =
                        std::make_shared<VariableData>(*this, superclass);
                    var->setConst(true);
                    ctx.params.insert(ctx.params.begin(), var);
                }

                MemberResolverVectorCtor resolver{ctx};
                if (!resolver.checkMethod()) {
                    // std::cout << "vector ctor skipped: class " << data.name
                    //<< ", p: " << parent->type() << ", s: " << superclass
                    // << std::endl;
                } else {
                    resolver.generate(out.sPublic, funcs, platformOutput);
                }
            }

            out.sPublic += format(R"(
    {0}( std::nullptr_t ) {}

    {0}()                          = delete;
    {0}( {0} const & ) = delete;
    {0}( {0} && rhs )  = default;
    {0} & operator=( {0} const & ) = delete;
    {0} & operator=( {0} && rhs ) = default;
)",
                                  name);

            output += generateClassString(name, out);
        }
    });
    return output;
}

void Generator::parseCommands(XMLNode *node) {
    if (verbose)
        std::cout << "Parsing commands" << std::endl;

    std::map<std::string_view, std::vector<std::string_view>> aliased;
    std::vector<XMLElement *> unaliased;

    for (XMLElement *commandElement : Elements(node) | ValueFilter("command")) {
        const char *alias = commandElement->Attribute("alias");
        if (alias) {
            const char *name = commandElement->Attribute("name");
            if (!name) {
                std::cerr << "Error: Command has no name" << std::endl;
                continue;
            }
            auto it = aliased.find(alias);
            if (it == aliased.end()) {
                aliased.emplace(alias, std::vector<std::string_view>{{name}});
            } else {
                it->second.push_back(name);
            }
        } else {
            unaliased.push_back(commandElement);
        }
    }

    for (XMLElement *element : unaliased) {

        CommandData command = parseClassMember(element, "");        
        commands.add(command);

        auto alias = aliased.find(command.name.original);
        if (alias != aliased.end()) {
            for (auto &a : alias->second) {
                CommandData data = command;
                data.setFlagBit(CommandFlags::ALIAS, true);
                data.setName(*this, std::string(a));                
                commands.add(data);
            }
        }
    }
    if (verbose)
        std::cout << "Parsing commands done" << std::endl;
}

void Generator::assignCommands() {

    //    struct Level {
    //        std::string
    //    };

    std::vector<std::string_view> deviceObjects;
    std::vector<std::string_view> instanceObjects;

    std::vector<CommandData> elementsUnassigned;

    for (auto &h : handles) {
        if (h.name.original == "VkDevice" || h.superclass == "Device") {
            deviceObjects.push_back(h.name.original);
        } else if (h.name.original == "VkInstance" ||
                   h.superclass == "Instance") {
            instanceObjects.push_back(h.name.original);
        }
    }

    const auto addCommand = [&](const std::string &type,
                                const std::string &level,
                                const CommandData &command) {
        auto &handle = findHandle(level);
        handle.addCommand(*this, command);
        // std::cout << "=> added direct to: " << handle.name << ", " <<
        // command.name << std::endl;
        if (command.isIndirectCandidate(type) && type != level) {
            auto &handle = findHandle(type);
            handle.addCommand(*this, command);
            // std::cout << "=> added indirect to: " << handle.name << ", " <<
            // command.name << std::endl;
        }
        if (command.params.size() >= 2) {
            const std::string &second = command.params.at(1)->original.type();
            if (command.isIndirectCandidate(second) && isHandle(second)) {
                auto &handle = findHandle(second);
                if (handle.superclass.original == type) {
                    handle.addCommand(*this, command, true);
                    // std::cout << "=> added raii indirect to: " << handle.name
                    // << ", s: " << handle.superclass << ", " << command.name
                    // << std::endl;
                }
            }
        }
    };

    const auto assignGetProc = [&](const std::string &className,
                                   const CommandData &command) {
        if (command.name.original == "vkGet" + className + "ProcAddr") {
            auto &handle = findHandle("Vk" + className);
            handle.getAddrCmd.emplace(ClassCommandData{this, &handle, command});
            return true;
        }
        return false;
    };

    const auto assignConstruct = [&](const CommandData &command) {
        if (command.isAlias()) {
            return;
        }

        switch (command.nameCat) {
        case MemberNameCategory::GET:
        case MemberNameCategory::ENUMERATE:
        case MemberNameCategory::CREATE:
        case MemberNameCategory::ALLOCATE:
            break;
        default:
            return;
        }

        const auto &last = command.params.rbegin()->get();
        std::string type = last->original.type();
        if (!last->isPointer() || !isHandle(type)) {
            return;
        }
        try {
            auto &handle = findHandle(type);
            auto superclass = handle.superclass;
            std::string name = handle.name;
            strStripPrefix(name, superclass);

            ClassCommandData data{this, &handle, command};
            if (command.params.rbegin()->get()->isArray()) {
                // std::cout << ">> VECTOR " << handle.name << ":" << command.name << std::endl;
                handle.vectorVariant = true;
                handle.vectorCmds.push_back(data);
            } else {
                handle.ctorCmds.push_back(data);
                //                std::cout << ">> " << handle.name << ": " <<
                //                command.name << std::endl;
            }

            bool destroyExists = false;
            if (command.nameCat == MemberNameCategory::CREATE) {
                const auto c = commands.find("vkDestroy" + name);
                if (c != commands.end()) {
                    destroyExists = true;
                    handle.creationCat = HandleCreationCategory::CREATE;
                    handle.dtorCmds.push_back(c.base());
                }
            } else if (command.nameCat == MemberNameCategory::ALLOCATE) {
                const auto c = commands.find("vkFree" + name);
                if (c != commands.end()) {
                    destroyExists = true;
                    handle.creationCat = HandleCreationCategory::ALLOCATE;
                    handle.dtorCmds.push_back(c.base());
                }
            }
        } catch (std::runtime_error e) {
            std::cerr << "warning: can't assign constructor: " << type
                      << " (from " << command.name << "): " << e.what()
                      << std::endl;
        }
    };

    const auto assignCommand = [&](const CommandData &command) {
        return assignGetProc("Instance", command) ||
               assignGetProc("Device", command);
    };

    for (auto &command : commands) {

        if (assignCommand(command)) {
            continue;
        }

        assignConstruct(command);

        std::string first;
        if (!command.params.empty()) {
            // type of first argument
            first = command.params.at(0)->original.type();
        }

        if (!isHandle(first)) {
            staticCommands.push_back(command);
            loader.addCommand(*this, command);
            continue;
        }

        if (isInContainter(deviceObjects,
                           first)) { // command is for device
            addCommand(first, "VkDevice", command);
        } else if (isInContainter(instanceObjects,
                                  first)) { // command is for instance
            addCommand(first, "VkInstance", command);
        } else {
            std::cerr << "warning: can't assign command: " << command.name
                      << std::endl;
        }
    }

//    for (auto &h : handles) {
//        if (h.first != h.second.name.original) {
//            std::cerr << "Error: handle intergity check. " << h.first << " vs "
//                      << h.second.name.original << std::endl;
//            abort();
//        }
//    }

    if (!elementsUnassigned.empty()) {
        std::cerr << "Unassigned commands: " << elementsUnassigned.size()
                  << std::endl;
        for (auto &c : elementsUnassigned) {
            std::cerr << "  " << c.name << std::endl;
        }
    }    

    return;
    std::cout << "Static commands: " << staticCommands.size() << " { "
              << std::endl;
    for (auto &&c : staticCommands) {
        std::cout << "  " << c.get().name << std::endl;
    }
    std::cout << "}" << std::endl;

    bool verbose = false;
    for (const auto &h : handles) {
        std::cout << "obj: " << h.name << " (" << h.name.original << ")";
        if (h.uniqueVariant()) {
            std::cout << " unique ";
        }
        if (h.vectorVariant) {
            std::cout << " vector ";
        }
        std::cout << std::endl;
        std::cout << "  super: " << h.superclass;
        if (h.parent) {
            std::cout << ", parent: " << h.parent->name;
        }
        std::cout << std::endl;
        for (const auto &c : h.dtorCmds) {
            std::cout << "  DTOR: " << c->name << " (" << c->name.original
                      << ")" << std::endl;
        }
        if (!h.members.empty()) {
            std::cout << "  commands: " << h.members.size();
            if (verbose) {
                std::cout << " {" << std::endl;
                for (const auto &c : h.members) {
                    std::cout << "  " << c.name.original << " ";
                    if (c.raiiOnly) {
                        std::cout << "R";
                    }
                    std::cout << std::endl;
                }
                std::cout << "}";
            }
            std::cout << std::endl;
        }
        std::cout << std::endl;
    }
}

std::string Generator::generatePFNs(const HandleData &data,
                                    GenOutputClass &out) {
    std::string load;
    std::string loadSrc;
    if (data.getAddrCmd.has_value() && !data.getAddrCmd->name.empty()) {
        loadSrc = strFirstLower(data.superclass) + ".";
    }

    for (const ClassCommandData &m : data.members) {
        const std::string &name = m.name.original;

        // PFN pointers declaration
        // TODO check order
        out.sProtected += genOptional(*m.src, [&](std::string &output) {
            output += format("    PFN_{0} m_{0} = {};\n", name);
        });

        // PFN pointers initialization
        load += genOptional(*m.src, [&](std::string &output) {
            output +=
                format("      m_{0} = {1}getProcAddr<PFN_{0}>(\"{0}\");\n",
                       name, loadSrc);
        });
    }

    return load;
}

std::string Generator::generateLoader() {
    GenOutputClass out{*this};
    std::string output;

    out.sProtected += R"(
    LIBHANDLE lib = {};
    PFN_vkGetInstanceProcAddr m_vkGetInstanceProcAddr = {};
)";
    std::string load = generatePFNs(loader, out);

    out.sPublic += format(R"(
#ifdef _WIN32
    static constexpr char const* defaultName = "vulkan-1.dll";
#else
    static constexpr char const* defaultName = "libvulkan.so.1";
#endif

    {0}() = default;

    {0}(const std::string &name) {
      load(name);
    }

    template<typename T>
    {INLINE} T getProcAddr(const char *name) const {
      return std::bit_cast<T>(m_vkGetInstanceProcAddr(nullptr, name));
    }

    void load(const std::string &name) {

#ifdef _WIN32
      lib = LoadLibraryA(name.c_str());
#else
      lib = dlopen(name.c_str(), RTLD_NOW);
#endif
      if (!lib) {
        throw std::runtime_error("Cant load library: " + name);
      }

#ifdef _WIN32
      m_vkGetInstanceProcAddr = std::bit_cast<PFN_vkGetInstanceProcAddr>(GetProcAddress(lib, "vkGetInstanceProcAddr"));
#else
      m_vkGetInstanceProcAddr = std::bit_cast<PFN_vkGetInstanceProcAddr>(dlsym(lib, "vkGetInstanceProcAddr"));
#endif
      if (!m_vkGetInstanceProcAddr) {
        throw std::runtime_error("Cant load vkGetInstanceProcAddr");
      }
{1}
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

    void load() {
      load(defaultName);
    }

    ~LibraryLoader() {
      unload();
    }

    PFN_vkGetInstanceProcAddr getInstanceProcAddr() const {
      return m_vkGetInstanceProcAddr;
    }
)",
                          loader.name, load);

    for (const auto &m : loader.members) {
        MemberContext ctx{m, Namespace::RAII};
        if (ctx.nameCat == MemberNameCategory::CREATE) {
            std::shared_ptr<VariableData> var =
                std::make_shared<VariableData>(*this, loader.name);
            var->setConst(true);
            var->setIgnorePFN(true);
            ctx.params.insert(ctx.params.begin(), var);
        }
        generateClassMember(ctx, out, outputFuncsRAII);
    }

    output += R"(
#ifdef _WIN32
#  define LIBHANDLE HINSTANCE
#else
#  define LIBHANDLE void*
#endif
)";
    output += generateClassString(loader.name, out);
    output += "  ";
    if (!cfg.gen.cppModules) {
        output += "static ";
    }
    output += loader.name + " libLoader;\n";

    return output;
}

std::string Generator::genMacro(const Macro &m) {
    std::string out;
    if (m.usesDefine) {
        out += format(R"(
#if !defined( {0} )
#  define {0} {1}
#endif
)",
                      m.define, m.value);
    }
    return out;
}

void Generator::initLoaderName() {
    loader.name.convert("VkLibraryLoader", true);
}

std::string Generator::beginNamespace(Namespace ns) const {
    std::string output;
    if (cfg.gen.cppModules) {
        output += "export ";
    }
    return output + "namespace " + getNamespace(ns, false) + " {\n";
}

std::string Generator::endNamespace(Namespace ns) const {
    return "}  // namespace " + getNamespace(ns, false) + "\n";
}

void Generator::loadFinished() {
    if (onLoadCallback) {
        onLoadCallback();
    }
}

Generator::Generator()
    : loader(HandleData{""}), outputFuncs{*this}, outputFuncsRAII{*this}, platformOutput{*this}
{
    verbose = false;
    unload();
    resetConfig();

    namespaces.insert_or_assign(Namespace::VK, &cfg.macro.mNamespace.data);
    namespaces.insert_or_assign(Namespace::RAII, &cfg.macro.mNamespaceRAII.data);
    namespaces.insert_or_assign(Namespace::STD, &cfg.macro.mNamespaceSTD);
}

std::string Generator::findDefaultRegistryPath() {
    const char* sdk = std::getenv("VULKAN_SDK");
    if (!sdk) {
        return "";
    }
    std::filesystem::path sdkPath{sdk};
    std::filesystem::path file{"share/vulkan/registry/vk.xml"};
    std::filesystem::path regPath = sdkPath / file;
    if (!std::filesystem::exists(regPath)) {
        return "";
    }
    return regPath.string();
}

void Generator::resetConfig() {
    cfg = Config();
}

void Generator::loadConfigPreset() {
    cfg = Config();
}

void Generator::bindGUI(const std::function<void()> &onLoad) {
    onLoadCallback = onLoad;

    if (isLoaded()) {
        loadFinished();
    }
}

void Generator::setOutputFilePath(const std::string &path) {
    outputFilePath = path;
    if (isOuputFilepathValid()) {
        std::string filename = std::filesystem::path(path).filename().string();
        filename = camelToSnake(filename);        
    }
}

void Generator::load(const std::string &xmlPath) {
    if (isLoaded()) {
        unload();
    }

    if (XMLError e = doc.LoadFile(xmlPath.c_str()); e != XML_SUCCESS) {
        throw std::runtime_error("XML load failed: " + std::to_string(e) +
                                 " (file: " + xmlPath + ")");
    }

    root = doc.RootElement();
    if (!root) {
        throw std::runtime_error("XML file is empty");
    }

    // specifies order of parsing vk.xml registry
    static const std::vector<OrderPair> loadOrder{
        {"platforms",
         std::bind(&Generator::parsePlatforms, this, std::placeholders::_1)},
        {"tags", std::bind(&Generator::parseTags, this, std::placeholders::_1)},
        {"types",
         std::bind(&Generator::parseTypes, this, std::placeholders::_1)},
        {"enums",
         std::bind(&Generator::parseEnums, this, std::placeholders::_1)},
        {"commands",
         std::bind(&Generator::parseCommands, this, std::placeholders::_1)},
        {"feature",
         std::bind(&Generator::parseFeature, this, std::placeholders::_1)},
        {"extensions",
         std::bind(&Generator::parseExtensions, this, std::placeholders::_1)}};

    std::string prev;
    // call each function in rootParseOrder with corresponding XMLNode
    for (auto &key : loadOrder) {
        for (XMLNode &n : Elements(root)) {
            if (key.first == n.Value()) {
                key.second(&n); // call function(XMLNode*)
            }
        }
    }

    {
        auto it = enums.find("VkResult");
        if (it == enums.end()) {
            throw std::runtime_error("Missing VkResult in xml registry");
        }
        for (const auto &m : it->members) {
            if (!m.isAlias && m.name.starts_with("eError")) {
                errorClasses.push_back(&m);
            }
        }
    }

    if (verbose)
        std::cout << "Building dependencies information" << std::endl;
    std::map<std::string, BaseType *> deps; // TODO refactor
    for (auto &d : enums) {
        deps.emplace(d.name.original, &d);
    }
    for (auto &d : structs) {
        deps.emplace(d.name.original, &d);
    }
    for (auto &d : handles) {
        deps.emplace(d.name.original, &d);
    }

    for (auto &s : structs) {

        for (const auto &m : s.members) {
            const std::string &type = m->original.type();
            if (!type.starts_with("Vk")) {                
                continue;
            }
            auto it = deps.find(type);
            if (it != deps.end()) {
                s.dependencies.insert(it->second);
            }
        }
    }

    for (auto &command : commands) {
        for (const auto &m : command.params) {
            const std::string &type = m->original.type();
            if (!type.starts_with("Vk")) {
                continue;
            }
            auto it = deps.find(type);
            if (it != deps.end()) {
                command.dependencies.insert(it->second);
            }
        }
    }

    if (verbose)
        std::cout << "Building dependencies done" << std::endl;

    assignCommands();

    const auto lockDependency = [&](const std::string &name) {
        auto it = deps.find(name);
        if (it != deps.end()) {
            it->second->forceRequired = true;
        }
        else {
            std::cerr << "Can't find element: " << name << std::endl;
        }
    };

    lockDependency("VkStructureType");
    lockDependency("VkResult");
    lockDependency("VkObjectType");
    lockDependency("VkDebugReportObjectTypeEXT");

    for (auto &d : deps) {
        d.second->setEnabled(true);
    }

    for (auto &c : commands) {
        c.setEnabled(true);
    }

#ifdef INST
    std::vector<std::string> cmds;
    cmds.reserve(commands.size());
    for (const auto &c : commands.items) {
        cmds.emplace_back(c.name.original);
    }
    Inst::processCommands(cmds);
#endif

    for (auto &s : structs) {
        for (const auto &m : s.members) {
            auto p = s.getProtect();
            if (!m->isPointer() && isStructOrUnion(m->original.type())) {
                auto i = structs.find(m->original.type());
                auto pm = i->getProtect();
                // std::cout << pm << std::endl;
                if (!p.empty() && !pm.empty() && p != pm) {
                    std::cout << ">> platform dependency: " << p << " -> " << pm << std::endl;
                }
            }
        }
    }

    std::cout << "loaded: " << xmlPath << std::endl;
    registryPath = xmlPath;
    loadFinished();
}

void Generator::unload() {
    root = nullptr;    
    registryPath = "";

    headerVersion.clear();
    platforms.clear();
    tags.clear();
    enums.clear();
    handles.clear();
    structs.clear();
    extensions.clear();    
    staticCommands.clear();
    commands.clear();
    errorClasses.clear();
    loader.clear();
    initLoaderName();
}

void Generator::generate() {

    std::cout << "generating" << std::endl;

    std::string p = outputFilePath;
    std::replace(p.begin(), p.end(), '\\', '/');
    if (!p.ends_with('/')) {
        p += '/';
    }
    std::filesystem::path path = std::filesystem::absolute(p);
    std::cout << "path: " << path << std::endl;
    if (!std::filesystem::exists(path)) {
        if (!std::filesystem::create_directory(path)) {
            throw std::runtime_error{"Can't create directory"};
        }
    }
    if (!std::filesystem::is_directory(path)) {
        throw std::runtime_error{"Output path is not a directory"};
    }

    // TODO check existing files

    outputFuncs.clear();
    outputFuncsRAII.clear();
    dispatchLoaderBaseGenerated = false;
//    for (auto &h : handles) {
//        h.clear();
//    }

    initLoaderName();
    loader.init(*this, loader.name);
    for (auto &h : handles) {        
        h.init(*this, loader.name);
    }

    generateFiles(path);
}

bool Generator::isInNamespace(const std::string &str) const {
    if (enums.contains(str)) {
        return true;
    }
    if (structs.contains(str)) {
        return true;
    }
    if (handles.contains(str)) {
        return true;
    }
    return false;
}

std::string Generator::getNamespace(Namespace ns, bool colons) const {
    if (ns == Namespace::NONE) {
        return "";
    }
    try {
        auto it = namespaces.at(ns);
        return it->get() + (colons ? "::" : "");
    } catch (std::exception) {
        throw std::runtime_error("getNamespace(): namespace does not exist.");
    }
}

template<size_t I, typename... T>
void Generator::saveConfigParam(XMLElement *parent, const std::tuple<T...> &t) {
    const auto &data = std::get<I>(t);    
    saveConfigParam(parent, data);
    // unroll tuple
    if constexpr (I+1 != sizeof...(T)) {
        saveConfigParam<I+1>(parent, t);
    }
}

template<typename T>
void Generator::saveConfigParam(XMLElement *parent, const T &data)
{
    if constexpr (std::is_base_of<ConfigGroup, T>::value) {
        // std::cout << "export: " << data.name << std::endl;
        XMLElement *elem = parent->GetDocument()->NewElement(data.name.c_str());
        saveConfigParam(elem, data.reflect());
        parent->InsertEndChild(elem);
    }
    else {
        // std::cout << "export:   * leaf" << std::endl;
        // export
        XMLElement *elem = parent->GetDocument()->NewElement("");
        elem->SetAttribute("name", data.name.c_str());
        data.xmlExport(elem);
        parent->InsertEndChild(elem);
    }

}

template<typename T>
void Generator::loadConfigParam(XMLElement *parent, T &data)
{
    if constexpr (std::is_base_of<ConfigGroup, T>::value) {
        // std::cout << "load: " << data.name << std::endl;
        XMLElement *elem = parent->FirstChildElement(data.name.c_str());
        loadConfigParam(elem, data.reflect());
    }
    else {
        // import
        // std::cout << "load:   * leaf: " << data.name << std::endl;
        data.reset();
        if (parent) {
            for (XMLElement &elem : Elements(parent)) {
                const char* name = elem.Attribute("name");
                if (name && std::string_view{name} == data.name) {
                    data.xmlImport(&elem);
                }
            }
        }
    }
}

template<size_t I, typename... T>
void Generator::loadConfigParam(XMLElement *parent, const std::tuple<T...> &t) {
    auto &data = std::get<I>(t);
    // import
    loadConfigParam(parent, data);
    // unroll
    if constexpr (I+1 != sizeof...(T)) {
        loadConfigParam<I+1>(parent, t);
    }
}

void Generator::saveConfigFile(const std::string &filename) {
    if (!isLoaded()) {
        return;
    }
    XMLDocument doc;

    XMLElement* root = doc.NewElement("config");
    root->SetAttribute("vk_version", headerVersion.c_str());

    XMLElement *whitelist = doc.NewElement("whitelist");

    configBuildList("platforms", platforms.items, whitelist);
    configBuildList("extensions", extensions.items, whitelist);
    configBuildList("structs", structs.items, whitelist);
    configBuildList("enums", enums.items, whitelist);
    configBuildList("handles", handles.items, whitelist);
    if (cfg.gen.orderCommands && !orderedCommands.empty()) {
        configBuildList("commands", orderedCommands, whitelist);
    }
    else {
        configBuildList("commands", commands.items, whitelist);
    }

    root->InsertEndChild(whitelist);
    doc.InsertFirstChild(root);

    XMLElement *conf = doc.NewElement("configuration");
    saveConfigParam(conf, cfg.reflect());

    root->InsertEndChild(conf);

    XMLError e = doc.SaveFile(filename.c_str());
    if (e == XMLError::XML_SUCCESS) {
        std::cout << "Saved config file: " << filename << std::endl;
    }
}

void Generator::loadConfigFile(const std::string &filename) {
    if (!isLoaded()) {
        throw std::runtime_error("Can't load config: registry is not loaded");
    }

    XMLDocument doc;
    if (XMLError e = doc.LoadFile(filename.c_str()); e != XML_SUCCESS) {
        auto msg = "XML config load failed: " + std::to_string(e) + " " + std::string{doc.ErrorStr()} +
                                 " (file: " + filename + ")";
        // std::cerr << msg << std::endl;
        throw std::runtime_error(msg);
    }

    XMLElement* root = doc.RootElement();
    if (!root) {
        throw std::runtime_error("XML config load failed: file is empty");
    }

    if (strcmp(root->Value(), "config") != 0) {
        throw std::runtime_error("XML config load failed: wrong XML structure");
    }

    XMLElement *conf = root->FirstChildElement("configuration");
    if (conf) {
        loadConfigParam(conf, cfg.reflect());
    }

    auto bEnums = WhitelistBinding{&enums.items, "enums"};
    auto bPlats = WhitelistBinding{&platforms.items, "platforms"};
    auto bExts  = WhitelistBinding{&extensions.items, "extensions"};
    auto bStructs = WhitelistBinding{&structs.items, "structs"};
    auto bCmds  = WhitelistBinding{&commands.items, "commands"};
    auto bHandles  = WhitelistBinding{&handles.items, "handles"};
    auto bindings = {
        dynamic_cast<AbstractWhitelistBinding*>(&bPlats),
        dynamic_cast<AbstractWhitelistBinding*>(&bExts),
        dynamic_cast<AbstractWhitelistBinding*>(&bStructs),
        dynamic_cast<AbstractWhitelistBinding*>(&bCmds),
        dynamic_cast<AbstractWhitelistBinding*>(&bEnums),
        dynamic_cast<AbstractWhitelistBinding*>(&bHandles)
    };

    class ConfigVisitor : public XMLVisitor {
        AbstractWhitelistBinding* parent;
    public:
        ConfigVisitor(AbstractWhitelistBinding *parent) : parent(parent)
        {}

        virtual bool Visit (const XMLText & text) override {
            std::string_view tag = text.Parent()->Value();
            std::string_view value = text.Value();
            // std::cout << "tag: " << tag << std::endl;
            if (tag == parent->name) {
                for (auto line : split2(value, "\n")) {
                    std::string t = regex_replace(std::string{line}, std::regex("(^\\s*)|(\\s*$)"), "");
                    if (!t.empty()) {
                        if (!parent->add(t)) {
                            std::cerr << "[Config load] Duplicate: " << t << std::endl;
                        }
                    }
                }
            }
            else if (tag == "regex") {
                // std::cout << "RGX: " << value << std::endl;
                try {
                    parent->add(std::regex{std::string{value}});
                }
                catch (const std::regex_error& err) {
                    std::cerr << "[Config load]: regex error: " << err.what() << std::endl;
                }
            }
            return true;
        }
    };

    XMLElement *whitelist = root->FirstChildElement("whitelist");
    if (whitelist) {        

        for (XMLNode &n : Elements(whitelist)) {
            bool accepted = false;
            for (auto &b : bindings) {
                if (b->name != n.Value()) {
                    continue;
                }
                accepted = true;

                ConfigVisitor prt(b);
                n.Accept(&prt);

//                const char *text = n.ToElement()->GetText();
//                for (auto t : split(text, "\n")) {
//                    t = regex_replace(t, std::regex("(^\\s*)|(\\s*$)"), "");
//                    if (!t.empty()) {
//                        if (!b->add(t)) {
//                            std::cerr << "[Config load] Duplicate: " << t << std::endl;
//                        }
//                    }
//                }
            }
            if (!accepted) {
                std::cerr << "[Config load] Warning: unknown element: " << n.Value() << std::endl;
            }
        }

//        std::vector<std::string> removed;
//        for (const auto &f : bTypes.filter) {
//            if (enums.find(f) != enums.end()) {
//                bEnums.filter.insert(f);
//                // std::cout << "[Config load] shift to enums: " << f << std::endl;
//                removed.push_back(f);
//            }
//        }
//        for (const auto &f : removed) {
//            bTypes.filter.erase(f);
//        }

        std::cout << "[Config load] whitelist built" << std::endl;

        for (auto &b : bindings) {
            b->apply();
        }

        orderedCommands.clear();
        orderedCommands.reserve(bCmds.ordered.size());
        for (auto &c : bCmds.ordered) {
            auto it = commands.find(c);
            if (it != commands.end()) {
                orderedCommands.push_back(*it);
            }
        }

        std::cout << "[Config load] whitelist applied" << std::endl;

//        std::cout << "order:" << std::endl;
//        std::ranges::for_each(orderedCommands, [](CommandData &c){
//            std::cout << "  " << c.name << std::endl;
//        });
    }    

    std::cout << "[Config load] Loaded: " << filename << std::endl;
}

template <class... Args>
std::string Generator::format(const std::string &format,
                              const Args... args) const {

    std::vector<std::string> list;
    static const std::unordered_map<std::string, const Macro &> macros = {
        {"NAMESPACE", cfg.macro.mNamespace},
        {"NAMESPACE_RAII", cfg.macro.mNamespaceRAII},
        {"CONSTEXPR", cfg.macro.mConstexpr},
        {"CONSTEXPR_14", cfg.macro.mConstexpr},
        {"NOEXCEPT", cfg.macro.mNoexcept},
        {"INLINE", cfg.macro.mInline},
        {"EXPLICIT", cfg.macro.mExplicit}
    };

    static const std::regex rgx = [&] {
        std::string s = "\\{([0-9]+";
        for (const auto &k : macros) {
            s += "|" + k.first;
        }
        s += ")\\}";
        return std::regex(s);
    }();
    list.reserve(sizeof...(args));
    (
        [&](const auto &arg) {
            list.push_back((std::stringstream() << arg).str());
        }(args),
        ...);

    bool matched = false;
    std::string str = regex_replace(format, rgx, [&](std::smatch const &match) {
        if (match.size() < 2) {
            return std::string{};
        }
        const std::string &str = match[1];
        if (str.empty()) {
            return std::string{};
        }

        const auto &m = macros.find(str);
        matched = true;
        if (m != macros.end()) {
            return m->second.get();
        }

        char *end;
        long index = strtol(str.c_str(), &end, 10);
        if (end && *end != '\0') {
            return std::string{};
        }
        if (index >= list.size()) {
            throw std::runtime_error{"format index out of range"};
        }
        return list[index];
    });
    if (!matched) {
        return format;
    }
    return str;
}

void Generator::HandleData::init(const Generator &gen,
                                      const std::string &loaderClassName) {    
    clear();

    const auto &cfg = gen.getConfig();
    effectiveMembers = 0;
    filteredMembers.clear();
    std::unordered_map<std::string, ClassCommandData*> stage;
    bool order = cfg.gen.orderCommands && !gen.orderedCommands.empty();

    for (auto &m : members) {
        std::string s = gen.genOptional(
            *m.src, [&](std::string &output)
        {
            effectiveMembers++;
            if (order) {
                stage.emplace(m.name.original, &m);
            }
            else {
                filteredMembers.push_back(&m);
            }
        });
    }
    if (order) {
        for (const auto &o : gen.orderedCommands) {
            auto it = stage.find(o.get().name.original);
            if (it != stage.end()) {
                filteredMembers.push_back(it->second);
                stage.erase(it);
            }
        }
        for (auto &c : stage) {
            filteredMembers.push_back(c.second);
        }
    }

    if (isSubclass) {
        VariableData var(gen);
        var.setFullType("", superclass, "");
        var.setIdentifier("m_owner");
        uniqueVars.push_back(var);
    }
    if (cfg.gen.allocatorParam) {
        VariableData var(gen);
        var.setFullType("const ", "AllocationCallbacks", " *");
        var.setIdentifier("m_allocationCallbacks");
        uniqueVars.push_back(var);
    }
    if (cfg.gen.dispatchParam) {
        VariableData var(gen);
        var.setFullType("const ", cfg.macro.mDispatchType->get(), " *");
        var.setIdentifier("m_dispatch");
        uniqueVars.push_back(var);
    }
}

void Generator::HandleData::addCommand(const Generator &gen,
                                       const CommandData &cmd, bool raiiOnly) {
    // std::cout << "Added to " << name << ", " << cmd.name << std::endl;
    ClassCommandData d{&gen, this, cmd};
    d.raiiOnly = raiiOnly;
    members.push_back(d);
}

bool Generator::EnumData::containsValue(const std::string &value) const {
    for (const auto &m : members) {
        if (m.name == value) {
            return true;
        }
    }
    return false;
}

std::string Generator::MemberResolverBase::getDbgtag() {
    if (!ctx.gen.getConfig().dbg.methodTags) {
        return "";
    }
    std::string out = "// <" + dbgtag + ">";
    if (ctx.isRaiiOnly()) {
        out += " <RAII indirect>";
    } else if (ctx.isIndirect()) {
        out += " <indirect>";
    }
    out += "\n";
    return out;
}

std::string Generator::MemberResolverBase::generateDeclaration() {
    //return ctx.gen.genOptional(ctx, [&](std::string &output) {
    std::string output;
        std::string indent = ctx.isStatic ? "  " : "    ";
        output += getProto(indent, true) + ";\n";
    //});
    return output;
}

std::string Generator::MemberResolverBase::generateDefinition(bool genInline, bool bypass) {
    std::string output;
    //return ctx.gen.genOptional(ctx, [&](std::string &output) {
        std::string indent = genInline ? "    " : "  ";
        output += getProto(indent, genInline) + " {\n";
        if (ctx.ns == Namespace::RAII && ctx.isIndirect() &&
            !ctx.constructor) {
            if (ctx.cls->ownerhandle.empty()) {
                std::cerr << "Error: can't generate funcion: class has "
                             "no owner ("
                          << ctx.cls->name << ", " << ctx.name << ")"
                          << std::endl;
            } else {
                String name =
                    convertName(ctx.name.original, ctx.cls->superclass);

                std::string args = createPassArguments();
                output += "    ";
                if (generateReturnType() != "void") {
                    output += "return ";
                }
                std::string temp;
                for (const auto &p : ctx.params) {
                    const std::string &str = p->getTemplate();
                    if (!str.empty()) {
                        temp += str + ", ";
                    }
                }
                strStripSuffix(temp, ", ");
                if (!temp.empty()) {
                    temp = "<" + temp + ">";
                }

                output += ctx.cls->ownerhandle + "->" + name + temp + "(" +
                          args + ");\n";
            }
        } else {
            for (const auto &p : ctx.params) {
                if (p->getIgnoreFlag()) {
                    continue;
                }
                if (p->getSpecialType() ==
                    VariableData::TYPE_ARRAY_PROXY) {
                    if (p->isLenAttribIndirect()) {
                        const auto &var = p->getLengthVar();
                        std::string size = var->identifier() + "." +
                                           p->getLenAttribRhs();
                        output += "    // if (" + p->identifier() +
                                  ".size()" + " != " + size +
                                  ") TODO\n";
                    }
                }
            }
#ifdef INST
            output += Inst::bodyStart(ctx.ns, ctx.name.original);
#endif
            output += generateMemberBody();
            if (!disableCheck &&
                ctx.pfnReturn == PFNReturnCategory::VK_RESULT) {
                output += generateCheck();
            }
            if (generateReturnType() != "void" && !returnValue.empty()) {
                output += "    return " + returnValue + ";\n";
            }
        }
        output += "  }\n\n";
    //}, bypass);
    return output;
}

void Generator::MemberResolverBase::generate(UnorderedOutput<> &decl, UnorderedOutput<> &def, UnorderedOutput<FileOutput> &platform_def, bool separate) {
    if (ctx.generateInline) {
        // decl += generateDefinition(true);
        decl.add(ctx, [&](std::string &output) {
            output += generateDefinition(true);
        });
    }
    else {
        // decl += generateDeclaration();
        decl.add(ctx, [&](std::string &output) {
            output += generateDeclaration();
        });

        if (separate && !ctx.getProtect().empty()) {
            // std::cout << "to platform: " << ctx.name << std::endl;
            platform_def.add(ctx, [&](std::string &output){
                output += generateDefinition(false);
            });
        }
        else {
            def.add(ctx, [&](std::string &output){
                output += generateDefinition(false);
            });
        }
    }
    ctx.cls->generated.push_back(ctx);
}

template<typename T>
void Generator::configBuildList(const std::string &name, const std::map<std::string, T> &from, XMLElement *parent, const std::string &comment) {
    WhitelistBuilder out;
    for (const auto &p : from) {
        out.add(p.second);
    }
    out.insertToParent(parent, name, comment);
}

template<typename T>
void Generator::configBuildList(const std::string &name, const std::vector<T> &from, XMLElement *parent, const std::string &comment) {
    WhitelistBuilder out;
    for (const auto &p : from) {
        out.add(p);
    }
    out.insertToParent(parent, name, comment);
}

template<typename T>
void Generator::WhitelistBinding<T>::apply() noexcept {
    for (auto &e : *dst) {
        bool match = false;
        auto it = filter.find(e.name.original);
        if (it != filter.end()) {
            match = true;
            filter.erase(it);
        }
        for (const auto &r : regexes) {
            if (std::regex_match(e.name.original, r)) {
                match = true;
                break;
            }
        }
        e.setEnabled(match);
    }
    for (auto &f : filter) {
        std::cerr << "[Config load] Not found: " << f << " (" << name << ")" << std::endl;
    }
}

template<>
void Generator::ConfigWrapper<Generator::Macro>::xmlExport(XMLElement *elem) const {
    // std::cout << "export macro:" << name << std::endl;
    elem->SetName("macro");
    elem->SetAttribute("define", data.define.c_str());
    elem->SetAttribute("value", data.value.c_str());
    elem->SetAttribute("usesDefine", data.usesDefine? "true" : "false");
}

template<>
void Generator::ConfigWrapper<bool>::xmlExport(XMLElement *elem) const {
    // std::cout << "export: " << name << std::endl;
    elem->SetName("bool");
    elem->SetAttribute("value", data? "true" : "false");
}

template<>
bool Generator::ConfigWrapper<Generator::Macro>::xmlImport(XMLElement *elem) const {
    // std::cout << "import macro:" << name << std::endl;
    if (std::string_view{elem->Value()} != "macro") {
        std::cerr << "[config import] node value mismatch" << std::endl;
        return false;
    }

    const char* v = elem->Attribute("usesDefine");
    if (v) {
        if (std::string_view{v} == "true") {
            const_cast<ConfigWrapper<Macro>*>(this)->data.usesDefine = true;
        }
        else if (std::string_view{v} == "false") {
            const_cast<ConfigWrapper<Macro>*>(this)->data.usesDefine = false;
        }
        else {
            std::cerr << "[config import] unknown value" << std::endl;
            return false;
        }
    }
    v = elem->Attribute("define");
    if (v) {
        const_cast<ConfigWrapper<Macro>*>(this)->data.define = v;
    }
    v = elem->Attribute("value");
    if (v) {
        const_cast<ConfigWrapper<Macro>*>(this)->data.value = v;
    }

    return true;
}

template<>
bool Generator::ConfigWrapper<bool>::xmlImport(XMLElement *elem) const {
    // std::cout << "import: " << name << std::endl;
    if (std::string_view{elem->Value()} != "bool") {
        std::cerr << "[config import] node mismatch" << std::endl;
        return false;
    }

    const char* v = elem->Attribute("value");
    if (v) {
        if (std::string_view{v} == "true") {
            const_cast<ConfigWrapper<bool>*>(this)->data = true;
        }
        else if (std::string_view{v} == "false") {
            const_cast<ConfigWrapper<bool>*>(this)->data = false;
        }
        else {
            std::cerr << "[config import] unknown value" << std::endl;
            return false;
        }
    }
    return true;
}
