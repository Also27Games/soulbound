#pragma once

#include "CoreMinimal.h"
#include "Components/Widget.h"
#include "GrimoireBook.h"
#include "GrimoireFont.h"
#include "HowToPlayLibrary.h"
#include "Styling/SlateBrush.h"
#include "HowToPlayBookWidget.generated.h"

class SHowToPlayBookPanel;
class UFont;
class UTexture2D;

/**
 * El libro de "Cómo se juega" para el menú (S-049, R-HUD-22, nombre aprobado): un widget de UMG que el autor arrastra a su pantalla del menú y que
 * pinta la misma pieza que la pestaña del Grimorio (SHowToPlayBookPanel), con su fondo, su marca de agua y sus láminas, sin pestañas, sin título y
 * sin las teclas de cerrar ni de pestañas. Sus ajustes son los de la vista (UHowToPlayLibrary::ConfiguredBookSettings): no tiene ajustes propios,
 * salvo si atiende las teclas de página. Su tamaño es el del hueco donde se coloca. Empieza en la primera vista cada vez que se crea y no comparte
 * página con el Grimorio de una partida. Carga la fuente, la imagen del libro, la marca de agua y las láminas una vez, las mantiene vivas y dice una
 * vez en el registro las que faltan. Si el archivo no carga, enseña el mensaje del cargador.
 */
UCLASS(meta = (DisplayName = "Libro de Cómo se juega"))
class SOULBOUND_API UHowToPlayBookWidget : public UWidget
{
	GENERATED_BODY()

public:
	/**
	 * Si las teclas de página configuradas (GrimoirePreviousPageKey y GrimoireNextPageKey de la vista; valores iniciales flecha arriba y flecha abajo)
	 * pasan de vista cuando el libro tiene el foco del teclado, y se dicen al pie. Valor inicial: encendidas. Si chocan con la navegación del menú, se
	 * apagan aquí, en el diseñador, y el libro se pasa con sus botones.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cómo se juega")
	bool bPageKeys = true;

	/** Vuelve a la primera vista (para cuando se abre la portada). */
	UFUNCTION(BlueprintCallable, Category = "Cómo se juega")
	void GoToFirstPage();

	/** La vista siguiente; en la última, se queda (FGrimoireTabState::NextPage). Lo hacen el botón "Siguiente" y la tecla. */
	void NextPage();
	/** La vista anterior; en la primera, se queda. Lo hacen el botón "Anterior" y la tecla. */
	void PreviousPage();
	/** La página que se ve, desde 0: la primera de su vista. */
	int32 GetPage() const;
	/** Cuántas hojas se ven a la vez: dos con la imagen del libro, una sin ella (HowToPlayPagesPerView). */
	int32 GetPagesPerView() const;
	/** El libro que enseña; sin páginas y con el mensaje del cargador si el archivo no carga. */
	const Soulbound::Presentation::FHowToPlayBook& GetBook() const;
	/** Los ajustes con que se pinta, los de la vista. */
	const Soulbound::Presentation::FHowToPlayBookSettings& GetSettings() const;
	/** La línea de teclas del pie: las de página (HowToPlayMenuFootHint) si bPageKeys; si no, ninguna. */
	FString FootHint() const;
	/**
	 * Carga el libro de un archivo, con los ajustes de la vista y sus texturas y su fuente, y vuelve a la primera vista. Lo hace solo, con el archivo
	 * del repositorio, la primera vez que hace falta; público para los tests (un archivo roto). Se llama antes de construir el widget.
	 * @param FilePath Ruta absoluta del archivo.
	 */
	void LoadBook(const FString& FilePath);

	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override;
#endif

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	/** Carga el libro del repositorio si aún no se cargó. */
	void EnsureLoaded();
	/** Pasa el libro y la página a la pieza, si está construida. */
	void Refresh();
	/** La letra del Grimorio para la pantalla de ahora (ScaledPanelSlateFontSize, el criterio de la vista); sin pantalla (el diseñador), a 1080. */
	int32 MenuFontSize() const;

	/** Las texturas de las láminas, la imagen del libro, la marca de agua y la fuente, vivas mientras lo esté el widget. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTexture2D>> PageTextures;
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> BookTexture;
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> WatermarkTexture;
	UPROPERTY(Transient)
	TObjectPtr<UFont> FontAsset;

	bool bLoaded = false;
	Soulbound::Presentation::FHowToPlayBookSettings Settings;
	Soulbound::Presentation::FHowToPlayBook Book;
	/** La página que se ve, con las reglas de pasar página del Grimorio; propia de este widget. */
	Soulbound::Presentation::FGrimoireTabState Pages;
	Soulbound::Presentation::FGrimoireFont Font;
	/** Los pinceles de las láminas, por su nombre; los apunta la pieza. */
	TMap<FString, FSlateBrush> PageBrushes;
	TOptional<FSlateBrush> BookBrush;
	TOptional<FSlateBrush> WatermarkBrush;
	TSharedPtr<SHowToPlayBookPanel> Piece;
};
