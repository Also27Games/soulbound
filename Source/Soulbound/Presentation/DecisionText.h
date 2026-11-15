#pragma once

#include "CoreMinimal.h"
#include "Match/MatchState.h"
#include "Match/UnreachableRules.h"

/**
 * Lo que enseña el panel de decisión (S-028, R-DEC-1; nombres propuestos): la decisión abierta para el asiento humano, descompuesta en título,
 * contexto, instrucción y opciones con su tecla, y el reloj de su barra. Funciones puras sobre el estado de la partida, sin actor ni mundo, como
 * StatusText. No deciden nada: las opciones son exactamente las que el motor tiene abiertas (FPendingChoice, votación, Oráculo, destino), y si
 * la mesa espera al Warden es un dato que reciben. El panel (SDecisionPanel) solo pinta lo que esto devuelve.
 */
namespace Soulbound::Presentation
{
	/** Cómo se contesta la decisión (R-DEC-2): con botones, con clic en una ficha (y botones), en el orbe (y botones) o en una casilla. */
	enum class EDecisionInput : uint8
	{
		Buttons,
		Token,
		Orb,
		Tile,
		/** S-033 (R-DEC-5, nombre propuesto): no es una decisión del humano, es la espera a otro asiento: sin botones y sin nada que pulsar. */
		Wait,
		/** S-033 (R-DEC-8, nombre propuesto): el resultado de una votación, unos segundos tras cerrarse: sin botones ni barra, con una línea por voto. */
		Result,
	};

	/** A qué entrada del anfitrión va una opción del panel: la misma que su tecla o su clic. */
	enum class EDecisionAnswer : uint8
	{
		/** UMatchHostSubsystem::SubmitHumanChoice(ChoiceKind, Value); Value INDEX_NONE es "no". */
		Choice,
		/** UMatchHostSubsystem::SubmitHumanVote(Value), con Value el asiento votado. */
		Vote,
		/** UMatchHostSubsystem::SubmitHumanOracleConsult. */
		OracleConsult,
		/** UMatchHostSubsystem::SubmitHumanOracleRefuse. */
		OracleRefuse,
	};

	/** Una opción del panel de decisión (S-028, nombre propuesto): su texto, su tecla y la respuesta que entrega. */
	struct FDecisionOption
	{
		FString Text;
		/** La tecla que la contesta, como se enseña delante del texto: "1" a "9", "0" o "R"; vacía si no tiene. */
		FString Key;
		EDecisionAnswer Answer = EDecisionAnswer::Choice;
		/** El tipo de la elección que contesta, con Answer Choice. */
		EChoiceKind ChoiceKind = EChoiceKind::None;
		/** La respuesta: la de FPendingChoice::AnswerAt, INDEX_NONE para "no", o el asiento votado. */
		int32 Value = INDEX_NONE;

		bool operator==(const FDecisionOption& Other) const
		{
			return Text == Other.Text && Key == Other.Key && Answer == Other.Answer && ChoiceKind == Other.ChoiceKind && Value == Other.Value;
		}
	};

	/** La decisión abierta para el humano, tal como la enseña el panel (S-028, R-DEC-1, nombre propuesto). */
	struct FDecisionView
	{
		EDecisionInput Input = EDecisionInput::Buttons;
		/** Qué se decide: el nombre del evento del Warden en las suyas. */
		FString Title;
		/** Una línea de contexto: el texto del evento en las del Warden; vacía si no hay nada que añadir. */
		FString Context;
		/** Cómo se contesta, cuando no basta con los botones: "Elige una casilla que late.", "o haz clic en su ficha". */
		FString Instruction;
		/** Un botón por opción, en el orden del motor; el "0  No" al final en las que admiten "no". */
		TArray<FDecisionOption> Options;
		/** Fin del plazo, en segundos del reloj de partida (DeadlineSeconds de la elección, de la votación o del Oráculo, o TurnDeadlineSeconds). */
		double DeadlineSeconds = 0.0;
		/** El reloj de partida del estado con que se calculó. */
		double ClockSeconds = 0.0;
		/**
		 * El valor de cada dado de la tirada que se enseña (S-039, R-DEC-15, nombre propuesto): los de FMatchState::DiceValues, en su orden. Vacío
		 * si no hay tirada que enseñar con dados, o con una tirada fijada por depuración (el contexto lleva entonces "un N").
		 */
		TArray<int32> DiceValues;
		/** Líneas de detalle bajo el contexto (S-033, nombre propuesto): una por voto en el resultado de una votación; vacía en las demás. */
		TArray<FString> Lines;
		/** Si lleva barra de plazo (S-033): falso en la espera de una prueba de habilidad o de un duelo, que no tienen plazo, y en el resultado. */
		bool bHasDeadline = true;
		/** La mesa espera al Warden (R-DEC-3): se enseña con los botones desactivados y "El Warden habla" en vez del reloj. */
		bool bBlocked = false;
		/** Qué decisión es, para saber si la que se ve es la misma que antes (el reloj de la barra): la entrada y, con botones, el tipo de elección. */
		FString Identity;
	};

