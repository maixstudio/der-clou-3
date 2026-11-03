#pragma once

#include "Sim/SimTypes.h"

namespace nw::sim
{
	/**
	 * Seeded SplitMix64 generator. Never use FMath::Rand or std::rand inside the simulation:
	 * all randomness must come from here so that plan + seed fully determine a run.
	 */
	class Random
	{
	public:
		explicit Random(uint64_t Seed = 0)
			: State(Seed)
		{
		}

		uint64_t Next()
		{
			uint64_t Z = (State += 0x9E3779B97F4A7C15ull);
			Z = (Z ^ (Z >> 30)) * 0xBF58476D1CE4E5B9ull;
			Z = (Z ^ (Z >> 27)) * 0x94D049BB133111EBull;
			return Z ^ (Z >> 31);
		}

		/** Uniform integer in [Min, Max]. Returns Min if Max <= Min. */
		int32_t RangeInclusive(int32_t Min, int32_t Max)
		{
			if (Max <= Min)
			{
				return Min;
			}
			const uint64_t Span = static_cast<uint64_t>(static_cast<int64_t>(Max) - Min + 1);
			return static_cast<int32_t>(static_cast<int64_t>(Min) + static_cast<int64_t>(Next() % Span));
		}

		uint64_t GetState() const
		{
			return State;
		}

	private:
		uint64_t State;
	};
}
