//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Alejandro Cabrera 2011.
// Distributed under the Boost
// Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or
// copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/bloom_filter for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#ifndef BOOST_BLOOM_FILTER_COUNTING_BLOOM_FILTER_HPP
#define BOOST_BLOOM_FILTER_COUNTING_BLOOM_FILTER_HPP 1
/**
 * \author Alejandro Cabrera
 * \brief A generic counting Bloom filter providing compile-time unrolling
 *        of hash function application.
 */
#include <cmath>
#include <iostream>

#include <boost/array.hpp>
#include <boost/config.hpp>
#include <boost/mpl/vector.hpp>
#include <boost/mpl/size.hpp>
#include <boost/static_assert.hpp>
#include <boost/type_traits/is_integral.hpp>
#include <boost/type_traits/is_unsigned.hpp>

#include <boost/bloom_filter/detail/counting_apply_hash.hpp>
#include <boost/bloom_filter/hash/default.hpp>

#ifndef BOOST_NO_0X_HDR_INITIALIZER_LIST
#include <initializer_list>
#endif 

namespace boost {
  namespace bloom_filters {
    template <typename T,
	      size_t NumBins,
	      size_t BitsPerBin = 4,
	      class HashFunctions = mpl::vector<boost_hash<T, 3> >,
	      typename Block = size_t>
    class counting_bloom_filter {

      // Block needs to be an integral type
      BOOST_STATIC_ASSERT( boost::is_integral<Block>::value == true);

      // Block needs to be an unsigned type
      BOOST_STATIC_ASSERT( boost::is_unsigned<Block>::value == true);

      // it doesn't make sense to ever support using a BitsPerBin value larger
      // than the number of bits per Block. In that case, the user shouldn't
      // be using a Bloom filter to represent their data.
      BOOST_STATIC_ASSERT( (BitsPerBin <= (sizeof(Block) * 8) ) );

      // because of the nature of this implementation, the Bloom filter
      // can have internal fragmentation if the calculation for 
      // bins_per_slot has a remainder. The severity of the  internal
      // fragmentation is equal to the remainder * the number of slots.
      // This check prevents internal fragmentation.
      // This also necessarily limits to bin sizes to one of:
      // [1,2,4,8,16,32,64] bits
      BOOST_STATIC_ASSERT( ((sizeof(Block) * 8) % BitsPerBin) == 0);

      // a slot is one element position in the array
      // a bin is a segment of a slot
      static const size_t slot_bits = sizeof(Block) * 8;
      static const size_t bins_per_slot = slot_bits / BitsPerBin;
      static const size_t bin_bits = NumBins * BitsPerBin;
      static const size_t array_size = bin_bits / slot_bits + 1;
      static const size_t effective_num_bins = array_size * bins_per_slot;
      static const size_t mask = 
	static_cast<Block>(0 - 1) >> (slot_bits - BitsPerBin);

    public:
      typedef T value_type;
      typedef T key_type;
      typedef HashFunctions hash_function_type;
      typedef Block block_type;

      typedef boost::array<Block, array_size> bucket_type;
      typedef typename bucket_type::iterator bucket_iterator;
      typedef typename bucket_type::const_iterator bucket_const_iterator;      

    public:
      counting_bloom_filter() 
      {
	this->clear();
      }

      template <typename InputIterator>
      counting_bloom_filter(const InputIterator start, 
			    const InputIterator end) 
      {
	this->clear();

	for (InputIterator i = start; i != end; ++i)
	  this->insert(*i);
      }

#ifndef BOOST_NO_0X_HDR_INITIALIZER_LIST
      counting_bloom_filter(const std::initializer_list<T>& ilist) 
      {
	this->clear();

	typedef typename std::initializer_list<T>::const_iterator citer;
	for (citer i = ilist.begin(), end = ilist.end(); i != end; ++i) {
	  this->insert(*i);
	}
      }
#endif

      static BOOST_CONSTEXPR size_t bit_capacity() 
      {
        return NumBins * BitsPerBin;
      }

      static BOOST_CONSTEXPR size_t num_hash_functions() 
      {
        return mpl::size<HashFunctions>::value;
      };

      double false_positive_rate() const 
      {
        const double n = static_cast<double>(this->count());
        static const double k = static_cast<double>(num_hash_functions());
        static const double m = static_cast<double>(NumBins);
        static const double e =
	  2.718281828459045235360287471352662497757247093699959574966;
        return std::pow(1 - std::pow(e, -k * n / m), k);
      };

      // returns the number of bins that have at least 1 bit set
      size_t count() const 
      {
	size_t ret = 0;

	for (bucket_const_iterator i = this->bits.begin(), 
	       end = this->bits.end(); 
	     i != end; ++i) {
	  for (size_t bin = 0; bin < bins_per_slot; ++bin) {
	    size_t offset_bits = bin * BitsPerBin;
	    size_t target_bits = (*i >> offset_bits) & mask; 

	    if (target_bits > 0)
	      ++ret;
	  }
	}

        return ret;
      };

      bool empty() const 
      {
	return this->count() == 0;
      }

      void insert(const T& t) 
      {
	static const unsigned N = mpl::size<HashFunctions>::value - 1;
	detail::counting_apply_hash<N, T, NumBins, 
				    BitsPerBin, HashFunctions,
				    Block, array_size, 
				    bins_per_slot>::insert(t, this->bits);
      }

