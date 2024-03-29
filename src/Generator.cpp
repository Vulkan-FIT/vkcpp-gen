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

#include <filesystem>
#include <fmt/format.h>
#include <sstream>

static constexpr char const *RES_HEADER{
    R"(
)"
};

static constexpr char const *RES_ERRORS{ R"(
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
)" };

static constexpr char const *RES_STRUCT_CHAIN{ R"(

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
)" };

static constexpr char const *RES_UNIQUE_HANDLE{ R"(
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
)" };

static constexpr char const *RES_RESULT_VALUE{ R"(
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
)" };

static constexpr char const *RES_ARRAY_PROXY{ R"(
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

static constexpr char const *RES_VECTOR{ R"(
#ifdef VULKAN_HPP_ENABLE_EXP
template<typename T, size_t N>
class Vector {
    // static_assert(std::is_trivially_constructible<T>::value, "Vector: T must be trivial type");

    // basic iterator implementation
    template<typename X>
    class Iterator
    {
    public:
        using value_type = X;
        using pointer    = X*;
        using reference  = X&;
        using difference_type = std::ptrdiff_t;
        using iterator_category = std::random_access_iterator_tag;

        Iterator() : v(nullptr) {}
        Iterator(reference r) : v(&r) {}
        Iterator(pointer p) : v(p) {}
        // Iterator(Vector<T, N>* v, int i): v(v),       i(i) {}
        // Default Copy/Move Are Fine.
        // Default Destructor fine.

        reference                 operator*()             { return *v; }
        std::add_const<reference> operator*()       const { return *v; }
        pointer                   operator->()            { return v; }
        std::add_const<pointer>   operator->()      const { return v; }
        reference                 operator[](int m)       { return *(v + m); }
        std::add_const<reference> operator[](int m) const { return *(v + m); }


        Iterator& operator++()       { ++v; return *this; }
        Iterator& operator--()       { --v; return *this; }
        Iterator  operator++(int)    { Iterator r(*this); ++v; return r; }
        Iterator  operator--(int)    { Iterator r(*this); --v; return r; }

        Iterator& operator+=(int n)  { v += n; return *this; }
        Iterator& operator-=(int n)  { v -= n; return *this; }

        Iterator operator+(int n)   const { Iterator r(*this); return r += n;}
        Iterator operator-(int n)   const { Iterator r(*this); return r -= n;}

        difference_type operator-(Iterator const& r) const { return v - r.v; }

        bool operator<(Iterator const& r)  const { return v <  r.v; }
        bool operator<=(Iterator const& r) const { return v <= r.v; }
        bool operator>(Iterator const& r)  const { return v >  r.v; }
        bool operator>=(Iterator const& r) const { return v >= r.v; }
        bool operator!=(const Iterator &r) const { return v != r.v; }
        bool operator==(const Iterator &r) const { return v == r.v; }

    private:
        pointer v;
    };

    using value_type = T;
    using iterator = Iterator<T>;
    using const_iterator = Iterator<std::add_const<T>>;

    T *m_begin = {};
    T *m_end = {};
    size_t cap = {};
    T array[N];

    void deallocate() {
        if (cap) {
            std::allocator<T>().deallocate(m_begin, cap);
        }
    }

    void allocate(size_t s) {
        m_begin = std::allocator<T>().allocate(s);
        cap = s;
    }

public:
    constexpr Vector() = default;

    constexpr explicit Vector(size_t s) {
        if (s <= N) {
            m_begin = array;
        }
        else {
            allocate(s);
        }
        m_end = m_begin + s;
    }

    ~Vector() noexcept {
        deallocate();
    }

    void resize_optim(size_t s) {
        if (s <= N) {
            deallocate();
            m_begin = array;
            cap = {};
        }
        else if (s > cap) {
            deallocate();
            allocate(s);
        }
    }

    void confirm(size_t s) {
        // assert
        m_end = m_begin + s;
    }

    auto size() const noexcept {
        return m_end - m_begin;
    }

    VULKAN_HPP_NODISCARD VULKAN_HPP_CONSTEXPR bool is_inline() const {
        return m_begin == array;
    }

    T *data() noexcept {
        return m_begin;
    }

    iterator begin() noexcept {
        return iterator{m_begin};
    }

    const_iterator begin() const noexcept {
        return const_iterator{m_begin};
    }

    iterator end() noexcept {
        return iterator{m_end};
    }

    const_iterator end() const noexcept {
        return const_iterator{m_end};
    }

};
#endif // VULKAN_HPP_ENABLE_EXP
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

vkgen::UnorderedFunctionOutputX::UnorderedFunctionOutputX() {
    output = std::make_unique<OutputBuffer>();
}

size_t vkgen::UnorderedFunctionOutputX::size() const {
    size_t s = output->size();
    for (const auto &k : segments) {
        s += k.second.size();
    }
    return s;
}

vkgen::OutputBuffer &vkgen::UnorderedFunctionOutputX::get(const std::span<Protect> protects) {
    UnorderedFunctionOutputX *output = this;
    for (const auto &p : protects) {
        if (!p.first.empty()) {
            output = &output->get(p.first, p.second);
        }
    }
    return output->get();
}

void vkgen::UnorderedFunctionOutputX::write(std::ostream &os) const {
    output->write(os);
    for (const auto &s : segments) {
        if (s.second.ifdef) {
            os << "#ifdef ";
        } else {
            os << "#ifndef ";
        }
        os << s.first << '\n';
        s.second.write(os);
        os << "#endif // " << s.first << '\n';
    }
}

vkgen::OutputBuffer::OutputBuffer() {
    emplace();  // sentinel
}

vkgen::OutputBuffer::OutputBuffer(std::string &&str) {
    list.emplace_back(UnmutableString{ std::move(str) });
}

size_t vkgen::OutputBuffer::size() const {
    return m_size;
}

std::string &vkgen::OutputBuffer::emplace() {
    auto &s = list.emplace_back(std::string{});
    return std::get<std::string>(s);
}

void vkgen::OutputBuffer::print() const {
    std::cout << "-- Out buffer {" << std::endl;
    for (const auto &l : list) {
        std::cout << "<" << l.index() << ">";
        std::visit([&](auto &&arg) { std::cout << ", " << arg.size() << "B"; }, l);
        std::cout << std::endl;
    }
    std::cout << "}" << std::endl;

    //    std::string output;
    //    for (const auto &l : list) {
    //        output += l;
    //    }
    //    std::cout << output << std::endl;
}

void vkgen::OutputBuffer::write(std::ostream &stream) const {
    for (const auto &l : list) {
        std::visit([&](auto &&arg) { stream << arg; }, l);
    }
}

vkgen::OutputBuffer &vkgen::OutputBuffer::operator+=(const std::string_view str) {
    list.emplace_back(std::string_view{ str });
    m_size += str.size();
    return *this;
}

vkgen::OutputBuffer &vkgen::OutputBuffer::operator+=(std::string &&str) {
    m_size += str.size();
    list.emplace_back(vkgen::UnmutableString{ std::move(str) });
    return *this;
}

vkgen::OutputBuffer &vkgen::OutputBuffer::operator+=(const std::string &str) {
    if (list.rbegin()->index() == 0) {
        std::get<std::string>(*list.rbegin()) += str;
        m_size += str.size();
    } else {
        list.emplace_back(std::string{ str });
        m_size += str.size();
    }
    return *this;
}

vkgen::OutputBuffer &vkgen::OutputBuffer::operator+=(const char * const str) {
    const auto &s = std::string::traits_type::length(str);
    list.emplace_back(std::string_view{ str, s });
    m_size += s;
    return *this;
}

vkgen::OutputBuffer &vkgen::OutputBuffer::operator+=(UnorderedFunctionOutputX &&out) {
    m_size += out.size();
    list.emplace_back(std::move(out));
    return *this;
}

[[deprecated]] static std::string
  regex_replace(const std::string &input, const std::regex &regex, std::function<std::string(std::smatch const &match)> format) {
    std::string                       output;
    std::sregex_iterator              it(input.begin(), input.end(), regex);
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

std::string vkgen::Generator::genWithProtect(const std::string &code, const std::string &protect) const {
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

std::string vkgen::Generator::genWithProtectNegate(const std::string &code, const std::string &protect) const {
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

std::pair<std::string, std::string>
  vkgen::Generator::genCodeAndProtect(const BaseType &type, const std::function<void(std::string &)> &function, bool bypass) const {
    if (!type.canGenerate() && !bypass) {
        return std::make_pair("", "");
    }
    std::string       output;
    std::string const protect = std::string{ type.getProtect() };
    function(output);
    return std::make_pair(output, protect);
}

std::string vkgen::Generator::genOptional(const BaseType &type, std::function<void(std::string &)> function, bool bypass) const {
    auto [code, protect] = genCodeAndProtect(type, function, bypass);
    return genWithProtect(code, protect);
}

std::string vkgen::Generator::gen(const Define &define, std::function<void(std::string &)> function) const {
    std::string output;
    if (define.state == Define::DISABLED) {
        return output;
    }
    if (define.state == Define::COND_ENABLED) {
        std::string inner;
        function(inner);
        if (!inner.empty()) {
            output += "#if !defined( " + define.define + " )\n";
            output += inner;
            output += "#endif // " + define.define + "\n";
        }
    } else {
        function(output);
    }
    return output;
}

std::string vkgen::Generator::gen(const NDefine &define, std::function<void(std::string &)> function) const {
    std::string output;
    if (define.state == Define::DISABLED) {
        return output;
    }
    if (define.state == Define::COND_ENABLED) {
        std::string inner;
        function(inner);
        if (!inner.empty()) {
            output += "#if !defined( " + define.define + " )\n";
            output += inner;
            output += "#endif // " + define.define + "\n";
        }
    } else {
        function(output);
    }
    return output;
}

std::string vkgen::Generator::genNamespaceMacro(const Macro &m) {
    std::string output = genMacro(m);
    if (m.usesDefine) {
        output += vkgen::format("#define {0}_STRING  VULKAN_HPP_STRINGIFY({1})\n", m.define, m.value);
    } else {
        output += vkgen::format("#define {0}_STRING  \"{1}\"\n", m.define, m.value);
    }
    return output;
}

std::string vkgen::Generator::generateDefines() {
    std::string output;

    output += R"(
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

    output += R"(
#define VULKAN_HPP_STRINGIFY2( text ) #text
#define VULKAN_HPP_STRINGIFY( text )  VULKAN_HPP_STRINGIFY2( text )
)";
    output += genNamespaceMacro(cfg.macro.mNamespace);

    if (cfg.gen.raii.interop) {
        output += R"(
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

    output += "\n";
    return output;
}

std::string vkgen::Generator::generateHeader() {
    std::string output;

    output += generateDefines();

    output += "#include <vulkan/vulkan.h>\n";

    output += vkgen::format(R"(
static_assert(VK_HEADER_VERSION == {0}, "Wrong VK_HEADER_VERSION!");
)",
                            headerVersion);

    output += R"(
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

)";

    output += "\n";
    return output;
}

void vkgen::Generator::generateFlags(vkgen::OutputBuffer &output) const {
    output += expIfndef("VULKAN_HPP_EXPERIMENTAL_NO_FLAG_TRAITS");
    output += RES_FLAG_TRAITS;
    output += expEndif("VULKAN_HPP_EXPERIMENTAL_NO_FLAG_TRAITS");
    output += RES_FLAGS_1;
    output += expIfndef("VULKAN_HPP_EXPERIMENTAL_NO_FLAG_TRAITS");
    output += R"(
        VULKAN_HPP_CONSTEXPR Flags<BitType> operator~() const VULKAN_HPP_NOEXCEPT
        {
          return Flags<BitType>( m_mask ^ FlagTraits<BitType>::allFlags.m_mask );
        }
)";
    output += expEndif("VULKAN_HPP_EXPERIMENTAL_NO_FLAG_TRAITS");
    output += RES_FLAGS_2;
    output += expIfndef("VULKAN_HPP_EXPERIMENTAL_NO_FLAG_TRAITS");
    output += R"(
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
    output += expEndif("VULKAN_HPP_EXPERIMENTAL_NO_FLAG_TRAITS");
}

void vkgen::Generator::generateMainFile(OutputBuffer &output) {
    output += generateHeader();

    output += beginNamespace();

    output += RES_ARRAY_PROXY;
    // PROXY TEMPORARIES
    output += RES_ARRAY_WRAPPER;
    if (cfg.gen.expApi) {
        output += RES_VECTOR;
    }

    generateFlags(output);

    if (cfg.gen.expApi) {
        output += R"(
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
        output += R"(
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
    output += RES_OPTIONAL;
    // if (cfg.gen.structChain) {
    output += expIfndef("VULKAN_HPP_EXPERIMENTAL_NO_STRUCT_CHAIN");
    output += RES_STRUCT_CHAIN;
    output += expEndif("VULKAN_HPP_EXPERIMENTAL_NO_STRUCT_CHAIN");
    // }
    // UNIQUE HANDLE
    output += generateDispatch();
    output += RES_BASE_TYPES;

    output += endNamespace();
    output += "#include \"vulkan_enums.hpp\"\n";
    output += R"(#if !defined( VULKAN_HPP_NO_TO_STRING )
#  include "vulkan_to_string.hpp"
#endif
)";

    output += beginNamespace();
    output += generateErrorClasses();
    output += "\n";
    output += RES_RESULT_VALUE;
    output += generateApiConstants();

    output += R"(
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

    output += endNamespace();

    output += "#include \"vulkan_handles_forward.hpp\"\n";
    output += "#include \"vulkan_structs_forward.hpp\"\n";

    output += "#include \"vulkan_handles.hpp\"\n";
    output += "#include \"vulkan_structs.hpp\"\n";
    // output += "#  ifndef VULKAN_HPP_EXPERIMENTAL_NO_VK_FUNCS\n";
    output += "#include \"vulkan_funcs.hpp\"\n";
    // output += "#  endif // VULKAN_HPP_EXPERIMENTAL_NO_VK_FUNCS\n";

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
        output += vkgen::format(R"(
#if {0}
#include "vulkan_platforms.hpp"
#endif
)",
                                ifdef);
    }

    // if (cfg.gen.structChain)
    {
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

std::pair<std::string, std::string> vkgen::Generator::generateModuleFunctions(GenOutput &out) {
    auto &funcs      = out.addFile("_funcs_exp");
    auto &funcs_impl = out.addFile("_funcs_impl_exp");
    // auto &mock2 = out.addFile("_mock2");

    const Handle empty{ *this };

    UnorderedFunctionOutputX decl1;
    UnorderedFunctionOutputX def1;

    for (auto &c : commands) {
        if (!c.isSuppored()) {
            continue;
        }

        /*
        if (!c.params.empty()) {
            const VariableData &first = *c.params.begin();
            if (first.isHandle()) {
            }
        }
        */

        {
            ClassCommand                data{ this, &empty, c };
            MemberGeneratorExperimental g{ *this, data, decl1, def1 };

            g.generate();
        }
    }

    funcs += std::move(decl1);
    funcs_impl += std::move(def1);

    std::string outputDecl;
    std::string outputDef;
    const auto  append = [&](std::string &output, const std::string &name) { output += "#include \"" + out.getFilename(name) + "\"\n"; };
    append(outputDecl, "_funcs_exp");
    append(outputDef, "_funcs_impl_exp");

    return std::make_pair(outputDecl, outputDef);
}

void vkgen::Generator::generateModuleEnums(OutputBuffer &output) {
    output += R"(
  //=============
  //=== ENUMs ===
  //=============
  using VULKAN_HPP_NAMESPACE::CppType;
)";

    UnorderedFunctionOutputX out;

    std::unordered_set<std::string> generated;
    std::array<Protect, 1>          protect;
    protect[0].second = true;
    for (const auto &e : enums) {
        if (!e.version) {
            continue;
        }

        const auto *t = find(e.name.original);
        if (t && !t->canGenerate())
            continue;

        //        const BaseType *base = reinterpret_cast<const BaseType*>(&e);
        //        if (t) {
        //            base = reinterpret_cast<const BaseType*>(t);
        //        }

        protect[0].first = e.getProtect();
        auto &output     = out.get(protect);
        if (generated.contains(e.name)) {
            // std::cout << ">> skip enum: " << e.name << std::endl;
            // output += std::move("  // skip: " + e.name + "\n");
            // continue;
        } else {
            output += std::move("  using VULKAN_HPP_NAMESPACE::" + e.name + ";\n");
            std::string name = e.name;
            auto        tag  = strRemoveTag(name);
            if (name.ends_with("FlagBits")) {
                name = std::regex_replace(e.name, std::regex("FlagBits"), "Flags");
                output += std::move("  using VULKAN_HPP_NAMESPACE::" + name + ";\n");
            }
        }

        generated.insert(e.name);
    }

    output += std::move(out);
}

void vkgen::Generator::generateModuleHandles(OutputBuffer &output) {
    output += R"(
  //===============
  //=== HANDLEs ===
  //===============
)";

    if (cfg.gen.expApi) {
        output += "using VULKAN_HPP_NAMESPACE::" + loader.name + ";\n";
    }
    for (const Handle &e : handles.ordered) {
        output += genOptional(e, [&](auto &output) { output += "  using VULKAN_HPP_NAMESPACE::" + e.name + ";\n"; });
    }

    output += R"(
  //======================
  //=== UNIQUE HANDLEs ===
  //======================
#ifndef  VULKAN_HPP_NO_SMART_HANDLE
)";

    for (const Handle &e : handles.ordered) {
        if (e.uniqueVariant()) {
            output += genOptional(e, [&](auto &output) { output += "  using VULKAN_HPP_NAMESPACE::Unique" + e.name + ";\n"; });
        }
    }

    output += "#endif // VULKAN_HPP_NO_SMART_HANDLE\n";
}

