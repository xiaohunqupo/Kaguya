#pragma once
#include "SystemCore.h"

class Stopwatch
{
public:
	static i64 GetTimestamp();

	Stopwatch() noexcept;

	[[nodiscard]] double GetDeltaTime() const noexcept;
	[[nodiscard]] double GetTotalTime() const noexcept;
	[[nodiscard]] double GetTotalTimePrecise() const noexcept;
	[[nodiscard]] bool	 IsPaused() const noexcept;

	void Resume() noexcept;
	void Pause() noexcept;
	void Signal() noexcept;
	void Restart() noexcept;

	static i64 Frequency; // Ticks per second

private:
	i64	   StartTime	= {};
	i64	   TotalTime	= {};
	i64	   PausedTime	= {};
	i64	   StopTime		= {};
	i64	   PreviousTime = {};
	i64	   CurrentTime	= {};
	double Period		= 0.0;
	double DeltaTime	= 0.0;

	bool Paused = false;
};
