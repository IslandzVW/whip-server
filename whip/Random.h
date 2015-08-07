#pragma once

#include <boost/random.hpp>

namespace Random 
{ 
        typedef boost::minstd_rand base_generator_type; 
        typedef boost::uniform_int<> distribution_type; 
        typedef boost::variate_generator<base_generator_type&, distribution_type> gen_type; 
        
		extern base_generator_type* generator;

        /*************************************************** 
         * initializes the random number generator * 
         **************************************************/ 
        void initialize();

        /*************************************************** 
         * generates a random number between a and b * 
         * @param a the starting range * 
         * @param b the integer ending range * 
         * @return N such that a <= N <= b * 
         **************************************************/ 
        template <class T> 
        T rand_int (const T& a, const T& b) 
        { 
                //Check the parameters 
                BOOST_STATIC_ASSERT(boost::is_integral<T>::value); 
                assert (b >= a); 
                //Set up the desired distribution 
                gen_type ran_gen(*generator, distribution_type(a, b)); 
                //Get a random number 
                return ran_gen(); 
        } 
} 