	/** El texto que sustituye al reloj mientras la mesa espera al Warden (R-DEC-3). */
	inline const TCHAR* const DecisionBlockedText = TEXT("El Warden habla");

	/**
	 * La decisión abierta para el asiento humano (R-DEC-1). Si hay más de una, la que el panel de estado pone primero (decisión del autor del
	 * 10/11/2026): su elección pendiente, después su destino (en su turno de tirada), después su consulta del Oráculo y, al final, su voto.
	 * @param State El estado de la partida.
	 * @param HumanSeat El asiento humano de esta máquina.
	 * @param bWardenSpeaking Si la mesa espera al Warden (UMatchHostSubsystem::IsWardenSpeaking): la vista sale bloqueada, con las mismas opciones.
	 * @return La vista, o nada si no hay decisión abierta para el humano, si ya votó o si la partida ha terminado.
	 */
	TOptional<FDecisionView> DecisionView(const FMatchState& State, int32 HumanSeat, bool bWardenSpeaking);

	/**
	 * Qué enseña el panel mientras la mesa espera a otro asiento (S-033, R-DEC-5, nombre propuesto). En el orden del panel de estado: la
	 * elección pendiente de otro asiento, su destino (su turno de tirada), su consulta del Oráculo, la votación abierta en la que el humano ya
	 * votó o no puede votar (con quién falta) y la prueba de habilidad o el duelo de otros asientos. No decide nada: quién y qué lo dice el estado.
	 * @param State El estado de la partida.
	 * @param HumanSeat El asiento humano de esta máquina.
	 * @return La vista de tipo Wait, sin opciones, o nada si la mesa no espera a otro asiento o la partida ha terminado.
	 */
	TOptional<FDecisionView> WaitView(const FMatchState& State, int32 HumanSeat);

	/**
	 * El resultado de la última votación (S-033, R-DEC-8, nombre propuesto): el más votado con sus votos, si desempató el Warden y una línea por
	 * voto de esa votación. Sale del registro de eventos y del reloj de partida, sin reloj real.
	 * @param State El estado de la partida.
	 * @param ResultSeconds Cuánto se enseña, en segundos del reloj de partida (VoteResultSeconds).
	 * @return La vista de tipo Result, o nada si no hay votación resuelta o hace ResultSeconds o más que se resolvió.
	 */
	TOptional<FDecisionView> VoteResultView(const FMatchState& State, double ResultSeconds);

	/**
	 * El desenlace de lo que no se ve en el tablero (S-033, R-DEC-10, añadido del autor; nombre propuesto): quién gana un duelo, qué opción de un
	 * dilema sale, qué roba el trilero y si otro asiento supera su prueba. Es el último suceso de esa lista en el registro, con su edad medida
	 * con ClockSeconds; título y una línea (Context), sin botones ni barra. No inventa nada: quién, a quién y qué lo dice el registro.
	 * @param State El estado de la partida.
	 * @param HumanSeat El asiento humano: su elección de dilema y sus pruebas no se enseñan (ya las ve él).
	 * @param OutcomeSeconds Cuánto se enseña, en segundos del reloj de partida (OutcomeSeconds).
	 * @return La vista de tipo Result, o nada si el último suceso de la lista es del humano, hace OutcomeSeconds o más, o no hay ninguno.
	 */
	TOptional<FDecisionView> OutcomeView(const FMatchState& State, int32 HumanSeat, double OutcomeSeconds);

	/**
	 * Lo que enseña el panel en cada momento (S-033, R-DEC-6, nombre propuesto): la decisión del humano si existe; si no, el resultado de una
	 * votación o el desenlace recientes (el más nuevo del registro, R-DEC-10); si no, la espera a otro asiento, salvo mientras la mesa espera al Warden (su frase ya está en pantalla); si no, nada.
	 * @param State El estado de la partida.
	 * @param HumanSeat El asiento humano de esta máquina.
	 * @param bWardenSpeaking Si la mesa espera al Warden (UMatchHostSubsystem::IsWardenSpeaking).
	 * @param VoteResultSeconds Cuánto dura el resultado de una votación.
	 * @param OutcomeSeconds Cuánto dura un desenlace.
	 * @return La vista, o nada.
	 */
	TOptional<FDecisionView> PanelView(const FMatchState& State, int32 HumanSeat, bool bWardenSpeaking, double VoteResultSeconds, double OutcomeSeconds);

