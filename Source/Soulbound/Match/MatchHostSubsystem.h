#pragma once

#include "CoreMinimal.h"
#include "Decider/Decider.h"
#include "Match/MatchState.h"
#include "Match/TurnRules.h"
#include "Match/UnreachableRules.h"
#include "AdvanceGuard.h"
#include "StepClock.h"
#include "Curtain.h"
#include "RingTrialRun.h"
#include "Math/RandomStream.h"
#include "Subsystems/WorldSubsystem.h"
#include "Warden/WardenBrain.h"
#include "MatchHostSubsystem.generated.h"

class ABoardViewActor;

#if !UE_BUILD_SHIPPING
/**
 * Qué pasó con una edición de depuración pedida al anfitrión (S-011, nombre propuesto): si se aplicó y la frase que
 * la describe (qué hizo, con todo el detalle, o por qué se rechazó). Solo existe fuera de la build de entrega.
 */
struct FDebugEditOutcome
{
	bool bApplied = false;
	FString Message;
};
#endif

/** Aviso de que el estado de la partida ha cambiado. Lo lanza solo el anfitrión de partida. */
DECLARE_MULTICAST_DELEGATE(FOnMatchStateChanged);

/**
 * Cómo acabó una prueba del anillo del humano (S-026; nombre propuesto en el PR), para que la vista lo enseñe un momento. Es lo que el
 * anfitrión entregó al motor por el humano, también si lo fijó SbTrial o SbDuel.
 */
struct FRingOutcome
{
	/** La prueba que era; None si era un duelo. */
	ESkillTrial Trial = ESkillTrial::None;
	/** Si era un duelo: entonces vale Performance y no bPassed. */
	bool bDuel = false;
	/** Si se superó (prueba de habilidad). */
	bool bPassed = false;
	/** Rendimiento en el duelo, de 0 a 1. */
	double Performance = 0.0;
	/** Si el resultado lo fijó un comando de depuración (SbTrial, SbDuel) en vez del anillo. */
	bool bForced = false;
	/** Precisiones de los pulsos jugados; con un resultado fijado, las que hubiera hasta entonces. */
	TArray<double> Precisions;
};

/** Se abre una prueba del anillo del humano (S-026). */
DECLARE_MULTICAST_DELEGATE(FOnRingOpened);

/** Se cierra la prueba del anillo del humano (S-026): con su resultado si se entregó, o sin él si se descartó (la fase cambió). */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnRingClosed, const TOptional<FRingOutcome>& /*Outcome*/);

/**
 * Anfitrión de partida (glosario "MatchHost", nombre propuesto en S-005): UWorldSubsystem, solo en el servidor y
 * solo en mapas de partida (en el spike, los que usan ASpikeOnlineGameMode; en los menús de UGT no existe). Es el
 * dueño de la partida: carga el tablero y los parámetros de sus archivos, llama a StartMatch, avanza la máquina de
 * turnos con el reloj real, consulta a los decisores dummy y recibe la intención del humano. Es el único que escribe
 * el estado y el único que avisa de que cambió. No contiene reglas: todas están en SoulboundCore.
 * También genera la presentación (ABoardViewActor); el mapa no cambia.
 * Es quien lleva el reloj de partida y por eso es quien tiene el modo paso a paso (S-007, glosario "StepMode"): con él
 * activo, la partida se detiene al entrar en BetweenTurns y el reloj se congela hasta que se concede un paso. La regla
 * (cuándo detener, qué marca el reloj) está en FStepClock.
 * Desde S-022 (R-PRO-1) también lleva el prólogo: al empezar, el reloj de partida queda detenido (FStepClock::Hold) mientras el Warden dice la
 * apertura, los segundos de OpeningSeconds en tiempo real; al vencer se reanuda (FStepClock::Resume) y empieza el primer turno.
 */
