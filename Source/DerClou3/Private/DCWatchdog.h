#pragma once

#include "CoreMinimal.h"

/**
 * Hang detector for the game: while a heist world plays, a background thread watches the game thread.
 * If no frame starts for a few seconds, it writes the game thread's call stack to the log and to
 * Saved/Logs/DerClou3-Haenger.txt (that file is written directly, so it is there even when the
 * editor has to be killed). Off in shipping builds, in commandlets and under a debugger.
 */
namespace DCWatchdog
{
	void Start();
	void Stop();
	/** Watch only while a game world plays (loading, menus and modal editor dialogs stay quiet). */
	void SetArmed(bool bArmed);
}