void vkgen::Generator::generateModuleStructs(OutputBuffer &output) {
    output += R"(
  //===============
  //=== STRUCTs ===
  //===============
)";
    UnorderedFunctionOutputX out;

    // std::unordered_set<std::string> const generated;
    std::array<Protect, 1> protect;
    protect[0].second = true;
    for (const auto &e : structs) {
        if (!e.version) {
            continue;
        }

        protect[0].first = e.getProtect();
        auto &output     = out.get(protect);
        output += std::move("  using VULKAN_HPP_NAMESPACE::" + e.name + ";\n");
    }

    output += std::move(out);
}

void vkgen::Generator::generateModules(GenOutput &main, std::filesystem::path path) {
    GenOutput outModule{ "vulkan", ".cppm", path };
    auto     &module_output = outModule.addFile("", ".cppm");

    const std::string ns      = getNamespace(Namespace::VK, false);
    const std::string ns_raii = getNamespace(Namespace::RAII, false);
    /*
    handles += beginNamespace();

    handles += expIfndef("VULKAN_HPP_EXPERIMENTAL_NO_TEMPLATES");
    handles += R"(
  template <typename Type>
  struct isVulkanHandleType
  {
    static VULKAN_HPP_CONST_OR_CONSTEXPR bool value = false;
  };
)";
    handles += expEndif("VULKAN_HPP_EXPERIMENTAL_NO_TEMPLATES");

    if (cfg.gen.expApi) {
        handles += "  namespace exp {\n";
        handles += "    class Instance;\n";
        handles += "    class Device;\n";
        handles += "  }\n";
    }

    UnorderedFunctionOutput unused(*this);

    for (const Handle &h : this->handles.ordered) {
        handles += genOptional(h, [&](auto &output) {
            // output += "  using " + h.name + " = Vk" + h.name + ";\n";
            output += generateClassStr(h, unused, unused, inter, false, true);
        });
    }
    handles += endNamespace();
    */

    //    if (false) {
    //        auto [functionsPublic, functionsPrivate] = generateModuleFunctions(out);
    //    }

    // generateExperimentalRAII(raii, main);

    /*
    {
        output += generateDefines();



        output += "#include <vulkan/vulkan.h>\n";

        output += vkgen::format(R"(
static_assert(VK_HEADER_VERSION == {0}, "Wrong VK_HEADER_VERSION!");
)",  headerVersion);

        // includes
        output += R"(
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

)";

        output += beginNamespace();

        output += RES_ARRAY_PROXY;
        // PROXY TEMPORARIES
        output += RES_ARRAY_WRAPPER;

        generateFlags(output);
        output += R"(
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

        output += RES_OPTIONAL;
        output += RES_STRUCT_CHAIN;
        output += RES_BASE_TYPES;

        output += endNamespace();

        output += R"(
#include "../vulkan_structs_forward.hpp"

#include "../vulkan_enums.hpp"
#if !defined( VULKAN_HPP_NO_TO_STRING )
#  include "../vulkan_to_string.hpp"
#endif
)";
        output += beginNamespace();
        output += generateErrorClasses();
        output += RES_RESULT_VALUE;
        output += generateApiConstants();

        output += endNamespace();

        output += R"(
#include "vulkan_handles.hpp"
#include "vulkan_context.hpp"
#include "../vulkan_structs.hpp"
)";
        output += beginNamespace();
        generateStructChains(output);
        output += endNamespace();

        output += mock;
    }
    */

    module_output += "module;\n\n";

    // Macro m("VULKAN_HPP_EXP_NAMESPACE", "exp", true);
    // module_output += genNamespaceMacro(m);
    // module_output += "\n";

    // module_output += "#ifndef VULKAN_HPP_ENABLE_EXP\n";
    // module_output += "#define VULKAN_HPP_ENABLE_EXP\n";
    // module_output += "#endif\n";
    module_output += "#include <iostream> // current workaround for clang\n";
    module_output += "#include \"" + main.getFilename("") + "\"\n";
    if (cfg.gen.raii.enabled) {
        module_output += "#include \"" + main.getFilename("_raii") + "\"\n";
    }

    module_output += "\nexport module vulkan;\n\n";

    module_output += "  // VULKAN CORE\n";
    module_output += "export {\n";

    module_output += R"(
  //=============
  //=== ENUMs ===
  //=============
)";

    auto &dbg = get(std::string_view{ "VkRefreshObjectFlagBitsKHR" });
    // std::cout << ">>> E: " << dbg.isSuppored() << "  " << dbg.name << "\n";
    for (const auto &e : enums) {
        if (!e.version) {
            continue;
        }
        const auto *t = find(e.name.original);
        if (t && !t->canGenerate())
            continue;

        const BaseType *base = reinterpret_cast<const BaseType *>(&e);
        if (t) {
            base = reinterpret_cast<const BaseType *>(t);
        }

        module_output += genOptional(*base, [&](auto &output) {
            std::string name = e.name.original;
            if (e.members.empty()) {
                name = std::regex_replace(name, std::regex("FlagBits"), "Flags");
            }
            output += "  using ::" + name + ";";
            // output += " // " + std::to_string(e.isSuppored()) + " " + e.name.original;
            output += "\n";
        });
    }

    module_output += R"(
  //===============
  //=== STRUCTs ===
  //===============
)";

    for (const auto &e : this->structs) {
        if (!e.version) {
            continue;
        }
        module_output += genOptional(e, [&](auto &output) { output += "  using ::" + e.name.original + ";\n"; });
    }

    module_output += R"(
  //===============
  //=== HANDLEs ===
  //===============
)";

    for (const auto &e : this->handles) {
        if (!e.version) {
            continue;
        }
        module_output += genOptional(e, [&](auto &output) { output += "  using ::" + e.name.original + ";\n"; });
    }

    module_output += "}\n";

    module_output += "export " + beginNamespace();

    module_output += format(R"(
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
                            ns);

    generateModuleEnums(module_output);

    module_output += format(R"(
  //=========================
  //=== Index Type Traits ===
  //=========================
  //using VULKAN_HPP_NAMESPACE::IndexTypeValue;

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
                            ns);

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
        module_output += "  using " + ns + "::" + e + ";\n";
    }
    for (const auto &e : platformErrors) {
        module_output += genOptional(e->value, [&](auto &output) { output += "  using " + ns + "::" + e->name + ";\n"; });
    }

    module_output += format(R"(#endif /*VULKAN_HPP_NO_EXCEPTIONS*/

  using {0}::createResultValueType;
  using {0}::ignore;
  using {0}::resultCheck;
  using {0}::ResultValue;
  using {0}::ResultValueType;

  //=========================================
  //=== CONSTEXPR CONSTANTs AND FUNCTIONs ===
  //=========================================
)",
                            ns);

    for (const auto &a : apiConstants) {
        module_output += genOptional(a, [&](std::string &output) { output += "  using " + ns + "::" + a.name + ";\n"; });
    }

    module_output += format(R"(
  //========================
  //=== CONSTEXPR VALUEs ===
  //========================
  using VULKAN_HPP_NAMESPACE::HeaderVersion;

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
                            ns);
    // === STRUCTs ===
    generateModuleStructs(module_output);

    // === HANDLEs ===
    generateModuleHandles(module_output);

    /*
        module_output += R"(
      //=====================
      //=== Format Traits ===
      //=====================

      //======================================
      //=== Extension inspection functions ===
      //======================================
    )";
    */

    //    if (false) {
    //        module_output += R"(
    //  //===========================
    //  //=== COMMANDs ===
    //  //===========================
    //)";
    //        module_output += std::move(functionsPublic);
    //    }

    module_output += "  namespace VULKAN_HPP_RAII_NAMESPACE {\n";

    module_output += R"(
  //======================
  //=== RAII HARDCODED ===
  //======================
)";
    module_output += format(R"(
    using {0}::{1};
  // using {0}::{1}Dispatcher;
  using {0}::DeviceDispatcher;
  using {0}::InstanceDispatcher;
  using {0}::exchange;
  )",
                            ns_raii,
                            loader.name);

    module_output += R"(
  //====================
  //=== RAII HANDLEs ===
  //====================
)";

    for (const Handle &e : this->handles.ordered) {
        module_output += genOptional(e, [&](auto &output) { output += "  using " + ns_raii + "::" + e.name + ";\n"; });
    }

    module_output += "  } // VULKAN_HPP_RAII_NAMESPACE\n";

    module_output += endNamespace();
    module_output += "module : private;\n";
    module_output += beginNamespace();

    // module_output += "  namespace VULKAN_HPP_EXP_NAMESPACE {\n";

    //    if (false) {
    //        module_output += std::move(functionsPrivate);
    //    }

    // module_output += "  } // VULKAN_HPP_EXP_NAMESPACE\n";

    module_output += endNamespace();

    outModule.writeFiles(*this);
}

