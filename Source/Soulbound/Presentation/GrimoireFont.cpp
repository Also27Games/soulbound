#include "GrimoireFont.h"

#include "Styling/CoreStyle.h"

namespace Soulbound::Presentation
{
	FSlateFontInfo GrimoireFontInfo(const FGrimoireFont& Font, bool bBold, int32 Size, bool bItalic)
	{
		const int32 Points = FMath::Max(Size, 1);
		if (!Font.Font)
		{
			// Sin fuente propia, la de hoy; con la cursiva (ampliación del autor del 17/11/2026), sus tipos Italic y BoldItalic.
			return FCoreStyle::GetDefaultFontStyle(bItalic ? (bBold ? "BoldItalic" : "Italic") : (bBold ? "Bold" : "Regular"), Points);
		}
		const FName Typeface = bItalic ? (bBold ? Font.BoldItalicTypeface : Font.ItalicTypeface) : (bBold ? Font.BoldTypeface : Font.RegularTypeface);
		return FSlateFontInfo(Font.Font, Points, Typeface);
	}
}
