// MIT License
// Copyright (c) 2021-2023  @guritchi
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//                                                           copies of the Software, and to permit persons to whom the Software is
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

#include <fstream>
#include "Instrumentation.h"

// temporarily unused
static constexpr char const *RES_HEADER{
    R"(
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
#  if defined( VULKAN_HPP_NO_TO_STRING )
      return std::to_string( ev );
#  else
      return VULKAN_HPP_NAMESPACE::to_string( static_cast<VULKAN_HPP_NAMESPACE::Result>( ev ) );
#  endif
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

static constexpr char const *RES_STRUCT_CHAIN{R"(

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
                              ( TestType::allowDuplicate || !StructureChainContains<Index - 1, TestType, ChainElements...>::value ) &&
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

    StructureChain & operator=( StructureChain && rhs ) = delete;

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
)"};

static constexpr char const *RES_UNIQUE_HANDLE{R"(
#  if !defined( VULKAN_HPP_NO_SMART_HANDLE )
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

  template <typename UniqueType>
  VULKAN_HPP_INLINE std::vector<typename UniqueType::element_type> uniqueToRaw( std::vector<UniqueType> const & handles )
  {
    std::vector<typename UniqueType::element_type> newBuffer( handles.size() );
    std::transform( handles.begin(), handles.end(), newBuffer.begin(), []( UniqueType const & handle ) { return handle.get(); } );
    return newBuffer;
  }

  template <typename Type, typename Dispatch>
  VULKAN_HPP_INLINE void swap( UniqueHandle<Type, Dispatch> & lhs, UniqueHandle<Type, Dispatch> & rhs ) VULKAN_HPP_NOEXCEPT
  {
    lhs.swap( rhs );
  }
#  endif
#endif  // VULKAN_HPP_DISABLE_ENHANCED_MODE
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

  VULKAN_HPP_INLINE void resultCheck( VkResult result, char const * message )
  {
#ifdef VULKAN_HPP_NO_EXCEPTIONS
    ignore( result );  // just in case VULKAN_HPP_ASSERT_ON_RESULT is empty
    ignore( message );
    VULKAN_HPP_ASSERT_ON_RESULT( result == VK_SUCCESS );
#else
    if ( result != VK_SUCCESS )
    {
      throwResultException( static_cast<Result>(result), message );
    }
#endif
  }

  VULKAN_HPP_INLINE void resultCheck( VkResult result, char const * message, std::initializer_list<VkResult> successCodes )
  {
#ifdef VULKAN_HPP_NO_EXCEPTIONS
    ignore( result );  // just in case VULKAN_HPP_ASSERT_ON_RESULT is empty
    ignore( message );
    ignore( successCodes );  // just in case VULKAN_HPP_ASSERT_ON_RESULT is empty
    VULKAN_HPP_ASSERT_ON_RESULT( std::find( successCodes.begin(), successCodes.end(), result ) != successCodes.end() );
#else
    if ( std::find( successCodes.begin(), successCodes.end(), result ) == successCodes.end() )
    {
      throwResultException( static_cast<Result>(result), message );
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
{0}
template <typename FlagBitsType>
struct FlagTraits
{
  static VULKAN_HPP_CONST_OR_CONSTEXPR bool isBitmask = false;
};
{1}

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

{0}
    VULKAN_HPP_CONSTEXPR Flags<BitType> operator~() const VULKAN_HPP_NOEXCEPT
    {
      return Flags<BitType>( m_mask ^ FlagTraits<BitType>::allFlags.m_mask );
    }
{1}

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
{0}
  // bitwise operators on BitType
  template <typename BitType, typename std::enable_if<FlagTraits<BitType>::isBitmask, bool>::type = true>
  VULKAN_HPP_INLINE VULKAN_HPP_CONSTEXPR Flags<BitType> operator&( BitType lhs, BitType rhs ) VULKAN_HPP_NOEXCEPT
  {
    return Flags<BitType>( lhs ) & rhs;
  }
{1}
{0}
  template <typename BitType, typename std::enable_if<FlagTraits<BitType>::isBitmask, bool>::type = true>
  VULKAN_HPP_INLINE VULKAN_HPP_CONSTEXPR Flags<BitType> operator~( BitType bit ) VULKAN_HPP_NOEXCEPT
  {
    return ~( Flags<BitType>( bit ) );
  }
{1}
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

// static constexpr char const *RES_{R"()"};


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

std::string Registry::findDefaultRegistryPath() {
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

Registry::Registry(Generator &gen)
    : loader(HandleData{gen})
{
    loader.name.convert("VkContext", true);
    loader.forceRequired = true;
}

std::string Registry::strRemoveTag(std::string &str) const {
    if (str.empty()) {
        return "";
    }
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

std::string Registry::strWithoutTag(const std::string &str) const {
    std::string out = str;
    for (const std::string &tag : tags) {
        if (out.ends_with(tag)) {
            out.erase(out.size() - tag.size());
            break;
        }
    }
    return out;
}

bool Registry::strEndsWithTag(const std::string_view str) const {
    for (const auto &tag : tags) {
        if (str.ends_with(tag)) {
            return true;
        }
    }
    return false;
}

std::pair<std::string, std::string>
Registry::snakeToCamelPair(std::string str) const {
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

std::string Registry::snakeToCamel(const std::string &str) const {
    const auto &p = snakeToCamelPair(str);
    return p.first + p.second;
}

std::string Registry::enumConvertCamel(const std::string &enumName,
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

bool Registry::isStructOrUnion(const std::string &name) const {
    return structs.contains(name);
}

bool Registry::containsFuncPointer(const StructData &data) const {


    for (const auto &m : data.members) {
        const auto &type = m->original.type();
        if (type.starts_with("PFN_")) {
            //std::cout << "    func pointer in: " << data.name << '\n';
            return true;
        }
        if (type != data.name.original) {
            const auto &s = structs.find(type);
            if (s != structs.end()) {
                if (containsFuncPointer(*s)) {
                    return true;
                }
            }
        }
    }
    return false;
}

void Registry::bindVars(const Generator &gen, Variables &vars) const {
    const auto findVar = [&](const std::string &id) -> VariableData* {
        for (auto &p : vars) {
            if (p->original.identifier() == id) {
                return p.get();
            }
        }
        std::cerr << "can't find param (" + id + ")" << '\n';
        return nullptr;
    };

    for (auto &p : vars) {
        if (!p->getArrayVars().empty()) {
            std::cerr << "Array vars not empty" << '\n';
            p->getArrayVars().clear();
        }
    }

    for (auto &p : vars) {
        const std::string &len = p->getLenAttribIdentifier();
        if (!len.empty() && p->getAltlenAttrib().empty()) {
            const auto &var = findVar(len);
            if (var) {
                p->bindLengthVar(*var);
                var->bindArrayVar(p.get());
            }
        }
#ifndef NDEBUG
        if (p->bound) {
            std::cerr << "Already bound variable" << '\n';
        }
        p->bound = true;
#endif
    }
    for (auto &p : vars) {
        p->evalFlags();
        p->save();
    }
}

String Registry::getHandleSuperclass(const HandleData &data) {
    if (!data.parent) {
        return loader.name;
    }
    if (data.name.original == "VkSwapchainKHR") { // TODO unnecessary
        return String("VkDevice", true);
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

void Registry::parsePlatforms(Generator &gen, XMLNode *node) {
    if (verbose)
        std::cout << "Parsing platforms" << '\n';
    // iterate contents of <platforms>, filter only <platform> children
    for (XMLElement *platform : Elements(node) | ValueFilter("platform")) {
        const char *name = platform->Attribute("name");
        const char *protect = platform->Attribute("protect");
        if (name && protect) {
            platforms.items.emplace_back(name, protect, defaultWhitelistOption);
        }
    }
    platforms.prepare();
    if (verbose)
        std::cout << "Parsing platforms done" << '\n';
}

void Registry::parseTags(Generator &gen, XMLNode *node) {
    if (verbose)
        std::cout << "Parsing tags" << '\n';
    // iterate contents of <tags>, filter only <tag> children
    for (XMLElement *tag : Elements(node) | ValueFilter("tag")) {
        const char *name = tag->Attribute("name");
        if (name) {
            tags.emplace(name);
        }
    }
    if (verbose)
        std::cout << "Parsing tags done" << '\n';
}

void Registry::parseTypes(Generator &gen, XMLNode *node) {
    if (verbose)
        std::cout << "Parsing declarations" << '\n';

    std::vector<std::pair<std::string_view, std::string_view>> aliasedTypes;
    std::vector<std::pair<std::string, std::string>> aliasedEnums;

    std::vector<std::pair<std::string, std::string>> renamedEnums;

    std::vector<XMLElement*> handleNodes;

    // iterate contents of <types>, filter only <type> children
    for (XMLElement *type : Elements(node) | ValueFilter("type")) {        
        const auto cat = getAttrib(type, "category").value_or("");
        const char *name = type->Attribute("name");

        if (cat == "enum") {
            if (name) {
                const char *alias = type->Attribute("alias");
                if (alias) {
                    aliasedEnums.push_back(std::make_pair(name, alias));
                } else {
                    auto &e = enums.items.emplace_back(name);
                }
            }
        } else if (cat == "bitmask") {

            const char *nameAttrib = type->Attribute("name");
            const char *aliasAttrib = type->Attribute("alias");
            const char *reqAttrib = type->Attribute("requires");

//            std::cout << "bitmask: " << '\n';
//            std::cout << "  name: " << (nameAttrib? std::string(nameAttrib) : "0") << '\n';
//            std::cout << "  alias: " << (aliasAttrib? std::string(aliasAttrib) : "0") << '\n';
//            std::cout << "  req: " << (reqAttrib? std::string(reqAttrib) : "0") << '\n';

            std::string temp;
            if (reqAttrib) {
                nameAttrib = reqAttrib;
            }
            else if (!aliasAttrib) {
                XMLElement *nameElem = type->FirstChildElement("name");
                if (!nameElem) {
                    std::cerr << "Error: bitmap has no name" << '\n';
                    continue;
                }
                nameAttrib = nameElem->GetText();
                std::string old = nameAttrib;
                temp = std::regex_replace(nameAttrib, std::regex("Flags"), "FlagBits");                
                nameAttrib = temp.c_str();
                if (temp != old) {
                    renamedEnums.push_back(std::make_pair(old, temp));
                }
                else {
//                    std::cout << "same rename " << temp << '\n';
                }
            }
            if (!nameAttrib) {
                std::cerr << "Error: bitmap alias has no name" << '\n';
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
//            std::cout << '\n';


            if (aliasAttrib) {
                std::string alias = std::regex_replace(aliasAttrib, std::regex("Flags"),
                                          "FlagBits");
                aliasedEnums.push_back(std::make_pair(name, alias));

            } else {
                enums.items.emplace_back(nameAttrib, true);
            }

        } else if (cat == "handle") {
            handleNodes.push_back(type);

        } else if (cat == "struct"|| cat == "union") {
            if (name) {
                const char *alias = type->Attribute("alias");
                if (alias) {
                    aliasedTypes.push_back(std::make_pair(name, alias));
                } else {
                    StructData::VkStructType t = (cat == "struct")
                                    ? StructData::VK_STRUCT
                                    : StructData::VK_UNION;

                    auto &s = structs.items.emplace_back(gen, name, t, type);

                    auto extends = getAttrib(type, "structextends");
                    if (extends) {

                        for (const auto &e : split2(extends.value(), ",")) {
                            s.extends.push_back(String(std::string{e}, true));
                        }
                    }

                }
            }
        } else if (cat == "define") {
            XMLDefineParser parser{type, gen};
            if (parser.name == "VK_HEADER_VERSION") {
                headerVersion = parser.value;
            }
        }
    }

    if (headerVersion.empty()) {
        throw std::runtime_error("header version not found.");
    }

    for (auto *e : handleNodes) {
        XMLElement *nameElem = e->FirstChildElement("name");
        if (nameElem) {
            const char *nameAttrib = nameElem->GetText();
            if (!nameAttrib || strlen(nameAttrib) == 0) {
                std::cerr << "Missing name in handle node" << '\n';
                continue;
            }
            std::string name{nameAttrib};

            auto parent = getAttrib(e, "parent");

            handles.items.emplace_back(gen, name, parent.value_or(""));
        }
    }
    handles.prepare();

    for (auto *e : handleNodes) {
        auto alias = getAttrib(e, "alias");
        if (alias) {
            auto name = getRequiredAttrib(e, "name");
            auto h = handles.find(alias.value());
            if (h == handles.end()) {
                std::cerr << "Error: handle not found: " << alias.value() << '\n';
                continue;
            }
            h->alias = name;
        }
    }

    const auto getParent = [&](auto &h) {
        if (!h.superclass.empty()) {
            const auto &p = handles.find(h.superclass);
            h.parent = &(*p);
        }
    };

    for (auto &h : handles.items) {
        getParent(h);
    }

    for (auto &h : handles.items) {
        h.init(gen);
    }

    enums.prepare();
    for (const auto &a : renamedEnums) {
        enums.addAlias(a.first, a.second);
    }
    for (const auto &a : aliasedEnums) {
        auto it = enums.find(a.second);
        if (it == enums.end()) {
            std::cerr << "not found" << '\n';
            continue;
        }
        it->aliases.push_back(String(a.first, true));
    }

    structs.prepare();
    for (auto &a : aliasedTypes) {
        const auto &it = structs.find(a.second.data());
        if (it == structs.end()) {
            std::cout << "Error: Type has no alias target: " << a.second << " ("
                      << a.first << ")" << '\n';
        } else {
            it->aliases.push_back(String(a.first.data(), true));
        }
    }

    orderStructs();

    if (verbose)
        std::cout << "Parsing declarations done" << '\n';
}

void Registry::parseEnums(Generator &gen, XMLNode *node) {

    const char *typeptr = node->ToElement()->Attribute("type");
    if (!typeptr) {
        return;
    }

    std::string_view type{typeptr};

    bool isBitmask = type == "bitmask";

    if (isBitmask || type == "enum") {

        const char *name = node->ToElement()->Attribute("name");
        if (!name) {
            std::cerr << "Can't get name of enum" << '\n';
            return;
        }

        auto it = enums.find(name);
        if (it == enums.end()) {
            std::cerr << "cant find " << name << "in enums" << '\n';
            return;
        }

        if (isBitmask) {
            std::string str = std::regex_replace(name, std::regex("FlagBits"), "Flags");
            if (str != name) {
                enums.addAlias(str, name);
            }
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

void Registry::parseCommands(Generator &gen, XMLNode *node) {
    if (verbose)
        std::cout << "Parsing commands" << '\n';

    std::map<std::string_view, std::vector<std::string_view>> aliased;
    std::vector<XMLElement *> unaliased;

    for (XMLElement *commandElement : Elements(node) | ValueFilter("command")) {
        const char *alias = commandElement->Attribute("alias");
        if (alias) {
            const char *name = commandElement->Attribute("name");
            if (!name) {
                std::cerr << "Error: Command has no name" << '\n';
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

        auto &command = commands.items.emplace_back(gen, element, std::string{});

        auto alias = aliased.find(command.name.original);
        if (alias != aliased.end() && !alias->second.empty()) {
            //if (alias->second.size() >= 2) {
                //std::cout << "multiple aliases: " << alias->first << '\n';
            //}

            for (auto &a : alias->second) {
                commands.items.emplace_back(gen, command, a);
            }
            command.alias = alias->second[0];
        }
    }
    commands.prepare();

    if (verbose)
        std::cout << "Parsing commands done" << '\n';
}

void Registry::assignCommands(Generator &gen) {

    auto &instance = findHandle("VkInstance");
    auto &device = findHandle("VkDevice");

    std::vector<std::string_view> deviceObjects;
    std::vector<std::string_view> instanceObjects;

    std::vector<CommandData> elementsUnassigned;

    for (auto &h : handles.items) {
        if (h.name.original == "VkDevice" || h.superclass == "Device") {
            deviceObjects.push_back(h.name.original);
        } else if (h.name.original == "VkInstance" ||
                   h.superclass == "Instance") {
            instanceObjects.push_back(h.name.original);
        }
    }

    const auto addCommand = [&](const std::string &type,
                                HandleData &handle,
                                CommandData &command)
    {
        bool indirect = type != handle.name.original && command.isIndirectCandidate(type);

        handle.addCommand(gen, command);

        // std::cout << "=> added direct to: " << handle.name << ", " << command.name << '\n';

        if (indirect) {
            command.setFlagBit(CommandFlags::INDIRECT, true);

            auto &handle = findHandle(type);
            handle.addCommand(gen, command);
            // std::cout << "=> added indirect to: " << handle.name << ", " << command.name << '\n';
        }

        HandleData* second = command.secondIndirectCandidate(gen);
        if (second) {
            if (second->superclass.original == type) {
                second->addCommand(gen, command, true);
                // std::cout << "=> added 2nd indirect to: " << second->name << ", " << command.name << '\n';
            }
        }
    };

    const auto assignGetProc = [&](HandleData &handle, CommandData &command) {
        if (command.name.original == "vkGet" + handle.name + "ProcAddr") {
            //auto &handle = findHandle("Vk" + className);
            handle.getAddrCmd.emplace(&gen, &handle, command);
            return true;
        }
        return false;
    };

    const auto assignConstruct = [&](CommandData &command) {
        if (!command.createsHandle() || command.isAlias()) {
            return;
        }

        auto last = command.getLastVar();
        if (!last) {
            std::cerr << "null access " << command.name << '\n';
            return;
        }

        std::string type = last->original.type();        
        if (!last->isPointer() || !isHandle(type)) {
            return;
        }
        try {
            auto &handle = findHandle(type);
            auto superclass = handle.superclass;
            std::string name = handle.name;
            strStripPrefix(name, superclass);

            if (last->isArrayOut() && (command.nameCat == MemberNameCategory::CREATE || command.nameCat == MemberNameCategory::ALLOCATE || command.nameCat == MemberNameCategory::ENUMERATE)) {
                // std::cout << ">> VECTOR " << handle.name << ":" << command.name << '\n';
                handle.vectorVariant = true;
                handle.vectorCmds.emplace_back(&gen, &handle, command);
            } else {
                handle.ctorCmds.emplace_back(&gen, &handle, command);
                //                std::cout << ">> " << handle.name << ": " <<
                //                command.name << '\n';
            }

        } catch (std::runtime_error e) {
            std::cerr << "warning: can't assign constructor: " << type
                      << " (from " << command.name << "): " << e.what()
                      << '\n';
        }
    };

    const auto assignDestruct2 = [&](CommandData &command, HandleCreationCategory cat) {
        try {
            std::string type = command.getLastHandleVar()->original.type();

            auto &handle = findHandle(type);
            if (handle.dtorCmd) {
                // std::cerr << "warning: handle already has destruct command:" << handle.name << '\n';
                return;
            }

            handle.creationCat = cat;
            handle.setDestroyCommand(gen, command);

        } catch (std::runtime_error e) {
            std::cerr << "warning: can't assign destructor: "
                  << " (from " << command.name << "): " << e.what()
                  << '\n';
        }
    };

    const auto assignDestruct = [&](CommandData &command) {
        if (command.name.starts_with("destroy")) {
            assignDestruct2(command, HandleCreationCategory::CREATE);
        }
        else if (command.name.starts_with("free")) {
            assignDestruct2(command, HandleCreationCategory::ALLOCATE);
        }
    };

    for (auto &command : commands) {        

        if (assignGetProc(instance, command) ||
            assignGetProc(device, command))
        {
            continue;
        }

        std::string first;
        if (command.hasParams()) {
            assignConstruct(command);            
            assignDestruct(command);            
            // type of first argument            
            first = command.params.begin()->get().original.type();
        }

        if (!isHandle(first)) {
            staticCommands.push_back(command);
            loader.addCommand(gen, command);
            continue;
        }

        if (isInContainter(deviceObjects, first))
        { // command is for device
            addCommand(first, device, command);
        }
        else if (isInContainter(instanceObjects, first))
        { // command is for instance
            addCommand(first, instance, command);
        } else {
            std::cerr << "warning: can't assign command: " << command.name
                      << '\n';
        }        
    }

    if (!elementsUnassigned.empty()) {
        std::cerr << "Unassigned commands: " << elementsUnassigned.size()
                  << '\n';
        for (auto &c : elementsUnassigned) {
            std::cerr << "  " << c.name << '\n';
        }
    }

    if (verbose)
        std::cout << "Assign commands done" << '\n';
}

void Registry::orderStructs(bool usenew) {
    DependencySorter<StructData> sorter(structs);

    if (usenew) {
        sorter.sort2("structs", [&](DependencySorter<StructData>::Item &i) {
            for (auto &m : i.data->members) {

                if (!m->isPointer() && isStructOrUnion(m->original.type())) {
                    i.addDependency(sorter, m->original.type());
                }
            }
        });
    }
    else {
        sorter.sort("structs", [&](DependencySorter<StructData>::Item &i) {
            for (auto &m : i.data->members) {

                if (!m->isPointer() && isStructOrUnion(m->original.type())) {
                    i.addDependency(sorter, m->original.type());
                }
            }
        });
    }

}

void Registry::orderHandles(bool usenew ) {
    DependencySorter<HandleData> sorter(handles);
    const auto filter = [&](DependencySorter<HandleData>::Item &i, ClassCommandData &m) {
        std::string_view name = m.name.original;
        static const std::array<std::string, 4> names {
            "vkCreate",
            "vkAllocate",
            "vkDestroy",
            "vkFree",
        };

        for (const auto &n : names) {
            if (name.starts_with(n)) {
                std::string_view str = name.substr(n.size());
                if (str == i.data->name) {
                    return false;
                }
            }
        }

        return true;
    };

    if (usenew) {
        sorter.sort2("handles", [&](DependencySorter<HandleData>::Item &i) {
            for (auto &m : i.data->members) {
                if (!filter(i, m)) {
                    continue;
                }
                for (auto &p : m.src->_params) {
                    if (p->isHandle() && !p->isPointer()) {
                        if (p->original.type() == "VkInstance" || p->original.type() == "VkDevice" ||  p->original.type() == i.data->name.original) {
                            continue;
                        }
                        i.addDependency(sorter, p->original.type());
                    }
                }
                auto p = m.src->getProtect();
                if (!p.empty()) {
                    i.plats.insert(std::string{p});
                }
            }
        });
    }
    else {
        sorter.sort("handles", [&](DependencySorter<HandleData>::Item &i) {
            for (auto &m : i.data->members) {
                if (!filter(i, m)) {
                    continue;
                }
                for (auto &p : m.src->_params) {
                    if (p->isHandle() && !p->isPointer()) {
                        if (p->original.type() == "VkInstance" || p->original.type() == "VkDevice" ||  p->original.type() == i.data->name.original) {
                            continue;
                        }
                        i.addDependency(sorter, p->original.type());
                    }
                }
                auto p = m.src->getProtect();
                if (!p.empty()) {
                    i.plats.insert(std::string{p});
                }
            }
        });
    }



}

void Registry::parseFeature(Generator &gen, XMLNode *node) {
    if (verbose)
        std::cout << "Parsing feature" << '\n';
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
        std::cout << "Parsing feature done" << '\n';
}

void Registry::parseExtensions(Generator &gen, XMLNode *node) {
    if (verbose)
        std::cout << "Parsing extensions" << '\n';

    // iterate contents of <extensions>, filter only <extension> children
    for (XMLElement *extension : Elements(node) | ValueFilter("extension")) {
        const auto name = getRequiredAttrib(extension, "name");

        bool supported = true;
        if (getAttrib(extension, "supported") == "disabled") {
            supported = false;
        }

        // std::cout << "Extension: " << name << '\n';
        if (supported) {
            PlatformData* platform = nullptr;
            const auto platformAttrib = getAttrib(extension, "platform");
            if (platformAttrib.has_value()) {
                auto it = platforms.find(platformAttrib.value());
                if (it != platforms.end()) {
                    platform = &(*it);
                } else {
                    std::cerr << "Warn: Unknown platform in extensions: "
                              << platformAttrib.value() << '\n';
                }
            }

            bool enabled = supported && defaultWhitelistOption;
            // std::cout << "add ext: " << name << '\n';
            extensions.items.emplace_back(std::string{name}, platform, supported, enabled);
//            std::cout << "ext: " << ext << '\n';
//            std::cout << "> n: " << ext->name.original << '\n';
        }
    }
    extensions.prepare();


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
            // std::cout << "  <require>" << '\n';
            for (XMLElement &entry : Elements(require)) {
                const std::string_view value = entry.Value();
                // std::cout << "    <" << value << ">" << '\n';

                if (value == "command") {
                    const char *name = entry.Attribute("name");
                    if (!name) {
                        std::cerr
                            << "Error: extension bind: command has no name"
                            << '\n';
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
                            << name << '\n';
                    }

                } else if (value == "type" /*&& ext && !ext->protect.empty()*/) {
                    const char *nameAttrib = entry.Attribute("name");
                    if (!nameAttrib) {
                        std::cerr << "Error: extension bind: type has no name"
                                  << '\n';
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
                            std::cerr << "type not found in extension: " << name << '\n';
                    }
                } else if (value == "enum" && ext) {
                    parseEnumExtend(entry, ext);
                }
            }
        }
    }
    if (verbose)
        std::cout << "Parsing extensions done" << '\n';
}

void Registry::parseEnumExtend(XMLElement &node, ExtensionData *ext) {
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
            std::cerr << "err: Cant find enum: " << extends << '\n';
        }
    }
}

void Registry::load(Generator &gen, const std::string &xmlPath) {
    using namespace std::placeholders;
    // specifies order of parsing vk.xml registry
    const std::array<OrderPair, 7> loadOrder {
        OrderPair{"platforms",
            std::bind(&Registry::parsePlatforms, this, _1, _2)},
        OrderPair{"tags",
            std::bind(&Generator::parseTags, this, _1, _2)},
        OrderPair{"types",
            std::bind(&Registry::parseTypes, this, _1, _2)},
        OrderPair{"enums",
            std::bind(&Registry::parseEnums, this, _1, _2)},
        OrderPair{"commands",
            std::bind(&Registry::parseCommands, this, _1, _2)},
        OrderPair{"feature",
            std::bind(&Registry::parseFeature, this, _1, _2)},
        OrderPair{"extensions",
            std::bind(&Registry::parseExtensions, this, _1, _2)}
    };

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

    std::string prev;
    // call each function in rootParseOrder with corresponding XMLNode
    for (auto &key : loadOrder) {
        for (XMLNode &n : Elements(root)) {
            if (key.first == n.Value()) {
                key.second(gen, &n); // call function(XMLNode*)
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
        std::cout << "Building dependencies information" << '\n';

    std::map<const std::string_view, BaseType *> deps;
    for (auto &d : enums.items) {
        deps.emplace(d.name.original, &d);
    }
    for (auto &d : structs.items) {
        deps.emplace(d.name.original, &d);
    }
    for (auto &d : handles.items) {
        deps.emplace(d.name.original, &d);
    }

    const auto find = [&](const std::string &type) -> BaseType* {
        if (!type.starts_with("Vk")) {
            return nullptr;
        }
        auto it = deps.find(type);
        if (it != deps.end()) {
            return it->second;
        }
        auto it2 = enums.find(type);
        if (it2 != enums.end()) {
            return &(*it2);
        }
        return nullptr;
    };

    for (auto &s : structs) {

        for (const auto &m : s.members) {
            const std::string &type = m->original.type();
            auto d = find(type);
            if (d) {
                s.dependencies.insert(d);
            }
        }
    }

    for (auto &command : commands) {
        for (const auto &m : command._params) {
            const std::string &type = m->original.type();
            auto d = find(type);
            if (d) {
                command.dependencies.insert(d);
            }
        }
    }

    assignCommands(gen);

    for (auto &h : handles.items) {
        for (auto &c : h.ctorCmds) {
            h.dependencies.insert(c.src);
        }
        if (h.dtorCmd) {
            h.dependencies.insert(h.dtorCmd);
        }
    }

    if (verbose)
        std::cout << "Building dependencies done" << '\n';



    orderHandles();

//    std::cout << "commands: {" << '\n';
//    for (auto &c : commands.items) {
//        std::cout << "  " << c.name << '\n';
//        for (auto &p : c.outParams) {
//            std::cout << "    out: " << p.get().original.type() << '\n';
//        }
//    }
//    std::cout << "}" << '\n';

    if (false) {

//        std::cout << "Static commands: " << staticCommands.size() << " { "
//                  << '\n';
//
//        for (CommandData &c : staticCommands) {
//            std::cout << "  " << c.name << '\n';
//        }
//        std::cout << "}" << '\n';

        bool verbose = false;
        for (const HandleData &h : handles.ordered) {
            std::cout << "Handle: " << h.name;
            // std::cout  << " (" << h.name.original << ")";
            if (h.uniqueVariant()) {
                std::cout << " (unique)";
            }
            if (h.vectorVariant) {
                std::cout << " (vector)";
            }
            std::cout << '\n';
            std::cout << "  super: " << h.superclass;
            if (h.parent) {
                std::cout << ", parent: " << h.parent->name;
            }
            std::cout << '\n';
            if (h.dtorCmd) {
//                std::cout << "  DTOR: " << h.dtorCmd->name << " (" << h.dtorCmd->name.original
//                          << ")" << '\n';
            }

            for (const auto &c : h.ctorCmds) {
                std::cout << "    ctor: " << c.name.original << std::endl;
            }
//            if (!h.members.empty()) {
//                std::cout << "  commands: " << h.members.size();
//                if (verbose) {
//                    std::cout << " {" << '\n';
//                    for (const auto &c : h.members) {
//                        std::cout << "  " << c.name.original << " ";
//                        if (c.raiiOnly) {
//                            std::cout << "R";
//                        }
//                        std::cout << '\n';
//                    }
//                    std::cout << "}";
//                }
//                std::cout << '\n';
//            }
            std::cout << '\n';
        }
    }


    const auto lockDependency = [&](const std::string &name) {
        auto it = deps.find(name);
        if (it != deps.end()) {
            it->second->forceRequired = true;
        }
        else {
            std::cerr << "Can't find element: " << name << '\n';
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


    for (auto &d : enums.items) {
        if (d.isSuppored()) {
            // std::cout << ">> enable: " << d.name << std::endl;
            d.setEnabled(true);
            if (!d.isEnabled()) {
                std::cout << "   not enabled!! " << std::endl;
            }
        }
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
                // std::cout << pm << '\n';
                if (!p.empty() && !pm.empty() && p != pm) {
                    std::cout << ">> platform dependency: " << p << " -> " << pm << '\n';
                }
            }
        }
    }

    registryPath = xmlPath;
    loadFinished();
}

void Registry::loadFinished() {
    if (onLoadCallback) {
        onLoadCallback();
    }
}

void Registry::bindGUI(const std::function<void()> &onLoad) {
    onLoadCallback = onLoad;

    if (isLoaded()) {
        loadFinished();
    }
}

void Registry::unload() {
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
        output += "#endif // " + protect + "\n";
    }
    return output;
}

std::string Generator::genWithProtectNegate(const std::string &code, const std::string &protect) const {
    std::string output;
    if (code.empty()) {
        return output;
    }
    if (!protect.empty()) {
        output += "#ifndef " + protect + "\n";
    }
    output += code;
    if (!protect.empty()) {
        output += "#endif // " + protect + "\n";
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

std::string Generator::generateDefines() {
    std::string output;

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

#if ( 201907 <= __cpp_lib_three_way_comparison ) && __has_include( <compare> ) && !defined( VULKAN_HPP_NO_SPACESHIP_OPERATOR )
#  define VULKAN_HPP_HAS_SPACESHIP_OPERATOR
#endif

#if ( 201803 <= __cpp_lib_span )
#  define VULKAN_HPP_SUPPORT_SPAN
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

)");

    output += format(R"(
#define VULKAN_HPP_STRINGIFY2( text ) #text
#define VULKAN_HPP_STRINGIFY( text )  VULKAN_HPP_STRINGIFY2( text )
)"      );
    output += genNamespaceMacro(cfg.macro.mNamespace);

    if (cfg.gen.cppModules) {
        output += genNamespaceMacro(cfg.macro.mNamespaceRAII);
    }

    if (cfg.gen.raiiInterop) {
        output += format(R"(
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

)");
    }

    output += "\n";
    return output;
}

std::string Generator::generateHeader() {
    std::string output;

    output += generateDefines();

    output += "#include <vulkan/vulkan.h>\n";

    output += format(R"(
static_assert(VK_HEADER_VERSION == {0}, "Wrong VK_HEADER_VERSION!");
)",  headerVersion);

        output += format(R"(
#include <algorithm>
#include <array>   // ArrayWrapperND
#include <string>  // std::string

#if 17 <= VULKAN_HPP_CPP_VERSION
#  include <string_view>  // std::string_view
#endif

#if !defined( VULKAN_HPP_DISABLE_ENHANCED_MODE )
#  include <tuple>   // std::tie
#  include <vector>  // std::vector
#endif

#if !defined( VULKAN_HPP_NO_EXCEPTIONS )
#  include <system_error>  // std::is_error_code_enum
#endif

#include <assert.h>

#ifndef VULKAN_HPP_EXPERIMENTAL_NO_STRUCT_COMPARE
#if defined( VULKAN_HPP_HAS_SPACESHIP_OPERATOR )
#  include <compare>
#endif
#endif

#if defined( VULKAN_HPP_SUPPORT_SPAN )
//#  include <span>
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

    output += "\n";
    return output;
}

void Generator::generateMainFile(std::string &output) {
    output += generateHeader();

    output += beginNamespace();

    output += format(RES_ARRAY_PROXY);
    // PROXY TEMPORARIES
    output += format(RES_ARRAY_WRAPPER);
    output += format(RES_FLAGS, expIfndef("VULKAN_HPP_EXPERIMENTAL_NO_FLAG_TRAITS"), expEndif("VULKAN_HPP_EXPERIMENTAL_NO_FLAG_TRAITS"));
    if (cfg.gen.expApi) {
        output += format(R"(
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
)");
    }
    else {
        output += format(R"(
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
)");
    }
    output += format(RES_OPTIONAL);
    if (cfg.gen.structChain) {
        output += expIfndef("VULKAN_HPP_EXPERIMENTAL_NO_STRUCT_CHAIN");
        output += format(RES_STRUCT_CHAIN);
        output += expEndif("VULKAN_HPP_EXPERIMENTAL_NO_STRUCT_CHAIN");
    }
    // UNIQUE HANDLE
    output += generateDispatch();
    output += format(RES_BASE_TYPES);

    output += endNamespace();
    output += "#include \"vulkan20_enums.hpp\"\n";
    output += format(
R"(#if !defined( VULKAN_HPP_NO_TO_STRING )
#  include "vulkan20_to_string.hpp"
#endif
)"  );

    output += generateErrorClasses();
    output += "\n";
    output += format(RES_RESULT_VALUE);

        output += endNamespace();
        output += "#include \"vulkan20_handles.hpp\"\n";
        output += "#include \"vulkan20_structs.hpp\"\n";
        output += "#  ifndef VULKAN_HPP_EXPERIMENTAL_NO_VK_FUNCS\n";
        output += "#include \"vulkan20_funcs.hpp\"\n";
        output += "#  endif // VULKAN_HPP_EXPERIMENTAL_NO_VK_FUNCS\n";
        /* individual platforms
        for (const auto &k : platformOutput.segments) {
            const std::string &protect = k.first;
            const std::string &suffix = k.second.filename;
            const std::string &content = k.second.content;
            if (content.empty()) {
                continue;
            }
            output += "#if defined( " + protect + " )\n";
            output += "#include \"vulkan20" + suffix + ".hpp\"\n";
            output += "#endif\n";
        }
        */

        std::string ifdef;
        for (const auto &k : platformOutput.segments) {
            const std::string &protect = k.first;
            const std::string &content = k.second;

            if (!protect.empty() && !content.empty()) {
                ifdef += "    defined( " + protect + " ) ||\\\n";
            }
        }
        if (!ifdef.empty()) {
            strStripPrefix(ifdef, "    ");
            strStripSuffix(ifdef, " ||\\\n");
            output += format(R"(
#if {0}
#include "vulkan20_platforms.hpp"
#endif
)", ifdef);
        }


        if (cfg.gen.structChain) {
            output += "#ifndef VULKAN_HPP_EXPERIMENTAL_NO_STRUCT_CHAIN\n";
            output += beginNamespace();
            generateStructChains(output);
            output += endNamespace();
            output += "#endif // VULKAN_HPP_EXPERIMENTAL_NO_STRUCT_CHAIN\n";
        }

        // DYNAMIC LOADER

#ifdef INST
        output += Inst::mainFileEnd();
#endif

}

void Generator::generateFiles(std::filesystem::path path) {

    std::cout << "gen files " << '\n';

    bool separate = separatePlatforms();
    std::string prefix = "vulkan20";
    std::string ext = ".hpp";

    GenOutput out{prefix, ext, path};
    if (cfg.gen.cppModules) {
        ext = ".cxx";
    }

    auto &enums = out.addFile("_enums");
    auto &to_string = out.addFile("_to_string");
    auto &handles = out.addFile("_handles");
    auto &smart_handles = out.addFile("_smart");
    auto &handles_forward = out.addFile("_handles_forward");
    auto &structs = out.addFile("_structs");
    auto &funcs = out.addFile("_funcs");
    auto &raii = out.addFile("_raii");
    auto &raii_forward = out.addFile("_raii_forward");
    auto &raii_funcs = out.addFile("_raii_funcs");
    auto &platforms = out.addFile("_platforms");
    auto &output = out.addFile("", Namespace::NONE, ext);

    generateEnums(enums, to_string);

    generateHandles(handles, smart_handles, handles_forward, out);
    structs = generateStructs();
    generateRAII(raii, raii_forward,out);
    funcs = beginNamespace();
    funcs += outputFuncs.get(separate);
    funcs += "#ifndef VULKAN_HPP_DISABLE_ENHANCED_MODE\n";
    funcs += outputFuncsEnhanced.get(separate);
    funcs += "#endif // VULKAN_HPP_DISABLE_ENHANCED_MODE\n";
    funcs += endNamespace();
    if (!cfg.gen.cppModules) {
        generateFuncsRAII(raii_funcs);
    }

    for (const auto &o : outputFuncs.segments) {
        const auto &key = o.first;
        if (key.empty()) {
            continue;
        }
        platformOutput.segments[key] += o.second.get(*this);

    }
    platforms = beginNamespace();
    platforms += platformOutput.get();
    platforms += endNamespace();


    if (cfg.gen.cppModules) {

        std::string def = generateDefines();
        auto def_name = out.createFilename("_defines", ".hpp");

        //global module fragment
        output += "// This header is generated from the Khronos Vulkan XML API Registry.\n\n";
        output += "module;\n\n";

        output += "#include \"" + def_name + "\"\n";
        output += "#include <vulkan/vulkan.h>\n";

        output += format(R"(
static_assert(VK_HEADER_VERSION == {0}, "Wrong VK_HEADER_VERSION!");
)",  headerVersion);

        output += format(R"(
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
)");
        output += format(R"(
#include <algorithm>
#include <array>   // ArrayWrapperND
#include <string>  // std::string

#if 17 <= VULKAN_HPP_CPP_VERSION
#  include <string_view>  // std::string_view
#endif

#if !defined( VULKAN_HPP_DISABLE_ENHANCED_MODE )
#  include <tuple>   // std::tie
#  include <vector>  // std::vector
#endif

#if !defined( VULKAN_HPP_NO_EXCEPTIONS )
#  include <system_error>  // std::is_error_code_enum
#endif

#include <assert.h>

#if defined( VULKAN_HPP_HAS_SPACESHIP_OPERATOR )
#  include <compare>
#endif

#if defined( VULKAN_HPP_SUPPORT_SPAN )
//#  include <span>
#endif
)"      );

        output += "#if !defined( VULKAN_HPP_NO_TO_STRING )\n";
        output += generateToStringInclude();
        output += "#endif // VULKAN_HPP_NO_TO_STRING\n";

        output += "#ifndef VULKAN_HPP_EXPERIMENTAL_NO_STRUCT_COMPARE\n";
        output += generateStructsInclude();
        // TODO include <compare>
        output += "#endif // VULKAN_HPP_EXPERIMENTAL_NO_STRUCT_COMPARE\n";

        output += format("export module {NAMESPACE};\n\n");

        output += generateForwardInclude(out);
/*
        output += format(R"(
import <algorithm>
import <array>   // ArrayWrapperND
import <string>  // std::string

#if 17 <= VULKAN_HPP_CPP_VERSION
import <string_view>  // std::string_view
#endif

#if !defined( VULKAN_HPP_DISABLE_ENHANCED_MODE )
import <tuple>   // std::tie
import <vector>  // std::vector
#endif

#if !defined( VULKAN_HPP_NO_EXCEPTIONS )
import <system_error>  // std::is_error_code_enum
#endif

import <assert.h>

#if defined( VULKAN_HPP_HAS_SPACESHIP_OPERATOR )
import <compare>
#endif

#if defined( VULKAN_HPP_SUPPORT_SPAN )
//import <span>
#endif

)"      );
*/

        output += beginNamespace();

        output += format(RES_ARRAY_PROXY);
        // PROXY TEMPORARIES
        output += format(RES_ARRAY_WRAPPER);
        output += format(RES_FLAGS, expIfndef("VULKAN_HPP_EXPERIMENTAL_NO_FLAG_TRAITS"), expEndif("VULKAN_HPP_EXPERIMENTAL_NO_FLAG_TRAITS"));
        output += format(RES_OPTIONAL);
        if (cfg.gen.structChain) {
            output += expIfndef("VULKAN_HPP_EXPERIMENTAL_NO_STRUCT_CHAIN");
            output += format(RES_STRUCT_CHAIN);
            output += expEndif("VULKAN_HPP_EXPERIMENTAL_NO_STRUCT_CHAIN");
        }
        // UNIQUE HANDLE
        // output += generateDispatch();
        output += format(RES_BASE_TYPES);


        output += enums;
        output += "#if !defined( VULKAN_HPP_NO_TO_STRING )\n";
        output += to_string;
        output += "#endif // VULKAN_HPP_NO_TO_STRING\n";

        output += generateErrorClasses();
        output += "\n";
        output += format(RES_RESULT_VALUE);

        output += handles;

        output += structs;

        if (cfg.gen.structChain) {
            output += "#ifndef VULKAN_HPP_EXPERIMENTAL_NO_STRUCT_CHAIN\n";
            generateStructChains(output);
            output += "#endif // VULKAN_HPP_EXPERIMENTAL_NO_STRUCT_CHAIN\n";
        }
        // DYNAMIC LOADER

        output += "  " + beginNamespaceRAII(true);

        output += raii;
        output += outputFuncsRAII.get();
        funcs += "#ifndef VULKAN_HPP_DISABLE_ENHANCED_MODE\n";
        funcs += outputFuncsEnhancedRAII.get(separate);
        funcs += "#endif // VULKAN_HPP_DISABLE_ENHANCED_MODE\n";

        output += "  " + endNamespaceRAII();

        if (cfg.gen.raiiInterop) {
            output += "#ifndef VULKAN_HPP_EXPERIMENTAL_NO_INTEROP\n";
            output += outputFuncsInterop.get();
            output += "#endif // VULKAN_HPP_EXPERIMENTAL_NO_INTEROP\n";
        }

        output += endNamespace();

        const auto getFile = [&](const std::string &key) -> const OutputFile& {
            auto it = out.files.find(key);
            if (it == out.files.end()) {
                throw std::runtime_error("Can't get file output");
            }
            return it->second;
        };

        const auto write = [&](const std::string &key, bool addProtect) {
            const auto &f = getFile(key);
            out.writeFile(*this, f.filename, f.content, addProtect);
        };


        write("", false);
        write("_handles_forward", true);
        write("_raii_forward", true);
        out.writeFile(*this, def_name, def, true);

    }
    else {
        generateMainFile(output);

        out.writeFiles(*this);
    }

}

std::string Generator::generateModuleImpl() {
    std::string out;
    // TODO
    return out;
}

void Generator::generateEnumStr(const EnumData &data, std::string &output, std::string &to_string_output) {

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

    if (data.isBitmask) {
        output += genFlagTraits(data, name, to_string_output);
    }
    else {

        to_string_output += genOptional(data, [&](std::string &output) {
            std::string str = to_string.get();
            if (str.empty()) {
                output += format(R"(
{INLINE} std::string to_string({0}) {
return {1};
}
)",
                                 name, "\"(void)\"");
            } else {
                str += format(R"(
  default: return "invalid (" + {NAMESPACE}::toHexString(static_cast<uint32_t>(value)) + {0};)",
                              "\" )\"");

                output += format(R"(
{INLINE} std::string to_string({0} value) {
switch (value) {
{1}
}
}
)",
                                 name, str);
            }
        });
    }
}

void Generator::generateEnum(const EnumData &data, std::string &output, std::string &to_string_output) {
    auto p = data.getProtect();
    if (!p.empty()) {
        // std::cout << "enum with p: " << p << '\n';
        platformOutput.add(data, [&](std::string &output) {
            generateEnumStr(data, output, output);
        });
    }
    else {
        output += genOptional(data, [&](std::string &output) {
            generateEnumStr(data, output, to_string_output);
        });
    }
}

std::string Generator::generateToStringInclude() const {
    if (cfg.gen.expApi) {
        return R"(
#ifdef VULKAN_HPP_EXPERIMENTAL_HEX
#  include <cstdio>   // std::snprintf
#elif __cpp_lib_format
#  include <format>   // std::format
#else
#  include <sstream>  // std::stringstream
#endif
)";
    }
    else {
        return R"(
#if __cpp_lib_format
#  include <format>   // std::format
#else
#  include <sstream>  // std::stringstream
#endif
)";
    }
}

void Generator::generateEnums(std::string &output, std::string &to_string_output) {

    if (verbose)
        std::cout << "gen enums " << '\n';

    if (!cfg.gen.cppModules) {
        to_string_output += generateToStringInclude();
        to_string_output += beginNamespace();
        output += beginNamespace();
    }

    output += expIfndef("VULKAN_HPP_EXPERIMENTAL_NO_TEMPLATES");
    output += format(R"(
  template <typename EnumType, EnumType value>
  struct CppType
  {};
)"  );
    output += expEndif("VULKAN_HPP_EXPERIMENTAL_NO_TEMPLATES");

    if (cfg.gen.expApi) {
        to_string_output += format(R"(
  {INLINE} std::string toHexString( uint32_t value )
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
)");
    }
    else {
        to_string_output += format(R"(
  {INLINE} std::string toHexString( uint32_t value )
  {
#if __cpp_lib_format
    return std::format( "{:x}", value );
#else
    std::stringstream stream;
    stream << std::hex << value;
    return stream.str();
#endif
  }
)");
    }

    std::unordered_set<std::string> generated;
    for (const auto &e : enums) {
        if (generated.contains(e.name)) {
            continue;
        }
        generateEnum(e, output, to_string_output);
        generated.insert(e.name);
    }

    if (!cfg.gen.cppModules) {
        to_string_output += endNamespace();
        output += endNamespace();
    }

    if (verbose)
        std::cout << "gen enums done" << '\n';
}

std::string Generator::genFlagTraits(const EnumData &data,
                                     std::string inherit, std::string &to_string_output) {
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
            output += "        | " + inherit + "::" + m.name + "\n";
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
        flags += "{}";
    }

    output += format(R"(
  using {0} = Flags<{1}>;
)",
                     name, inherit);

    std::string bitmask;
    if (data.isBitmask) {
        bitmask = "static VULKAN_HPP_CONST_OR_CONSTEXPR bool             isBitmask = true;\n    ";
    }
    output += expIfndef("VULKAN_HPP_EXPERIMENTAL_NO_FLAG_TRAITS");
    output += format(R"(
  template <>
  struct FlagTraits<{0}> {
    {1}static VULKAN_HPP_CONST_OR_CONSTEXPR {3} allFlags = {2};
  };
)",
                     inherit, bitmask, flags, name);
    output += expEndif("VULKAN_HPP_EXPERIMENTAL_NO_FLAG_TRAITS");

    if (str.empty()) {
        to_string_output += format(R"(
  {INLINE} std::string to_string({0}) {
    return "{}";
  }
)",
                                   name, inherit, str);
    } else {
        to_string_output += format(R"(
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

//    if (data.members.empty()) {
//        return output;
//    }
/*
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
*/
    return output;
}

std::string Generator::generateDispatch() {
    std::string output;
    output += generateDispatchLoaderBase();
    output += "#if !defined( VK_NO_PROTOTYPES )\n";
    output += generateDispatchLoaderStatic();
    output += "#endif // VK_NO_PROTOTYPES\n";
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
)");
    return output;
}

std::string Generator::generateErrorClasses() {
    std::string output;

    if (!cfg.gen.cppModules) {
        output += beginNamespace();
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

    HandleData empty{*this};
    const auto genCommand = [&](CommandData &c){
        output += genOptional(c, [&](std::string &output) {
            ClassCommandData d{this, &empty, c};
            MemberContext ctx{.ns = Namespace::VK, .disableDispatch = true, .disableAllocatorRemoval = true};
            MemberResolverStaticDispatch r{*this, d, ctx};

            output += r.temporary();

        });
    };

    if (cfg.gen.vulkanCommands) {
        for (auto &command : commands) {
            genCommand(command);
        }
    }
    else {
        for (auto &command : staticCommands) {
            genCommand(command);
        }
    }

    output += "  };\n";
    output += "//#endif\n";
    return output;
}

std::string Generator::generateStructDecl(const StructData &d) {
    return genOptional(d, [&](std::string &output) {        

        if (d.type == StructData::VK_STRUCT) {
            output += "  struct " + d.name + ";\n";
        } else {
            output += "  union " + d.name + ";\n";
        }

        for (auto &a : d.aliases) {
            output += "  using " + strStripVk(a) + " = " + d.name + ";\n";
        }
    });
}

std::string Generator::generateClassDecl(const HandleData &data, const std::string &name) {
    return genOptional(data, [&](std::string &output) {
        output += "  class " + name + ";\n";
    });
}

std::string Generator::generateClassDecl(const HandleData &data) {
    return generateClassDecl(data, data.name);
}

std::string Generator::generateClassString(const std::string &className,
                                           const GenOutputClass &from, Namespace ns) {
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

    std::string funcs;
    if (ns == Namespace::VK) {
        std::string temp = from.sFuncs.get();
        if (!temp.empty()) {
            funcs += expIfndef("VULKAN_HPP_EXPERIMENTAL_NO_VK_FUNCS");
            funcs += temp;
            funcs += expEndif("VULKAN_HPP_EXPERIMENTAL_NO_VK_FUNCS");
        }
    }
    else {
        funcs = from.sFuncs.get();
    }
    std::string funcsEnhanced = from.sFuncsEnhanced.get();
    if (!funcsEnhanced.empty()) {
        funcs += expIfndef("VULKAN_HPP_DISABLE_ENHANCED_MODE");
        funcs += funcsEnhanced;
        funcs += expEndif("VULKAN_HPP_DISABLE_ENHANCED_MODE");
    }

    addSection("public", from.sPublic.get() + funcs);
    addSection("private", from.sPrivate.get());
    addSection("protected", from.sProtected.get());
    output += "  };\n";
    return output;
}

std::string Generator::generateForwardInclude(GenOutput &out) const {
    std::string output;
    output += format(R"(
#include "{0}"
)", out.getFilename("_handles_forward"));

    if (cfg.gen.raiiInterop) {
        output += format(R"(
#include "{0}"
)",     out.getFilename("_raii_forward"));
    }

    return output;
}

void Generator::generateHandles(std::string &output, std::string &output_smart, std::string &output_forward, GenOutput &out) {
    if (verbose)
        std::cout << "gen handles " << '\n';

    output_forward += beginNamespace();

    for (HandleData &e : handles.ordered) {
        output_forward += generateClassDecl(e);
    }
    if (cfg.gen.smartHandles && !cfg.gen.cppModules) {
        for (HandleData &e : handles.ordered) {
            if (e.uniqueVariant()) {
                output_forward += generateClassDecl(e, "Unique" + e.name);
            }
        }
    }

    output_forward += endNamespace();

    if (!cfg.gen.cppModules) {
        output += generateForwardInclude(out);
    }

//    output += format(R"(
//#include "{0}"
//)", out.getFilename("_handles_forward"));
//
//    if (cfg.gen.raiiInterop) {
//        output += format(R"(
//#include "{0}"
//)", out.getFilename("_raii_forward"));
//    }

    if (!cfg.gen.cppModules) {
        output += beginNamespace();
    }

    for (const StructData &e : structs.ordered) {
        output += generateStructDecl(e);
    }

    output += expIfndef("VULKAN_HPP_EXPERIMENTAL_NO_TEMPLATES");
    output += format(R"(
  template <typename Type>
  struct isVulkanHandleType
  {
    static VULKAN_HPP_CONST_OR_CONSTEXPR bool value = false;
  };
)");
    output += expEndif("VULKAN_HPP_EXPERIMENTAL_NO_TEMPLATES");

    if (cfg.gen.internalFunctions) {
        std::string spec;
        if (!cfg.gen.cppModules) {
            spec = "static";
        }

        output += format(R"(
  namespace internal {

    template<typename T, typename V, typename S, typename PFN, typename... Args>
    {0} inline std::vector<T> createArrayVoidPFN(const PFN pfn, const char *const msg, Args&&... args) {
        std::vector<T> data;
        S count;
        pfn(std::forward<Args>(args)..., &count, nullptr);

        data.resize( count );

        pfn(std::forward<Args>(args)..., &count, std::bit_cast<V*>(data.data()));

        if (count < data.size()) {
            data.resize( count );
        }

        return data;
    }

    template<typename T, typename V, typename S, typename PFN, typename... Args>
    {0} inline typename ResultValueType<std::vector<T>>::type createArray(const PFN pfn, const char *const msg, Args&&... args) {
        std::vector<T> data;
        S count;
        VkResult result;

        do {
          result = pfn(std::forward<Args>(args)..., &count, nullptr);
          if (result == VK_SUCCESS && count) {
            data.resize( count );
            result = pfn(std::forward<Args>(args)..., &count, std::bit_cast<V*>(data.data()));
          }
        } while (result == VK_INCOMPLETE);

        resultCheck(static_cast<Result>(result), msg);
        if (count < data.size()) {
            data.resize( count );
        }

        return createResultValueType(static_cast<Result>(result), data);
    }

  }  // namespace internal

)", spec);
    }

    if (!cfg.gen.cppModules) {
        // static commands

        HandleData empty{*this};
        GenOutputClass out{*this};
        for (auto &c : staticCommands) {            
            ClassCommandData d{this, &empty, c};
            MemberContext ctx{.ns = Namespace::VK, .isStatic = true};
            generateClassMember(d, ctx, out, outputFuncs, outputFuncsEnhanced);
        }
        output += out.sPublic.get();
    }

    for (HandleData &e : handles.ordered) {
        // std::cout << "gen class " << e.name << '\n';
            output += generateClass(e, outputFuncs, outputFuncsEnhanced);
    }

    if (!cfg.gen.cppModules) {
        generateUniqueHandles(output_smart);

        output += "#ifndef  VULKAN_HPP_NO_SMART_HANDLE\n";
        output += format("#include \"{0}\"\n", out.getFilename("_smart"));
        output += "#endif // VULKAN_HPP_NO_SMART_HANDLE\n";
    }

    if (!cfg.gen.cppModules) {
        output += endNamespace();
    }

    if (verbose)
        std::cout << "gen handles done" << '\n';

}

void Generator::generateUniqueHandles(std::string &output) {
    if (!cfg.gen.smartHandles || cfg.gen.cppModules) {
        return;
    }
    // output += "#ifndef  VULKAN_HPP_NO_SMART_HANDLE\n";
    for (HandleData &e : handles.ordered) {
        if (e.uniqueVariant()) {
            output += generateUniqueClass(e, outputFuncs);
        }
    }
    // output += "#endif // VULKAN_HPP_NO_SMART_HANDLE\n";
}

std::string Generator::generateStructsInclude() const {
    return "#include <cstring>  // strcmp\n";
}

std::string Generator::generateStructs() {
    UnorderedOutput output{*this};

    for (const StructData &e : structs.ordered) {
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
    std::string out;
    if (!cfg.gen.cppModules) {
        out += '\n';
        out += expIfndef("VULKAN_HPP_EXPERIMENTAL_NO_STRUCT_COMPARE");
        out += generateStructsInclude();
        out += expEndif("VULKAN_HPP_EXPERIMENTAL_NO_STRUCT_COMPARE");
        out += '\n';
        out += beginNamespace();
    }
//    if (!cfg.gen.expApi) {
//        out += "extern \"C\" {\n";
//    }
    out += output.get();
//    if (!cfg.gen.expApi) {
//        out += "}\n";
//    }

    if (!cfg.gen.cppModules) {
        out += endNamespace();
    }

    return out;
}

void Generator::generateStructChains(std::string &output) {
    UnorderedOutput out{*this};
    for (const StructData &s : structs.ordered) {
        if (s.extends.empty()) {
            continue;
        }
        out.add(s, [&](std::string &output){
            for (const auto &e : s.extends) {
                output += format(R"(
  template <>
  struct StructExtends<{0}, {1}>
  {
    enum
    {
      value = true
    };
  };)",         s.name, e);
            }
            output += "\n";
        });
    }
    output += out.get();
}

bool Generator::generateStructConstructor(const Registry::StructData &data, bool transform, std::string &output) {

    InitializerBuilder init{"      "};
    std::string args;
    std::string pNextArg;
    std::string templ;
    bool hasProxy = false;

    for (const auto &p : data.members) {
        std::string id = p->identifier() + "_";

        bool toProxy = transform && p->hasLengthVar();
        if (p->hasLengthVar()) {
            hasProxy = true;
        }

        if (p->type() == "StructureType") {
            if (data.name != "BaseOutStructure" && data.name != "BaseInStructure") {
                continue;
            }
            args += p->fullType();
            args += " ";
            args += id;
            args += " = ";
            args += cfg.macro.mNamespace->get();
            args += "::StructureType::eApplicationInfo";
        }
        else if (p->identifier() == "pNext") {
            pNextArg += p->fullType();
            pNextArg += " ";
            pNextArg += id;
            pNextArg += " = nullptr";
        }
        else {
            if (toProxy) {
                VariableData var(*p);
                var.removeLastAsterisk();
                if (var.type() == "void" && !var.isPointer()) {
                    var.setType("T");
                    templ = "    template <typename T>\n";
                }
                args += format("{NAMESPACE}::ArrayProxyNoTemporaries<{0}> const & {1}", var.fullType(), id);
            } else {
                args += p->fullType();
                args += " ";
                args += id;
                if (!transform) {
                    args += " = {}";
                }
            }
            args += ", ";
        }

        std::string lhs = p->identifier();
        if (toProxy) {
            init.append(lhs, id + ".data()");
        }
        else {
            std::string rhs = id;
            const auto &vars = p->getArrayVars();
            if (!vars.empty() && transform) {
                rhs = "static_cast<uint32_t>(";
                // std::cout << "> array vars: " << vars.size() << std::endl;
                for (size_t i = 0; i < vars.size(); ++i) {
                    const auto &v = vars[i];
                    const std::string &id = v->identifier();
                    if (i != vars.size() - 1) {
                        rhs += " !";
                        rhs += id;
                        rhs += "_";
                        rhs += ".empty()? ";
                        rhs += id;
                        rhs += "_.size() :\n";
                    }
                    else {
                        rhs += id;
                        rhs += "_.size()";
                    }
                    if (v->type() == "void" && !v->isPointer()) {
                        rhs += " * sizeof(T)";
                    }
                }
                rhs += ")";
            }
            init.append(lhs, rhs);
        }
    }

    strStripSuffix(args, ", ");
    if (!pNextArg.empty()) {
        if (!args.empty()) {
            args += ", ";
        }
        args += pNextArg;
    }

    output += templ;
    output += "    ";
    if (!transform) {
        output += cfg.macro.mConstexpr->get();
        output += " ";
    }
    output += data.name + "( " + args + " )\n";
    output += "      " + init.string();
    output += "\n    {}\n";

    return hasProxy;
}

std::string Generator::generateStruct(const StructData &data) {
    std::string output;

        bool genConstructor = false;
        bool genSetters = false;
        bool genSettersProxy = false;
        bool containsFloatingPoints = false;
        bool genCompareOperators = data.type == StructData::VK_STRUCT;


        bool hasSetters = false;
        for (const auto &m : data.members) {
            if (m->type() == "StructureType") {
                continue;
            }
            hasSetters = true;
        }

        if (data.type == StructData::VK_STRUCT) {
            genConstructor = cfg.gen.structConstructor;
            genSetters = cfg.gen.structSetters;
            genSettersProxy = cfg.gen.structSettersProxy;
        } else {
            genConstructor = cfg.gen.unionConstructor;
            genSetters = cfg.gen.unionSetters;
            genSettersProxy = cfg.gen.unionSettersProxy;
        }

        genSetters &= !data.returnedonly;
        genSetters &= hasSetters;
        genSettersProxy &= !data.returnedonly;
        genSettersProxy &= hasSetters;

        std::string members;
        std::string funcs;
        std::string structureType;
        if (!data.structTypeValue.empty()) {
            structureType = "StructureType::" + data.structTypeValue;
        }

        for (const auto &m : data.members) {
            if (data.type == StructData::VK_STRUCT) {
                std::string assignment = "{}";
                const auto &type = m->original.type();
                if (type == "VkStructureType") {
                    assignment = structureType.empty()
                                     ?
                                     /*cfg.macro.mNamespace.get() + "::"*/
                                     "StructureType::eApplicationInfo"
                                     : structureType;
                }                


                m->setAssignment(" = " + assignment);
                members += "    " + m->toStructStringWithAssignment() + ";\n";

                const auto &s = structs.find(type);
                if (s != structs.end()) {
                    if (s->type == StructData::VK_UNION) {
                        genCompareOperators = false;
                    }
                }
                if ((type == "float" || type == "double") && !m->isPointer()) {
                    containsFloatingPoints = true;
                }

            } else {
                members += "    " + m->toStructString() + ";\n";
            }
            if (m->hasArrayLength()) {
                m->setSpecialType(VariableData::TYPE_ARRAY);
            }
        }

        // setters
        if (genSetters || genSettersProxy) {
            if (data.type == StructData::VK_STRUCT) {
                funcs += "#if !defined( VULKAN_HPP_NO_STRUCT_SETTERS )\n";
            }
            else {
                funcs += "#if !defined( VULKAN_HPP_NO_UNION_SETTERS )\n";
            }
        }
        if (genSetters) {
            for (const auto &m : data.members) {
                if (m->type() == "StructureType") {
                    continue;
                }
                auto var = VariableData(*m);
                var.setIdentifier(m->identifier() + "_");

                std::string id = m->identifier();
                std::string name = strFirstUpper(id);
                std::string arg = var.toString();
                funcs += format(R"(
    {CONSTEXPR_14} {0}& set{1}({2}) {NOEXCEPT} {
      {3} = {4};
      return *this;
    }
            )"          , data.name, name, arg, id, var.identifier() );
            }
        }

        bool hasArrayMember = false;
        // arrayproxy setters
        if (genSettersProxy) {
            for (const auto &m : data.members) {
                if (m->hasLengthVar()) {
                    hasArrayMember = true;
                    auto var = VariableData(*m);
                    std::string name = var.identifier();
                    // name conversion
                    if (name.size() >= 3 && name.starts_with("pp") &&
                        std::isupper(name[2])) {
                        name = name.substr(1);
                    } else if (name.size() >= 2 && name.starts_with("p") &&
                               std::isupper(name[1])) {
                        name = name.substr(1);
                    }
                    name = strFirstUpper(name);

                    var.setIdentifier(m->identifier() + "_");
                    var.removeLastAsterisk();

                    std::string id = m->identifier();
                    std::string modif;
                    if (var.original.type() == "void" && !var.original.isConstSuffix()) {
                        funcs += "    template <typename DataType>\n";
                        var.setType("DataType");
                        modif = " * sizeof(DataType)";
                    }                                       

                    std::string arg = "ArrayProxyNoTemporaries<" + var.fullType() + "> const &" + var.identifier();
                    funcs += format(R"(
    {0}& set{1}({2}) {NOEXCEPT} {
      {4} = static_cast<uint32_t>({3}.size(){6});
      {5} = {3}.data();
      return *this;
    }
                )"          , data.name, name, arg, var.identifier(), m->getLengthVar()->identifier(), m->identifier(), modif);
                }
            }
        }
        if (genSetters || genSettersProxy) {
            if (data.type == StructData::VK_STRUCT) {
                funcs += "#endif // VULKAN_HPP_NO_STRUCT_SETTERS\n";
            }
            else {
                funcs += "#endif // VULKAN_HPP_NO_UNION_SETTERS\n";
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

        }

        // constructor
        if (genConstructor) {
            if (data.type == StructData::VK_STRUCT) {


                output += "#if !defined( VULKAN_HPP_NO_STRUCT_CONSTRUCTORS )\n";

                bool hasProxy = generateStructConstructor(data, false, output);

                if (hasProxy) {
                    output += "#  if !defined( VULKAN_HPP_DISABLE_ENHANCED_MODE )\n";
                    generateStructConstructor(data, true, output);
                    output += "#  endif // VULKAN_HPP_DISABLE_ENHANCED_MODE \n";
                }
                output += "#endif // VULKAN_HPP_NO_STRUCT_CONSTRUCTORS\n";
            }
            else {
                std::map<std::string, uint8_t> types;
                for (const auto &m : data.members) {
                    auto it = types.find(m->type());
                    if (it != types.end()) {
                        it->second++;
                        continue;
                    }
                    types.emplace(m->type(), 1);
                }

                output += "#if !defined( VULKAN_HPP_NO_UNION_CONSTRUCTORS )\n";
                bool first = true;
                for (const auto &m : data.members) {
                    if (m->original.type() == "VkBool32") {
                        continue;
                    }
                    const auto &type = m->type();
                    std::string id = m->identifier();

                    auto it = types.find(type);
                    // multiple same types, generate only once
                    if (it != types.end()) {
                        if (it->second == 0) {
                            continue;
                        }
                        if (it->second > 1) {
                            it->second = 0;
                            id = strFirstLower(type);
                        }
                    }

                    VariableData var = *m;
                    var.setIdentifier(id + "_");
                    id = m->identifier();

                    std::string arg = var.toString();
                    std::string assignment;
                    if (first) {
                        assignment = " = {}";
                        first = false;
                    }
                    funcs += format(R"(
    {CONSTEXPR_14} {0}({1}{2}) : {3}( {4} ) {}
                )"          , data.name, arg, assignment, id, var.identifier());
                }
                output += "#endif // VULKAN_HPP_NO_UNION_CONSTRUCTORS\n";
            }            
        }

        if (data.type == StructData::VK_STRUCT) {
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

        // operator {0}*() { return this; }
        output += format(R"(

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

        if (cfg.gen.structCompareOperators && genCompareOperators) {

            // https://github.com/KhronosGroup/Vulkan-Hpp/blob/main/VulkanHppGenerator.cpp
            static const std::set<std::string> simpleTypes = { "char",   "double",  "DWORD",    "float",    "HANDLE",  "HINSTANCE", "HMONITOR",
                                                               "HWND",   "int",     "int8_t",   "int16_t",  "int32_t", "int64_t",   "LPCWSTR",
                                                               "size_t", "uint8_t", "uint16_t", "uint32_t", "uint64_t",
                                                               "void"};

            std::string prefix;
            std::string compareMembers;
            std::string spaceshipMembers;
            bool nonDefaultCompare = false;
            std::string ordering;
            if (containsFloatingPoints) {
                ordering = "std::partial_ordering";
            }
            else {
                ordering = "std::strong_ordering";
            }

            for (const auto &m : data.members) {
                const auto &id = m->identifier();
                const auto &type = m->original.type();
                if (m->getNamespace() != Namespace::VK && simpleTypes.find(type) == simpleTypes.end() && enums.find(type) == enums.end()) {
                    nonDefaultCompare = true;

                    compareMembers += prefix +
                        format("( memcmp( &{0}, &rhs.{0}, sizeof( {1} ) ) == 0 )", id, type);

                    static const std::string spaceshipMemberTemplate =
        R"(      if ( auto cmp = memcmp( &{0}, &rhs.{0}, sizeof( {1} ) ); cmp != 0 )
        return ( cmp < 0 ) ? {2}::less : {2}::greater;
)";
                    spaceshipMembers += format(spaceshipMemberTemplate, id, type, ordering);
                }
                else if (type == "char" && !m->getLenAttrib().empty()) {
                    nonDefaultCompare = true;

                    if ( m->lenExpressions.size() == 1 ) {
                        compareMembers += prefix +
                            format("( ( {0} == rhs.{0} ) || ( strcmp( {0}, rhs.{0} ) == 0 ) )", id, type);

                        static const std::string spaceshipMemberTemplate =
          R"(     if ( {0} != rhs.{0} )
        if ( auto cmp = strcmp( {0}, rhs.{0} ); cmp != 0 )
          return ( cmp < 0 ) ? {1}::less : {1}::greater;
)";
                        spaceshipMembers += format(spaceshipMemberTemplate, id, ordering);
                    }
                    else {
                        //assert( member.lenExpressions[1] == "null-terminated" );
                        //assert( ( member.type.prefix == "const" ) && ( member.type.postfix == "* const *" ) );
                        static const std::string compareMemberTemplate =
                            R"(std::equal( {1}, {1} + {0}, rhs.{1}, []( char const * left, char const * right ) { return ( left == right ) || ( strcmp( left, right ) == 0 ); } ))";

                        compareMembers += prefix + format(compareMemberTemplate, m->lenExpressions[0], id);

                        static const std::string spaceshipMemberTemplate =
R"(      for ( size_t i = 0; i < {0}; ++i )
      {
        if ( {1}[i] != rhs.{1}[i] )
          if ( auto cmp = strcmp( {1}[i], rhs.{1}[i] ); cmp != 0 )
            return cmp < 0 ? {2}::less : {2}::greater;
      }
                       )";
                        spaceshipMembers += format(spaceshipMemberTemplate, m->lenExpressions[0], id, ordering);
                    }
                }
                else {
                    compareMembers += prefix + "( " + id + " == rhs." + id + " )";
                    spaceshipMembers += "      if ( auto cmp = " + id + " <=> rhs." + id + "; cmp != 0 ) return cmp;\n";
                }
                prefix = "\n          && ";
            }

            std::string compareBody, spaceshipOperator;
            if (nonDefaultCompare) {
                compareBody = "      return " + compareMembers + ";";

                if (!containsFuncPointer(data)) {
                    static const std::string spaceshipOperatorTemplate =
R"(    {2} operator<=>( {0} const & rhs ) const VULKAN_HPP_NOEXCEPT
    {
{1}
      return {2}::equivalent;
    }
)";

                    spaceshipOperator = format(spaceshipOperatorTemplate, data.name, spaceshipMembers, ordering);
                }
            }
            else {
                static const std::string compareBodyTemplate =
R"(#if defined( VULKAN_HPP_USE_REFLECT )
      return this->reflect() == rhs.reflect();
#else
      return {0};
#endif // VULKAN_HPP_USE_REFLECT)";
                compareBody = format(compareBodyTemplate, compareMembers);

                if (!containsFuncPointer(data)) {
                    spaceshipOperator = "    auto operator<=>( " + data.name + " const & ) const = default;";
                }
            }

            output += expIfndef("VULKAN_HPP_EXPERIMENTAL_NO_STRUCT_COMPARE");
            if (!spaceshipOperator.empty()) {
                output += "#  if defined( VULKAN_HPP_HAS_SPACESHIP_OPERATOR )\n";
                output += spaceshipOperator + "\n";
                output += "#  else\n";
            }
            output += format(R"(
    bool operator==( {0} const & rhs ) const {NOEXCEPT}
    {
{1}
    }
    bool operator!=( {0} const & rhs ) const {NOEXCEPT}
    {
      return !operator==( rhs );
    }
)",         data.name, compareBody);
            if (!spaceshipOperator.empty()) {
                output += "#  endif\n";
            }
            output += expEndif("VULKAN_HPP_EXPERIMENTAL_NO_STRUCT_COMPARE");

        }

        output += members;
        output += "  };\n\n";

        if (data.type == StructData::VK_STRUCT && !structureType.empty()) {
            output += expIfndef("VULKAN_HPP_EXPERIMENTAL_NO_TEMPLATES");
            output += format(R"(
  template <>
  struct CppType<StructureType, {0}> {
    using Type = {1};
  };
)",
                                 structureType, data.name);
            output += expEndif("VULKAN_HPP_EXPERIMENTAL_NO_TEMPLATES");
        }


        for (const auto &a : data.aliases) {
            output += "  using " + a + " = " + data.name + ";\n";
        }
    //});
    return output;
}

std::string Generator::generateIncludeRAII(GenOutput &out) const {
    return format(R"(
#include <memory>
#include <vector>
#include <utility>  // std::exchange, std::forward
#include "{0}"
#include "{1}"
)",     out.getFilename(""), out.getFilename("_raii_forward"));
}

void Generator::generateRAII(std::string &output, std::string &output_forward, GenOutput &out) {

    output_forward += genNamespaceMacro(cfg.macro.mNamespaceRAII);
    output_forward += beginNamespace();
    output_forward += "  " + beginNamespaceRAII(true);

    if (!cfg.gen.cppModules) {


        output += generateIncludeRAII(out);
//        output += format(R"(
//#include <memory>
//#include <utility>  // std::exchange, std::forward
//#include "{0}"
//#include "{1}"
//)",
//       out.getFilename(""), out.getFilename("_raii_forward"));

        output += beginNamespace();
        output += "  " + beginNamespaceRAII();
        //output += "  using namespace " + cfg.macro.mNamespace->get() + ";\n";
    }

    output += format(RES_RAII);

    if (cfg.gen.internalFunctions) {
        std::string spec;
        if (!cfg.gen.cppModules) {
                spec = "static";
        }

        output += format(R"(
  namespace internal {

    template<typename T, typename V, typename S, typename PFN, typename... Args>
    {0} inline std::vector<T> createArrayVoidPFN(const PFN pfn, const char *const msg, Args&&... args) {
        std::vector<T> data;
        S count;
        pfn(std::forward<Args>(args)..., &count, nullptr);

        if (count) {
          data.resize( count );
          pfn(std::forward<Args>(args)..., &count, std::bit_cast<V*>(data.data()));
        }
        if (count < data.size()) {
            data.resize( count );
        }

        return data;
    }

    template<typename T, typename V, typename S, typename PFN, typename... Args>
    {0} inline std::vector<T> createArray(const PFN pfn, const char *const msg, Args&&... args) {
        std::vector<T> data;
        S count;
        VkResult result;

        do {
          result = pfn(std::forward<Args>(args)..., &count, nullptr);
          if (result == VK_SUCCESS && count) {
            data.resize( count );
            result = pfn(std::forward<Args>(args)..., &count, std::bit_cast<V*>(data.data()));
          }
        } while (result == VK_INCOMPLETE);

        resultCheck(static_cast<Result>(result), msg);
        if (count < data.size()) {
            data.resize( count );
        }

        return data;
    }
)"/*
    template<typename T, typename Parent, typename PFN, typename... Types>
    static inline std::vector<T> createHandlesRAII(T &obj, const Parent &parent, const PFN pfn, size_t size, const char *const msg, Types... args) {
        std::vector<typename T::CType> data(size);

        VkResult result = pfn(args..., data.data());;

        resultCheck(static_cast<Result>(result), msg);

        obj.reserve(data.size());
        for (auto const &o : data) {
          obj.emplace_back(parent, o);
        }
    }

    template<typename T, typename Parent, typename PFN, typename... Types>
    static inline std::vector<T> createHandlesWithSizeRAII(T &obj, const Parent &parent, const PFN pfn, size_t size, const char *const msg, Types... args) {
        std::vector<typename T::CType> data(size);

        VkResult result = pfn(args..., data.data());;

        resultCheck(static_cast<Result>(result), msg);

        obj.reserve(data.size());
        for (auto const &o : data) {
          obj.emplace_back(parent, o);
        }
    }

    template<typename T, typename Parent, typename Pool, typename PFN, typename... Types>
    static inline std::vector<T> createHandlesWithPoolRAII(T &obj, const Parent &parent, const Pool &pool, const PFN pfn, size_t size, const char *const msg, Types... args) {
        std::vector<typename T::CType> data(size);

        VkResult result = pfn(args..., data.data());;

        resultCheck(static_cast<Result>(result), msg);

        obj.reserve(data.size());
        for (auto const &o : data) {
          obj.emplace_back(parent, o, pool);
        }
    }*/
R"(
  }  // namespace internal

)", spec);
    }

    generateDispatchRAII(output);

    output_forward += generateClassDecl(loader);
    for (HandleData &e : handles.ordered) {
        output_forward += generateClassDecl(e);
    }
    output += generateLoader();

    for (const HandleData &e : handles.ordered) {
        output += generateClassRAII(e);
    }    

    output_forward += endNamespace();
    output_forward += "  " + endNamespaceRAII();

    if (!cfg.gen.cppModules) {

        output += endNamespace();
        output += "  " + endNamespaceRAII();

        output += format(R"(
#include "{0}"
)",     out.getFilename("_raii_funcs"));

    }
}


void Generator::generateFuncsRAII(std::string &output) {
    output = beginNamespace();
    output += "  " + beginNamespaceRAII();
    output += outputFuncsRAII.get();
    output += "#ifndef VULKAN_HPP_DISABLE_ENHANCED_MODE\n";
    output += outputFuncsEnhancedRAII.get();
    output += "#endif // VULKAN_HPP_DISABLE_ENHANCED_MODE\n";
    output += "  " + endNamespaceRAII();
    if (cfg.gen.raiiInterop) {
        output += "#ifndef VULKAN_HPP_EXPERIMENTAL_NO_INTEROP\n";
        output += outputFuncsInterop.get();
        output += "#endif // VULKAN_HPP_EXPERIMENTAL_NO_INTEROP\n";
    }
    output += endNamespace();
}

void Generator::generateDispatchRAII(std::string &output) {
    auto &instance = findHandle("VkInstance");
    auto &device = findHandle("VkDevice");

    const auto gen = [&](const HandleData &h) {
        GenOutputClass out{*this};
        std::string name = h.name + "Dispatcher";
        std::string handle = "NULL";

        std::string getAddr = "vkGetInstanceProcAddr";
        std::string src;
        if (h.name == loader.name) {
            // handle = "NULL";
        }
        else if (h.getAddrCmd.has_value()) {
            getAddr = h.getAddrCmd->name.original;
            handle = strFirstLower(h.name);
            src = ", " + h.name.original + " " + handle;
        }

        out.sPublic += name + "() = default;\n";

        std::string init;
        std::string init2;
        // PFN function pointers
        for (auto d : h.filteredMembers) {            
            const std::string &name = d->name.original;
            out.sPublic += genOptional(*d, [&](std::string &output) {
                output += format("    PFN_{0} {0} = {};\n", name);
            });
            init += genOptional(*d, [&](std::string &output) {
                output += format("      {0} = PFN_{0}( {1}({2}, \"{0}\") );\n", name, getAddr, handle);
            });
            out.sPublic += genOptional(*d, [&](std::string &output) {
                auto alias = d->src->alias;
                if (!alias.empty()) {

                    const auto &cmd = commands.find(alias);
                    if (cmd == commands.end()) {
                        std::cout << ">> error: generateDispatchRAII() cmd not found: " << alias << std::endl;
                    }
                    else {
                        if (!cmd->canGenerate()) {
                            alias = format("PFN_{0}( {1}({2}, \"{3}\") )", name, getAddr, handle, alias);
                        }
                    }

                    init2 += format(R"(      if ( !{0} )
        {0} = {1};
)",                  name, alias);
                }
            });
        }
        out.sPublic += "    PFN_" + getAddr + " " + getAddr + " = {};\n";
        if (h.name == "Instance") {
            std::string name = "vkGetDeviceProcAddr";
            out.sPublic += "    PFN_" + name + " " + name + " = {};\n";
            init += format("      {0} = PFN_{0}( {1}({2}, \"{0}\") );\n", name, getAddr, handle);
        }

        out.sPublic += format(R"(
    {0} (PFN_{1} getProcAddr{2}) : {1}(getProcAddr) {
{3}{4}
    }
)",
                            name, getAddr, src, init, init2);

        output += generateClassString(name, out, Namespace::RAII);

//        if (h.name == "Instance" && cfg.gen.staticInstancePFN) {
//            output += "    static " + name + " instanceDispatcher;\n";
//        }
//        else if (h.name == "Device" && cfg.gen.staticDevicePFN) {
//            output += "    static " + name + " deviceDispatcher;\n";
//        }
    };

    gen(loader);
    gen(instance);
    gen(device);
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

void Generator::generateClassMember(ClassCommandData &m, MemberContext &ctx, GenOutputClass &out,
                                    UnorderedFunctionOutput &funcs, UnorderedFunctionOutput &funcsEnhanced, bool inlineFuncs) {

    MemberGenerator g{*this, m, ctx, out, funcs, funcsEnhanced};
    g.generate();

}

void Generator::generateClassMembers(const HandleData &data, GenOutputClass &out,
                                     UnorderedFunctionOutput &funcs, UnorderedFunctionOutput &funcsEnhanced, Namespace ns, bool inlineFuncs) {
    std::string output;
    if (ns == Namespace::RAII) {
        const auto &className = data.name;
        const auto &handle = data.vkhandle.identifier();
        const std::string &ldr = loader.name;

        const auto &superclass = findHandle("Vk" + data.superclass);
        VariableData super(*this, superclass.name);
        super.setConst(true);

        /*
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
*/

        std::string call;
        if (data.dtorCmd) {
            MemberContext ctx{.ns = ns};
            ClassCommandData d(this, &data, *data.dtorCmd);
            MemberResolverClearRAII r{*this, d, ctx};
            call = r.temporary(handle);

        }

        std::string clear;
        std::string swap;
        data.foreachVars(VariableData::Flags::CLASS_VAR_RAII, [&](const VariableData &v ) {
            clear += "      " + v.identifier() + " = nullptr;\n";
            swap += "      std::swap( " + v.identifier() + ", rhs." + v.identifier() + " );\n";
        });

        output += format(R"(
    inline void {0}::clear() {NOEXCEPT} {
{1}{2}
    }

    inline void {0}::swap({NAMESPACE_RAII}::{0} &rhs) {NOEXCEPT} {
{3}
    }
)",
                         className, call, clear, swap);

    }

    if (!output.empty()) {
        funcs += genOptional(data, [&](std::string &out) { out += output; });
    }

    if (ns == Namespace::VK && !cfg.gen.vulkanCommands) {
        return;
    }
    // wrapper functions
    for (ClassCommandData &m : const_cast<HandleData&>(data).members) {
        if (ns == Namespace::VK && m.src->isIndirect()) {
            continue;
        }
        MemberContext ctx{.ns = ns};
        generateClassMember(m, ctx, out, funcs, funcsEnhanced, inlineFuncs);
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
                          data.name, strFirstLower(data.name), data.vkhandle.identifier());
}

void Generator::generateClassConstructorsRAII(const HandleData &data,
                                              GenOutputClass &out,
                                              UnorderedFunctionOutput &funcs) {
    static constexpr Namespace ns = Namespace::RAII;

    const auto &superclass = data.superclass;
    const std::string &owner = data.ownerhandle;

    const auto genCtor = [&](ClassCommandData &d, auto &parent, bool insert = false) {
        MemberContext ctx{.ns = ns};
        ctx.insertSuperclassVar = insert;
        MemberResolverCtor resolver{*this, d, ctx};

        if (!resolver.hasDependencies) {
            // std::cout << "ctor skipped: class " << data.name << ", p: " <<
            // parent->type() << ", s: " << superclass << '\n';
            return;
        }

        resolver.generate(out.sPublic, funcs);
    };

    for (auto &m : const_cast<HandleData&>(data).ctorCmds) {
//        if (cfg.dbg.methodTags) {
//            out.sPublic += "    /*ctor: " + m.name.original + " */\n";
//        }

        const auto &parent = *m.src->_params.begin()->get();
        if (parent.original.type() != superclass.original) {

            genCtor(m, parent, true);

            if (parent.isHandle()) {
                const auto &handle = findHandle(parent.original.type());
                if (handle.superclass.original != superclass.original) {
                    std::cerr << "ctor: impossible combination" << '\n';
                    continue;
                }
            }
        }

        if (parent.isHandle()) {
            genCtor(m, parent);
        }

    }

    {
        if (cfg.dbg.methodTags) {
            out.sPublic += "    /*handle constructor*/\n";
        }

        std::string parent = strFirstLower(superclass);
        std::string handle = strFirstLower(data.name);
        InitializerBuilder init("        ");

        init.append(data.vkhandle.identifier(), handle);
        if (data.ownerRaii) {
            init.append(data.ownerRaii->identifier(), "&" + parent);
        }

        std::string argDecl;
        std::string argDef;
        std::string dispatcherInit;
        if (!data.isSubclass) {
            if (cfg.gen.staticInstancePFN && data.name == "Instance") {
                dispatcherInit = "dispatcher = {0}Dispatcher( {2}.getDispatcher()->vkGet{0}ProcAddr, {3} );";
            } else if (cfg.gen.staticDevicePFN && data.name == "Device") {
                dispatcherInit = "dispatcher = {0}Dispatcher( {2}.getDispatcher()->vkGet{0}ProcAddr, {3} );";
            } else {
                dispatcherInit = "m_dispatcher.reset( new {0}Dispatcher( {2}.getDispatcher()->vkGet{0}ProcAddr, {3} ) );";
            }
        }
        if (data.secondOwner) {
            std::string id = strFirstLower(data.secondOwner->type());
            argDecl += format(", Vk{0} {1}", data.secondOwner->type(), id);
            argDef += format(", Vk{0} {1}", data.secondOwner->type(), id);
            init.append(data.secondOwner->identifier(), id);
        }
        if (cfg.gen.allocatorParam && data.creationCat != HandleCreationCategory::NONE) {
            argDecl += format(", {NAMESPACE}::Optional<const {NAMESPACE}::AllocationCallbacks> allocator = nullptr");
            argDef += format(", {NAMESPACE}::Optional<const {NAMESPACE}::AllocationCallbacks> allocator");
            init.append(cvars.raiiAllocator.identifier(), "static_cast<const VULKAN_HPP_NAMESPACE::AllocationCallbacks *>( allocator )");
        }
        if (false) { // TODO add class var
            init.append("m_dispacher", "& //getDispatcher()");
        }

        out.sPublic += format(R"(    {INLINE} {0}( {NAMESPACE_RAII}::{1} const & {2}, Vk{0} {3}{4} );)",
            data.name, superclass, parent, handle, argDecl);

        funcs.add(data, [&](std::string &output){
        output += format(R"(  {0}::{0}( {NAMESPACE_RAII}::{1} const & {2}, Vk{0} {3}{4} ){5}
{
)" + dispatcherInit +
        R"(
}
)",
        data.name, superclass, parent, handle, argDef, init.string());
        }, "");
    }

    InitializerBuilder init("        ");
    std::string assign = "\n";

    data.foreachVars(VariableData::Flags::CLASS_VAR_RAII, [&](const VariableData &v ) {
        if (v.identifier() == "m_dispatcher") {
            init.append(v.identifier(), format("rhs.{0}.release()", v.identifier()));
            assign += format("        {0}.reset( rhs.{0}.release() );\n",  v.identifier());
        }
        else {
            init.append(v.identifier(), format("{NAMESPACE_RAII}::exchange(rhs.{0}, {})", v.identifier()));
            assign += format("        {0} = {NAMESPACE_RAII}::exchange(rhs.{0}, {});\n",  v.identifier());
        }
    });

    out.sPublic += format(R"(
    {0}(std::nullptr_t) {NOEXCEPT} {}
    ~{0}() {
        clear();
    }

    {0}() = delete;
    {0}({0} const&) = delete;
    {0}({0}&& rhs) {NOEXCEPT}   {1}
    {
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

//    std::string loadCall;
//    if (data.hasPFNs()) {
//        loadCall = "\n      //loadPFNs(" + owner + ");\n    ";
//    }
}

std::string Generator::generateUniqueClassStr(const HandleData &data,
                                              UnorderedFunctionOutput &funcs, bool inlineFuncs) {
    std::string output;
    if (!data.dtorCmd) {
        std::cerr << "class has no destructor info!" << '\n';
        return "";
    }

    GenOutputClass out{*this};
    const std::string &base = data.name;
    const std::string &className = "Unique" + base;
    const std::string &handle = data.vkhandle.identifier();
    const std::string &superclass = data.superclass;
    bool isSubclass = data.isSubclass;

    ClassCommandData d{this, &data, *data.dtorCmd};
    MemberContext ctx{.ns = Namespace::VK, .inUnique = true};
    MemberResolverUniqueCtor r{*this, d, ctx};

    bool hasAllocation = false;
    for (VariableData &p : r.cmd->params) {
        p.setIgnoreFlag(true);
        p.setIgnoreProto(true);
        if (p.original.type() == "VkAllocationCallbacks") {
            hasAllocation = true;
        }
    }

    std::string destroyCall;
    std::string destroyMethod = data.creationCat == HandleCreationCategory::CREATE ? "destroy" : "free";

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
    }
    else {
        destroyCall = base + "::" + destroyMethod + "(";
        std::string args;
        if (cfg.gen.allocatorParam && hasAllocation) {
            args+= cvars.uniqueAllocator.identifier();
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
        }
        else {
            p.setIgnoreFlag(false);
            p.setIgnoreProto(false);
        }
        if (p.isHandle()) {
            p.setConst(true);
            p.convertToReference();
        }
    }


    std::string pass;
    out.inherits = "public " + base;

    InitializerBuilder copyCtor("        ");
    copyCtor.append(base, "other.release()");

    std::string assignemntOp;
    data.foreachVars(VariableData::Flags::CLASS_VAR_UNIQUE, [&](const VariableData &v){
//        if (v.type() == data.name) {
//            return;
//        }
        out.sPrivate += "    " + v.toClassVar();
        assignemntOp += "\n      " + v.identifier() + " = std::move(other." + v.identifier() + ");";
        copyCtor.append(v.identifier(), "std::move(other." + v.identifier() + ")");
    });

    out.sPublic += "    " + className + "() = default;\n";


    //out.sPublic += "/*\n";
    r.generate(out.sPublic, funcs);
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

    output += generateClassString(className, out, Namespace::VK);

    output += format(R"(
  {INLINE} void swap({0} &lhs, {0} &rhs) {NOEXCEPT} {
  lhs.swap(rhs);
  }

)",
                     className);
    return output;
}

std::string Generator::generateUniqueClass(const HandleData &data,
                                           UnorderedFunctionOutput &funcs) {
    std::string output;
    if (!data.getProtect().empty()) {
        // std::cout << "class " << data.name << " to " << data.getProtect() << '\n';
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


std::string Generator::generateClassStr(const HandleData &data, UnorderedFunctionOutput &funcs, UnorderedFunctionOutput &funcsEnhanced, bool inlineFuncs) {
    std::string output;
    GenOutputClass out{*this};

    const std::string &className = data.name;
    const std::string &classNameLower = strFirstLower(className);
    const std::string &handle = data.vkhandle.identifier();
    const std::string &superclass = data.superclass;

    // std::cout << "Gen class: " << className << '\n';
    //std::cout << "  ->superclass: " << superclass << '\n';

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

    if (cfg.gen.raiiInterop) {

        const auto &superclass = data.superclass;
//        const std::string &owner = data.ownerhandle;

        const auto genCtor = [&](ClassCommandData &d, auto &parent, bool insert = false) {
            MemberContext ctx{.ns = Namespace::RAII};
            ctx.insertSuperclassVar = insert;

            MemberResolverCtor resolver{*this, d, ctx};

            if (!resolver.hasDependencies) {
                std::cout << "ctor skipped: class " << data.name << ", s: " << superclass << '\n';
                return;
            }

            out.sPublic += "// INTER:\n";
            resolver.guard = "VULKAN_HPP_EXPERIMENTAL_NO_INTEROP";
            resolver.constructorInterop = true;
            resolver.generate(out.sPublic, outputFuncsInterop);
        };

        for (auto &m : const_cast<HandleData&>(data).ctorCmds) {
            //        if (cfg.dbg.methodTags) {
            //            out.sPublic += "    /*ctor: " + m.name.original + " */\n";
            //        }


            const auto &parent = *m.src->_params.begin()->get();
            if (parent.original.type() != superclass.original) {

                // out.sPublic += "  // RAII CTOR 2\n";
                genCtor(m, parent, true);

                if (parent.isHandle()) {
                    const auto &handle = findHandle(parent.original.type());
                    if (handle.superclass.original != superclass.original) {
                            std::cerr << "ctor: impossible combination" << '\n';
                            continue;
                    }
                }
            }

            // out.sPublic += "  // RAII CTOR\n";
            if (parent.isHandle()) {
                genCtor(m, parent);
            }

        }
    }


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

{3}
#   if defined( VULKAN_HPP_HAS_SPACESHIP_OPERATOR )
    auto operator<=>( {0} const & ) const = default;
#  else
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
#  endif
{4}
)",
                          className, classNameLower, handle, expIfndef("VULKAN_HPP_EXPERIMENTAL_NO_CLASS_COMPARE"), expEndif("VULKAN_HPP_EXPERIMENTAL_NO_CLASS_COMPARE"));
    if (!cfg.gen.cppModules) {
        generateClassMembers(data, out, funcs, funcsEnhanced,Namespace::VK, inlineFuncs);
    }

    output += generateClassString(className, out, Namespace::VK);

    output += expIfndef("VULKAN_HPP_EXPERIMENTAL_NO_TEMPLATES");
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
    output += expEndif("VULKAN_HPP_EXPERIMENTAL_NO_TEMPLATES");

    return output;
}

std::string Generator::generateClass(const HandleData &data, UnorderedFunctionOutput &funcs, UnorderedFunctionOutput &funcsEnhanced) {
    std::string output;
//    if (data.canGenerate()) {
//        std::string str;
//        for (const auto &s : data.subscribers) {
//            str += " " + s->name;
//        }
//        std::cout << " gen class " << data.name << " " << str << '\n';
//    }

    if (!data.getProtect().empty()) {
        platformOutput.add(data, [&](std::string &output) {
            output += generateClassStr(data, funcs, funcsEnhanced, true);
        });
    }
    else {
        output += genOptional(data, [&](std::string &output) {
            output += generateClassStr(data, funcs, funcsEnhanced, false);
        });
    }
    return output;
}

 std::string Generator::generateClassRAII(const HandleData &data) {
    std::string output;
    output += genOptional(data, [&](std::string &output) {
        GenOutputClass out{*this};

        const std::string &className = data.name;
        const std::string &classNameLower = strFirstLower(className);
        const std::string &handle = data.vkhandle.identifier();
        const auto &superclass = data.superclass;
        const std::string &owner = data.ownerhandle;

        std::string debugReportValue = "Unknown";
        auto en = enums.find("VkDebugReportObjectTypeEXT");
        if (en != enums.end() && en->containsValue("e" + className)) {
            debugReportValue = className;
        }

        out.sPublic += format(R"(
    using CType      = Vk{0};    

    static VULKAN_HPP_CONST_OR_CONSTEXPR {NAMESPACE}::ObjectType objectType =
      {NAMESPACE}::ObjectType::e{0};
    static VULKAN_HPP_CONST_OR_CONSTEXPR {NAMESPACE}::DebugReportObjectTypeEXT debugReportObjectType =
      {NAMESPACE}::DebugReportObjectTypeEXT::e{1};

)",
                              className, debugReportValue);

        generateClassConstructorsRAII(data, out, outputFuncsRAII);

        std::string release;
        data.foreachVars(VariableData::Flags::CLASS_VAR_RAII, [&](const VariableData &v){
            out.sPrivate += "    " + v.toClassVar();
            if (v.identifier() != handle) {
                release += "      " + v.identifier() + " = nullptr;\n";
            }
        });


        out.sPublic += format(R"(

    {NAMESPACE}::{0} const &operator*() const {NOEXCEPT} {
        return {1};
    }

    void clear() {NOEXCEPT};
    void swap({NAMESPACE_RAII}::{0} &) {NOEXCEPT};
)",
                            className, handle);



        out.sPublic += format(R"(

    {NAMESPACE}::{0} release()
    {
{1}      return {NAMESPACE_RAII}::exchange( {2}, nullptr );
    }
)",
                              className, release, handle);


        outputFuncsRAII.add(data, [&](std::string &output) {

            std::string dispatchSrc;
            std::string type = data.name;
            if (cfg.gen.staticInstancePFN && data.name.original == "VkInstance") {
                dispatchSrc = "      return &" + format("{NAMESPACE_RAII}::") + "Instance::dispatcher;\n";
                out.sPublic += "    inline static " + type + "Dispatcher dispatcher;\n";
//                funcs += "  " + format("{NAMESPACE_RAII}::Instance ") + type + "Dispatcher dispatcher;\n";
            }
            else if (cfg.gen.staticDevicePFN && data.name.original == "VkDevice") {
                dispatchSrc = "      return &" + format("{NAMESPACE_RAII}::") + "Device::dispatcher;\n";
                out.sPublic += "    inline static " + type + "Dispatcher dispatcher;\n";
//                funcs += "  " + format("{NAMESPACE_RAII}::Device ") + type + "Dispatcher dispatcher;\n";
            }
            else if (data.name.original == "VkInstance" || data.name.original == "VkDevice") {
                dispatchSrc = "      return &*m_dispatcher;\n";
            }
            else {
                type = superclass;
                if (data.ownerRaii) {
                    dispatchSrc = "      return " + data.ownerRaii->identifier() + "->getDispatcher();\n";
                }                
            }

            std::string spec;
            if (!cfg.gen.cppModules) {
                spec = cfg.macro.mInline->get() + " ";
            }
            out.sPublic += format(R"(
    {2}{NAMESPACE_RAII}::{0}Dispatcher const * getDispatcher() const;
    )",
            type, dispatchSrc, spec);

            output += format(R"(
    {3}{NAMESPACE_RAII}::{0}Dispatcher const * {1}::getDispatcher() const
    {
      //VULKAN_HPP_ASSERT( m_dispatcher->getVkHeaderVersion() == VK_HEADER_VERSION );
{2}
    }
    )",
            type, data.name, dispatchSrc, spec);
        }, ""); // TODO change to +=

        if (data.ownerRaii) {
            out.sPublic += format(R"(
    {INLINE} {NAMESPACE_RAII}::{0} const & get{0}() const
    {
      return *{1};
    }
)",
            data.ownerRaii->type(), data.ownerRaii->identifier());
        }

        generateClassMembers(data, out, outputFuncsRAII, outputFuncsEnhancedRAII, Namespace::RAII);

        output += generateClassString(className, out, Namespace::RAII);

        if (!data.vectorCmds.empty()) {

            GenOutputClass out{*this};
            std::string name = className + "s";

            out.inherits +=
                format("public std::vector<{NAMESPACE_RAII}::{0}>", className);

            //HandleData cls = data;
            //cls.name = name;

            int passed = 0;
            for (auto &m : data.vectorCmds) {

                if (m.src->_params.empty()) {
                    std::cerr << "RAII vector constructor: no params" << '\n';
                    continue;
                }

                MemberContext ctx{.ns = Namespace::RAII};
                const auto &parent = *m.src->_params.begin()->get();
                if (parent.original.type() != superclass.original) {
                    ctx.insertSuperclassVar = true;
                }

                MemberResolverVectorCtor r{*this, const_cast<ClassCommandData&>(m), ctx};
                if (!r.hasDependencies) {
                    std::cout << "vector ctor skipped: class " << data.name
                              << ", p: " << parent.type() << ", s: " << superclass
                              << '\n';
                    continue;
                }
                r.generate(out.sPublic, outputFuncsRAII);
                passed++;

            }            

            if (passed > 0) {
                out.sPublic += format(R"(
    {0}( std::nullptr_t ) {}

    {0}()                          = delete;
    {0}( {0} const & )             = delete;
    {0}( {0} && rhs )              = default;
    {0} & operator=( {0} const & ) = delete;
    {0} & operator=( {0} && rhs )  = default;
)",
                                  name);          

                output += generateClassString(name, out, Namespace::RAII);
            }
            else {
                std::cout << "no suitable constructors for class: " << data.name << '\n';
            }
        }
    });
    return output;
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
        out.sProtected += genOptional(m, [&](std::string &output) {
            output += format("    PFN_{0} m_{0} = {};\n", name);
        });

        // PFN pointers initialization
        load += genOptional(m, [&](std::string &output) {
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
    std::unique_ptr<ContextDispatcher> m_dispatcher;
)";    

    out.sPublic += format(R"(
#ifdef _WIN32
    static constexpr char const* defaultLibpath = "vulkan-1.dll";
#else
    static constexpr char const* defaultLibpath = "libvulkan.so.1";
#endif

    {0}() = default;

    ~{0}() {
      unload();
    }

    {0}(const std::string &libpath) {
      load(libpath);
    }

    template<typename T>
    {INLINE} T getProcAddr(const char *name) const {
      return std::bit_cast<T>(m_dispatcher->vkGetInstanceProcAddr(nullptr, name));
    }

    void load(const std::string &libpath) {

#ifdef _WIN32
      lib = LoadLibraryA(libpath.c_str());
#else
      lib = dlopen(libpath.c_str(), RTLD_NOW);
#endif
      if (!lib) {
        throw std::runtime_error("Cant load library: " + libpath);
      }

#ifdef _WIN32
      PFN_vkGetInstanceProcAddr getInstanceProcAddr = std::bit_cast<PFN_vkGetInstanceProcAddr>(GetProcAddress(lib, "vkGetInstanceProcAddr"));
#else
      PFN_vkGetInstanceProcAddr getInstanceProcAddr = std::bit_cast<PFN_vkGetInstanceProcAddr>(dlsym(lib, "vkGetInstanceProcAddr"));
#endif
      if (!getInstanceProcAddr) {
        throw std::runtime_error("Context: Can't load vkGetInstanceProcAddr");
      }

      m_dispatcher.reset( new ContextDispatcher( getInstanceProcAddr ) );
    }

    void load() {
      load(defaultLibpath);
    }

    void unload() {
      if (lib) {
#ifdef _WIN32
        FreeLibrary(lib);
#else
        dlclose(lib);
#endif
        lib = nullptr;
        m_dispatcher = nullptr;
      }
    }

    ContextDispatcher const * getDispatcher() const
    {
      // VULKAN_HPP_ASSERT( m_dispatcher->getVkHeaderVersion() == VK_HEADER_VERSION );
      return &*m_dispatcher;
    }
)",
    loader.name);

    for (auto &m : loader.members) {        

        MemberContext ctx{.ns = Namespace::RAII};
        if (m.src->nameCat == MemberNameCategory::CREATE) {
            ctx.insertClassVar = true;
        }
        generateClassMember(m, ctx, out, outputFuncsRAII, outputFuncsEnhancedRAII);
    }

    output += R"(