      template <typename InputIterator>
      void insert(const InputIterator start, const InputIterator end) 
      {
	for (InputIterator i = start; i != end; ++i) {
	  this->insert(*i);
	}
      }
			   
      void remove(const T& t) 
      {
	static const unsigned N = mpl::size<HashFunctions>::value - 1;
	detail::counting_apply_hash<N, T, NumBins, 
				    BitsPerBin, HashFunctions,
				    Block, array_size, 
				    bins_per_slot>::remove(t, this->bits);
      }
      
      template <typename InputIterator>
      void remove(const InputIterator start, const InputIterator end) 
      {
	for (InputIterator i = start; i != end; ++i) {
	  this->remove(*i);
	}
      }

      bool probably_contains(const T& t) const 
      {
	static const unsigned N = mpl::size<HashFunctions>::value - 1;
	return detail::counting_apply_hash<N, T, NumBins, 
					   BitsPerBin, HashFunctions,
					   Block, array_size, 
					   bins_per_slot>::contains(t, this->bits);
      }

      void clear() 
      {
	for (bucket_iterator i = bits.begin(), end = bits.end();
	     i != end; ++i) {
	  *i = 0;
	}
      }

      void swap(counting_bloom_filter& other) 
      {
	counting_bloom_filter tmp = other;
	other = *this;
	*this = tmp;
      }

      counting_bloom_filter& operator|=(const counting_bloom_filter& rhs)
      {
	const bucket_iterator this_end = this->bits.end();
	bucket_iterator this_start = this->bits.begin();
	bucket_iterator rhs_start = rhs.bits.begin();

	for (; this_start != this_end; ++this_start, ++rhs_start) {
	  *this_start |= *rhs_start;
	}

	return *this;
      }

      counting_bloom_filter& operator&=(const counting_bloom_filter& rhs)
      {
	const bucket_iterator this_end = this->bits.end();
	bucket_iterator this_start = this->bits.begin();
	bucket_iterator rhs_start = rhs.bits.begin();

	for (; this_start != this_end; ++this_start, ++rhs_start) {
	  *this_start &= *rhs_start;
	}

	return *this;
      }

      // equality comparison operators
      template <typename _T, size_t _Bins, size_t _BitsPerBin, 
		typename _HashFns, typename _Block>
      friend bool 
      operator==(const counting_bloom_filter<_T, _Bins, _BitsPerBin,
					     _HashFns, _Block>& lhs,
		 const counting_bloom_filter<_T, _Bins, _BitsPerBin,
					     _HashFns, _Block>& rhs);
			     
      template <typename _T, size_t _Bins, size_t _BitsPerBin, 
		typename _HashFns, typename _Block>
      friend bool 
      operator!=(const counting_bloom_filter<_T, _Bins, _BitsPerBin,
					     _HashFns, _Block>& lhs,
		 const counting_bloom_filter<_T, _Bins, _BitsPerBin,
					     _HashFns, _Block>& rhs);
			     

    private:
      bucket_type bits;
    };

    // union
    template<class T, size_t NumBins, size_t BitsPerBin, class HashFunctions,
	     typename Block>
    counting_bloom_filter<T, NumBins, BitsPerBin, HashFunctions, Block>
    operator|(const counting_bloom_filter<T, NumBins, BitsPerBin, 
					  HashFunctions, Block>& lhs,
	      const counting_bloom_filter<T, NumBins, BitsPerBin,
					  HashFunctions, Block>& rhs)
    {
      counting_bloom_filter<T, NumBins, BitsPerBin, 
			    HashFunctions, Block> ret(lhs);
      ret |= rhs;
      return ret;
    }

    // intersection
    template<class T, size_t NumBins, size_t BitsPerBin, class HashFunctions,
	     typename Block>
    counting_bloom_filter<T, NumBins, BitsPerBin, HashFunctions, Block>
    operator&(const counting_bloom_filter<T, NumBins, BitsPerBin, 
					  HashFunctions, Block>& lhs,
	      const counting_bloom_filter<T, NumBins, BitsPerBin,
					  HashFunctions, Block>& rhs)
    {
      counting_bloom_filter<T, NumBins, BitsPerBin, 
			    HashFunctions, Block> ret(lhs);
      ret &= rhs;
      return ret;
    }

    template<class T, size_t NumBins, size_t BitsPerBin, class HashFunctions,
	     typename Block>
    void
    swap(counting_bloom_filter<T, NumBins, BitsPerBin, 
			       HashFunctions, Block>& lhs,
	 counting_bloom_filter<T, NumBins, BitsPerBin,
			       HashFunctions, Block>& rhs)
	 
    {
      lhs.swap(rhs);
    }

    template<class T, size_t NumBins, size_t BitsPerBin, class HashFunctions,
	     typename Block>
    bool
    operator==(const counting_bloom_filter<T, NumBins, BitsPerBin, 
					   HashFunctions, Block>& lhs,
	       const counting_bloom_filter<T, NumBins, BitsPerBin,
					   HashFunctions, Block>& rhs)
    {
      return (lhs.bits == rhs.bits);
    }

    template<class T, size_t NumBins, size_t BitsPerBin, class HashFunctions,
	     typename Block>
    bool
    operator!=(const counting_bloom_filter<T, NumBins, BitsPerBin, 
					   HashFunctions, Block>& lhs,
	       const counting_bloom_filter<T, NumBins, BitsPerBin,
					   HashFunctions, Block>& rhs)
    {
      return !(lhs == rhs);
    }

  } // namespace bloom_filter
} // namespace boost
#endif
