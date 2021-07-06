#pragma once

#define NONCOPYABLE(TypeName)                                                                                          \
	TypeName(const TypeName&) = delete;                                                                                \
	TypeName& operator=(const TypeName&) = delete;

#define NONMOVABLE(TypeName)                                                                                           \
	TypeName(TypeName&&) = delete;                                                                                     \
	TypeName& operator=(TypeName&&) = delete;

template<typename T>
constexpr inline T AlignUp(T Size, T Alignment)
{
	return (T)(((size_t)Size + (size_t)Alignment - 1) & ~((size_t)Alignment - 1));
}

template<typename T>
constexpr inline T AlignDown(T Size, T Alignment)
{
	return (T)((size_t)Size & ~((size_t)Alignment - 1));
}

template<typename T>
constexpr inline T RoundUpAndDivide(T Value, size_t Alignment)
{
	return (T)((Value + Alignment - 1) / Alignment);
}

template<typename T>
constexpr inline bool IsPowerOfTwo(T Value)
{
	return 0 == (Value & (Value - 1));
}

template<typename T>
constexpr inline bool IsDivisible(T Value, T Divisor)
{
	return (Value / Divisor) * Divisor == Value;
}

inline std::uint8_t Log2(std::uint64_t Value)
{
	unsigned long mssb; // most significant set bit
	unsigned long lssb; // least significant set bit

	// If perfect power of two (only one set bit), return index of bit.  Otherwise round up
	// fractional log by adding 1 to most signicant set bit's index.
	if (_BitScanReverse64(&mssb, Value) > 0 && _BitScanForward64(&lssb, Value) > 0)
		return std::uint8_t(mssb + (mssb == lssb ? 0 : 1));

	return 0;
}

constexpr inline std::size_t operator"" _KiB(std::size_t X)
{
	return X * 1024;
}

constexpr inline std::size_t operator"" _MiB(std::size_t X)
{
	return X * 1024 * 1024;
}

constexpr inline std::size_t operator"" _GiB(std::size_t X)
{
	return X * 1024 * 1024 * 1024;
}

inline std::size_t ToKiB(std::size_t Byte)
{
	return Byte / 1024;
}

inline std::size_t ToMiB(std::size_t Byte)
{
	return Byte / 1024 / 1024;
}

inline std::size_t ToGiB(std::size_t Byte)
{
	return Byte / 1024 / 1024 / 1024;
}
