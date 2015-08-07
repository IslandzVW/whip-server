#pragma once

#include <set>
#include <functional>

#include <boost/shared_ptr.hpp>

namespace whip
{
	template <typename T, typename Pool>
	class ObjectPoolLease;

	/**
	 * Stores live instances of objects for later reuse
	 */
	template <typename T, typename Creator, typename Cmp = std::less<T>> 
	class ObjectPool
	{
	private:
		Creator _creatorFunctor;
		std::set<T, Cmp> _leasedObjects;
		std::vector<T> _freeObjects;

	public:
		ObjectPool(Creator creator) 
			: _creatorFunctor(creator)
		{
			
		}

		virtual ~ObjectPool()
		{
		}

		/**
		 * Lease an object from the pool
		 */
		T lease()
		{
			if (_freeObjects.size() > 0) {
				T obj = _freeObjects.pop_back();
				_leasedObjects.insert(obj);

				return obj;

			} else {
				T newObj = _creatorFunctor();
				_leasedObjects.insert(newObj);

				return newObj;
			}
		}

		/**
		 * Return an object to the pool
		 */
		void unlease(T obj)
		{
			_leasedObjects.erase(obj);
			_freeObjects.push_back(obj);
		}
	};
}