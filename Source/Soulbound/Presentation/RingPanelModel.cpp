#include "RingPanelModel.h"

#include "Match/MatchState.h"

double Soulbound::Presentation::RingInnerRadius(double CloseFraction, double RuneRadius, double MaxRadius)
{
	return RuneRadius + (MaxRadius - RuneRadius) * (1.0 - FMath::Clamp(CloseFraction, 0.0, 1.0));
}

FRingProgress Soulbound::Presentation::RingProgress(TConstArrayView<double> Precisions, int32 NumPulses, ERingCriterion Criterion, double PassThreshold)
{
	FRingProgress Progress;
	if (NumPulses > 0)
	{
		double Sum = 0.0;
		for (const double Precision : Precisions)
		{
			Sum += Precision;
		}
		Progress.Fill = FMath::Clamp(Sum / NumPulses, 0.0, 1.0);
	}
	if (Criterion == ERingCriterion::MeanAtLeastThreshold)
	{
		Progress.Mark = PassThreshold;
	}
	return Progress;
}

int32 Soulbound::Presentation::RingRuneDomain(const FMatchState& State, int32 Seat, ESkillTrial Trial)
{
	if (Trial != ESkillTrial::ForgottenSource || !State.Seats.IsValidIndex(Seat))
	{
		return INDEX_NONE;
	}
	const int32 TileId = State.Seats[Seat].TileId;
	return State.Board.Tiles.IsValidIndex(TileId) ? State.Board.Tiles[TileId].DomainId : INDEX_NONE;
}
