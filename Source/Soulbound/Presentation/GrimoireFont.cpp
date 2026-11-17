#include "GrimoireFont.h"

#include "Styling/CoreStyle.h"

namespace Soulbound::Presentation
{
	FSlateFontInfo GrimoireFontInfo(const FGrimoireFont& Font, bool bBold, int32 Size)
	{
		const int32 Points = FMath::Max(Size, 1);
		if (!Font.Font)
		{
			// Sin fuente propia, la de hoy.
			return FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", Points);
		}
		return FSlateFontInfo(Font.Font, Points, bBold ? Font.BoldTypeface : Font.RegularTypeface);
	}
}