#ifdef _WIN32
#  define LIBHANDLE HINSTANCE
#else
#  define LIBHANDLE void*
#endif
)";
    output += generateClassString(loader.name, out, Namespace::RAII);
    output += "  ";
    if (!cfg.gen.cppModules) {
        output += "static ";
    }
    output += loader.name + " defaultContext;\n";

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

std::string Generator::beginNamespace(bool noExport) const {
    return beginNamespace(cfg.macro.mNamespace, noExport);
}

std::string Generator::beginNamespaceRAII(bool noExport) const {
    return beginNamespace(cfg.macro.mNamespaceRAII, noExport);
}

std::string Generator::beginNamespace(const Registry::Macro &ns, bool noExport) const {
    std::string output;
    if (cfg.gen.cppModules && !noExport) {
        output += "export ";
    }
    return output + "namespace " + ns.getDefine() + " {\n";
}

std::string Generator::endNamespace() const {
    return endNamespace(cfg.macro.mNamespace);
}

std::string Generator::endNamespaceRAII() const {
    return endNamespace(cfg.macro.mNamespaceRAII);
}

std::string Generator::endNamespace(const Registry::Macro &ns) const {
    return "}  // namespace " + ns.getDefine() + "\n";
}

void Generator::bindVars(Variables &vars) const {
    Registry::bindVars(*this, vars);
}