template <typename T>
static void generateForwardDeclarations(vkgen::OutputBuffer &output, const std::vector<T> &items, vkgen::Generator &gen) {
    output += gen.beginNamespace();

    for (const vkgen::BaseType &i : items) {
        output += gen.genOptional(i, [&](auto &output) { output += "  " + i.metaTypeDeclaration() + " " + i.name + ";\n"; });
    }

    output += gen.endNamespace();
}

void vkgen::Generator::wrapNamespace(vkgen::OutputBuffer &output, std::function<void(OutputBuffer &)> func) {
    output += beginNamespace();
    func(output);
    output += endNamespace();
}

void vkgen::Generator::generateForwardHandles(vkgen::OutputBuffer &output) {
    wrapNamespace(output, [&](auto &output) {
        for (const auto &e : handles.ordered) {
            output += generateClassDecl(e);
        }
        for (const Handle &e : handles.ordered) {
            if (e.uniqueVariant()) {
                output += generateClassDecl(e, "Unique" + e.name);
            }
        }
    });
}

void vkgen::Generator::generateFiles(std::filesystem::path path) {
    // std::cout << "gen files " << '\n';

    std::string const prefix = "vulkan";
    std::string       ext    = ".hpp";

    GenOutput out{ prefix, ext, path };

    auto &enums           = out.addFile("_enums");
    auto &enums_forward   = out.addFile("_enums_forward");
    auto &to_string       = out.addFile("_to_string");
    auto &handles         = out.addFile("_handles");
    auto &smart_handles   = out.addFile("_smart");
    auto &forward         = out.addFile("_forward");
    auto &handles_forward = out.addFile("_handles_forward");
    auto &structs_forward = out.addFile("_structs_forward");
    auto &structs         = out.addFile("_structs");
    auto &funcs           = out.addFile("_funcs");
    auto &raii            = out.addFile("_raii");
    auto &raii_forward    = out.addFile("_raii_forward");
    auto &raii_funcs      = out.addFile("_raii_funcs");
    auto &platforms       = out.addFile("_platforms");
    auto &output          = out.addFile("");

    generateEnums(enums, enums_forward, to_string);

    generateForwardHandles(handles_forward);

    structs_forward += beginNamespace();
    for (const Struct &e : this->structs.ordered) {
        structs_forward += generateStructDecl(e);
    }
    structs_forward += endNamespace();

    forward += "#include \"" + out.getFilename("_enums_forward") + "\"\n";
    forward += "#include \"" + out.getFilename("_structs_forward") + "\"\n";
    forward += "#include \"" + out.getFilename("_handles_forward") + "\"\n";

    std::string funcs2;
    generateHandles(handles, smart_handles, out, funcs2);

    structs += generateStructs();

    // generatedDestroyOverloads.clear();

    generateRAII(raii, raii_forward, out);

    funcs += beginNamespace();
    funcs += outputFuncs.get(false);
    funcs += "#ifndef VULKAN_HPP_DISABLE_ENHANCED_MODE\n";
    funcs += outputFuncsEnhanced.get(false);
    funcs += "#endif // VULKAN_HPP_DISABLE_ENHANCED_MODE\n";
    funcs += funcs2;
    funcs += endNamespace();
    // if (!cfg.gen.cppModules) {
    generateFuncsRAII(raii_funcs);
    // }

    for (const auto &o : outputFuncs.segments) {
        const auto &key = o.first;
        if (key.empty()) {
            continue;
        }
        platformOutput.segments[key] += o.second.get(*this);
    }
    platforms += beginNamespace();
    platforms += platformOutput.get();
    platforms += endNamespace();

    generateMainFile(output);

    if (cfg.gen.expApi) {
        auto                   &context = out.addFile("_context");
        UnorderedFunctionOutput inter(*this);
        generateContext(context, inter);
    }

    if (cfg.gen.cppModules) {
        generateModules(out, path);
    }

    out.writeFiles(*this);
}

void vkgen::Generator::generateEnumStr(const Enum &data, std::string &output, std::string &to_string_output) {
    const std::string &name = data.name;

    UnorderedOutput members{ *this };
    UnorderedOutput to_string{ *this };

    for (const auto &m : data.members) {
        members.add(m, [&](std::string &output) { output += "    " + m.name + " = " + m.name.original + ",\n"; });
        if (!m.isAlias) {
            to_string.add(m, [&](std::string &output) {
                std::string value = m.name;
                strStripPrefix(value, "e");
                output += "      case " + name;
                output += "::" + m.name;
                output += ": return \"" + value + "\";\n";
            });
        }
    }
    output += "  enum class " + name;
    if (data.isBitmask) {
        std::string const base = std::regex_replace(data.name.original, std::regex("FlagBits"), "Flags");
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
    } else {
        to_string_output += genOptional(data, [&](std::string &output) {
            std::string str = to_string.get();
            if (str.empty()) {
                output += vkgen::format(R"(
  VULKAN_HPP_INLINE std::string to_string({0}) {{
    return {1};
  }}
)",
                                        name,
                                        "\"(void)\"");
            } else {
                str += R"*(
  default: return "invalid ( " + VULKAN_HPP_NAMESPACE::toHexString(static_cast<uint32_t>(value))  + " )";
)*";

                output += vkgen::format(R"(
  VULKAN_HPP_INLINE std::string to_string({0} value) {{
    switch (value) {{
      {1}
    }}
  }}
)",
                                        name,
                                        str);
            }
        });
    }
}

void vkgen::Generator::generateEnum(const Enum &data, OutputBuffer &output, OutputBuffer &output_forward, OutputBuffer &to_string_output) {
    auto p = data.getProtect();
    if (!p.empty()) {
        // std::cout << "enum with p: " << p << '\n';
        platformOutput.add(data, [&](std::string &output) { generateEnumStr(data, output, output); });
    } else {
        output += genOptional(data, [&](std::string &output) { generateEnumStr(data, output, to_string_output.emplace()); });
    }

    output_forward += genOptional(data, [&](std::string &output) {
        output += "  enum class " + data.name;
        if (data.isBitmask) {
            std::string const base = std::regex_replace(data.name.original, std::regex("FlagBits"), "Flags");
            output += " : " + base;
        }
        output += ";\n";
    });
}

std::string vkgen::Generator::generateToStringInclude() const {
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

    return R"(
#if __cpp_lib_format
#  include <format>   // std::format
#else
#  include <sstream>  // std::stringstream
#endif
)";
}

void vkgen::Generator::generateEnums(OutputBuffer &output, OutputBuffer &output_forward, OutputBuffer &to_string_output) {
    if (verbose) {
        std::cout << "gen enums " << '\n';
    }

    to_string_output += generateToStringInclude();
    to_string_output += beginNamespace();
    output += beginNamespace();
    output_forward += beginNamespace();

    output += expIfndef("VULKAN_HPP_EXPERIMENTAL_NO_TEMPLATES");
    output += R"(
  template <typename EnumType, EnumType value>
  struct CppType
  {};
)";
    output += expEndif("VULKAN_HPP_EXPERIMENTAL_NO_TEMPLATES");

    if (cfg.gen.expApi) {
        to_string_output += R"(
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
        to_string_output += R"(
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

    std::unordered_set<std::string> generated;
    for (const auto &e : enums) {
        if (generated.contains(e.name)) {
            continue;
        }
        generateEnum(e, output, output_forward, to_string_output);
        generated.insert(e.name);
    }

    to_string_output += endNamespace();
    output += endNamespace();
    output_forward += endNamespace();

    if (verbose) {
        std::cout << "gen enums done" << '\n';
    }
}

std::string vkgen::Generator::genFlagTraits(const Enum &data, std::string inherit, std::string &to_string_output) {
    std::string output;

    std::string name = std::regex_replace(data.name, std::regex("FlagBits"), "Flags");

    std::string flags;
    std::string str;

    for (size_t i = 0; i < data.members.size(); ++i) {
        const auto &m = data.members[i];
        if (m.isAlias) {
            continue;
        }
        flags += genOptional(m, [&](std::string &output) { output += "        | " + inherit + "::" + m.name + "\n"; });
        str += genOptional(m, [&](std::string &output) {
            std::string value = m.name;
            strStripPrefix(value, "e");
            output += vkgen::format(R"(
    if (value & {0}::{1})
      result += "{2} | ";
)",
                                    inherit,
                                    m.name,
                                    value);
        });
    }
    strStripPrefix(flags, "        | ");
    if (flags.empty()) {
        flags += "{}";
    }

    output += vkgen::format(R"(
  using {0} = Flags<{1}>;
)",
                            name,
                            inherit);

    std::string bitmask;
    if (data.isBitmask) {
        bitmask = "static VULKAN_HPP_CONST_OR_CONSTEXPR bool             isBitmask = true;\n    ";
    }
    output += expIfndef("VULKAN_HPP_EXPERIMENTAL_NO_FLAG_TRAITS");
    output += vkgen::format(R"(
  template <>
  struct FlagTraits<{0}> {{
    {1}static VULKAN_HPP_CONST_OR_CONSTEXPR {3} allFlags = {2};
  }};
)",
                            inherit,
                            bitmask,
                            flags,
                            name);
    output += expEndif("VULKAN_HPP_EXPERIMENTAL_NO_FLAG_TRAITS");

    if (str.empty()) {
        to_string_output += vkgen::format(R"(
  VULKAN_HPP_INLINE std::string to_string({0}) {{
    return "{{}}";
  }}
)",
                                          name,
                                          inherit,
                                          str);
    } else {
        to_string_output += vkgen::format(R"(
  VULKAN_HPP_INLINE std::string to_string({0} value) {{
    if ( !value )
      return "{{}}";

    std::string result;
    {2}

    return "{{ " + result.substr( 0, result.size() - 3 ) + " }}";
  }}
)",
                                          name,
                                          inherit,
                                          str);
    }

    return output;
}

