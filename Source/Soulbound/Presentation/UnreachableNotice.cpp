#include "UnreachableNotice.h"

void Soulbound::Presentation::FUnreachableNotice::Show(const FString& NewText, double NowReal)
{
	Text = NewText;
	ShownAt = NowReal;
}

void Soulbound::Presentation::FUnreachableNotice::Dismiss()
{
	Text.Reset();
}

FString Soulbound::Presentation::FUnreachableNotice::Current(double NowReal, double Seconds) const
{
	return !Text.IsEmpty() && NowReal - ShownAt < Seconds ? Text : FString();
}