Generator::Generator()
    : Registry(*this),
      cvars{ClassVariables{
        .raiiAllocator = VariableData{*this, VariableDataInfo{
            .prefix = "const ",
            .vktype = "VkAllocationCallbacks",
            .suffix = " *",
            .identifier = "m_allocator",
            .assigment = " = {}",
            .ns = Namespace::VK,
            .flag = VariableData::Flags::CLASS_VAR_RAII
        }},
        .raiiInstanceDispatch = VariableData{*this, VariableDataInfo{
            .stdtype = "std::unique_ptr<InstanceDispatcher>",
            .identifier = "m_dispatcher",
            .ns = Namespace::NONE,
            .flag = VariableData::Flags::CLASS_VAR_RAII
        }},
        .raiiDeviceDispatch = VariableData{*this, VariableDataInfo{
            .stdtype = "std::unique_ptr<DeviceDispatcher>",
            .identifier = "m_dispatcher",
            .ns = Namespace::NONE,
            .flag = VariableData::Flags::CLASS_VAR_RAII
        }},
//        .raiiInstanceDispatch = VariableData{*this, VariableDataInfo{
//            .prefix = "",
//            .vktype = "InstanceDispatcher",
//            .suffix = " const *",
//            .identifier = "m_dispatcher",
//            .assigment = " = nullptr",
//            .ns = Namespace::RAII,
//            .flag = VariableData::Flags::CLASS_VAR_RAII
//        }},
//        .raiiDeviceDispatch = VariableData{*this, VariableDataInfo{
//            .prefix = "",
//            .vktype = "DeviceDispatcher",
//            .suffix = " const *",
//            .identifier = "m_dispatcher",
//            .assigment = " = nullptr",
//            .ns = Namespace::RAII,
//            .flag = VariableData::Flags::CLASS_VAR_RAII
//        }},
        .uniqueAllocator = VariableData{*this, VariableDataInfo{
            .prefix = "const ",
            .vktype = "VkAllocationCallbacks",
            .suffix = " *",
            .identifier = "m_allocator",
            .assigment = " = {}",
            .ns = Namespace::VK,
            .flag = VariableData::Flags::CLASS_VAR_UNIQUE
        }},
        .uniqueDispatch = VariableData{*this, VariableDataInfo{
            .prefix = "const ",
            .vktype = "VkDispatch",
            .suffix = " *",
            .identifier = "m_dispatch",
            .assigment = " = {}",
            .ns = Namespace::NONE,
            .flag = VariableData::Flags::CLASS_VAR_UNIQUE,
            .specialType = VariableData::TYPE_DISPATCH
        }}
    }},
    outputFuncs{*this},
    outputFuncsEnhanced{*this},
    outputFuncsRAII{*this},
    outputFuncsEnhancedRAII{*this},
    outputFuncsInterop{*this},
    platformOutput{*this}
{    
    unload();
    // resetConfig();

    // std::cout << "cfg: " << &cfg << "  " << &getConfig() << std::endl;

    namespaces.insert_or_assign(Namespace::VK, &cfg.macro.mNamespace.data);
    namespaces.insert_or_assign(Namespace::RAII, &cfg.macro.mNamespaceRAII.data);
    namespaces.insert_or_assign(Namespace::STD, &cfg.macro.mNamespaceSTD);    

}