std::string vkgen::Generator::generateDispatch() {
    std::string output;
    output += generateDispatchLoaderBase();
    output += "#if !defined( VK_NO_PROTOTYPES )\n";
    output += generateDispatchLoaderStatic();
    output += "#endif // VK_NO_PROTOTYPES\n";
    output += R"(
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
#    define VULKAN_HPP_DEFAULT_DISPATCHER ::VULKAN_HPP_NAMESPACE::defaultDispatchLoaderDynamic
#    define VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE                     \
      namespace VULKAN_HPP_NAMESPACE                                                        \
      {                                                                            \
        VULKAN_HPP_STORAGE_API DispatchLoaderDynamic defaultDispatchLoaderDynamic; \
      }
  extern VULKAN_HPP_STORAGE_API DispatchLoaderDynamic defaultDispatchLoaderDynamic;
#  else
  static inline ::VULKAN_HPP_NAMESPACE::DispatchLoaderStatic & getDispatchLoaderStatic()
  {
    static ::VULKAN_HPP_NAMESPACE::DispatchLoaderStatic dls;
    return dls;
  }
#    define VULKAN_HPP_DEFAULT_DISPATCHER ::VULKAN_HPP_NAMESPACE::getDispatchLoaderStatic()
#    define VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE
#  endif
#endif

#if !defined( VULKAN_HPP_DEFAULT_DISPATCHER_TYPE )
#  if VULKAN_HPP_DISPATCH_LOADER_DYNAMIC == 1
#    define VULKAN_HPP_DEFAULT_DISPATCHER_TYPE ::VULKAN_HPP_NAMESPACE::DispatchLoaderDynamic
#  else
#    define VULKAN_HPP_DEFAULT_DISPATCHER_TYPE ::VULKAN_HPP_NAMESPACE::DispatchLoaderStatic
#  endif
#endif
)";
    return output;
}

std::string vkgen::Generator::generateApiConstants() {
    std::string output;

    output += R"(
    //===========================
    //=== CONSTEXPR CONSTANTs ===
    //===========================
)";

    for (const auto &a : apiConstants) {
        output +=
          genOptional(a, [&](std::string &output) { output += "    VULKAN_HPP_CONSTEXPR_INLINE " + a.type + " " + a.name + " = " + a.name.original + ";\n"; });
    }

    output += R"(
    //========================
    //=== CONSTEXPR VALUEs ===
    //========================
    VULKAN_HPP_CONSTEXPR_INLINE uint32_t HeaderVersion = VK_HEADER_VERSION;
)";

    return output;
}

std::string vkgen::Generator::generateErrorClasses() {
    std::string output;
    output += RES_ERRORS;

    std::string str;

    for (const auto &e : errorClasses) {
        //        std::string name = e->name;
        //        strStripPrefix(name, "eError");
        //        name += "Error";

        output += genOptional(e.value, [&](std::string &output) {
            output += vkgen::format(R"(
  class {0} : public SystemError
  {{
  public:
    {0}( std::string const & message ) : SystemError( make_error_code( Result::{1} ), message ) {{}}
    {0}( char const * message ) : SystemError( make_error_code( Result::{1} ), message ) {{}}
  }};
)",
                                    e.name,
                                    e.value.name);
        });

        str += genOptional(e.value, [&](std::string &output) { output += "        case Result::" + e.value.name + ": throw " + e.name + "(message);\n"; });
    }

    output += "  namespace {\n";
    output += vkgen::format(R"(
    [[noreturn]] void throwResultException(VULKAN_HPP_NAMESPACE::Result result, char const *message) {{
      switch (result) {{
{0}
        default: throw SystemError( make_error_code( result ) );
      }}
    }}
)",
                            str);
    output += "  } // namespace\n";

    return output;
}

std::string vkgen::Generator::generateDispatchLoaderBase() {
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
    dispatchLoaderBaseGenerated = true;
    return output;
}

std::string vkgen::Generator::generateDispatchLoaderStatic() {
    std::string output;
    output += "//#if !defined( VK_NO_PROTOTYPES )\n";
    output += "  class DispatchLoaderStatic : public DispatchLoaderBase {\n";
    output += "  public:\n";

    Handle     empty{ *this };
    const auto genCommand = [&](Command &c) {
        output += genOptional(c, [&](std::string &output) {
            ClassCommand                       d{ this, &empty, c };
            MemberContext                      ctx{ .ns = Namespace::VK, .disableDispatch = true, .disableAllocatorRemoval = true };
            MemberResolverStaticDispatch const r{ *this, d, ctx };

            output += r.temporary();
        });
    };

    for (auto &command : commands) {
        genCommand(command);
    }

    output += "  };\n";
    output += "//#endif\n";
    return output;
}

std::string vkgen::Generator::generateStructDecl(const Struct &d) const {
    return genOptional(d, [&](std::string &output) {
        if (d.isStruct()) {
            output += "  struct " + d.name + ";\n";
        } else {
            output += "  union " + d.name + ";\n";
        }

        for (const auto &a : d.aliases) {
            output += "  using " + strStripVk(a) + " = " + d.name + ";\n";
        }
    });
}

std::string vkgen::Generator::generateClassDecl(const Handle &data, const std::string &name) const {
    return genOptional(data, [&](std::string &output) { output += "  class " + name + ";\n"; });
}

std::string vkgen::Generator::generateClassDecl(const Handle &data) const {
    return generateClassDecl(data, data.name);
}

