#include "HowToPlayBookPanel.h"

#include "Styling/SlateStyle.h"
#include "Widgets/SNullWidget.h"

// TODO(R-HUD-21): por implementar (tests en rojo).
void SHowToPlayBookPanel::Construct(const FArguments& InArgs)
{
	ChildSlot[SNullWidget::NullWidget];
}

void SHowToPlayBookPanel::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
}

bool SHowToPlayBookPanel::SupportsKeyboardFocus() const
{
	return false;
}

FReply SHowToPlayBookPanel::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	return FReply::Unhandled();
}

void SHowToPlayBookPanel::SetHowToPlay(const Soulbound::Presentation::FHowToPlayBook& InBook, int32 InPage)
{
}

Soulbound::Presentation::FGrimoireBookLayout SHowToPlayBookPanel::CurrentLayout() const
{
	return Soulbound::Presentation::FGrimoireBookLayout();
}

float SHowToPlayBookPanel::ColumnWidth(bool bRight) const
{
	return 1.f;
}

TAttribute<float> SHowToPlayBookPanel::TextWidth(bool bRight, float Indent) const
{
	return TAttribute<float>(1.f);
}

TSharedRef<SWidget> SHowToPlayBookPanel::MakePages()
{
	return SNullWidget::NullWidget;
}

TSharedRef<SWidget> SHowToPlayBookPanel::MakeFrame(const TSharedRef<SWidget>& Pages)
{
	return Pages;
}

void SHowToPlayBookPanel::Rebuild()
{
}

TSharedRef<SWidget> SHowToPlayBookPanel::MakePageButton(const FString& Label, bool bShown, const FSimpleDelegate& OnClicked)
{
	return SNullWidget::NullWidget;
}

TSharedRef<SWidget> SHowToPlayBookPanel::MakeSheet(const FHowToPlayPage* InPage, bool bRight)
{
	return SNullWidget::NullWidget;
}

void SHowToPlayBookPanel::UpdateStyles()
{
}