UCLASS(Config = Game, BlueprintType)
class SOULBOUND_API UMatchHostSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	/** Si este mundo es una partida con autoridad: no es un cliente y usa el GameMode del spike. */
	static bool IsMatchWorld(const UWorld& World);

	/** Si la partida se inició. Falso si faltó un archivo de datos (el motivo está en el log). */
	bool HasMatch() const { return bHasMatch; }

	/** Estado de la partida, en solo lectura. Solo tiene sentido si HasMatch(). */
	const FMatchState& GetState() const { return State; }

	/** Asiento que juega el humano con el ratón. */
	int32 GetHumanSeat() const { return HumanSeat; }

	/** Semilla con la que se inició la partida (la fijada, o la aleatoria elegida). Repetir una partida es fijarla. */
	int32 GetSeed() const { return UsedSeed; }

	/** Segundos de partida ahora: los que marca el reloj (congelado en el paso a paso, S-007). El anfitrión del Warden los mira (S-016). */
	double GetMatchSeconds() const { return MatchSeconds(); }

	/**
	 * Intención de destino del asiento humano (glosario "Intención"). Se entrega tal cual a SubmitDestinationIntent:
	 * el motor decide si vale; una intención inválida se descarta sin castigo. Devuelve lo que dijo el motor.
	 */
	EIntentResult SubmitHumanIntent(int32 TileId);

	/**
	 * El último destino del humano que el motor rechazó por inalcanzable (S-039, R-DEC-16; nombre propuesto): lo que la vista necesita para
	 * enseñar el aviso. Serial sube en cada rechazo, también si el motivo y la tirada son los de antes: dos rechazos seguidos iguales se
	 * distinguen, y el segundo sustituye al primero y vuelve a contar. Antes del primer rechazo, Serial vale 0. Es presentación: no se
	 * guarda en el estado de la partida y el motor no lo sabe.
	 */
	struct FHumanRejection
	{
		int32 Serial = 0;
		EUnreachableReason Reason = EUnreachableReason::Roll;
		/** La tirada del turno (la suma de los dados) cuando se rechazó. */
		int32 Roll = 0;
		/**
		 * Dónde estaba el cursor al hacer clic, en coordenadas absolutas de Slate, leído en la misma llamada que el clic (no en el siguiente refresco del
		 * HUD, que ya podría ser otro punto). La vista lo pasa a coordenadas de su capa. Cero si no hay Slate o no hay cursor.
		 */
		FVector2f ScreenPosition = FVector2f::ZeroVector;
	};

	/** El último rechazo por inalcanzable del humano (FHumanRejection). */
	const FHumanRejection& GetLastHumanRejection() const { return LastHumanRejection; }

	/**
	 * Intención de avance del asiento Seat (S-020), sea humano o dummy (decisión del autor del 21/10/2026): la vista la entrega al
	 * ver entrar su ficha en cada casilla del camino. Se entrega tal cual a SubmitMoveProgress, que solo acepta la del asiento
	 * activo; una que no vale (la salvaguarda ya entró en esa casilla, o el turno se cortó) se descarta sin castigo. Devuelve lo
	 * que dijo el motor.
	 */
	// TODO(R-RED-1): con red, el avance lo entrega la presentación del servidor; si un cliente llega a entregarlo, el servidor tendrá
	// que comprobar que el asiento es suyo, como con el resto de intenciones.
	EIntentResult SubmitMoveProgress(int32 Seat, int32 TileId);

	/**
	 * Respuesta del asiento humano a su elección pendiente (S-009): dados, repetir, salto, acción contra un jugador, runa
	 * de Koschei o esfera del trilero (S-010), también cuando no tiene el turno. Se entrega tal cual a SubmitChoiceIntent,
	 * como el destino: el motor decide si vale. Kind: el tipo de elección que contesta. Option: una respuesta válida
	 * (FPendingChoice::AnswerAt), o INDEX_NONE para "no". Devuelve lo que dijo el motor.
	 */
	EIntentResult SubmitHumanChoice(EChoiceKind Kind, int32 Option);

	/**
	 * Consulta del Oráculo del asiento humano (S-017, R-ORA-3): la vista la entrega con un clic en el orbe. Se entrega tal cual a
	 * SubmitOracleConsult: el motor decide si vale (consulta abierta, suya y en plazo). Devuelve lo que dijo el motor.
	 */
	EIntentResult SubmitHumanOracleConsult();

	/** Rechazo de Tifón del asiento humano (S-017, R-ORA-5), con el clic derecho en el orbe; como SubmitHumanOracleConsult. */
	EIntentResult SubmitHumanOracleRefuse();

	/**
	 * Voto del asiento humano por Voted (S-017, R-VOT-1): la vista lo entrega con un clic en la ficha del votado. Se entrega tal cual a
	 * SubmitVote: el motor decide si vale. Devuelve lo que dijo el motor.
	 */
	EIntentResult SubmitHumanVote(int32 Voted);

	/**
	 * El anfitrión del Warden pide una evaluación (S-016): el motor la hace y la aplica sobre el estado
	 * (Soulbound::Rules::EvaluateWarden), aquí se escribe y se avisa, y la partida sigue (un golpe deja el turno entre turnos
	 * con el evento en marcha). Devuelve la decisión, para el log del anfitrión del Warden.
	 */
	FWardenDecision EvaluateWarden();

	/**
	 * Modo paso a paso (S-007). Activo, la partida se detiene al acabar cada turno y el reloj se congela hasta que se
	 * concede un paso (StepOnce). Cambiable en caliente: al desactivarlo la partida sigue sola.
	 */
	UFUNCTION(BlueprintCallable, Category = "Partida")
	void SetStepMode(bool bEnabled);

	UFUNCTION(BlueprintPure, Category = "Partida")
	bool IsStepMode() const { return bStepMode; }

	/** Suelta exactamente un turno. Solo hace algo si la partida está detenida esperando; devuelve si lo hizo. */
	UFUNCTION(BlueprintCallable, Category = "Partida")
	bool StepOnce();

	/**
	 * Si la partida está detenida esperando el paso, o en el prólogo (S-022): el reloj no corre y el Warden no evalúa. Para distinguir el
	 * prólogo, IsInPrologue.
	 */
	UFUNCTION(BlueprintPure, Category = "Partida")
	bool IsWaitingForStep() const { return StepClock.bHeld; }

	/**
	 * Si la partida está en el prólogo (S-022, R-PRO-1; nombre aprobado): el Warden dice la apertura y el reloj de partida sigue en cero hasta
	 * que termina. Ni Espacio ni SbStepMode 0 lo acortan; SbEvent, SbStrike y SbClock lo acaban antes de aplicarse.
	 */
	UFUNCTION(BlueprintPure, Category = "Partida")
	bool IsInPrologue() const { return bInPrologue; }

	/**
	 * Sube el telón (S-024, R-PRO-1 precisión; nombre propuesto "telón", aún no está en el glosario): la presentación lo llama cuando la
	 * pantalla de carga se ha ido, y el prólogo empieza a contar desde ese instante. Si el telón no está bajado (sin prólogo, o ya subido)
	 * no hace nada. En los menús el subsistema no existe: quien lo llama comprueba que es válido.
	 */
	UFUNCTION(BlueprintCallable, Category = "Partida")
	void RaiseCurtain();

	/**
	 * Si el telón está bajado: el prólogo espera a la presentación y ni las frases del Warden ni el reloj de partida han empezado. Sigue
	 * siendo verdadero durante CurtainGraceSeconds tras subirlo (ampliación del 3/11), con el tiempo del mundo que lleva el anfitrión.
	 */
	UFUNCTION(BlueprintPure, Category = "Partida")
	bool IsCurtainDown() const;

	/**
	 * Si la mesa espera al Warden (S-023, decisión del autor del 29/10/2026; glosario propuesto "espera del Warden"): una frase que no es
	 * un comentario, un aviso del final ni una instrucción se está enseñando y ni los dummies, ni el turno, ni las intenciones, ni la
	 * evaluación del Warden avanzan. El reloj de partida sigue; al acabar, los plazos abiertos se desplazan lo que ha durado. No es el prólogo.
	 */
	UFUNCTION(BlueprintPure, Category = "Partida")
	bool IsWardenSpeaking() const { return bWardenSpeaking; }

	/** Si el humano juega sus pruebas con el anillo (S-026, bHumanPlaysSkillTrials). Apagado, las resuelve un dummy al azar, como antes. */
	bool HumanPlaysSkillTrials() const { return bHumanPlaysSkillTrials; }

	/**
	 * La prueba del anillo del humano en curso (S-026), en solo lectura, o nullptr si no hay. Su tiempo es el reloj real
	 * (FPlatformTime::Seconds): la vista pinta con él.
	 */
	const FRingTrialRun* GetRingRun() const { return HumanRing.IsSet() ? &HumanRing->Run : nullptr; }

	/** Qué prueba juega el humano con el anillo: None si no hay o si es un duelo (IsRingDuel). */
	ESkillTrial GetRingTrial() const { return HumanRing.IsSet() ? HumanRing->Trial : ESkillTrial::None; }

	/** Si la prueba del anillo en curso es un duelo. */
	bool IsRingDuel() const { return HumanRing.IsSet() && HumanRing->bDuel; }

	/** Dificultad con que se juega la prueba del anillo en curso; normal si no hay. */
	ESkillDifficulty GetRingDifficulty() const { return HumanRing.IsSet() ? HumanRing->Params.Difficulty : ESkillDifficulty::Normal; }

	/**
	 * Pulsación del humano en el anillo (S-026), fechada con el reloj real del evento de entrada (R-MIN-18): PressSeconds. Sin prueba en
	 * curso no hace nada. Devuelve si contó (FRingTrialRun::Press); el resultado se entrega al motor en el siguiente avance.
	 */
	bool PressRing(double PressSeconds);

	/**
	 * El humano pulsa "Empezar" (S-042, R-MIN-19; nombre propuesto), con el botón o con su tecla, en PressSeconds del reloj real. Si su anillo espera
	 * a que esté listo, la prueba empieza (FRingTrialRun::Start) y el primer pulso arranca tras RingStartBreatherSeconds; si la cuenta ya había
	 * vencido, empieza sola en su instante. Deja en el acta cómo empezó y cuánto esperó. No es una pulsación del anillo: no resuelve ningún pulso.
	 * @param PressSeconds Instante de la pulsación, con FPlatformTime::Seconds.
	 * @return Si empezó por esta pulsación; false sin anillo, con la prueba ya empezada o si empezó sola.
	 */
	bool StartRing(double PressSeconds);

	/**
	 * Deja en el acta (S-030, R-ACT-7) una línea con el prefijo "warden" y la marca de tiempo de la partida: la usa el anfitrión del Warden para
	 * la línea de cada evaluación del cerebro. Antes vuelca los eventos pendientes del registro, para que el archivo vaya en orden cronológico.
	 * Sin acta abierta (apagada o sin partida) no hace nada.
	 */
	void RecordWardenNote(const FString& Text);

	/** Se lanza al abrir una prueba del anillo del humano (S-026). */
	FOnRingOpened OnRingOpened;

	/** Se lanza al cerrarla, con su resultado o sin él si se descartó (S-026). */
	FOnRingClosed OnRingClosed;