std::string vkgen::Generator::generateClassString(const std::string &className, const GenOutputClass &from, Namespace ns) const {
    std::string output = "  class " + className;
    if (!from.inherits.empty()) {
        output += " : " + from.inherits;
    }
    output += " {\n";

    const auto addSection = [&](const std::string &visibility, const std::string &s) {
        if (!s.empty()) {
            output += "  " + visibility + ":\n" + s;
        }
    };

    std::string funcs;
    if (ns == Namespace::VK) {
        std::string const temp = from.sFuncs.get();
        if (!temp.empty()) {
            funcs += expIfndef("VULKAN_HPP_EXPERIMENTAL_NO_VK_FUNCS");
            funcs += temp;
            funcs += expEndif("VULKAN_HPP_EXPERIMENTAL_NO_VK_FUNCS");
        }
    } else {
        funcs = from.sFuncs.get();
    }
    std::string const funcsEnhanced = from.sFuncsEnhanced.get();
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

std::string vkgen::Generator::generateForwardInclude(GenOutput &out) const {
    std::string output;
    output += "#include \"" + out.getFilename("_forward") + "\"\n";
    // output += "#include \"" + out.getFilename("_handles_forward") + "\"\n";
    // output += "#include \"" + out.getFilename("_structs_forward") + "\"\n";

    if (cfg.gen.raii.interop) {
        output += "#include \"" + out.getFilename("_raii_forward") + "\"\n";
    }

    return output;
}

void vkgen::Generator::generateHandles(OutputBuffer &output, OutputBuffer &output_smart, GenOutput &out, std::string &out_funcs) {
    if (verbose) {
        std::cout << "gen handles " << '\n';
    }

    // if (!cfg.gen.cppModules) {
    output += generateForwardInclude(out);
    // }

    if (cfg.gen.expApi) {
        output += "#include \"vulkan_context.hpp\"\n";
    }

    // if (!cfg.gen.cppModules) {
    output += beginNamespace();
    // }

//    for (const Struct &e : structs.ordered) {
//        output += generateStructDecl(e);
//    }

    output += expIfndef("VULKAN_HPP_EXPERIMENTAL_NO_TEMPLATES");
    output += R"(
  template <typename Type>
  struct isVulkanHandleType
  {
    static VULKAN_HPP_CONST_OR_CONSTEXPR bool value = false;
  };
)";
    output += expEndif("VULKAN_HPP_EXPERIMENTAL_NO_TEMPLATES");

    if (cfg.gen.internalFunctions) {
        std::string spec;
        if (!cfg.gen.cppModules) {
            spec = "static";
        }

        output += R"(
  namespace internal {

    template<typename T, typename V, typename S, typename PFN, typename... Args>
    inline std::vector<T> createArrayVoidPFN(const PFN pfn, const char *const msg, Args&&... args) {
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
    inline typename ResultValueType<std::vector<T>>::type createArray(const PFN pfn, const char *const msg, Args&&... args) {
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

)";
    }

    if (!cfg.gen.expApi) {
        // static commands

        const Handle   empty{ *this };
        GenOutputClass out{ *this };
        for (auto &c : staticCommands) {
            ClassCommand  d{ this, &empty, c };
            MemberContext ctx{ .ns = Namespace::VK, .isStatic = true };
            generateClassMember(d, ctx, out, outputFuncs, outputFuncsEnhanced);
        }
        output += out.sPublic.get();
    }

    // UnorderedFunctionOutput funcs{*this};
    UnorderedFunctionOutputX funcs2{};

    UnorderedFunctionOutput expfuncs(*this);
    for (Handle &e : handles.ordered) {
        // std::cout << "gen class " << e.name << '\n';
        if (cfg.gen.expApi && !e.isSubclass) {
            output += std::move(generateClassWithPFN(e, outputFuncs, funcs2));

        } else {
            output += generateClass(e, outputFuncs, outputFuncsEnhanced, expfuncs);
        }
    }

    std::stringstream temp;
    funcs2.write(temp);
    out_funcs += "// FUNCS2 {\n";
    out_funcs += temp.str();
    out_funcs += "// } FUNCS2\n";

    if (!cfg.gen.cppModules) {
        generateUniqueHandles(output_smart);

        output += "#ifndef  VULKAN_HPP_NO_SMART_HANDLE\n";
        output += "#include \"" + out.getFilename("_smart") + "\"\n";
        output += "#endif // VULKAN_HPP_NO_SMART_HANDLE\n";
    }

    /*
    { // extended
        output += "#ifndef VULKAN_HPP_EXPERIMENTAL_NO_INTEROP\n";
        for (Handle &e : handles.ordered) {
            if (!e.vectorVariant) {
                continue;
            }

            output += "// vector class: " + e.name + "s\n";
            for (auto &c : e.vectorCmds) {
                output += "//    " + c.name.original + "\n";

                MemberContext ctx;
                MemberResolverCtor r{*this, c, ctx};
//                output += r.generateDeclaration();

            }
            output += "\n";

        }
        output += "#endif // VULKAN_HPP_EXPERIMENTAL_NO_INTEROP\n";
    }
    */

    // if (!cfg.gen.cppModules) {
    output += endNamespace();
    // }

    if (verbose) {
        std::cout << "gen handles done" << '\n';
    }
}

void vkgen::Generator::generateUniqueHandles(OutputBuffer &output) {
    if (cfg.gen.cppModules) {
        return;
    }
    // output += "#ifndef  VULKAN_HPP_NO_SMART_HANDLE\n";
    for (Handle const &e : handles.ordered) {
        if (e.uniqueVariant()) {
            output += generateUniqueClass(e, outputFuncs);
        }
    }
    // output += "#endif // VULKAN_HPP_NO_SMART_HANDLE\n";
}

std::string vkgen::Generator::generateStructsInclude() const {
    return "#include <cstring>  // strcmp\n";
}

std::string vkgen::Generator::generateStructs(bool exp) {
    /*
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
     */
    std::string out;
    out += "#include \"vulkan_structs_forward.hpp\"\n";
    if (!exp) {
        out += '\n';
        out += expIfndef("VULKAN_HPP_EXPERIMENTAL_NO_STRUCT_COMPARE");
        out += generateStructsInclude();
        out += expEndif("VULKAN_HPP_EXPERIMENTAL_NO_STRUCT_COMPARE");
        out += '\n';
    }
    out += beginNamespace();
    //    if (!cfg.gen.expApi) {
    //        out += "extern \"C\" {\n";
    //    }
    for (const Struct &e : structs.ordered) {
        out += genOptional(e, [&](auto &output) { output += generateStruct(e, exp); });
    }
    //    if (!cfg.gen.expApi) {
    //        out += "}\n";
    //    }

    out += endNamespace();

    return out;
}

void vkgen::Generator::generateStructChains(vkgen::OutputBuffer &output) {
    UnorderedOutput out{ *this };
    for (const Struct &s : structs.ordered) {
        if (s.extends.empty()) {
            continue;
        }
        auto p = s.getProtect();
        //        if (!p.empty())
        //            out += "//1: " + std::string{p} + "\n";
        out += genOptional(s, [&](std::string &output) {
            for (const auto &e : s.extends) {
                auto p2 = e->getProtect();
                //                if (!p2.empty())
                //                    output += "//2: " + std::string{p2} + "\n";
                bool bypass = p == p2;
                output += genOptional(
                  *e,
                  [&](std::string &output) {
                      output += "template <>\n";
                      output += "struct StructExtends<" + s.name + ", " + e->name + ">\n";
                      output += R"(
 {
   enum
   {
     value = true
   };
 };
)";
                  },
                  bypass);
            }
        });
    }
    output += out.get();
}

bool vkgen::Generator::generateStructConstructor(const Struct &data, bool transform, std::string &output) {
    InitializerBuilder init{ "      " };
    std::string        args;
    std::string        pNextArg;
    std::string        templ;
    bool               hasProxy = false;

    for (const auto &p : data.members) {
        std::string id = p->identifier() + "_";

        bool const toProxy = transform && p->hasLengthVar();
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
            args += cfg.macro.mNamespace.get();
            args += "::StructureType::eApplicationInfo";
        } else if (p->identifier() == "pNext") {
            pNextArg += p->fullType();
            pNextArg += " ";
            pNextArg += id;
            pNextArg += " = nullptr";
        } else {
            if (toProxy) {
                VariableData var(*p);
                var.removeLastAsterisk();
                if (var.type() == "void" && !var.isPointer()) {
                    var.setType("T");
                    templ = "    template <typename T>\n";
                }
                args += vkgen::format("VULKAN_HPP_NAMESPACE::ArrayProxyNoTemporaries<{0}> const & {1}", var.fullType(), id);
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

        std::string const lhs = p->identifier();
        if (toProxy) {
            init.append(lhs, id + ".data()");
        } else {
            std::string rhs  = id;
            const auto &vars = p->getArrayVars();
            if (!vars.empty() && transform) {
                rhs = "static_cast<uint32_t>(";
                // std::cout << "> array vars: " << vars.size() << std::endl;
                for (size_t i = 0; i < vars.size(); ++i) {
                    const auto        &v  = vars[i];
                    const std::string &id = v->identifier();
                    if (i != vars.size() - 1) {
                        rhs += " !";
                        rhs += id;
                        rhs += "_";
                        rhs += ".empty()? ";
                        rhs += id;
                        rhs += "_.size() :\n";
                    } else {
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
        output += cfg.macro.mConstexpr.get();
        output += " ";
    }
    output += data.name + "( " + args + " )\n";
    output += "      " + init.string();
    output += "\n    {}\n";

    return hasProxy;
}

std::string vkgen::Generator::generateStruct(const Struct &data, bool exp) {
    std::string output;

    bool hasSetters = false;
    for (const auto &m : data.members) {
        if (m->type() == "StructureType") {
            continue;
        }
        hasSetters = true;
    }

    bool genSetters          = hasSetters && !data.returnedonly;
    bool genSettersProxy     = genSetters;
    bool genCompareOperators = data.isStruct();

    std::string members;
    std::string funcs;
    std::string structureType;
    if (!data.structTypeValue.empty()) {
        structureType = "StructureType::" + data.structTypeValue;
    }

    bool containsFloatingPoints = false;
    for (const auto &m : data.members) {
        if (data.isStruct()) {
            std::string assignment = "{}";
            const auto &type       = m->original.type();
            if (type == "VkStructureType") {
                assignment = structureType.empty() ?
                                                   /*cfg.macro.mNamespace.get() + "::"*/
                               "StructureType::eApplicationInfo"
                                                   : structureType;
            }

            m->setAssignment(" = " + assignment);
            members += "    " + m->toStructStringWithAssignment() + ";\n";

            const auto &s = structs.find(type);
            if (s != structs.end()) {
                if (s->isUnion()) {
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

    if (genSetters || genSettersProxy) {
        const auto &define = data.isStruct() ? cfg.gen.structSetters : cfg.gen.unionSetters;
        funcs += gen(define, [&](std::string &output) {
            if (genSetters) {
                for (const auto &m : data.members) {
                    if (m->type() == "StructureType") {
                        continue;
                    }
                    auto var = VariableData(*m);
                    var.setIdentifier(m->identifier() + "_");

                    std::string id   = m->identifier();
                    std::string name = strFirstUpper(id);
                    std::string arg  = var.toString();
                    output += vkgen::format(R"(
    VULKAN_HPP_CONSTEXPR_14 {0}& set{1}({2}) VULKAN_HPP_NOEXCEPT {{
      {3} = {4};
      return *this;
    }}
            )",
                                            data.name,
                                            name,
                                            arg,
                                            id,
                                            var.identifier());
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

                        std::string const id = m->identifier();
                        std::string       modif;
                        if (var.original.type() == "void" && !var.original.isConstSuffix()) {
                            output += "    template <typename DataType>\n";
                            var.setType("DataType");
                            modif = " * sizeof(DataType)";
                        }

                        std::string arg = "ArrayProxyNoTemporaries<" + var.fullType() + "> const &" + var.identifier();
                        output += vkgen::format(R"(
    {0}& set{1}({2}) VULKAN_HPP_NOEXCEPT {{
      {4} = static_cast<uint32_t>({3}.size(){6});
      {5} = {3}.data();
      return *this;
    }}
                )",
                                                data.name,
                                                name,
                                                arg,
                                                var.identifier(),
                                                m->getLengthVar()->identifier(),
                                                m->identifier(),
                                                modif);
                    }
                }
            }
        });
    }

    output += "  " + data.metaTypeDeclaration() + " " + data.name + " {\n";
    output += "    using NativeType = " + data.name.original + ";\n";

    if (data.isStruct()) {
        if (!structureType.empty()) {
            output +=
              "    static const bool                               "
              "   allowDuplicate = false;\n";
            output += "    static VULKAN_HPP_CONST_OR_CONSTEXPR " + format("VULKAN_HPP_NAMESPACE") + "::StructureType structureType = " + structureType + ";\n";
        }
    }

    // constructor
    if (true) {
        if (data.isStruct()) {
            output += gen(cfg.gen.structConstructors, [&](std::string &output) {
                bool const hasProxy = generateStructConstructor(data, false, output);

                if (hasProxy) {
                    output += "#  if !defined( VULKAN_HPP_DISABLE_ENHANCED_MODE )\n";
                    generateStructConstructor(data, true, output);
                    output += "#  endif // VULKAN_HPP_DISABLE_ENHANCED_MODE \n";
                }
            });
        } else {
            output += gen(cfg.gen.unionConstructors, [&](std::string &output) {
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

                    std::string arg = var.toString();
                    std::string assignment;
                    if (first) {
                        assignment = " = {}";
                        first      = false;
                    }
                    output += vkgen::format(R"(
    VULKAN_HPP_CONSTEXPR_14 {0}({1}{2}) : {3}( {4} ) {{}}
                )",
                                            data.name,
                                            arg,
                                            assignment,
                                            id,
                                            var.identifier());
                }
            });
        }
    }

    if (data.isStruct()) {
        output += vkgen::format(R"(
    {0} & operator=({0} const &rhs) VULKAN_HPP_NOEXCEPT = default;

    {0} & operator=({1} const &rhs) VULKAN_HPP_NOEXCEPT {{
      *this = *reinterpret_cast<VULKAN_HPP_NAMESPACE::{0} const *>(&rhs);
      return *this;
    }}
)",
                                data.name,
                                data.name.original);
    }

    output += funcs;

    // operator {0}*() { return this; }
    output += vkgen::format(R"(

    explicit operator {1} const &() const VULKAN_HPP_NOEXCEPT {{
      return *reinterpret_cast<const {1} *>(this);
    }}

    explicit operator {1}&() VULKAN_HPP_NOEXCEPT {{
      return *reinterpret_cast<{1} *>(this);
    }}

)",
                            data.name,
                            data.name.original);

    gen(cfg.gen.structReflect, [&](std::string &output) {
        ArgumentBuilder types{ false };
        ArgumentBuilder tie{ false };
        for (const auto &m : data.members) {
            types.append(m->fullType(), "");
            tie.append("", m->identifier());
        }
        output += vkgen::format(R"(
#  if 14 <= VULKAN_HPP_CPP_VERSION
    auto
#  else
    std::tuple<{0}>
#  endif
      reflect() const VULKAN_HPP_NOEXCEPT
    {{
      return std::tie({1});
    }}
)",
                                types.string(),
                                tie.string());
    });

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
        if (containsFloatingPoints) {
            ordering = "std::partial_ordering";
        } else {
            ordering = "std::strong_ordering";
        }

        for (const auto &m : data.members) {
            const auto &id   = m->identifier();
            const auto &type = m->original.type();
            if (m->getNamespace() != Namespace::VK && simpleTypes.find(type) == simpleTypes.end() && enums.find(type) == enums.end()) {
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
                    // assert( member.lenExpressions[1] == "null-terminated" );
                    // assert( ( member.type.prefix == "const" ) && ( member.type.postfix == "* const *" ) );

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
        if (nonDefaultCompare) {
            compareBody = "      return " + compareMembers + ";";

            if (!containsFuncPointer(data)) {
                constexpr auto spaceshipOperatorTemplate =
                  R"(    {2} operator<=>( {0} const & rhs ) const VULKAN_HPP_NOEXCEPT
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
#endif // VULKAN_HPP_USE_REFLECT)";
            compareBody = vkgen::format(compareBodyTemplate, compareMembers);

            if (!containsFuncPointer(data)) {
                spaceshipOperator = "    auto operator<=>( " + data.name + " const & ) const = default;";
            }
        }

        output += gen(cfg.gen.structCompare, [&](std::string &output) {
            if (!spaceshipOperator.empty()) {
                output += "#  if defined( VULKAN_HPP_HAS_SPACESHIP_OPERATOR )\n";
                output += spaceshipOperator + "\n";
                output += "#  else\n";
            }
            output += vkgen::format(R"(
    bool operator==( {0} const & rhs ) const VULKAN_HPP_NOEXCEPT
    {{
{1}
    }}
    bool operator!=( {0} const & rhs ) const VULKAN_HPP_NOEXCEPT
    {{
      return !operator==( rhs );
    }}
    )",
                                    data.name,
                                    compareBody);
            if (!spaceshipOperator.empty()) {
                output += "#  endif\n";
            }
        });
    }

    output += members;
    output += "  };\n\n";

    if (data.isStruct() && !structureType.empty()) {
        output += expIfndef("VULKAN_HPP_EXPERIMENTAL_NO_TEMPLATES");
        output += vkgen::format(R"(
  template <>
  struct CppType<StructureType, {0}> {{
    using Type = {1};
  }};
)",
                                structureType,
                                data.name);
        output += expEndif("VULKAN_HPP_EXPERIMENTAL_NO_TEMPLATES");
    }

    for (const auto &a : data.aliases) {
        output += "  using " + a + " = " + data.name + ";\n";
    }
    //});
    return output;
}

std::string vkgen::Generator::generateIncludeRAII(GenOutput &out) const {
    return vkgen::format(R"(
#include <memory>
#include <vector>
#include <utility>  // std::exchange, std::forward
#include "{0}"
#include "{1}"
)",
                         out.getFilename(""),
                         out.getFilename("_raii_forward"));
}

std::string vkgen::Generator::generateClassWithPFN(Handle &h, UnorderedFunctionOutput &funcs, UnorderedFunctionOutputX &funcs2) {
    const auto       &name     = h.name;
    const std::string dispatch = name + "Dispatcher";
    const std::string handle   = "m_" + strFirstLower(h.name);
    std::string       output;
    output += "  class " + name + " {\n";
    output += "  protected:\n";
    // output += "    VULKAN_HPP_NAMESPACE::" + name + " " + handle + " = {};\n";
    output += "    Vk" + name + " " + handle + " = {};\n";
    output += "    " + dispatch + " m_dispatcher = {};\n";
    output += "  public:\n";

    output += "    explicit " + name + "(std::nullptr_t) VULKAN_HPP_NOEXCEPT {}\n";

    UnorderedFunctionOutput  ctors{ *this };
    UnorderedFunctionOutputX members{};

    for (auto &d : h.ctorCmds) {
        MemberContext ctx{ .ns = Namespace::VK };
        ctx.insertSuperclassVar = true;
        ctx.generateInline      = true;
        ctx.disableDispatch     = true;
        ctx.exp                 = true;
        MemberResolverCtor resolver{ *this, d, ctx };

        if (!resolver.hasDependencies) {
            std::cout << "[EXP] ctor skipped: class " << h.name
                      << ", p: "
                      // parent->type() << ", s: " << superclass
                      << '\n';
            continue;
        }
        resolver.generate(ctors, funcs);
    }

    for (auto &d : h.members) {
        MemberGeneratorExperimental g{ *this, d, members, funcs2 };
        g.generate();
    }

    output += ctors.get();

    output += "    " + name + "() = delete;\n";
    output += "    " + name + "(" + name + " const&) = delete;\n";

    output += "    " + name + "(" + name + "&& rhs) VULKAN_HPP_NOEXCEPT\n";
    output += "      : " + handle + "(exchange(rhs." + handle + ", {})),\n";
    output += "        m_dispatcher(rhs.m_dispatcher)\n";
    output += "      {}\n";

    output += "    " + name + "& operator=(" + name + " const &) = delete;\n";

    output += "    " + name + "& operator=(" + name + "&& rhs) VULKAN_HPP_NOEXCEPT\n";
    output += "    {\n";
    output += "      " + handle + " = exchange(rhs." + handle + ", {});\n";
    output += "      m_dispatcher = rhs.m_dispatcher;\n";
    output += "      return *this;\n";
    output += "    }\n";

    output += "    operator Vk" + name + "() const {\n";
    output += "      return " + handle + ";\n";
    output += "    }\n";

    output += "    Vk" + name + " const &operator*() const VULKAN_HPP_NOEXCEPT {\n";
    output += "      return " + handle + ";\n";
    output += "    }\n";

    output += "    VULKAN_HPP_NODISCARD VULKAN_HPP_INLINE " + dispatch + " const * getDispatcher() const VULKAN_HPP_NOEXCEPT \n";
    output += "    {\n";
    output += "      return &m_dispatcher;\n";
    output += "    }\n";

    std::stringstream temp;
    members.write(temp);
    // std::cout << ">>> S: " << tempstr.size() << "\n";
    output += temp.str();

    output += "  };\n";
    return output;
}

void vkgen::Generator::generateContext(OutputBuffer &output, UnorderedFunctionOutput &inter) {
    output += beginNamespace();
    // output += "  namespace exp {\n";
    output += R"(
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
)";
    generateDispatchRAII(output);
    output += generateLoader(true);

    // output += "  } // exp\n";

    output += "// INTER: {\n";
    output += std::move(inter.get());
    output += "// } INTER:\n";

    output += endNamespace();
}