void Generator::resetConfig() {
    cfg.reset();
}

void Generator::loadConfigPreset() {
    resetConfig();
}

void Generator::setOutputFilePath(const std::string &path) {
    outputFilePath = path;
    if (isOuputFilepathValid()) {
        std::string filename = std::filesystem::path(path).filename().string();
        filename = camelToSnake(filename);        
    }
}

void Generator::load(const std::string &xmlPath) {
    auto start = std::chrono::system_clock::now();
    Registry::load(*this, xmlPath);
    auto end = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration<double>(end - start);
    std::cout << "loaded in " << elapsed.count() << "s  (" <<  xmlPath << ")" << std::endl;
}

void Generator::generate() {

    if (outputFilePath.empty()) {
        std::cerr << "empty output path" << '\n';
        return;
    }

    std::cout << "generating" << '\n';

    auto start = std::chrono::system_clock::now();

    std::string p = outputFilePath;
    std::replace(p.begin(), p.end(), '\\', '/');
    if (!p.ends_with('/')) {
        p += '/';
    }
    std::filesystem::path path = std::filesystem::absolute(p);
    std::cout << "path: " << path << '\n';
    if (!std::filesystem::exists(path)) {
        std::error_code ec;
        if (!std::filesystem::create_directories(path, ec)) {
            if (!std::filesystem::exists(path)) {
                throw std::runtime_error{"Can't create directory" +
                                         ec.message()};
            }
        }
    }
    if (!std::filesystem::is_directory(path)) {
        throw std::runtime_error{"Output path is not a directory"};
    }

    // TODO check existing files?

    outputFuncs.clear();
    outputFuncsRAII.clear();
    outputFuncsInterop.clear();
    platformOutput.clear();
    dispatchLoaderBaseGenerated = false;

    generatedDestroyCommands.clear();

    loader.prepare(*this);
    for (auto &h : handles) {        
        h.prepare(*this);
    }

    cvars.uniqueDispatch.setType(cfg.macro.mDispatchType->get());

    generateFiles(path);

    auto end = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration<double>(end - start);
    std::cout << "generated in " << elapsed.count() << "s" << std::endl;
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

const Registry::Macro& Generator::findNamespace(Namespace ns) const {
    auto it = namespaces.find(ns);
    if (it == namespaces.end()) {
        throw std::runtime_error("getNamespace(): namespace does not exist");
    }
    return *it->second;
}

std::string Generator::getNamespace(Namespace ns, bool colons) const {
    if (ns == Namespace::NONE) {
        return "";
    }
    auto it = namespaces.find(ns);
    if (it == namespaces.end()) {
        std::cerr << "getNamespace(): namespace does not exist.\n";
        return "";
    }
    return it->second->get() + (colons ? "::" : "");
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
        // std::cout << "export: " << data.name << '\n';
        XMLElement *elem = parent->GetDocument()->NewElement(data.name.c_str());
        saveConfigParam(elem, data.reflect());
        parent->InsertEndChild(elem);
    }
    else {
        // std::cout << "export:   * leaf" << '\n';
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
        // std::cout << "load: " << data.name << '\n';
        XMLElement *elem = parent->FirstChildElement(data.name.c_str());
        loadConfigParam(elem, data.reflect());
    }
    else {
        // import
        // std::cout << "load:   * leaf: " << data.name << '\n';
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
        std::cout << "Saved config file: " << filename << '\n';
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
        // std::cerr << msg << '\n';
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
        dynamic_cast<AbstractWhitelistBinding*>(&bEnums),
        dynamic_cast<AbstractWhitelistBinding*>(&bStructs),
        dynamic_cast<AbstractWhitelistBinding*>(&bHandles),
        dynamic_cast<AbstractWhitelistBinding*>(&bCmds)
    };

    class ConfigVisitor : public XMLVisitor {
        AbstractWhitelistBinding* parent;
    public:
        ConfigVisitor(AbstractWhitelistBinding *parent) : parent(parent)
        {}

        virtual bool Visit (const XMLText & text) override {
            std::string_view tag = text.Parent()->Value();
            std::string_view value = text.Value();
            // std::cout << "tag: " << tag << '\n';
            if (tag == parent->name) {
                for (auto line : split2(value, "\n")) {
                    std::string t = regex_replace(std::string{line}, std::regex("(^\\s*)|(\\s*$)"), "");
                    if (!t.empty()) {
                        if (!parent->add(t)) {
                            std::cerr << "[Config load] Duplicate: " << t << '\n';
                        }
                    }
                }
            }
            else if (tag == "regex") {
                // std::cout << "RGX: " << value << '\n';
                try {
                    parent->add(std::regex{std::string{value}});
                }
                catch (const std::regex_error& err) {
                    std::cerr << "[Config load]: regex error: " << err.what() << '\n';
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
//                            std::cerr << "[Config load] Duplicate: " << t << '\n';
//                        }
//                    }
//                }
            }
            if (!accepted) {
                std::cerr << "[Config load] Warning: unknown element: " << n.Value() << '\n';
            }
        }

//        std::vector<std::string> removed;
//        for (const auto &f : bTypes.filter) {
//            if (enums.find(f) != enums.end()) {
//                bEnums.filter.insert(f);
//                // std::cout << "[Config load] shift to enums: " << f << '\n';
//                removed.push_back(f);
//            }
//        }
//        for (const auto &f : removed) {
//            bTypes.filter.erase(f);
//        }

        std::cout << "[Config load] whitelist built" << '\n';

        for (auto &b : bindings) {
            b->prepare();
        }

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

        std::cout << "[Config load] whitelist applied" << '\n';

//        std::cout << "order:" << '\n';
//        std::ranges::for_each(orderedCommands, [](CommandData &c){
//            std::cout << "  " << c.name << '\n';
//        });
    }    

    cfg.macro.mNamespaceRAII->parent = &cfg.macro.mNamespace.data;

    std::cout << "[Config load] Loaded: " << filename << '\n';

    if (cfg.gen.sort2) {
        orderStructs(true);
        orderHandles(true);
    }

}

template <class... Args>
std::string Generator::format(const std::string &format,
                              const Args... args) const {

    std::vector<std::string> list;
    static const std::unordered_map<std::string, const Macro &> macros = {
        {"NAMESPACE", cfg.macro.mNamespace},
        {"NAMESPACE_RAII", cfg.macro.mNamespaceRAII},
        {"CONSTEXPR", cfg.macro.mConstexpr},
        {"CONSTEXPR_14", cfg.macro.mConstexpr14},
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

Registry::HandleData::HandleData(Generator &gen, const std::string_view name, const std::string_view parent)
    : BaseType(Type::Handle, name, true),
      superclass(std::string{parent}),
      vkhandle(gen, VariableDataInfo {
        .vktype = this->name.original,
        .identifier = "m_" + strFirstLower(this->name),
        .assigment = " = {}",
        .ns = Namespace::VK,
        .flag = VariableData::Flags::HANDLE | VariableData::Flags::CLASS_VAR_VK | VariableData::Flags::CLASS_VAR_RAII
      })
{    
    isSubclass = name != "VkInstance" && name != "VkDevice";
}

void Registry::HandleData::init(Generator &gen) {
    // std::cout << "Handle init: " << name << '\n';
    superclass = gen.getHandleSuperclass(*this);
    if (isSubclass) {
        ownerhandle = "m_" + strFirstLower(superclass);
    }

    if (isSubclass) {
        ownerUnique = std::make_unique<VariableData>(gen, VariableDataInfo{
            .vktype = superclass.original,
            .identifier = "m_owner",
            .assigment = " = {}",
            .ns = Namespace::VK,
            .flag = VariableData::Flags::HANDLE | VariableData::Flags::CLASS_VAR_UNIQUE
        });

        ownerRaii = std::make_unique<VariableData>(gen, VariableDataInfo{
            .vktype = superclass.original,
            .suffix = " const *",
            .identifier = "m_" + strFirstLower(superclass),
            .assigment = " = nullptr",
            .ns = Namespace::RAII,
            .flag = VariableData::Flags::HANDLE| VariableData::Flags::CLASS_VAR_RAII
        });
    }
    // std::cout << "Handle init done " << '\n';
}

void Generator::HandleData::prepare(const Generator &gen) {
    clear();

    const auto &cfg = gen.getConfig();
    effectiveMembers = 0;
    filteredMembers.clear();
    filteredMembers.reserve(members.size());

    std::unordered_map<std::string, ClassCommandData*> stage;
    bool order = cfg.gen.orderCommands && !gen.orderedCommands.empty();

    for (auto &m : members) {
        std::string s = gen.genOptional(m, [&](std::string &output)
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

    vars.clear();
    if (this == &gen.loader) {
        return;
    }
    vars.emplace_back(std::ref(vkhandle));
    if (ownerUnique) {
        vars.emplace_back(std::ref(*ownerUnique));
    }
    if (ownerRaii) {
        vars.emplace_back(std::ref(*ownerRaii));
    }
    if (secondOwner) {
        vars.emplace_back(std::ref(*secondOwner));
    }
    if (cfg.gen.allocatorParam) {
        vars.emplace_back(std::ref(gen.cvars.raiiAllocator));
        vars.emplace_back(std::ref(gen.cvars.uniqueAllocator));
    }

    vars.emplace_back(std::ref(gen.cvars.uniqueDispatch));

    if (name.original == "VkInstance" && !cfg.gen.staticInstancePFN) {
        vars.emplace_back(std::ref(gen.cvars.raiiInstanceDispatch));
    }
    else if (name.original == "VkDevice" && !cfg.gen.staticDevicePFN) {
        vars.emplace_back(std::ref(gen.cvars.raiiDeviceDispatch));
    }
}

void Generator::HandleData::addCommand(const Generator &gen,
                                       CommandData &cmd, bool raiiOnly) {
    auto &c = members.emplace_back(&gen, this, cmd);
    c.raiiOnly = raiiOnly;
    // std::cout << "Added to " << name << ", " << cmd.name << '\n';
}

void Registry::HandleData::setDestroyCommand(const Generator &gen, CommandData &cmd) {
    dtorCmd = &cmd;
    for (const auto &p : cmd._params) {
        if (!p->isHandle()) {
            continue;
        }
        if (p->original.type() == name.original) {
            continue;
        }
        if (p->original.type() == superclass.original) {
            continue;
        }
//        std::cout << name << '\n';
//        std::cout << "  <>" << p->fullType() << '\n';
        secondOwner = std::make_unique<VariableData>(gen, VariableDataInfo{
            .vktype = p->original.type(),
            .identifier = "m_" + strFirstLower(p->type()),
            .assigment = " = {}",
            .ns = Namespace::VK,
            .flag = VariableData::Flags::HANDLE| VariableData::Flags::CLASS_VAR_RAII | VariableData::Flags::CLASS_VAR_UNIQUE
        });
    }
}

bool Generator::EnumData::containsValue(const std::string &value) const {
    for (const auto &m : members) {
        if (m.name == value) {
            return true;
        }
    }
    return false;
}

std::string Generator::MemberResolver::getDbgtag() {
    if (!gen.getConfig().dbg.methodTags) {
        return "";
    }
    std::string out = "// <" + dbgtag + ">";
//    if (ctx.raiiOnly) {
//        out += " <RAII indirect>";
//    } else
    if (isIndirect()) {
        out += " <indirect>";
    }
    if (cmd->createsHandle()) {
        out += " [handle]";
    }
    out += "\n";
    out += dbg2;
    return out;
}

std::string Generator::MemberResolver::generateDeclaration() {
    std::string output;
    std::string indent = ctx.isStatic ? "  " : "    ";
    if (gen.getConfig().dbg.methodTags) {
        output += "// declaration\n";
    }
    bool usesTemplate = false;
    output += getProto(indent, true, usesTemplate) + ";\n";
    if (usesTemplate) {
        output += "#endif // VULKAN_HPP_EXPERIMENTAL_NO_TEMPLATES\n";
    }
    output += "\n";
    return output;
}

std::string Generator::MemberResolver::generateDefinition(bool genInline, bool bypass) {
    std::string output;
    std::string indent = genInline ? "    " : "  ";

    if (gen.getConfig().dbg.methodTags) {
        output += genInline ? "// inline definition\n" : "// definition\n";
    }
    bool usesTemplate = false;
    output += getProto(indent, genInline, usesTemplate) + "\n  {\n";
    if (ctx.ns == Namespace::RAII && isIndirect() && !constructor) {
        if (cls->ownerhandle.empty()) {
            std::cerr << "Error: can't generate function: class has "
                         "no owner ("
                      << cls->name << ", " << name << ")"
                      << '\n';
        } else {
            pfnSourceOverride = cls->ownerhandle + "->getDispatcher()->";
        }
    }
    for (const VariableData &p : cmd->params) {
        if (p.getIgnoreFlag()) {
            continue;
        }
        if (p.getSpecialType() ==
            VariableData::TYPE_ARRAY_PROXY) {
            if (p.isLenAttribIndirect()) {
                const auto &var = p.getLengthVar();
                std::string size = var->identifier() + "." +
                                   p.getLenAttribRhs();
                output += "    // VULKAN_HPP_ASSERT (" + p.identifier() +
                          ".size()" + " == " + size +
                          ")\n";
            }
        }
    }

#ifdef INST
    std::cout << "Inst::bodyStart" << '\n';
    output += Inst::bodyStart(ctx.ns, name.original);
#endif

    output += generateMemberBody();

    if (generateReturnType() != "void" && !returnValue.empty()) {
        output += "    return " + returnValue + ";\n";
    }
    output += "  }\n";
    if (usesTemplate) {
        output += "#endif // VULKAN_HPP_EXPERIMENTAL_NO_TEMPLATES\n";
    }
    output += "\n";
    return output;
}

void Generator::MemberResolver::generate(UnorderedFunctionOutput &decl, UnorderedFunctionOutput &def) {

    // std::cout << "generate: " << dbgtag << '\n';

    if (gen.getConfig().dbg.methodTags) {
        for (auto &p : cmd->_params) {
            dbg2 += p->dbgstr();
        }
    }

    if (ctx.generateInline) {
        decl.add(*cmd, [&](std::string &output) {
            output += generateDefinition(true);
        }, guard);
    }
    else {        
        decl.add(*cmd, [&](std::string &output) {
            output += generateDeclaration();            
        }, guard);

        def.add(*cmd, [&](std::string &output){
            output += generateDefinition(false);            
        }, guard);
    }

    reset(); // TODO maybe unnecessary
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
void Generator::WhitelistBinding<T>::prepare() noexcept {
    for (auto &e : *dst) {
        e.setEnabled(false);
    }
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
        if (match) {
            e.setEnabled(true);
        }
        // std::cout << "enable: " << e.name << " " << e.typeString() << std::endl;
    }
    for (auto &f : filter) {
        std::cerr << "[Config load] Not found: " << f << " (" << name << ")" << '\n';
    }
}

template<>
void Generator::ConfigWrapper<Generator::Macro>::xmlExport(XMLElement *elem) const {
    // std::cout << "export macro:" << name << '\n';
    elem->SetName("macro");
    elem->SetAttribute("define", data.define.c_str());
    elem->SetAttribute("value", data.value.c_str());
    elem->SetAttribute("usesDefine", data.usesDefine? "true" : "false");
}

template<>
void Generator::ConfigWrapper<bool>::xmlExport(XMLElement *elem) const {
    // std::cout << "export: " << name << '\n';
    elem->SetName("bool");
    elem->SetAttribute("value", data? "true" : "false");
}

template<>
bool Generator::ConfigWrapper<Generator::Macro>::xmlImport(XMLElement *elem) const {
    // std::cout << "import macro:" << name << '\n';
    if (std::string_view{elem->Value()} != "macro") {
        std::cerr << "[config import] node value mismatch" << '\n';
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
            std::cerr << "[config import] unknown value" << '\n';
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
    // std::cout << "import: " << name << '\n';
    if (std::string_view{elem->Value()} != "bool") {
        std::cerr << "[config import] node mismatch" << '\n';
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
            std::cerr << "[config import] unknown value" << '\n';
            return false;
        }
    }
    return true;
}

void Registry::CommandData::init(Generator &gen) {
    gen.bindVars(_params);

    initParams();

    bool hasHandle = false;
    bool canTransform = false;
    for (auto &p : _params) {
        if (p->isOutParam()) {
            if (p->getArrayVars().empty()) {
                outParams.push_back(std::ref(*p));
                if (p->isHandle()) {
                    hasHandle = true;

                }
            }
        }

        auto sizeVar = p->getLengthVar();
        if (sizeVar) {
            canTransform = true;
        }
        if (gen.isStructOrUnion(p->original.type())) {
            canTransform = true;
        }
    }

    if (hasHandle) {
        setFlagBit(CommandFlags::CREATES_HANDLE, true);
    }
    if (canTransform) {
        setFlagBit(CommandFlags::CPP_VARIANT, true);
    }

    prepared = true;
}

Registry::CommandData::CommandData(Generator &gen, XMLElement *command, const std::string &className) noexcept
    : BaseType(Type::Command)
{
    // iterate contents of <command>    
    std::string dbg;
    std::string name;
    // add more space to prevent reallocating later
    _params.reserve(8);
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
                type = typeElement->GetText();
            }
        }
        // <param> section
        else if (std::string_view(child.Value()) == "param") {
            _params.emplace_back(std::make_unique<VariableData>(gen, &child));
        }
    }
    if (name.empty()) {
        std::cerr << "Command has no name" << '\n';
    }
    setName(gen, name);

    const char *successcodes = command->Attribute("successcodes");
    if (successcodes) {
        for (const auto &str : split(std::string(successcodes), ",")) {
            successCodes.push_back(str);
        }
    }

    init(gen);
    // std::cout << "Command init " << this << '\n';
}

Registry::CommandData::CommandData(Generator &gen, const CommandData &o, const std::string_view alias)
    : BaseType(Type::Command)
{
    type = o.type;
    successCodes = o.successCodes;
    nameCat = o.nameCat;
    pfnReturn = o.pfnReturn;
    flags = o.flags;
    setFlagBit(CommandFlags::ALIAS, true);
    setName(gen, std::string(alias));

    _params.reserve(o._params.size());
    for (auto &p : o._params) {
        _params.push_back(std::make_unique<VariableData>(*p));
    }

    init(gen);
    // std::cout << "Command copy init done" << '\n';
}

Registry::HandleData *Registry::CommandData::secondIndirectCandidate(Generator &gen) const {
    if (_params.size() < 2) {
        return nullptr;
    }    
    if (!_params[1]->isHandle()) {
        return nullptr;
    }
    if (destroysObject()) {
        return nullptr;
    }

    const auto &type = _params[1]->original.type();
    bool isCandidate = true;
    try {
        const auto &var = getLastPointerVar();
        if (getsObject() || createsHandle()) {
            if (nameCat != MemberNameCategory::GET) {
                isCandidate = !var->isArray();
            }
            else {
                isCandidate = var->original.type() != type;
            }
        } else if (destroysObject()) {            
            isCandidate = var->original.type() != type;
        }
    } catch (std::runtime_error) {
    }
    if (!isCandidate) {
        return nullptr;
    }    
    auto &handle = gen.findHandle(type);
    return &handle;
}

void Registry::CommandData::setName(const Generator &gen, const std::string &name) {
    this->name.convert(name);
    pfnReturn = gen.evaluatePFNReturn(type);
    gen.evalCommand(*this);
}

Generator::ClassCommandData::ClassCommandData(const Generator *gen, const HandleData *cls, CommandData &o)
    : cls(cls), src(&o),
      name(gen->convertCommandName(o.name.original, cls->name), false)
{
    name.original = o.name.original;

    if (cls->name.original == "VkCommandBuffer" && name.starts_with("cmd")) {
        auto old = name;
        name = strFirstLower(name.substr(3));
        // std::cout << "convert name: " << old << " -> " << name << std::endl;
    }
}

std::string Generator::MemberResolver::createArgumentWithType(const std::string &type) const {
    for (const VariableData &p : cmd->params) {
        if (p.type() == type) {
            return p.identifier();
        }
    }
    // type is not in command parameters, look inside structs
    for (const VariableData &p : cmd->params) {
        auto it = gen.structs.find(p.original.type());
        if (it != gen.structs.end()) {
            for (const auto &m : it->members) {
                if (m->type() == type) {
                    return p.identifier() + (p.isPointer()? "->" : ".") + m->identifier();
                }
            }
        }
    }
    // not found
    return "";
}

std::string Generator::MemberResolver::successCodesCondition(const std::string &id, const std::string &indent) const {
    std::string output;
    for (const auto & c : cmd->successCodes) {
        if (c == "VK_INCOMPLETE") {
            continue;
        }
        "( " + id + " == Result::" + gen.enumConvertCamel("Result", c) + " ) ||\n" + indent;
    }
    strStripSuffix(output, " ||\n" + indent);
    return output;
}

std::string Generator::MemberResolver::successCodesList(const std::string &indent) const {
    std::string output;
    if (cmd->successCodes.empty()) {
        return output;
    }
    output += ",\n" + indent + "{ ";
    std::string suffix =  ",\n" + indent + "  ";
    for (const auto & c : cmd->successCodes) {
        if (c == "VK_INCOMPLETE") {
            continue;
        }
        if (gen.getConfig().gen.internalVkResult) {
            output += c + suffix;
        }
        else {
            output += "Result::" + gen.enumConvertCamel("Result", c) + suffix;
        }
    }
    strStripSuffix(output, suffix);
    output += "}";
    return output;
}

std::string Generator::MemberResolver::createArgument(std::function<bool(const VariableData &, bool)> filter,
                                                    std::function<std::string (const VariableData &)> function,
                                                    bool proto,
                                                    bool pfn,
                                                    const VariableData &var) const
{
    bool sameType = var.original.type() == cls->name.original && !var.original.type().empty();

    if (!filter(var, sameType)) {
        return "";
    }

    if (!proto) {

        if (pfn) {
            std::string alt = var.getAltPNF();
            if (!alt.empty()) {
                return alt;
            }
        }

        auto v = varSubstitution.find(var.original.identifier());
        if (v != varSubstitution.end()) {
            return "/* $V */" + v->second;
        }

        if (var.getIgnoreProto() && !var.isLocalVar()) {
            if (var.original.type() == "VkAllocationCallbacks" && !gen.cfg.gen.allocatorParam) {
                // return pfn ? "nullptr/*ALLOC*/" : "/*- ALLOC*/";
                if (ctx.ns == Namespace::VK && !ctx.disableAllocatorRemoval) {
                    return "";
                }
                return pfn ? "nullptr" : "";
            }

            VariableData::Flags flag = VariableData::Flags::CLASS_VAR_VK;
            if (ctx.ns == Namespace::RAII) {
                flag = VariableData::Flags::CLASS_VAR_RAII;
            }
            else if (ctx.inUnique) {
                flag = VariableData::Flags::CLASS_VAR_UNIQUE;
            }
            for (const VariableData &v : cls->vars) {
                if (!hasFlag(v.getFlags(), flag)) {
                    continue;
                }
                static const auto cmp = [&](const std::string_view lhs, const std::string_view rhs) {
                    return !lhs.empty() && lhs == rhs;
                };
                if (cmp(var.type(), v.type()) || cmp(var.original.type(), v.original.type())) {
                    if (sameType && ctx.useThis) {
                        break;
                    }
                    return v.toVariable(var, pfn);
                }
            }
            if (sameType) {
                // std::cerr << "> to this: " << var.type() << '\n';
                std::string s;
                if (ctx.ns == Namespace::RAII && (var.getNamespace() != Namespace::RAII || pfn)) {
                    s = "*";
                }
                s += (var.isPointer()? "this" : "*this");
                if (pfn) {
                    s = "static_cast<" + cls->name.original + ">(" + s + ")";
                }
                return s; //"/*" + var.original.type() + "*/";
            }
        }
    }

    return function(var);
}

std::string Generator::MemberResolver::createArgument(const VariableData &var, bool sameType, bool pfn) const {
    auto v = varSubstitution.find(var.original.identifier());
    if (v != varSubstitution.end()) {
        return "/* $V */" + v->second;
    }

    if (var.getIgnoreProto()) {
        VariableData::Flags flag = VariableData::Flags::CLASS_VAR_VK;
        if (ctx.ns == Namespace::RAII) {
            flag = VariableData::Flags::CLASS_VAR_RAII;
        }
        else if (ctx.inUnique) {
            flag = VariableData::Flags::CLASS_VAR_UNIQUE;
        }
        for (const VariableData &v : cls->vars) {
            if (!hasFlag(v.getFlags(), flag)) {
                continue;
            }
            static const auto cmp = [&](const std::string_view lhs, const std::string_view rhs) {
                return !lhs.empty() && lhs == rhs;
            };
            if (cmp(var.type(), v.type()) || cmp(var.original.type(), v.original.type())) {
                if (sameType && ctx.useThis) {
                    break;
                }
                return v.toVariable(var, pfn);
            }
        }
        if (sameType) {
            // std::cerr << "> to this: " << var.type() << '\n';
            std::string s;
            if (ctx.ns == Namespace::RAII && (var.getNamespace() != Namespace::RAII || pfn)) {
                s = "*";
            }
            s += (var.isPointer()? "this" : "*this");
            if (pfn) {
                s = "static_cast<" + cls->name.original + ">(" + s + ")";
            }
            return s; //"/*" + var.original.type() + "*/";
        }
    }
    return "";
}

std::string Generator::MemberResolver::createArguments(std::function<bool (const VariableData &, bool)> filter, std::function<std::string (const VariableData &)> function, bool proto, bool pfn) const {
    static const auto sep = "\n      ";
    std::string out;
    // bool last = true;
    for (const VariableData &p : cmd->params) {
        std::string arg = createArgument(filter, function, proto, pfn, p);
        if (gen.getConfig().dbg.methodTags) {
            out += sep;
            out += p.argdbg();
        }
        else if (!arg.empty()) {
            out += sep;
        }

        if (!arg.empty()) {
            out += arg + ", ";
        }

    }
    if (gen.getConfig().dbg.methodTags) {
        if (out.ends_with("*/ ")) {
            auto it = out.find_last_of(',');
            if (it != std::string::npos) {
                out.erase(it, 1);
            }
        }
    }
    strStripSuffix(out, ", ");

    return out;
}

Registry::StructData::StructData(Generator &gen, const std::string_view name, VkStructType type, XMLElement *e) : BaseType(Type::Struct, name, true), type(type) {
    // TODO union

    auto returnedonly = getAttrib(e, "returnedonly");
    if (returnedonly == "true") {
        this->returnedonly = true;
    }

    // iterate contents of <type>, filter only <member> children
    for (XMLElement *member : Elements(e) | ValueFilter("member")) {

        auto &v = members.emplace_back(std::make_unique<VariableData>(gen, member));

        const std::string &type = v->type();
        const std::string &name = v->identifier();

        if (const char *values = member->ToElement()->Attribute("values")) {
            std::string value = gen.enumConvertCamel(type, values);
            v->setAssignment(" = " + type + "::" + value);
            if (v->original.type() == "VkStructureType") { // save sType information for structType
                structTypeValue = value;
            }
        }

    }
    gen.bindVars(members);
}

void Generator::GenOutput::writeFile(Generator &gen, OutputFile &file) {
    bool modules = gen.getConfig().gen.cppModules;

    writeFile(gen, file.filename, file.content, true, file.ns);
}


void Generator::GenOutput::writeFile(Generator &gen, const std::string &filename, const std::string &content, bool addProtect, Namespace ns) {
    std::string protect;
    if (addProtect) {
        protect = getFileNameProtect(filename);
    }
    std::ofstream output;
    auto p = std::filesystem::path(this->path).replace_filename(filename);
    output.open(p, std::ios::out | std::ios::trunc);
    if (!output.is_open()) {
        throw std::runtime_error("Can't open file: " + p.string());
    }

    if (!protect.empty()) {
        output << "#ifndef " << protect << "\n";
        output << "#define " << protect << "\n";
    }
    if (ns == Namespace::NONE) {
        output << content;
    } else {
        //output << gen.beginNamespace(gen.findNamespace(ns));
        output << content;
        //output << gen.endNamespace(gen.findNamespace(ns));
    }
    if (!protect.empty()) {
        output << "#endif // " + protect + "\n";
    }

    std::cout << "Generated: " << p << '\n';
}