#if !UE_BUILD_SHIPPING
	// Herramientas de depuración (S-011, glosario "DebugEdit"). Son las únicas vías por las que los comandos Sb… cambian
	// el estado: cada una pide la edición al motor (Soulbound::Debug), avisa a la vista si se aplicó y deja el detalle en
	// el log y en el resultado. No existen en la build de entrega (UE_BUILD_SHIPPING); los comandos de consola del
	// controlador siguen declarados, pero su cuerpo no hace nada.

	/** Da Count runas de tipo RuneType al asiento Seat, sacadas de la reserva de su Fuente. */
	FDebugEditOutcome DebugGiveRune(int32 Seat, int32 RuneType, int32 Count);

	/** Quita Count runas de tipo RuneType al asiento Seat y las devuelve a la reserva de su Fuente. */
	FDebugEditOutcome DebugTakeRune(int32 Seat, int32 RuneType, int32 Count);

	/** Coloca la ficha del asiento Seat en la casilla TileId, sin resolverla. */
	FDebugEditOutcome DebugMove(int32 Seat, int32 TileId);

	/** Vincula al asiento Seat con el Elegido del Dominio DomainId, como si hubiera reclamado su Nicho. */
	FDebugEditOutcome DebugBond(int32 Seat, int32 DomainId);

	/** Sella la casilla TileId con las reglas de Sello. */
	FDebugEditOutcome DebugSeal(int32 TileId);

	/** Libera el Sello de la casilla TileId. */
	FDebugEditOutcome DebugUnseal(int32 TileId);

	/** Termina la carrera por el vínculo ahora. */
	FDebugEditOutcome DebugEndRace();

	/** Fija la próxima tirada de la partida. */
	FDebugEditOutcome DebugFixRoll(int32 Value);

	/** Fija el asiento del siguiente turno que empiece. */
	FDebugEditOutcome DebugFixNextSeat(int32 Seat);

	/**
	 * Fija el resultado del próximo minijuego de habilidad del humano (hoy lo resuelve un dummy al azar, S-008). Vive aquí
	 * y no en el motor porque el anfitrión es quien responde por el humano. El motor solo deja el evento de la edición
	 * (Soulbound::Debug::FixTrial).
	 */
	FDebugEditOutcome DebugFixTrial(bool bPass);

	/**
	 * Fija el resultado del próximo duelo en que participe el humano (S-012): al resolverse, el humano rinde 1 y su rival 0
	 * para ganar, o al revés para perder, sea quien sea Shuten-dōji (el margen es menor que 1). Como DebugFixTrial, vive
	 * aquí y el motor solo deja el evento de la edición (Soulbound::Debug::FixDuel).
	 */
	FDebugEditOutcome DebugFixDuel(bool bWin);

	/** Destierra al asiento Seat al Vacío, sin la Furia (S-013, Soulbound::Debug::BanishSeat). */
	FDebugEditOutcome DebugBanish(int32 Seat);

	/** Liberación: todos los del Vacío salen a su hogar (S-013). */
	FDebugEditOutcome DebugRelease();

	/** Vuelta al hogar del asiento Seat (S-013). */
	FDebugEditOutcome DebugSendHome(int32 Seat);

	/** Fija la insolencia del asiento Seat en Level, de 0 a 4, sin castigo (S-016, Soulbound::Debug::SetInsolence). */
	FDebugEditOutcome DebugSetInsolence(int32 Seat, int32 Level);

	/**
	 * Lanza el evento del Warden EventId como entrega directa, entre turnos (S-014, Soulbound::Debug::LaunchEvent), con el
	 * sorteo del Warden, que desde S-016 vive en el estado (WardenDraw). Si la partida estaba detenida en el paso a paso y el
	 * evento espera una elección, el reloj se reanuda sin conceder turno: las elecciones del evento tienen plazo, y la partida
	 * vuelve a detenerse al acabar el evento, antes del siguiente turno. Con el prólogo en marcha, primero lo acaba (S-022), y lo deja en
	 * el log, aunque luego el evento se rechace.
	 */
	FDebugEditOutcome DebugLaunchEvent(const FString& EventId);

	/**
	 * Lanza el evento del Warden EventId como golpe ahora (S-020, SbStrike, Soulbound::Debug::StrikeEvent): corta el turno en curso,
	 * con el sorteo de la partida para el consumo fijo, y el evento entra en el hueco con el sorteo del Warden; sin turno en curso,
	 * como DebugLaunchEvent. En un duelo, un trilero o un minijuego se rechaza. Como DebugLaunchEvent, reanuda el reloj del paso a
	 * paso si el evento espera una elección, y acaba el prólogo antes de golpear (S-022). Desde S-041, con bViaOracle, el golpe va por el
	 * Oráculo (DebugStrikeViaOracle).
	 */
	FDebugEditOutcome DebugStrikeEvent(const FString& EventId, bool bViaOracle = false);

	/**
	 * El golpe por el Oráculo (S-041, `SbStrike <evento> oraculo`, herramienta de depuración pedida en el plan): como lo decide el cerebro
	 * (ApplyWardenDecision con bViaOracle), corta el turno en curso y abre la consulta del cortado, con origen Strike y el evento EventId de
	 * sentencia, sin frase. Sin turno en curso, abre la consulta del último que jugó. Se rechaza con una consulta o un evento en curso, sin
	 * Warden, con un evento que no existe o en un duelo, un trilero o un minijuego. No toca SoulboundCore.
	 */
	FDebugEditOutcome DebugStrikeViaOracle(const FString& EventId, bool bTurnInProgress);

	/**
	 * Adelanta el reloj de partida a Seconds (S-018, SbClock, Soulbound::Debug::SetClock): el estado y el reloj del anfitrión
	 * (FStepClock::Skip). Lo que venza por ello (fin de carrera, evaluaciones del Warden, fin de la partida a los 40 minutos) llega en
	 * el siguiente paso, como si el tiempo hubiera pasado. Hacia atrás se rechaza. Con el prólogo en marcha, primero lo acaba (S-022).
	 */
	FDebugEditOutcome DebugSetClock(double Seconds);

	/**
	 * Dice la frase Key de la tabla del Warden ahora (S-021, SbSay, Soulbound::Debug::SayLine), para probar los textos sin esperar al
	 * suceso. Una clave que no está en la tabla se rechaza.
	 */
	FDebugEditOutcome DebugSay(const FString& Key);
