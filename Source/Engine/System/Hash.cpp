#include "Hash.h"
#include "Dependencies/google/cityhash/city.h"

u64 Hash::Hash64(const void* Object, size_t SizeInBytes)
{
	return CityHash64(static_cast<const char*>(Object), SizeInBytes);
}
