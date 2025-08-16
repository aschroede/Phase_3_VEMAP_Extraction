/*  This file is part of libDAI - http://www.libdai.org/
 *
 *  Copyright (c) 2006-2011, The libDAI authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license that can be found in the LICENSE file.
 */


/// \file
/// \brief Defines TProb<> and Prob classes which represent (probability) vectors (e.g., probability distributions of discrete random variables)


#ifndef __defined_libdai_instantiation_h
#define __defined_libdai_instantiation_h


#include <cmath>
#include <vector>
#include <ostream>
#include <algorithm>
#include <numeric>
#include <functional>
#include <dai/util.h>
#include <dai/exceptions.h>
#include <dai/var.h>

namespace dai {



/// Represents a vector with entries of type \a T.
/** It is simply a <tt>std::vector</tt><<em>T</em>> with an interface designed for dealing with probability mass functions.
 *
 *  It is mainly used for representing measures on a finite outcome space, for example, the probability
 *  distribution of a discrete random variable. However, entries are not necessarily non-negative; it is also used to
 *  represent logarithms of probability mass functions.
 *
 *  \tparam T Should be a scalar that is castable from and to dai::Real and should support elementary arithmetic operations.
 */
class Instantiation {
    public:
        /// Type of data structure used for storing the values
        typedef std::vector<std::map<Var, size_t>> container_type;

        /// Shorthand
        typedef Instantiation this_type;

    private:
        /// The data structure that stores the values
        container_type _i;

    public:
    /// \name Constructors and destructors
    //@{
        /// Default constructor (constructs empty vector)
        Instantiation() : _i() {}

        explicit Instantiation(size_t n) : _i(n, std::map<Var, size_t>()) {}
    //@}

        /// Constant iterator over the elements
        typedef typename container_type::const_iterator const_iterator;
        /// Iterator over the elements
        typedef typename container_type::iterator iterator;
        /// Constant reverse iterator over the elements
        typedef typename container_type::const_reverse_iterator const_reverse_iterator;
        /// Reverse iterator over the elements
        typedef typename container_type::reverse_iterator reverse_iterator;

    /// \name Iterator interface
    //@{
        /// Returns iterator that points to the first element
        iterator begin() { return _i.begin(); }
        /// Returns constant iterator that points to the first element
        const_iterator begin() const { return _i.begin(); }

        /// Returns iterator that points beyond the last element
        iterator end() { return _i.end(); }
        /// Returns constant iterator that points beyond the last element
        const_iterator end() const { return _i.end(); }

        /// Returns reverse iterator that points to the last element
        reverse_iterator rbegin() { return _i.rbegin(); }
        /// Returns constant reverse iterator that points to the last element
        const_reverse_iterator rbegin() const { return _i.rbegin(); }

        /// Returns reverse iterator that points beyond the first element
        reverse_iterator rend() { return _i.rend(); }
        /// Returns constant reverse iterator that points beyond the first element
        const_reverse_iterator rend() const { return _i.rend(); }
    //@}

    /// \name Miscellaneous operations
    //@{
        void resize( size_t sz ) {
            _i.resize( sz );
        }
    //@}

    /// \name Get/set individual entries
    //@{
        /// Gets \a i 'th entry
        std::map<Var, size_t> get( size_t i ) const { 
#ifdef DAI_DEBUG
            return _i.at(i);
#else
            return _i[i];
#endif
        }

        /// Sets \a i 'th entry to \a val
        void set( size_t i, std::map<Var, size_t> val) {
            DAI_DEBASSERT( i < _i.size() );
            _i[i] = val;
        }
    //@}

    /// \name Queries
    //@{
        /// Returns a const reference to the wrapped container
        const container_type& i() const { return _i; }

        /// Returns a reference to the wrapped container
        container_type& i() { return _i; }

        /// Returns a copy of the \a i 'th entry
        std::map<Var, size_t> operator[]( size_t i ) const { return get(i); }

        /// Returns length of the vector (i.e., the number of entries)
        size_t size() const { return _i.size(); }

        /// Comparison
        bool operator==( const this_type& q ) const {
            if( size() != q.size() )
                return false;
            return i() == q.i();
        }

        this_type& pwBinaryOp(const this_type& q) {
            // Check that the sizes of the two objects are the same.
            DAI_DEBASSERT(size() == q.size());

            // Use a loop to copy elements from 'q._p' to '_p'.
            // if(q._i.size() > 0){
            //     for (size_t i = 0; i < size(); ++i) {
            //         _i[i].insert(q._i[i].begin(), q._i[i].end());
            //         _i[i].merge(q._i[i]);

            //     }
            // }
            

            // Return a reference to the modified object.
            return *this;
        }

        this_type pwBinaryTr( const this_type &q) const {
            DAI_DEBASSERT( size() == q.size() );
            Instantiation r = Instantiation(size());
            
            for (size_t i = 0; i < size(); ++i) {
                r._i[i] = q._i[i];
            }
            
            return r;
        }

        // /// Formats a TProb as a string
        // std::string toString() const {
        //     std::stringstream ss;
        //     ss << *this;
        //     return ss.str();
        // }
    //@}
};



/// Writes a TProb<T> to an output stream
/** \relates TProb
 */
// std::ostream& operator<< (std::ostream& os, const Instantiation& i) {
//     os << "[";
//     for (const auto& myMap : i) {
//         os << myMap << ", ";
//     }
//     os << "]";
//     return os;
// }



/// Represents a vector with entries of type dai::Real.
//typedef TProb<Real> Prob;


} // end of namespace dai


#endif