#endif

	/** Se lanza tras cada cambio del estado. */
	FOnMatchStateChanged OnMatchStateChanged;

	/** Ruta del tablero, relativa a Content/. Valor inicial: el tablero de 15 casillas por Dominio (S-005). */
	UPROPERTY(Config)
	FString BoardRelativePath = TEXT("Data/Boards/tablero_15.json");

	/** Ruta de los parámetros de partida, relativa a Content/. */
	UPROPERTY(Config)
	FString MatchParamsRelativePath = TEXT("Data/Match/partida.json");

	/** Ruta de los Elegidos, relativa a Content/ (S-008). */
	UPROPERTY(Config)
	FString ElectsRelativePath = TEXT("Data/Elects/elegidos.json");

	/** Ruta de los eventos del Warden, relativa a Content/ (S-014). */
	UPROPERTY(Config)
	FString WardenEventsRelativePath = TEXT("Data/Events/eventos.json");

	/** Ruta de los parámetros del Warden, relativa a Content/ (S-016). */
	UPROPERTY(Config)
	FString WardenParamsRelativePath = TEXT("Data/Warden/warden.json");

	/** Ruta de las frases del Warden, relativa a Content/ (S-021). */
	UPROPERTY(Config)
	FString WardenLinesRelativePath = TEXT("Data/Warden/frases.json");

	/** Semilla de la partida. Valor inicial -1: aleatoria. Un valor de 0 en adelante la fija para repetir una partida. */
	UPROPERTY(Config)
	int32 FixedSeed = -1;

	/** Asiento del humano. Valor inicial 0 (story S-005). */
	UPROPERTY(Config)
	int32 HumanSeat = 0;

	/**
	 * Segundos que tarda un dummy en confirmar su destino desde que tiene la tirada, y en contestar cada elección (S-009). Su
	 * camino lo recorre su ficha, que entrega el avance de cada casilla como la del humano (S-020, decisión del autor del 21/10/2026).
	 * Valor inicial 5 (como en la simulación, S-003); menor que el reloj de elección, así un dummy nunca lo agota. Si el
	 * plazo de una elección es más corto (la esfera del trilero, S-010), contesta a la mitad de lo que queda.
	 */
	UPROPERTY(Config)
	float DummyResponseSeconds = 5.f;

	/**
	 * Segundos que el telón espera a la presentación (S-024) antes de subirse solo, para que el juego no dependa de la conexión en
	 * Blueprint con la pantalla de carga. Valor inicial 6 (Config/DefaultGame.ini). En PIE sin pantalla de carga es lo que tarda en
	 * empezar cada partida. Cero o negativo: el tope vence en el acto.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Partida")
	float CurtainTimeoutSeconds = 6.f;

	/**
	 * Margen tras subir el telón (S-024, ampliación del 3/11): la vista y el HUD lo siguen tratando como bajado y el prólogo no cuenta
	 * hasta que pasa, para que la primera frase no llegue pegada a la desaparición de la pantalla de carga. Valor inicial 1,5
	 * (Config/DefaultGame.ini). Cero: sin margen.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Partida")
	float CurtainGraceSeconds = 1.5f;

	/** Modo paso a paso al iniciar la partida. Valor inicial false (S-007). Se cambia en juego con SetStepMode o SbStepMode. */
	UPROPERTY(Config, BlueprintGetter = IsStepMode, BlueprintSetter = SetStepMode, Category = "Partida")
	bool bStepMode = false;

	/**
	 * El humano juega sus pruebas de habilidad y sus duelos con el anillo (S-026, decisión del autor del 06/11/2026). Valor inicial true
	 * (Config/DefaultGame.ini). En false, como antes de S-026: un dummy (HumanTrialDecider) responde por el humano al azar y los textos
	 * dicen "resuelto al azar". Los dummies juegan siempre con su decisor.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Partida")
	bool bHumanPlaysSkillTrials = true;

	/**
	 * Respiro del anillo (S-042, R-MIN-19; nombre propuesto): segundos entre empezar la prueba (botón "Empezar", su tecla o la cuenta a cero) y el
	 * inicio del primer pulso, para que la pulsación de empezar no se encadene con la del primer pulso. Sustituye al anuncio fijo de S-026
	 * (RingAnnounceSeconds); la espera máxima es un dato de partida (FRingParams::StartWaitSeconds). Valor inicial 0,5 (Config/DefaultGame.ini).
	 * Negativo cuenta como cero.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Partida")
	float RingStartBreatherSeconds = 0.5f;

	/**
	 * Si cada partida deja su acta en un archivo de texto (S-030, R-ACT-9; glosario propuesto "Acta de partida"). Valor inicial true
	 * (Config/DefaultGame.ini). Apagado, no se crea ni la carpeta. Si el archivo no se puede crear o escribir, un aviso en el log y la partida
	 * sigue: el acta nunca para una partida.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Partida")
	bool bWriteMatchRecord = true;

	/** Carpeta de las actas, relativa a Saved/ (FPaths::ProjectSavedDir, S-030, R-ACT-9). Valor inicial "Partidas". */
	UPROPERTY(Config, EditAnywhere, Category = "Partida")
	FString MatchRecordDirectory = TEXT("Partidas");

	/** Cada cuántos segundos reales se avanza la máquina de turnos. Valor inicial 0,1. */
	UPROPERTY(Config)
	float AdvanceIntervalSeconds = 0.1f;

