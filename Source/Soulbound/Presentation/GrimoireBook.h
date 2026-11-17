#pragma once

#include "CoreMinimal.h"
#include "DecisionText.h"
#include "HowToPlay.h"
#include "LegendPanel.h"
#include "StatusText.h"

struct FMatchState;

/**
 * El Grimorio como libro del juego (S-044, R-HUD-10 a R-HUD-12): sus cinco pestañas, lo que lleva cada una, cuándo se cierra solo y su botón.
 * Lógica pura sobre el estado de la partida y lo que ya calculan las funciones de la leyenda, del Grimorio de S-027 y de "Cómo se juega": sin
 * Slate, sin actor y sin disco, con tests. SGrimoirePanel solo pinta lo que esto devuelve; ABoardViewActor lo alimenta.
 */
namespace Soulbound::Presentation
{
	/** Las pestañas del Grimorio (S-044, R-HUD-10, nombre propuesto), en su orden. */
	enum class EGrimoireTab : uint8
	{
		You,
		Table,
		Board,
		HowToPlay,
		Chronicle,
	};

	/** Las cinco pestañas, en el orden en que se enseñan: Tú, La mesa, El tablero, Cómo se juega y Crónica. */
	const TArray<EGrimoireTab>& GrimoireTabs();

	/** El nombre de una pestaña tal como se lee en su cabecera ("Tú", "La mesa", "El tablero", "Cómo se juega", "Crónica"). */
	FString GrimoireTabTitle(EGrimoireTab Tab);

	/**
	 * La pestaña activa del Grimorio (S-044, R-HUD-10, nombre propuesto). Al abrirse vuelve siempre a "Tú" (decisión del autor del 16/11/2026: no
	 * recuerda la última). Con las flechas pasa a la siguiente o a la anterior de forma cíclica (aceptado por el autor el 17/11/2026).
	 */
	struct FGrimoireTabState
	{
		EGrimoireTab Active = EGrimoireTab::You;

		/** El Grimorio se abre: la pestaña activa es "Tú". */
		void Open();
		/** La siguiente pestaña; desde "Crónica", "Tú". */
		void Next();
		/** La anterior; desde "Tú", "Crónica". */
		void Previous();
		/** Una pestaña concreta (el clic en su nombre). */
		void Select(EGrimoireTab Tab);
	};

	/** Lo que lleva la pestaña "Tú" (S-044, R-HUD-10, nombre propuesto). */
	struct FGrimoireYouView
	{
		/** La tarjeta del humano: la fila suya de GrimoireRows (rol, Elegido y afinidad, runas con su poder, poderes activos). */
		FGrimoireRow Row;
		/** "Insolencia: N de 5". */
		FString InsolenceLine;
		/** Para un Eco tras la carrera (con hogar), lo único que le da una runa (R-ECO-7); vacía en el resto. */
		FString HomeRuneLine;
		/** Al pie, una línea por poder con su frase (GrimoirePowerLines). */
		TArray<FString> PowerLines;

		/** Igualdad de todo lo que se pinta (R-VIS-21): el panel solo se reconstruye si cambia. */
		bool operator==(const FGrimoireYouView& Other) const
		{
			return Row == Other.Row && InsolenceLine == Other.InsolenceLine && HomeRuneLine == Other.HomeRuneLine && PowerLines == Other.PowerLines;
		}
	};

	/**
	 * La pestaña "Tú" (S-044, R-HUD-10): la tarjeta del humano del Grimorio de S-027, su insolencia y, si es un Eco tras la carrera, la línea de
	 * la runa de su hogar ("La runa de tu hogar (D4, nombre) te da Celeridad: tiras dos dados"); al pie, las seis líneas de poderes.
	 * @param State El estado de la partida.
	 * @param HumanSeat El asiento humano de esta máquina.
	 * @param Texts Los textos de grimorio.json.
	 * @param SeatColors El color de cada asiento, el de la vista.
	 * @return Lo que se pinta; con un asiento humano no válido, una fila vacía y sin líneas propias (las de poderes, sí).
	 */
	FGrimoireYouView GrimoireYouView(const FMatchState& State, int32 HumanSeat, const FGrimoireTexts& Texts, const TArray<FLinearColor>& SeatColors);

	/** El texto que marca a un aspirante en "La mesa" (S-044, R-HUD-10). */
	inline const TCHAR* const TableAspirantMark = TEXT(" | ASPIRANTE");

