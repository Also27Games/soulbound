#include "HowToPlayBookWidget.h"

#include "HowToPlayBookPanel.h"
#include "Widgets/SNullWidget.h"

// TODO(R-HUD-22): por implementar (tests en rojo).
void UHowToPlayBookWidget::GoToFirstPage()
{
}

void UHowToPlayBookWidget::NextPage()
{
}

void UHowToPlayBookWidget::PreviousPage()
{
}

int32 UHowToPlayBookWidget::GetPage() const
{
	return -1;
}

int32 UHowToPlayBookWidget::GetPagesPerView() const
{
	return 0;
}

const Soulbound::Presentation::FHowToPlayBook& UHowToPlayBookWidget::GetBook() const
{
	return Book;
}

const Soulbound::Presentation::FHowToPlayBookSettings& UHowToPlayBookWidget::GetSettings() const
{
	return Settings;
}

FString UHowToPlayBookWidget::FootHint() const
{
	return FString();
}

void UHowToPlayBookWidget::LoadBook(const FString& FilePath)
{
}

void UHowToPlayBookWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	Piece.Reset();
}

#if WITH_EDITOR
const FText UHowToPlayBookWidget::GetPaletteCategory()
{
	return FText::FromString(TEXT("Soulbound"));
}
#endif

TSharedRef<SWidget> UHowToPlayBookWidget::RebuildWidget()
{
	return SNullWidget::NullWidget;
}

void UHowToPlayBookWidget::EnsureLoaded()
{
}

void UHowToPlayBookWidget::Refresh()
{
}

int32 UHowToPlayBookWidget::MenuFontSize() const
{
	return 1;
}