private:
	/** Segundos de partida: los que marca el reloj (congelado en el paso a paso, S-007). */
	double MatchSeconds() const;

	/** Avanza la máquina de turnos hasta que haya que esperar a alguien, consultando a los dummies. */
	void Advance();

	/** Carga los archivos de datos e inicia la partida. Devuelve false, con el motivo en el log, si no puede. */
	bool StartFromData(UWorld& World);

	/** Acaba el prólogo, si dura: el reloj de partida se reanuda desde cero (FStepClock::Resume). Why dice por qué, para el log. */
	void EndPrologue(const TCHAR* Why);

	/** Sube el telón, si está bajado, y fija el final del prólogo desde ahora. Why dice quién (presentación, tope...), para el log. */
	void LiftCurtain(const TCHAR* Why);

	/**
	 * Mira lo que ha dicho el Warden desde la última vez (S-023): si algo detiene la mesa (WardenWaitSeconds), empieza la espera o la
	 * alarga hasta el más tardío entre su final y ahora más lo nuevo. Devuelve si hubo algo nuevo que la detiene.
	 */
	bool NoteWardenLines();

	/**
	 * Acaba la espera del Warden (S-023): desplaza los plazos abiertos (ShiftOpenDeadlines) y las horas de las respuestas programadas de los
	 * dummies lo que ha durado en tiempo real. El reloj de partida no se toca: nunca se detuvo.
	 */
	void EndWardenWait();

	/** Si la mesa espera al Warden, deja en el log que What se rechaza (S-023) y devuelve true: la intención no se entrega. */
	bool RejectWhileWardenSpeaks(const TCHAR* What) const;

	/**
	 * Respuesta ya decidida de un dummy, a la espera de su hora. Va atada al turno que la originó (asiento y
	 * TurnCount) y a lo que contesta (ChoiceKind, None para el destino y el minijuego): si cambia por cualquier otra vía
	 * (reloj agotado, turno extra del mismo asiento, otra elección), se descarta.
	 */
	struct FPendingDecision
	{
		bool bPending = false;
		int32 Seat = INDEX_NONE;
		int32 TurnCount = INDEX_NONE;
		double ReadyAtSeconds = 0.0;
		int32 TileId = INDEX_NONE;
		/** Resultado del minijuego que confirmará, si lo que espera es la prueba de una Fuente (S-008). */
		bool bPassedTrial = false;
		/** Elección que contesta (S-009), o None si es el destino, el minijuego o el duelo. */
		EChoiceKind ChoiceKind = EChoiceKind::None;
		/** Opción que entregará, si contesta una elección (S-009). */
		int32 Option = INDEX_NONE;
		/** Si lo que espera son los rendimientos de un duelo (S-012): entonces los dos se entregan juntos, a la misma hora. */
		bool bDuel = false;
		/** Rendimientos del retador y del retado que entregará, si espera un duelo (S-012). */
		double ChallengerPerformance = 0.0;
		double ChallengedPerformance = 0.0;
		/** Si lo que espera es una consulta del Oráculo (S-017), y si la rechazará (Tifón; los dummies nunca). */
		bool bOracle = false;
		bool bRefuseOracle = false;
		/**
		 * Si responde por el humano con el anillo (S-026): entonces lo de arriba se entrega cuando el anillo se cierra (HumanRing vacío),
		 * no a su hora. Con un resultado fijado antes de empezar, el anillo no llega a abrirse.
		 */
		bool bRing = false;
	};

	/** La prueba del anillo del humano en curso (S-026): la prueba sin mundo y qué uso es. */
	struct FHumanRing
	{
		FRingTrialRun Run;
		FRingTrialParams Params;
		ESkillTrial Trial = ESkillTrial::None;
		bool bDuel = false;
		/** Cuántos pulsos resueltos tienen ya su línea en el log (LogRingPulses). */
		int32 LoggedPulses = 0;
		/** Fotogramas del motor (GFrameCounter) y segundos reales (FPlatformTime::Seconds) al abrirse, para medir los fotogramas por segundo (S-030, R-ACT-10). */
		uint64 OpenFrame = 0;
		double OpenRealSeconds = 0.0;
	};

	/**
	 * Escribe Text en el log del motor con la verbosidad dada (Log, Warning o Error) y, si el acta está abierta, también en ella con el
	 * prefijo "host" (S-030, R-ACT-7): el texto del log no cambia y el acta no depende de que UE_LOG esté activo. RecordText, si no está vacío,
	 * es lo que va al acta en vez de Text (el cierre del anillo añade los fotogramas por segundo).
	 */
	void Note(ELogVerbosity::Type Verbosity, const FString& Text, const FString& RecordText = FString());

	/** Abre el acta (S-030, R-ACT-6): crea el archivo en Saved/<MatchRecordDirectory>/ con la cabecera. Sin efecto con bWriteMatchRecord apagado. */
	void OpenRecord();

	/** Añade Text (una o varias líneas, sin salto final) al archivo del acta y lo vuelca al disco. Si falla, un aviso y el acta se cierra: la partida sigue. */
	void AppendToRecord(const FString& Text);

	/**
	 * Vuelca al acta los eventos del registro que aún no están (lectura incremental, como la crónica y la espera del Warden), en orden, y escribe
	 * el cierre si la partida ha terminado. Se llama tras cada aviso de cambio del estado y antes de escribir cualquier otra línea del acta.
	 */
	void FlushRecordEvents();

	/** Una línea del acta con Prefix ("host", "humano" o "warden") y la marca de tiempo de la partida; antes vuelca los eventos pendientes. */
	void RecordLine(const TCHAR* Prefix, const FString& Text);

	/** Una línea "humano" del acta (S-030, R-ACT-8): lo que el humano hizo y lo que devolvió el motor. */
	void RecordHuman(const FString& Text) { RecordLine(TEXT("humano"), Text); }

	/** Escribe el cierre del acta (R-ACT-4, R-ACT-6) y la deja cerrada: se escribe una sola vez, termine la partida o se abandone. */
	void WriteRecordClosing();

	/**
	 * Abre una prueba del anillo del humano (S-026) con Params: sortea su secuencia con RingRng al abrir y, desde S-042 (R-MIN-19), la deja
	 * esperando a que el humano pulse "Empezar" (StartRing) o a que pase la espera de datos (FRingParams::StartWaitSeconds), en el reloj real, y
	 * avisa (OnRingOpened). Trial: la prueba, o None con bDuel.
	 */
	void OpenRing(const FRingTrialParams& Params, ESkillTrial Trial, bool bDuel);

	/** Si el anillo del humano espera y su cuenta ha vencido en Now, lo empieza solo (FRingTrialRun::StartIfDue) y lo deja en el acta (S-042). */
	void StartRingIfDue(double Now);

	/** La línea del acta y del log con cómo empezó el anillo del humano y cuánto esperó (S-042, R-MIN-19; RingStartLine). */
	void NoteRingStart();

	/** Cierra la prueba del anillo, si la hay, y avisa con Outcome (OnRingClosed); sin Outcome es que se descarta. Why va al log. */
	void CloseRing(const TOptional<FRingOutcome>& Outcome, const TCHAR* Why);

	/**
	 * Avanza el anillo del humano en el reloj real y, si ha terminado, pone su resultado en Pending (superada o no, o el rendimiento del
	 * humano en el duelo) y lo cierra. Devuelve si está cerrado: entonces lo de Pending se puede entregar.
	 */
	bool SettleRing();

	/**
	 * Deja en el log una línea por cada pulso resuelto que aún no la tiene, en orden: "Anillo: pulso k de N, precisión p.", con
	 * " (vencido sin pulsar)" si venció sin pulsación. Así el registro de una prueba tiene siempre una línea por pulso. bLastPressed: si el
	 * último pulso resuelto lo resolvió una pulsación; los demás nuevos vencieron.
	 */
	void LogRingPulses(bool bLastPressed);

	/** El resultado del anillo según su uso: prueba (bPassed) o duelo (Performance), con las precisiones. */
	FRingOutcome MakeRingOutcome(bool bForced) const;

	FMatchState State;
	FRandomStream MatchRng;
	FRandomStream DeciderRng;
	/**
	 * Sorteo de las secuencias del anillo del humano (S-026): semilla de la partida + 5, porque la simulación ya usa + 1 a + 4 (decisión
	 * del autor del 06/11/2026). No toca los dados, ni los dummies, ni el Warden.
	 */
	FRandomStream RingRng;
	/** La prueba del anillo del humano en curso (S-026). Vacía si no hay. */
	TOptional<FHumanRing> HumanRing;
	TArray<TSharedPtr<IDecider>> Deciders;
	/**
	 * Responde por el asiento humano lo que aún no tiene interfaz: las pruebas de habilidad (S-008) y el rendimiento en un
	 * duelo (S-012), al azar, hasta que exista el minijuego real.
	 */
	FDummyDecider HumanTrialDecider;
	FPendingDecision Pending;
	/**
	 * Votos de los dummies en la votación abierta (S-017): a quién votará cada asiento y cuándo (su retardo, o la mitad de lo que
	 * queda). Indexados por asiento; se vacían al cerrarse la votación.
	 */
	TArray<TOptional<TPair<double, int32>>> PendingVotes;
	/** Plazo de la votación a la que pertenecen PendingVotes: si cambia, es otra votación y se vacían. */
	double PendingVotesDeadline = -1.0;