void vkgen::Generator::generateExperimentalRAII(OutputBuffer &output, GenOutput &out) {
    output += genNamespaceMacro(cfg.macro.mNamespaceRAII);
    output += generateIncludeRAII(out);

    output += beginNamespace();
    output += "  " + beginNamespaceRAII(true);

    output += RES_RAII;

    for (const Handle &e : handles.ordered) {
        output += generateClassRAII(e, true);
    }

    output += "  " + endNamespaceRAII();
    output += endNamespace();
}

void vkgen::Generator::generateRAII(OutputBuffer &output, OutputBuffer &output_forward, GenOutput &out) {
    output_forward += genNamespaceMacro(cfg.macro.mNamespaceRAII);

    output_forward += beginNamespace();
    output_forward += "  " + beginNamespaceRAII(true);

    // if (!cfg.gen.cppModules) {

    output += generateIncludeRAII(out);

    if (cfg.gen.expApi) {
        output += "#include \"vulkan_context.hpp\"\n";
    }
    //        output += format(R"(
    // #include <memory>
    // #include <utility>  // std::exchange, std::forward
    // #include "{0}"
    // #include "{1}"
    //)",
    //       out.getFilename(""), out.getFilename("_raii_forward"));

    output += beginNamespace();
    output += "  " + beginNamespaceRAII();
    // output += "  using namespace " + cfg.macro.mNamespace->get() + ";\n";
    // }

    output += RES_RAII;

    if (cfg.gen.internalFunctions) {
        std::string spec;
        if (!cfg.gen.cppModules) {
            spec = "static";
        }

        output += R"(
  namespace internal {

    template<typename T, typename V, typename S, typename PFN, typename... Args>
    inline std::vector<T> createArrayVoidPFN(const PFN pfn, const char *const msg, Args&&... args) {
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
    inline std::vector<T> createArray(const PFN pfn, const char *const msg, Args&&... args) {
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
  }  // namespace internal
)";
    }

    for (Handle &e : handles.ordered) {
        if (cfg.gen.expApi && !e.isSubclass) {
            continue;
        }
        output_forward += generateClassDecl(e);
    }
    if (!cfg.gen.expApi) {
        output_forward += generateClassDecl(loader);
        generateDispatchRAII(output);
        output += generateLoader();
    } else {
        output += "  using " + cfg.macro.mNamespace.get() + "::" + loader.name + ";\n";
        for (const Handle &h : topLevelHandles) {
            output += "  using " + cfg.macro.mNamespace.get() + "::" + h.name + ";\n";
            output += "  using " + cfg.macro.mNamespace.get() + "::" + h.name + "Dispatcher;\n";
        }
    }

    for (const Handle &e : handles.ordered) {
        if (cfg.gen.expApi && !e.isSubclass) {
            continue;
        } else {
            output += generateClassRAII(e);
        }
    }

    output_forward += "  " + endNamespaceRAII();
    output_forward += endNamespace();

    // if (!cfg.gen.cppModules) {

    output += "  " + endNamespaceRAII();
    output += endNamespace();

    output += "#include \"" + out.getFilename("_raii_funcs") + "\"\n";
    // }
}

void vkgen::Generator::generateFuncsRAII(OutputBuffer &output) {
    output += beginNamespace();
    output += "  " + beginNamespaceRAII();
    output += outputFuncsRAII.get();
    output += "#ifndef VULKAN_HPP_DISABLE_ENHANCED_MODE\n";
    output += outputFuncsEnhancedRAII.get();
    output += "#endif // VULKAN_HPP_DISABLE_ENHANCED_MODE\n";
    output += "  " + endNamespaceRAII();
    if (cfg.gen.raii.interop) {
        output += "#ifndef VULKAN_HPP_EXPERIMENTAL_NO_INTEROP\n";
        output += outputFuncsInterop.get();
        output += "#endif // VULKAN_HPP_EXPERIMENTAL_NO_INTEROP\n";
    }
    output += endNamespace();
}

void vkgen::Generator::generateDispatchRAII(OutputBuffer &output) {
    auto &instance = findHandle("VkInstance");
    auto &device   = findHandle("VkDevice");

    const auto gen = [&](const Handle &h) {
        GenOutputClass out{ *this };
        std::string    name   = h.name + "Dispatcher";
        std::string    handle = "NULL";

        std::string getAddr = "vkGetInstanceProcAddr";
        std::string src;
        if (h.name == loader.name) {
            // handle = "NULL";
        } else if (h.getAddrCmd.has_value()) {
            getAddr = h.getAddrCmd->name.original;
            handle  = strFirstLower(h.name);
            src     = ", " + h.name.original + " " + handle;
        }

        out.sPublic += name + "() = default;\n";

        std::string init;
        std::string init2;
        // PFN function pointers
        for (auto d : h.filteredMembers) {
            const std::string &name = d->name.original;
            out.sPublic += genOptional(*d, [&](std::string &output) { output += vkgen::format("    PFN_{0} {0} = {{}};\n", name); });
            init +=
              genOptional(*d, [&](std::string &output) { output += vkgen::format("      {0} = PFN_{0}( {1}({2}, \"{0}\") );\n", name, getAddr, handle); });
            out.sPublic += genOptional(*d, [&](std::string &output) {
                for (const auto &alias : d->src->aliases) {
                    std::string str = alias;
                    const auto &cmd = commands.find(alias);
                    if (cmd == commands.end()) {
                        std::cout << ">> error: generateDispatchRAII() cmd not found: " << alias << std::endl;
                    } else {
                        if (!cmd->canGenerate()) {
                            str = vkgen::format("PFN_{0}( {1}({2}, \"{3}\") )", name, getAddr, handle, alias);
                        }
                    }

                    init2 += vkgen::format(R"(      if ( !{0} )
        {0} = {1};
)",
                                           name,
                                           str);
                }
            });
        }
        out.sPublic += "    PFN_" + getAddr + " " + getAddr + " = {};\n";
        if (h.name == "Instance") {
            std::string name = "vkGetDeviceProcAddr";
            out.sPublic += "    PFN_" + name + " " + name + " = {};\n";
            init += vkgen::format("      {0} = PFN_{0}( {1}({2}, \"{0}\") );\n", name, getAddr, handle);
        }

        out.sPublic += vkgen::format(R"(
    {0} (PFN_{1} getProcAddr{2}) : {1}(getProcAddr) {{
{3}{4}
    }}
)",
                                     name,
                                     getAddr,
                                     src,
                                     init,
                                     init2);

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

void vkgen::Generator::evalCommand(Command &cmd) const {
    std::string const name = cmd.name;
    std::string const tag  = strWithoutTag(name);
    cmd.nameCat            = evalNameCategory(name);
}

vkgen::vkr::Command::NameCategory vkgen::Generator::evalNameCategory(const std::string &name) {
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

void vkgen::Generator::generateClassMember(
  ClassCommand &m, MemberContext &ctx, GenOutputClass &out, UnorderedFunctionOutput &funcs, UnorderedFunctionOutput &funcsEnhanced, bool inlineFuncs) {
    MemberGenerator g{ *this, m, ctx, out, funcs, funcsEnhanced };
    g.generate();
}

void vkgen::Generator::generateClassMembers(
  const Handle &data, GenOutputClass &out, UnorderedFunctionOutput &funcs, UnorderedFunctionOutput &funcsEnhanced, Namespace ns, bool inlineFuncs) {
    std::string output;
    if (ns == Namespace::RAII) {
        const auto        &className = data.name;
        const auto        &handle    = data.vkhandle.identifier();
        const std::string &ldr       = loader.name;

        const auto  &superclass = findHandle("Vk" + data.superclass);
        VariableData super(superclass.name);
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

    inline void {0}::swap(VULKAN_HPP_NAMESPACE::VULKAN_HPP_RAII_NAMESPACE::{0} &rhs) VULKAN_HPP_NOEXCEPT {{
{3}
    }}
)",
                                className,
                                call,
                                clear,
                                swap);
    }

    if (!output.empty()) {
        funcs += genOptional(data, [&](std::string &out) { out += output; });
    }

    // wrapper functions
    for (ClassCommand &m : const_cast<Handle &>(data).members) {
        if (ns == Namespace::VK && m.src->isIndirect()) {
            continue;
        }
        MemberContext ctx{ .ns = ns };
        generateClassMember(m, ctx, out, funcs, funcsEnhanced, inlineFuncs);
    }
}

void vkgen::Generator::generateClassConstructors(const Handle &data, GenOutputClass &out, UnorderedFunctionOutput &funcs) {
    const std::string &superclass = data.superclass;

    out.sPublic += vkgen::format(R"(
    VULKAN_HPP_CONSTEXPR {0}() = default;
    VULKAN_HPP_CONSTEXPR {0}(std::nullptr_t) VULKAN_HPP_NOEXCEPT {{}}

    VULKAN_HPP_TYPESAFE_EXPLICIT {0}(Vk{0} {1}) VULKAN_HPP_NOEXCEPT  : {2}({1}) {{}}
)",
                                 data.name,
                                 strFirstLower(data.name),
                                 data.vkhandle.identifier());

    // if (data.isSubclass && cfg.gen.expApi) {
    if (true) {
        const auto &superclass = data.superclass;

        for (auto &m : const_cast<Handle &>(data).ctorCmds) {
            if (cfg.dbg.methodTags) {
                out.sPublic += "    /* exp ctor: " + m.name.original + " */\n";
            }
            const auto genCtor = [&](ClassCommand &d
                                     //, auto &parent, bool insert = false
                                 ) {
                MemberContext ctx{ .ns = Namespace::VK };
                ctx.insertSuperclassVar = true;
                ctx.disableDispatch     = true;
                ctx.exp                 = true;
                MemberResolverCtor resolver{ *this, d, ctx };

                if (!resolver.hasDependencies) {
                    // std::cout << "ctor skipped: class " << data.name << ", p: " <<
                    // parent->type() << ", s: " << superclass << '\n';
                    return;
                }

                resolver.generate(out.sPublic, funcs);
            };

            //            const auto &parent = *m.src->_params.begin()->get();
            //            if (parent.original.type() != superclass.original) {
            //
            //                    genCtor(m, parent, true);
            //
            //                    if (parent.isHandle()) {
            //                        const auto &handle = findHandle(parent.original.type());
            //                        if (handle.superclass.original != superclass.original) {
            //                            std::cerr << "ctor: impossible combination" << '\n';
            //                            continue;
            //                        }
            //                    }
            //            }
            //            if (parent.isHandle()) {
            //                genCtor(m, parent);
            //            }

            genCtor(m);
        }
    }
}