	/**
	 * La pestaña "La mesa" (S-044, R-HUD-10): de cada asiento, lo mismo que hoy enseña la leyenda, ni más ni menos (corrección del autor del
	 * 17/11/2026), con los filtros de R-HUD-6 y R-RUN-7; la marca de aspirante es lo único nuevo. Dos filas: "Jugadores", con una muestra por asiento
	 * de su color (SeatLegendText, la marca de aspirante y, en una segunda línea, "Runas: " con BracerSummary), y "Los seis Dominios y su Elegido",
	 * con una muestra por Dominio de su color (DomainLegendText).
	 * @param State El estado de la partida.
	 * @param HumanSeat El asiento humano de esta máquina (quien mira).
	 * @param ForgottenShown Si la vista pinta cada Dominio como Olvidado (en el prólogo va con la frase de corrupción); falta, no Olvidado.
	 * @param SeatColors El color de cada asiento.
	 * @param DomainColors El color de cada Dominio.
	 * @return Las dos filas, sin muestras con TextGetter (el texto ya va puesto).
	 */
	TArray<FLegendRow> GrimoireTableRows(const FMatchState& State, int32 HumanSeat, const TArray<bool>& ForgottenShown, const TArray<FLinearColor>& SeatColors,
		const TArray<FLinearColor>& DomainColors);

	/** Los nombres de las teclas que dice el Grimorio al pie de "Cómo se juega" (S-044, R-HUD-10, nombre propuesto); los pone la vista. */
	struct FGrimoireKeyNames
	{
		FString Grimoire;
		FString PreviousTab;
		FString NextTab;
		FString ColumnTab;
		FString Panels;
		FString View;
		FString RingStart;
	};

	/**
	 * La pestaña "Cómo se juega" (S-044, R-HUD-10): el texto del menú tal como lo prepara HowToPlayDisplayText, o, si el cargador falló, su
	 * mensaje, para que la pestaña nunca quede vacía; al final, las teclas.
	 * @param Loaded Lo que devolvió LoadHowToPlayText.
	 * @param Keys Los nombres de las teclas configuradas.
	 * @return El texto de la pestaña.
	 */
	FString GrimoireHowToPlayText(const FHowToPlayLoadResult& Loaded, const FGrimoireKeyNames& Keys);

	/**
	 * Cuándo se cierra solo el Grimorio (S-044, R-HUD-11, nombre propuesto). Se mira en cada refresco del HUD, esté abierto o no, y dice si acaba
	 * de aparecer algo que lo cierra: una decisión propia nueva (también bloqueada porque habla el Warden) o un anillo del humano. Lo que ya estaba
	 * a la vista no lo cierra: con una decisión propia abierta, el Grimorio se puede abrir para consultar (precisión del autor del 17/11/2026). La
	 * espera a otro asiento, un resultado o un desenlace no lo cierran.
	 */
	class FGrimoireCloseWatch
	{
	public:
		/**
		 * @param Panel Lo que enseña ahora el panel de decisión (PanelView).
		 * @param bRingOpen Si el anillo del humano está abierto.
		 * @return Verdadero si desde la última vez apareció una decisión propia nueva o se abrió el anillo.
		 */
		bool Observe(const TOptional<FDecisionView>& Panel, bool bRingOpen);

	private:
		/** La identidad de la decisión propia que se veía la última vez; vacía si no había. */
		FString LastOwnDecision;
		bool bLastRingOpen = false;
	};

	/** Si el Grimorio se puede abrir (S-044, R-HUD-11): no durante un anillo del humano, ni con el botón ni con la tecla (decisión del autor del 17/11/2026). */
	bool CanOpenGrimoire(bool bRingOpen);

	/** El texto del botón del Grimorio (S-044, R-HUD-12): "Grimorio (G)", con el nombre de la tecla configurada. */
	FString GrimoireButtonLabel(const FString& KeyName);

	/**
	 * Si el botón del Grimorio se ve (S-044, R-HUD-12, corrección del autor del 17/11/2026): con la regla de la caja de la línea de objetivo, no en
	 * el prólogo, ni con la partida terminada, ni mientras hay una frase del Warden en pantalla. No depende de ninguna tecla de ocultar paneles: no
	 * las recibe. El telón lo mira su capa, como en las demás.
	 */
	bool IsGrimoireButtonShown(bool bPrologue, bool bMatchOver, bool bWardenLineOnScreen);
}
