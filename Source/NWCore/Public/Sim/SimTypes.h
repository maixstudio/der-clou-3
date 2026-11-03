#pragma once

// Plain C++ (no Unreal headers): this file is also compiled by Tools/SimTests.

#include <cstdint>

#ifndef NWCORE_API
#define NWCORE_API
#endif

namespace nw::sim
{
	using Tick = int32_t;
	using NodeId = int32_t;
	using DoorId = int32_t;
	using AlarmId = int32_t;
	using SafeId = int32_t;
	using SignalId = int32_t;

	constexpr int32_t InvalidId = -1;
	constexpr Tick NoTick = -1;

	/** The simulation runs at a fixed rate; presentation interpolates between ticks. */
	constexpr int32_t TicksPerSecond = 10;

	constexpr Tick SecondsToTicks(int32_t Seconds)
	{
		return Seconds * TicksPerSecond;
	}

	/** Integer position in centimetres. Integers keep the simulation bit-exact across platforms. */
	struct IntVec2
	{
		int32_t X = 0;
		int32_t Y = 0;
	};

	inline int64_t DistSq(const IntVec2& A, const IntVec2& B)
	{
		const int64_t DX = static_cast<int64_t>(A.X) - B.X;
		const int64_t DY = static_cast<int64_t>(A.Y) - B.Y;
		return DX * DX + DY * DY;
	}

	/** Deterministic integer square root (floor). */
	inline int32_t IntSqrt(int64_t Value)
	{
		if (Value <= 0)
		{
			return 0;
		}
		int64_t X = Value;
		int64_t Y = (X + 1) / 2;
		while (Y < X)
		{
			X = Y;
			Y = (X + Value / X) / 2;
		}
		return static_cast<int32_t>(X);
	}
}