void vkgen::Generator::generateClassConstructorsRAII(const Handle &data, GenOutputClass &out, UnorderedFunctionOutput &funcs) {
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

        resolver.generate(out.sPublic, funcs);
    };

    for (auto &m : const_cast<Handle &>(data).ctorCmds) {
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
            if (cfg.gen.raii.staticInstancePFN && data.name == "Instance") {
                dispatcherInit = "    dispatcher = " + data.name;
                dispatcherInit += "Dispatcher( " + parent;
                dispatcherInit += ".getDispatcher()->vkGet" + data.name + "ProcAddr, " + handle + " );\n";
            } else if (cfg.gen.raii.staticDevicePFN && data.name == "Device") {
                dispatcherInit = "    dispatcher = " + data.name;
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
            argDecl += ", VULKAN_HPP_NAMESPACE::Optional<const VULKAN_HPP_NAMESPACE::AllocationCallbacks> allocator = nullptr";
            argDef += ", VULKAN_HPP_NAMESPACE::Optional<const VULKAN_HPP_NAMESPACE::AllocationCallbacks> allocator";
            init.append(cvars.raiiAllocator.identifier(), "static_cast<const VULKAN_HPP_NAMESPACE::AllocationCallbacks *>( allocator )");
        }
        if (false) {  // TODO add class var
            init.append("m_dispacher", "& //getDispatcher()");
        }

        out.sPublic += vkgen::format(R"(
    VULKAN_HPP_INLINE {0}( VULKAN_HPP_NAMESPACE::VULKAN_HPP_RAII_NAMESPACE::{1} const & {2},
      Vk{0} {3}{4} );
)",
                                     data.name,
                                     superclass,
                                     parent,
                                     handle,
                                     argDecl);

        funcs.add(
          data,
          [&](std::string &output) {
              output += vkgen::format("  {0}::{0}( VULKAN_HPP_NAMESPACE::VULKAN_HPP_RAII_NAMESPACE::{1} const & {2}, Vk{0} {3}{4} ){5}\n",
                                      data.name,
                                      superclass,
                                      parent,
                                      handle,
                                      argDef,
                                      init.string());
              output += "  {\n";
              output += dispatcherInit;
              output += "  }\n";
          },
          "");
    }
}

std::string vkgen::Generator::generateUniqueClassStr(const Handle &data, UnorderedFunctionOutput &funcs, bool inlineFuncs) {
    std::string output;
    if (!data.dtorCmd) {
        std::cerr << "class has no destructor info!" << '\n';
        return "";
    }

    GenOutputClass     out{ *this };
    const std::string &base       = data.name;
    const std::string &className  = "Unique" + base;
    const std::string &handle     = data.vkhandle.identifier();
    const std::string &superclass = data.superclass;
    bool const         isSubclass = data.isSubclass;

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
        out.sPrivate += "    " + v.toClassVar();
        assignemntOp += "\n      " + v.identifier() + " = std::move(other." + v.identifier() + ");";
        copyCtor.append(v.identifier(), "std::move(other." + v.identifier() + ")");
    });

    out.sPublic += "    " + className + "() = default;\n";

    // out.sPublic += "/*\n";
    r.generate(out.sPublic, funcs);
    // out.sPublic += "*/\n";

    out.sPublic += vkgen::format(R"(
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

    out.sPublic += vkgen::format(R"(
    {0}& operator=({0} && other) VULKAN_HPP_NOEXCEPT {{
      reset(other.release());{1}
      return *this;
    }}
)",
                                 className,
                                 assignemntOp);

    out.sPublic += vkgen::format(R"(

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

    output += generateClassString(className, out, Namespace::VK);

    output += vkgen::format(R"(
  VULKAN_HPP_INLINE void swap({0} &lhs, {0} &rhs) VULKAN_HPP_NOEXCEPT {{
    lhs.swap(rhs);
  }}

)",
                            className);
    return output;
}

std::string vkgen::Generator::generateUniqueClass(const Handle &data, UnorderedFunctionOutput &funcs) {
    std::string output;
    if (!data.getProtect().empty()) {
        // std::cout << "class " << data.name << " to " << data.getProtect() << '\n';
        platformOutput.add(data, [&](std::string &output) { output += generateUniqueClassStr(data, funcs, true); });
    } else {
        output += genOptional(data, [&](std::string &output) { output += generateUniqueClassStr(data, funcs, false); });
    }
    return output;
}

std::string vkgen::Generator::generateClassStr(const Handle            &data,
                                               UnorderedFunctionOutput &funcs,
                                               UnorderedFunctionOutput &funcsEnhanced,
                                               UnorderedFunctionOutput &expfuncs,
                                               bool                     inlineFuncs,
                                               bool                     noFuncs) {
    std::string    output;
    GenOutputClass out{ *this };

    const std::string &className      = data.name;
    const std::string &classNameLower = strFirstLower(className);
    const std::string &handle         = data.vkhandle.identifier();
    const std::string &superclass     = data.superclass;

    // std::cout << "Gen class: " << className << '\n';
    // std::cout << "  ->superclass: " << superclass << '\n';

    std::string debugReportValue = "Unknown";
    auto        en               = enums.find("VkDebugReportObjectTypeEXT");
    if (en != enums.end() && en->containsValue("e" + className)) {
        debugReportValue = className;
    }

    out.sPublic += vkgen::format(R"(
using CType      = Vk{0};
using NativeType = Vk{0};

static VULKAN_HPP_CONST_OR_CONSTEXPR VULKAN_HPP_NAMESPACE::ObjectType objectType =
  VULKAN_HPP_NAMESPACE::ObjectType::e{0};
static VULKAN_HPP_CONST_OR_CONSTEXPR VULKAN_HPP_NAMESPACE::DebugReportObjectTypeEXT debugReportObjectType =
  VULKAN_HPP_NAMESPACE::DebugReportObjectTypeEXT::e{1};

)",
                                 className,
                                 debugReportValue);

    generateClassConstructors(data, out, funcs);

    if (cfg.gen.raii.interop && false) {
        const auto &superclass = data.superclass;
        //        const std::string &owner = data.ownerhandle;

        const auto genCtor = [&](ClassCommand &d, auto &parent, bool insert = false) {
            MemberContext ctx{ .ns = Namespace::RAII };
            ctx.insertSuperclassVar = insert;

            MemberResolverCtor resolver{ *this, d, ctx };

            if (!resolver.hasDependencies) {
                // std::cout << "ctor skipped: class " << data.name << ", s: " << superclass << '\n';
                return;
            }

            out.sPublic += "// INTER:\n";
            resolver.guard              = "VULKAN_HPP_EXPERIMENTAL_NO_INTEROP";
            resolver.constructorInterop = true;
            resolver.generate(out.sPublic, outputFuncsInterop);
        };

        for (auto &m : const_cast<Handle &>(data).ctorCmds) {
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

    out.sPublic += vkgen::format(R"(
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
    if (!noFuncs) {
        generateClassMembers(data, out, funcs, funcsEnhanced, Namespace::VK, inlineFuncs);
    }

    output += generateClassString(className, out, Namespace::VK);

    output += expIfndef("VULKAN_HPP_EXPERIMENTAL_NO_TEMPLATES");
    output += vkgen::format(R"(
template <>
struct CppType<VULKAN_HPP_NAMESPACE::ObjectType, VULKAN_HPP_NAMESPACE::ObjectType::e{0}>
{{
using Type = VULKAN_HPP_NAMESPACE::{0};
}};

)",
                            className,
                            debugReportValue);

    if (debugReportValue != "Unknown") {
        output += vkgen::format(R"(
template <>
struct CppType<VULKAN_HPP_NAMESPACE::DebugReportObjectTypeEXT,
             VULKAN_HPP_NAMESPACE::DebugReportObjectTypeEXT::e{1}>
{{
using Type = VULKAN_HPP_NAMESPACE::{0};
}};

)",
                                className,
                                debugReportValue);
    }

    output += vkgen::format(R"(
template <>
struct isVulkanHandleType<VULKAN_HPP_NAMESPACE::{0}>
{{
  static VULKAN_HPP_CONST_OR_CONSTEXPR bool value = true;
}};
)",
                            className);
    output += expEndif("VULKAN_HPP_EXPERIMENTAL_NO_TEMPLATES");

    return output;
}

std::string vkgen::Generator::generateClass(const Handle            &data,
                                            UnorderedFunctionOutput &funcs,
                                            UnorderedFunctionOutput &funcsEnhanced,
                                            UnorderedFunctionOutput &expfuncs) {
    std::string output;

    if (!data.getProtect().empty()) {
        platformOutput.add(data, [&](std::string &output) { output += generateClassStr(data, funcs, funcsEnhanced, expfuncs, true); });
    } else {
        output += genOptional(data, [&](std::string &output) { output += generateClassStr(data, funcs, funcsEnhanced, expfuncs, false); });
    }
    return output;
}

