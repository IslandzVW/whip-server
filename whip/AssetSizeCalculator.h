#pragma once

class AssetSizeCalculator
{
private:
	static const unsigned int UUID_OVERHEAD = 32;
	static const unsigned int MISC_OVERHEAD = 8;
	static const unsigned int TOTAL_OVERHEAD = UUID_OVERHEAD + MISC_OVERHEAD;

	unsigned long long _size;

public:
	AssetSizeCalculator() : _size(0)
	{

	}

	unsigned long long get_size() const {
		return _size;
	}

	template <typename T>
	void on_add(T item) {
		_size += item->getSize() + TOTAL_OVERHEAD;
	}

	template <typename T>
	void on_remove(T item) {
		_size -= item->getSize() + TOTAL_OVERHEAD;
	}

	void on_clear() {
		_size = 0;
	}
};
