#include "DCWatchdog.h"

#include <atomic>

#include "DerClou3.h"
#include "HAL/Event.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformStackWalk.h"
#include "HAL/PlatformTime.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "Misc/CommandLine.h"
#include "Misc/CoreDelegates.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

namespace
{
	/** No new frame for this long: write down where the game thread is (once per limit, then quiet until it recovers). */
	constexpr double WatchdogReportAfter[] = { 5.0, 20.0, 60.0 };

	std::atomic<double> WatchdogLastBeat{ 0.0 };
	std::atomic<bool> WatchdogArmed{ false };

	class FDCWatchdogThread final : public FRunnable
	{
	public:
		explicit FDCWatchdogThread(const FString& InReportFile)
			: ReportFile(InReportFile)
		{
			WakeUp = FPlatformProcess::GetSynchEventFromPool(false);
			Thread = FRunnableThread::Create(this, TEXT("DCWatchdog"), 0, TPri_BelowNormal);
		}

		virtual ~FDCWatchdogThread() override
		{
			bStop = true;
			WakeUp->Trigger();
			if (Thread)
			{
				Thread->WaitForCompletion();
				delete Thread;
			}
			FPlatformProcess::ReturnSynchEventToPool(WakeUp);
		}

		virtual uint32 Run() override
		{
			int32 Reported = 0;
			double StuckBeat = 0.0;
			while (!bStop)
			{
				WakeUp->Wait(500);
				if (bStop)
				{
					break;
				}
				const double Beat = WatchdogLastBeat.load();
				if (Reported > 0 && Beat != StuckBeat)
				{
					Report(FString::Printf(TEXT("Watchdog: the game thread is running again - that frame took %.1f s"), Beat - StuckBeat), false);
					Reported = 0;
				}
				if (!WatchdogArmed.load() || Beat <= 0.0 || FPlatformMisc::IsDebuggerPresent())
				{
					Reported = 0;
					continue;
				}
				const double Stuck = FPlatformTime::Seconds() - Beat;
				if (Reported < static_cast<int32>(UE_ARRAY_COUNT(WatchdogReportAfter)) && Stuck >= WatchdogReportAfter[Reported])
				{
					StuckBeat = Beat;
					++Reported;
					Report(FString::Printf(TEXT("Watchdog: no new frame for %.1f s - the game thread is here:"), Stuck), true);
				}
			}
			return 0;
		}

	private:
		void Report(const FString& Header, bool bWithStack)
		{
			FString Text = Header;
			if (bWithStack)
			{
				FPlatformStackWalk::InitStackWalking();
				constexpr int32 MaxFrames = 80;
				uint64 Frames[MaxFrames];
				const int32 Count = static_cast<int32>(FPlatformStackWalk::CaptureThreadStackBackTrace(GGameThreadId, Frames, MaxFrames));
				// Symbols only after the capture: the game thread runs again by then (no lock it holds can block us).
				for (int32 Index = 0; Index < Count; ++Index)
				{
					ANSICHAR Buffer[1024];
					Buffer[0] = '\0';
					FPlatformStackWalk::ProgramCounterToHumanReadableString(Index, Frames[Index], Buffer, sizeof(Buffer));
					Text += FString(TEXT("\n    ")) + ANSI_TO_TCHAR(Buffer);
				}
			}
			UE_LOG(LogDerClou, Warning, TEXT("%s"), *Text);
			// The log file is written by the game thread's side; this file is there even if the process gets killed.
			const FString Line = FDateTime::Now().ToString(TEXT("%Y-%m-%d %H:%M:%S  ")) + Text + LINE_TERMINATOR;
			FFileHelper::SaveStringToFile(Line, *ReportFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
		}

		FString ReportFile;
		FRunnableThread* Thread = nullptr;
		FEvent* WakeUp = nullptr;
		std::atomic<bool> bStop{ false };
	};

	TUniquePtr<FDCWatchdogThread> WatchdogThread;
	FDelegateHandle WatchdogBeatHandle;
}

void DCWatchdog::Start()
{
#if !UE_BUILD_SHIPPING
	if (WatchdogThread || IsRunningCommandlet() || FParse::Param(FCommandLine::Get(), TEXT("DCNoWatchdog")))
	{
		return;
	}
	WatchdogBeatHandle = FCoreDelegates::OnBeginFrame.AddLambda([]() { WatchdogLastBeat.store(FPlatformTime::Seconds()); });
	WatchdogThread = MakeUnique<FDCWatchdogThread>(FPaths::ConvertRelativePathToFull(FPaths::ProjectLogDir() / TEXT("DerClou3-Haenger.txt")));
#endif
}

void DCWatchdog::Stop()
{
	FCoreDelegates::OnBeginFrame.Remove(WatchdogBeatHandle);
	WatchdogBeatHandle.Reset();
	WatchdogThread.Reset();
}

void DCWatchdog::SetArmed(bool bArmed)
{
	WatchdogLastBeat.store(FPlatformTime::Seconds());
	WatchdogArmed.store(bArmed);
}