std::string vkgen::Generator::generateClassStrRAII(const Handle &data, bool exp) {
    GenOutputClass out{ *this };
    std::string    output;

    const std::string &className      = data.name;
    const std::string &classNameLower = strFirstLower(className);
    const std::string &handle         = data.vkhandle.identifier();
    const auto        &superclass     = data.superclass;
    const std::string &owner          = data.ownerhandle;
    bool               inherit        = exp && owner.empty();

    if (inherit) {
        out.inherits += "public " + getNamespace(Namespace::VK) + className;
    } else {
        std::string debugReportValue = "Unknown";
        auto        en               = enums.find("VkDebugReportObjectTypeEXT");
        if (en != enums.end() && en->containsValue("e" + className)) {
            debugReportValue = className;
        }

        out.sPublic += " // " + owner + "\n";
        out.sPublic += vkgen::format(R"(
    using CType      = Vk{0};

    static VULKAN_HPP_CONST_OR_CONSTEXPR VULKAN_HPP_NAMESPACE::ObjectType objectType =
      VULKAN_HPP_NAMESPACE::ObjectType::e{0};
    static VULKAN_HPP_CONST_OR_CONSTEXPR VULKAN_HPP_NAMESPACE::DebugReportObjectTypeEXT debugReportObjectType =
      VULKAN_HPP_NAMESPACE::DebugReportObjectTypeEXT::e{1};

)",
                                     className,
                                     debugReportValue);
    }
    // if (!exp) {
    generateClassConstructorsRAII(data, out, outputFuncsRAII);

    InitializerBuilder init("        ");
    std::string        assign = "\n";

    if (inherit) {
        init.append(getNamespace(Namespace::VK) + className + "::" + className, "std::forward<" + className + ">(rhs)");
        assign += "      " + getNamespace(Namespace::VK) + className + "::operator=(std::forward<" + className + ">(rhs));\n";
    }
    data.foreachVars(VariableData::Flags::CLASS_VAR_RAII, [&](const VariableData &v) {
        if (inherit && v.type() == className) {
            return;
        }
        if (v.identifier() == "m_dispatcher") {
            init.append(v.identifier(), vkgen::format("rhs.{0}.release()", v.identifier()));
            assign += vkgen::format("        {0}.reset( rhs.{0}.release() );\n", v.identifier());
        } else {
            init.append(v.identifier(), vkgen::format("VULKAN_HPP_NAMESPACE::VULKAN_HPP_RAII_NAMESPACE::exchange(rhs.{0}, {{}})", v.identifier()));
            assign += vkgen::format("        {0} = VULKAN_HPP_NAMESPACE::VULKAN_HPP_RAII_NAMESPACE::exchange(rhs.{0}, {{}});\n", v.identifier());
        }
    });

    if (inherit) {
        out.sPublic += "    explicit " + data.name + "    (std::nullptr_t) VULKAN_HPP_NOEXCEPT\n";
        out.sPublic += "      : VULKAN_HPP_NAMESPACE::" + data.name + "(nullptr)\n";
        out.sPublic += "    {}\n";
    } else {
        out.sPublic += "    explicit " + data.name + "(std::nullptr_t) VULKAN_HPP_NOEXCEPT {}\n";
    }

    out.sPublic += vkgen::format(R"(
    ~{0}() {{
        clear();
    }}

    {0}() = delete;
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

    //    }

    std::string release;
    //        if (data.vkhandle.getAssignment() != " = {}") {
    //            std::cerr << &data << " vkhandle: " << data.vkhandle.identifier() << " invalid assignment: " << data.vkhandle.getAssignment() << std::endl;
    //        }

    data.foreachVars(VariableData::Flags::CLASS_VAR_RAII, [&](const VariableData &v) {
        if (inherit && v.type() == className) {
            return;
        }
        out.sPrivate += "    " + v.toClassVar();
        if (v.identifier() != handle) {
            release += "      " + v.identifier() + " = nullptr;\n";
        }
    });

    out.sPublic += vkgen::format(R"(
    VULKAN_HPP_NAMESPACE::{0} const &operator*() const VULKAN_HPP_NOEXCEPT {{
        return {1};
    }}
    void clear() VULKAN_HPP_NOEXCEPT;
    void swap(VULKAN_HPP_NAMESPACE::VULKAN_HPP_RAII_NAMESPACE::{0} &) VULKAN_HPP_NOEXCEPT;
)",
                                 className,
                                 handle);

    out.sPublic += vkgen::format(R"(

    VULKAN_HPP_NAMESPACE::{0} release()
    {{
{1}      return VULKAN_HPP_NAMESPACE::VULKAN_HPP_RAII_NAMESPACE::exchange( {2}, nullptr );
    }}
)",
                                 className,
                                 release,
                                 handle);

    if (!exp) {
        outputFuncsRAII.add(
          data,
          [&](std::string &output) {
              std::string dispatchSrc;
              std::string type = data.name;
              if (cfg.gen.raii.staticInstancePFN && data.name.original == "VkInstance") {
                  dispatchSrc = "      return &VULKAN_HPP_NAMESPACE::VULKAN_HPP_RAII_NAMESPACE::Instance::dispatcher;\n";
                  out.sPublic += "    inline static " + type + "Dispatcher dispatcher;\n";
                  //                funcs += "  " + format("VULKAN_HPP_NAMESPACE::VULKAN_HPP_RAII_NAMESPACE::Instance ") + type + "Dispatcher dispatcher;\n";
              } else if (cfg.gen.raii.staticDevicePFN && data.name.original == "VkDevice") {
                  dispatchSrc = "      return &VULKAN_HPP_NAMESPACE::VULKAN_HPP_RAII_NAMESPACE::Device::dispatcher;\n";
                  out.sPublic += "    inline static " + type + "Dispatcher dispatcher;\n";
                  //                funcs += "  " + format("VULKAN_HPP_NAMESPACE::VULKAN_HPP_RAII_NAMESPACE::Device ") + type + "Dispatcher dispatcher;\n";
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
              out.sPublic += vkgen::format(R"(
    {2}VULKAN_HPP_NAMESPACE::VULKAN_HPP_RAII_NAMESPACE::{0}Dispatcher const * getDispatcher() const;
    )",
                                           type,
                                           dispatchSrc,
                                           spec);

              output += vkgen::format(R"(
    {3}VULKAN_HPP_NAMESPACE::VULKAN_HPP_RAII_NAMESPACE::{0}Dispatcher const * {1}::getDispatcher() const
    {{
      //VULKAN_HPP_ASSERT( m_dispatcher->getVkHeaderVersion() == VK_HEADER_VERSION );
{2}
    }}
    )",
                                      type,
                                      data.name,
                                      dispatchSrc,
                                      spec);
          },
          "");  // TODO change to +=
    }

    if (data.ownerRaii) {
        out.sPublic += vkgen::format(R"(
    VULKAN_HPP_INLINE VULKAN_HPP_NAMESPACE::VULKAN_HPP_RAII_NAMESPACE::{0} const & get{0}() const
    {{
      return *{1};
    }}
)",
                                     data.ownerRaii->type(),
                                     data.ownerRaii->identifier());
    }

    if (!exp) {
        generateClassMembers(data, out, outputFuncsRAII, outputFuncsEnhancedRAII, Namespace::RAII);
    }

    output += generateClassString(className, out, Namespace::RAII);

    if (!exp && !data.vectorCmds.empty()) {
        GenOutputClass out{ *this };
        std::string    name = className + "s";

        out.inherits += vkgen::format("public std::vector<VULKAN_HPP_NAMESPACE::VULKAN_HPP_RAII_NAMESPACE::{0}>", className);

        // HandleData cls = data;
        // cls.name = name;

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
            out.sPublic += vkgen::format(R"(
    {0}( std::nullptr_t ) {{}}

    {0}()                          = delete;
    {0}( {0} const & )             = delete;
    {0}( {0} && rhs )              = default;
    {0} & operator=( {0} const & ) = delete;
    {0} & operator=( {0} && rhs )  = default;
)",
                                         name);

            output += generateClassString(name, out, Namespace::RAII);
        } else {
            std::cout << "no suitable constructors for class: " << data.name << '\n';
        }
    }

    return output;
}

std::string vkgen::Generator::generateClassRAII(const Handle &data, bool exp) {
    return genOptional(data, [&](std::string &output) { output += generateClassStrRAII(data, exp); });
}

std::string vkgen::Generator::generatePFNs(const Handle &data, GenOutputClass &out) const {
    std::string load;
    std::string loadSrc;
    if (data.getAddrCmd.has_value() && !data.getAddrCmd->name.empty()) {
        loadSrc = strFirstLower(data.superclass) + ".";
    }

    for (const ClassCommand &m : data.members) {
        const std::string &name = m.name.original;

        // PFN pointers declaration
        // TODO check order
        out.sProtected += genOptional(m, [&](std::string &output) { output += vkgen::format("    PFN_{0} m_{0} = {{}};\n", name); });

        // PFN pointers initialization
        load += genOptional(m, [&](std::string &output) { output += vkgen::format("      m_{0} = {1}getProcAddr<PFN_{0}>(\"{0}\");\n", name, loadSrc); });
    }

    return load;
}

std::string vkgen::Generator::generateLoader(bool exp) {
    GenOutputClass out{ *this };

    UnorderedFunctionOutput funcs{ *this };
    UnorderedFunctionOutput funcsEnhanced{ *this };

    const std::string dispatcher = loader.name + "Dispatcher";

    std::string output;

    out.sProtected += "    LIBHANDLE lib = {};\n";
    out.sProtected += "    std::unique_ptr<" + dispatcher + "> m_dispatcher;\n";

    out.sPublic += R"(
#ifdef _WIN32
    static constexpr char const* defaultLibpath = "vulkan-1.dll";
#else
    static constexpr char const* defaultLibpath = "libvulkan.so.1";
#endif
)";
    out.sPublic += format(R"(
    {0}() = default;

    ~{0}() {{
      unload();
    }}

    {0}(const std::string &libpath) {{
      load(libpath);
    }}

    {1} const * getDispatcher() const
    {{
      // VULKAN_HPP_ASSERT( m_dispatcher->getVkHeaderVersion() == VK_HEADER_VERSION );
      return &*m_dispatcher;
    }}
)",
                          loader.name,
                          dispatcher);

    out.sPublic += R"(
    template<typename T>
    VULKAN_HPP_INLINE T getProcAddr(const char *name) const {
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
)";
    out.sPublic += "      m_dispatcher.reset( new " + dispatcher + "( getInstanceProcAddr ) );\n";
    out.sPublic += R"(
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
)";

    if (!exp) {
        for (auto &m : loader.members) {
            MemberContext ctx{ .ns = Namespace::RAII };
            if (m.src->nameCat == Command::NameCategory::CREATE) {
                ctx.insertClassVar = true;
            }
            generateClassMember(m, ctx, out, funcs, funcsEnhanced);
        }
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
    //    if (!cfg.gen.cppModules) {
    //        output += "static ";
    //    }
    //    output += loader.name + " defaultContext;\n\n";

    if (!exp) {
        outputFuncsRAII += funcs.get();
        outputFuncsEnhancedRAII += funcsEnhanced.get();
    }

    return output;
}

std::string vkgen::Generator::genMacro(const Macro &m) {
    std::string out;
    if (m.usesDefine) {
        out += vkgen::format(R"(
#if !defined( {0} )
#  define {0} {1}
#endif
)",
                             m.define,
                             m.value);
    }
    return out;
}

std::string vkgen::Generator::beginNamespace(bool noExport) const {
    return beginNamespace(cfg.macro.mNamespace, noExport);
}

std::string vkgen::Generator::beginNamespaceRAII(bool noExport) const {
    return beginNamespace(cfg.macro.mNamespaceRAII, noExport);
}

std::string vkgen::Generator::beginNamespace(const Macro &ns, bool noExport) const {
    std::string output;
    //    if (cfg.gen.cppModules && !noExport) {
    //        output += "export ";
    //    }
    return output + "namespace " + ns.getDefine() + " {\n";
}

std::string vkgen::Generator::endNamespace() const {
    return endNamespace(cfg.macro.mNamespace);
}

std::string vkgen::Generator::endNamespaceRAII() const {
    return endNamespace(cfg.macro.mNamespaceRAII);
}

std::string vkgen::Generator::endNamespace(const Macro &ns) const {
    return "}  // namespace " + ns.getDefine() + "\n";
}

vkgen::Generator::Generator()
  : Registry(*this)
  , cvars{ ClassVariables{
      .raiiAllocator        = VariableData{ VariableDataInfo{ .prefix     = "const ",
                                                              .vktype     = "VkAllocationCallbacks",
                                                              .suffix     = " *",
                                                              .identifier = "m_allocator",
                                                              .assigment  = " = {}",
                                                              .ns         = Namespace::VK,
                                                              .flag       = VariableData::Flags::CLASS_VAR_RAII } },
      .raiiInstanceDispatch = VariableData{ VariableDataInfo{
        .stdtype = "std::unique_ptr<InstanceDispatcher>", .identifier = "m_dispatcher", .ns = Namespace::NONE, .flag = VariableData::Flags::CLASS_VAR_RAII } },
      .raiiDeviceDispatch   = VariableData{ VariableDataInfo{
          .stdtype = "std::unique_ptr<DeviceDispatcher>", .identifier = "m_dispatcher", .ns = Namespace::NONE, .flag = VariableData::Flags::CLASS_VAR_RAII } },
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
                                                              .specialType = VariableData::TYPE_DISPATCH } } } }
  , outputFuncs{ *this }
  , outputFuncsEnhanced{ *this }
  , outputFuncsRAII{ *this }
  , outputFuncsEnhancedRAII{ *this }
  , outputFuncsInterop{ *this }
  , platformOutput{ *this } {
    unload();
    // resetConfig();

    namespaces.insert_or_assign(Namespace::VK, &cfg.macro.mNamespace);
    namespaces.insert_or_assign(Namespace::RAII, &cfg.macro.mNamespaceRAII);
    namespaces.insert_or_assign(Namespace::STD, &cfg.macro.mNamespaceSTD);
}

void vkgen::Generator::resetConfig() {
    cfg.reset();
}

void vkgen::Generator::loadConfigPreset() {
    resetConfig();
}

void vkgen::Generator::setOutputFilePath(const std::string &path) {
    outputFilePath = path;
    if (isOuputFilepathValid()) {
        std::string filename = std::filesystem::path(path).filename().string();
        filename             = camelToSnake(filename);
    }
}

void vkgen::Generator::load(const std::string &xmlPath) {
    auto start = std::chrono::system_clock::now();
    Registry::load(*this, xmlPath);
    auto end     = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration<double>(end - start);
    std::cout << "loaded in " << elapsed.count() << "s  (" << xmlPath << ")" << std::endl;

    // mockTestConfigs();
}

void vkgen::Generator::generate() {
    auto start = std::chrono::system_clock::now();

    std::string p = outputFilePath;
    if (outputFilePath.empty()) {
        p = ".";
    }
    std::replace(p.begin(), p.end(), '\\', '/');
    if (!p.ends_with('/')) {
        p += '/';
    }
    std::filesystem::path const path = std::filesystem::absolute(p);

    std::cout << "generating to: " << path << '\n';
    if (!std::filesystem::is_directory(path)) {
        throw std::runtime_error{ "Output path is not a directory" };
    }
    createPath(path);

    // TODO check existing files?

    outputFuncs.clear();
    outputFuncsRAII.clear();
    outputFuncsInterop.clear();
    platformOutput.clear();
    dispatchLoaderBaseGenerated = false;

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

    generateFiles(path);

    auto end     = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration<double>(end - start);
    std::cout << "generated in " << elapsed.count() << "s" << std::endl;
}

void vkgen::Generator::saveConfigFile(const std::string &filename) {
    if (!isLoaded()) {
        return;
    }
    cfg.save(*this, filename);
}

void vkgen::Generator::loadConfigFile(const std::string &filename) {
    if (!isLoaded()) {
        throw std::runtime_error("Can't load config: registry is not loaded");
    }
    cfg.load(*this, filename);
}