#if !UE_BUILD_SHIPPING
	/** Resultado fijado del próximo minijuego del humano (S-011); sin valor, lo resuelve el dummy. Se gasta al usarlo. */
	TOptional<bool> ForcedTrialResult;

	/** Resultado fijado del próximo duelo del humano (S-012): true gana, false pierde. Se gasta al usarlo. */
	TOptional<bool> ForcedDuelResult;

	/** Cierra una edición de depuración: la deja en el log, avisa a la vista si se aplicó y devuelve su resultado. */
	FDebugEditOutcome FinishDebugEdit(bool bApplied, const FString& Message);
#endif
	FTimerHandle AdvanceTimer;
	FStepClock StepClock;
	/** Si dura el prólogo (S-022): el reloj está detenido por él, no por el paso a paso. */
	bool bInPrologue = false;
	/** Tiempo del mundo en que acaba el prólogo. */
	double PrologueEndsWorldSeconds = 0.0;
	/** El telón (S-024): bajado, el prólogo no ha empezado. */
	FCurtain Curtain;
	/** Cuándo avisa Advance de que su bucle superó el tope (S-032, R-ANF-2): el primero, y después uno cada 5 s como mucho. */
	FAdvanceTripNotice AdvanceTripNotice;
	/** Lo que dura el prólogo (OpeningSeconds), anotado al iniciar para fijar su final cuando suba el telón. */
	double PrologueSeconds = 0.0;
	/** Si la mesa espera al Warden (S-023). El reloj de partida sigue: no es una espera de FStepClock. */
	bool bWardenSpeaking = false;
	/** Tiempo del mundo en que empezó la espera del Warden y en que acaba. */
	double WardenWaitStartedWorldSeconds = 0.0;
	double WardenWaitEndsWorldSeconds = 0.0;
	/** Primera posición del registro que la espera aún no ha mirado. */
	int32 WardenLinesScanned = 0;
	int32 UsedSeed = 0;
	bool bHasMatch = false;
	/** El último destino del humano rechazado por inalcanzable (S-039). */
	FHumanRejection LastHumanRejection;
	/** Ruta completa del archivo del acta (S-030); vacía si no se abrió. */
	FString RecordPath;
	/** Si el acta está abierta: se escribe hasta que se cierra (el cierre, o un fallo de escritura). */
	bool bRecordOpen = false;
	/** Primera posición del registro que el acta aún no ha escrito (su propia lectura incremental). */
	int32 RecordEventsWritten = 0;

	UPROPERTY()
	TObjectPtr<ABoardViewActor> View;
};
