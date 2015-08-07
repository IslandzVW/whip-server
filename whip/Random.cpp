#include "StdAfx.h"

#include "Random.h"

namespace Random 
{ 

//Declare the generator as a global variable for easy reuse 
base_generator_type* generator;

void initialize() 
{ 
	generator = new base_generator_type(static_cast<unsigned long>((unsigned long) time(NULL)));
    for (int i = 0; i < 30000; ++i) 
            generator->seed(static_cast<unsigned long>(clock() + 1)); 
} 

}