	/**
	 * Si el panel enseña una decisión propia del humano (S-033, adición a del autor): lo que hace que el panel de estado no repita la petición.
	 * Nunca con la espera ni con el resultado, y no con el anillo abierto.
	 * @param View Lo que devolvió PanelView.
	 * @param bRingOpen Si el anillo del humano está abierto.
	 * @return Si se enseña y es una decisión del humano.
	 */
	bool IsOwnDecisionShown(const TOptional<FDecisionView>& View, bool bRingOpen);

	/**
	 * Si el panel de decisión se enseña (R-DEC-3): hay vista y el anillo del humano no está abierto. Bloqueada o no, se enseña. La tecla de los
	 * paneles y el telón los mira la capa del panel, como en los demás.
	 * @param View Lo que devolvió DecisionView.
	 * @param bRingOpen Si el anillo del humano está abierto (UMatchHostSubsystem::GetRingRun).
	 * @return Si se enseña.
	 */
	bool IsDecisionPanelShown(const TOptional<FDecisionView>& View, bool bRingOpen);

	/**
	 * El texto del aviso de casilla inalcanzable (R-DEC-16, S-039), en la voz del Warden: uno por motivo (R-TAB-29). Los tres son del autor.
	 * @param Reason Por qué el motor no deja ir a la casilla (UnreachableReason).
	 * @param Roll La tirada del turno: la suma de los dados; solo la usa Roll.
	 * @return El texto.
	 */
	FString UnreachableNoticeText(EUnreachableReason Reason, int32 Roll);

	/**
	 * La opción de la vista que contesta la tecla Key, para que la tecla y el botón entreguen lo mismo.
	 * @param View La vista.
	 * @param Key La tecla: "1" a "9", "0" o "R".
	 * @return La opción, o null si ninguna lleva esa tecla.
	 */
	const FDecisionOption* FindDecisionOption(const FDecisionView& View, const FString& Key);

	/** El reloj de la barra del panel (S-028, nombre propuesto): qué decisión mide y cuánto quedaba cuando la vista la vio aparecer. */
	struct FDecisionClock
	{
		FString Identity;
		double TotalSeconds = 0.0;
	};

	/**
	 * Avanza el reloj de la barra con la vista de ahora y devuelve qué parte del plazo queda (decisión del autor del 10/11/2026): el total es lo
	 * que quedaba cuando la vista vio aparecer la decisión; si el plazo se alarga (la espera del Warden lo desplaza), el total pasa a lo que queda.
	 * Sin vista, el reloj se vacía.
	 * @param Clock El reloj, que se actualiza.
	 * @param View La vista de ahora.
	 * @return La fracción que queda, de 0 a 1; 0 sin vista o con el plazo vencido.
	 */
	double AdvanceDecisionClock(FDecisionClock& Clock, const TOptional<FDecisionView>& View);

	/**
	 * Qué parte del plazo queda: lo que falta hasta DeadlineSeconds entre TotalSeconds, de 0 a 1. La usan AdvanceDecisionClock y la barra del
	 * panel en cada pintado, con el reloj de partida del momento.
	 * @param TotalSeconds El total del reloj de la barra.
	 * @param DeadlineSeconds Fin del plazo, en segundos del reloj de partida.
	 * @param NowSeconds El reloj de partida ahora.
	 * @return La fracción; 0 si el total no es positivo.
	 */
	double DecisionTimeFraction(double TotalSeconds, double DeadlineSeconds, double NowSeconds);

	/**
	 * Si los botones de una decisión van en fila (corrección del autor del 11/11/2026): todos tienen el ancho del más largo, y van en fila si
	 * así caben en el ancho disponible; si no, en columna, también iguales.
	 * @param Count Cuántos botones.
	 * @param WidestButton El ancho del más largo, sin su hueco.
	 * @param SlotGap El hueco que lleva cada botón (los dos lados de su margen).
	 * @param AvailableWidth El ancho del que disponen los botones.
	 * @return Verdadero si van en fila: Count por (WidestButton más SlotGap) no pasa de AvailableWidth. Sin botones, en fila.
	 */
	bool DecisionButtonsInRow(int32 Count, float WidestButton, float SlotGap, float AvailableWidth);

	/**
	 * El hueco libre del tablero en horizontal (corrección del autor del 11/11/2026, R-HUD-3): lo que queda del ancho de la ventana, en unidades de
	 * Slate, a la izquierda de la columna de paneles, con 16 de margen a cada lado. Con él se decide el ancho máximo del panel de decisión y el
	 * ancho en que se parte la frase del Warden.
	 * @param ViewportSlateWidth El ancho de la ventana, ya dividido por la escala de DPI.
	 * @param PanelColumnWidth El ancho de la columna de paneles.
	 * @param Cap El tope del ancho.
	 * @return El ancho libre, entre 0 y Cap.
	 */
	float BoardFreeWidth(float ViewportSlateWidth, float PanelColumnWidth, float Cap);
}
