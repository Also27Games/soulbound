#pragma once

#include "CoreMinimal.h"
#include "Match/MatchState.h"

struct FRandomStream;

/**
 * Resultado de un efecto (glosario "Efecto", GDD 05, S-013, nombre propuesto): si se aplicó o por qué su objetivo no es
 * válido (R-DIR-2). Solo Applied cambia el estado.
 */
enum class EEffectResult : uint8
{
	/** El efecto se aplicó y dejó su evento. */
	Applied,
	/** El asiento no existe. */
	InvalidSeat,
	/** El objetivo está en el Vacío: no es objetivo válido de ningún efecto salvo Liberación (R-VAC-1, R-INT-3). */
	InVoid,
	/**
	 * El objetivo está en juego: es el asiento activo con el turno empezado (cualquier fase salvo BetweenTurns) o participa
	 * en un duelo o un trilero en curso (decisión del autor del 08/10/2026; las interrupciones son de S-015).
	 */
	InPlay,
	/** Vuelta al hogar a quien ya está en su hogar (R-EFE-4, R-NIC-2). */
	AlreadyHome,
	/** Liberación sin nadie en el Vacío (R-EFE-17). */
	NobodyInVoid,
	/** Destierro durante la carrera sin la Furia (R-CAR-7); desde S-014, también la Noche de los Ecos durante la carrera. */
	BondRace,
	/** El objetivo no tiene runas: no vale Revelar Brazal ni Perder una runa (R-DIR-3, S-014). */
	NoRunes,
	/** Ninguna Fuente tiene reserva: no vale Runa regalada (R-EFE-16, S-014). */
	NoReserve,
	/** No hay Ecos a los que aplicar la Noche, ni Ecos que mandar a casa ni Noche que terminar con la Ley (S-014). */
	NoEchoes,
	/** No hay casilla que sellar (R-EFE-6, R-EFE-7, S-014). */
	NoValidTile,
	/** Nadie se marca a sí mismo (R-EFE-8, S-014). */
	SelfTarget,
	/** El tipo de runa no es una opción válida (S-014). */
	InvalidRune,
	/** El efecto se aplica en su story: hoy, Alto y Consulta forzada al Oráculo (S-016, S-017). */
	NotImplemented,
	/** La Noche de los Ecos con otra Noche activa (R-EFE-18; decisión del autor del 14/10/2026). */
	AlreadyActive,
	/**
	 * Una Prueba del Warden sobre quien ya la tiene (R-EFE-10, S-015; principio del autor del 14/10/2026: el Warden nunca
	 * parece equivocarse). Nombre propuesto en S-015.
	 */
	AlreadyPending,
};

/**
 * Efectos del Warden (glosario "Efecto", GDD 05; S-013): cada uno con su comprobación de objetivo válido (R-DIR-2), que
 * dice si se puede y por qué no, y su aplicación, que comprueba lo mismo y no cambia nada si no vale. Los llaman los eventos
 * del Warden (Match/WardenEventRules.h, S-014), que les dan dirección y registran la aplicación, y las ediciones de
 * depuración (SbBanish, SbRelease, SbSendHome). Ninguno toca el sorteo de la partida ni el turno en curso.
 */
namespace Soulbound::Rules
{
	/**
	 * Si el asiento Seat está en juego (decisión del autor del 08/10/2026): es el asiento activo y su turno ha empezado
	 * (TurnPhase distinta de BetweenTurns), o participa en el duelo en curso, o es el ladrón o la víctima del trilero en
	 * curso. Entre turnos nadie está en juego. Nombre propuesto en S-013.
	 */
	SOULBOUNDCORE_API bool IsSeatInPlay(const FMatchState& State, int32 Seat);

	/**
	 * Si el asiento Seat puede ser desterrado (R-EFE-14, R-CAR-7), en este orden: InvalidSeat; InVoid si ya está dentro;
	 * BondRace durante la carrera salvo con bFury (la Furia del Warden, R-CAR-7); InPlay si está en juego. Applied si vale.
	 * Nombre propuesto en S-013.
	 */
	SOULBOUNDCORE_API EEffectResult CanBanish(const FMatchState& State, int32 Seat, bool bFury);

	/**
	 * Destierra al asiento Seat (R-VAC-1, R-TAB-24, R-EFE-14): su ficha sale del tablero (TileId INDEX_NONE), su cuenta de
	 * turnos dentro empieza de cero (R-VAC-4) y se registra Banished (Value 1 con la Furia). bFury: si lo destierra la
	 * Furia, lo único que permite desterrar en la carrera. El Vacío de la Furia, donde no funcionan Desvío ni Liberación,
	 * es de S-015. NowSeconds: marca del evento. Devuelve CanBanish; si no es Applied, no cambia nada.
	 * Nombre propuesto en S-013.
	 */
	SOULBOUNDCORE_API EEffectResult Banish(FMatchState& State, int32 Seat, bool bFury, double NowSeconds);

	/** Si la Liberación es válida (R-EFE-17): NobodyInVoid si nadie está en el Vacío; Applied si alguien. S-013. */
	SOULBOUNDCORE_API EEffectResult CanReleaseVoid(const FMatchState& State);

	/**
	 * Liberación (glosario `ReleaseVoid`, R-VAC-5, R-EFE-17): todos los del Vacío salen a la vez a su hogar, sin esperar
	 * turnos, en orden de asiento (LeaveVoid con Released, todos con la misma marca). Si uno de ellos esperaba en su turno
	 * (elección de Desvío o escape), la máquina de turnos cierra la espera y termina el turno (R-VAC-3; decisión aceptada
	 * por el autor el 08/10/2026). NowSeconds: marca de los eventos. Devuelve CanReleaseVoid; si no es Applied, no cambia
	 * nada. Nombre propuesto en S-013.
	 */
	SOULBOUNDCORE_API EEffectResult ReleaseVoid(FMatchState& State, double NowSeconds);

	/**
	 * Si el asiento Seat puede recibir la Vuelta al hogar (R-EFE-4, R-NIC-1, R-NIC-2), en este orden: InvalidSeat; InVoid;
	 * AlreadyHome si ya está en su hogar (HomeTile); InPlay si está en juego. Applied si vale. Nombre propuesto en S-013.
	 */
	SOULBOUNDCORE_API EEffectResult CanSendHome(const FMatchState& State, int32 Seat);

	/**
	 * Vuelta al hogar (glosario `SendHome`, R-EFE-4): la ficha del asiento Seat va a su hogar (HomeTile) y se registra
	 * SentHome con la casilla. No es un movimiento suyo: no resuelve casilla ni cambia sus turnos; el siguiente se juega con
	 * normalidad (R-NIC-1). NowSeconds: marca del evento. Devuelve CanSendHome; si no es Applied, no cambia nada.
	 * Nombre propuesto en S-013.
	 */
	SOULBOUNDCORE_API EEffectResult SendHome(FMatchState& State, int32 Seat, double NowSeconds);

	// S-014: el resto del catálogo (Match/EffectCatalog.h). Cada efecto con su comprobación (R-DIR-2, R-DIR-3) y su
	// aplicación, que comprueba lo mismo y no cambia nada si no vale. Ninguno toca el sorteo de la partida: los que sortean
	// (Sello, Perder una runa al azar) usan el que se les pasa, el del Warden.

	/** Si el asiento Seat está Marcado (R-EFE-8): su marca caduca en una ronda posterior a la actual. S-014. */
	SOULBOUNDCORE_API bool IsMarked(const FMatchState& State, int32 Seat);

	/** Si el Brazal del asiento Seat está revelado (R-EFE-2): se oculta en una ronda posterior a la actual. S-014. */
	SOULBOUNDCORE_API bool IsBracerRevealed(const FMatchState& State, int32 Seat);

	/** Si se puede revelar el Brazal de Seat (R-EFE-2, R-DIR-3): InvalidSeat, InVoid, NoRunes; Applied si vale. S-014. */
	SOULBOUNDCORE_API EEffectResult CanRevealBracer(const FMatchState& State, int32 Seat);

	/**
	 * Revelar Brazal (glosario `RevealBracer`, R-EFE-2): los tipos de runa de Seat quedan visibles para todos durante
	 * FMatchParams::RevealBracerRounds rondas; si ya estaba revelado, se prolonga esas rondas desde donde caducaba. Registra
	 * BracerRevealed con la ronda en que se oculta. Devuelve CanRevealBracer; si no es Applied, no cambia nada. S-014.
	 */
	SOULBOUNDCORE_API EEffectResult RevealBracer(FMatchState& State, int32 Seat, double NowSeconds);

	/** Si Seat puede recibir turnos perdidos o extra (R-EFE-5, R-EFE-15, R-DIR-2): InvalidSeat, InVoid; Applied si vale. S-014. */
	SOULBOUNDCORE_API EEffectResult CanChangeTurns(const FMatchState& State, int32 Seat);

	/**
	 * Turno perdido (glosario `LoseTurn`, R-EFE-5): Count turnos perdidos a Seat, hasta el tope de los datos
	 * (FMatchParams::MaxLostTurns); si alguno excede el tope, se convierte en una Vuelta al hogar (SendHome), y si esa no vale
	 * (ya está en su hogar) queda la burla (WardenTaunt de SendHome; decisión del autor del 12/10/2026). Los perdidos se
	 * cancelan con los extra uno a uno al empezar y al terminar su turno (R-TUR-10). Devuelve CanChangeTurns; si no es
	 * Applied, no cambia nada. S-014.
	 */
	SOULBOUNDCORE_API EEffectResult LoseTurn(FMatchState& State, int32 Seat, int32 Count, double NowSeconds);

	/**
	 * Turno extra (glosario `ExtraTurn`, R-EFE-15): Count turnos extra a Seat, hasta el tope que ya existe (GrantExtraTurns).
	 * Concedido entre turnos, se juega tras su próximo turno (decisión del autor del 12/10/2026). Devuelve CanChangeTurns; si
	 * no es Applied, no cambia nada. S-014.
	 */
	SOULBOUNDCORE_API EEffectResult ExtraTurn(FMatchState& State, int32 Seat, int32 Count, double NowSeconds);

	/** Casillas que el Warden puede sellar ahora (CanSealTile Accepted), en orden ascendente (R-EFE-6). S-014. */
	SOULBOUNDCORE_API TArray<int32> SealableTiles(const FMatchState& State);

	/**
	 * Sello (glosario `Seal`, R-EFE-6): el Warden sella una casilla válida al azar entre SealableTiles, con el sorteo Random
	 * (el del Warden; el cerebro real elegirá en su story). Devuelve NoValidTile, sin cambiar nada, si no hay ninguna. S-014.
	 */
	SOULBOUNDCORE_API EEffectResult PlaceSeal(FMatchState& State, double NowSeconds, FRandomStream& Random);

	/**
	 * Casilla que sellaría el Sello contra rival sobre Rival (R-EFE-7): la primera sellable del camino más corto desde su
	 * casilla hasta su objetivo más probable (el Altar si tiene los seis tipos; si no, la Fuente con reserva de un tipo que le
	 * falta, y en la que puede obtener, más cercana), con su transitabilidad (MakeMoverTraits, R-TAB-21); sin casilla
	 * sellable en ese camino, o sin objetivo, la sellable más cercana a él. Empates de distancia, la de identificador menor.
	 * INDEX_NONE si no hay ninguna o Rival no está en el tablero. S-014.
	 */
	SOULBOUNDCORE_API int32 SealAgainstRivalTile(const FMatchState& State, int32 Rival);

	/** Si vale el Sello contra rival sobre Rival: InvalidSeat, InVoid, NoValidTile; Applied si vale. S-014. */
	SOULBOUNDCORE_API EEffectResult CanSealAgainstRival(const FMatchState& State, int32 Rival);

	/** Sello contra rival (glosario `SealAgainstRival`, R-EFE-7): sella SealAgainstRivalTile. Devuelve CanSealAgainstRival. S-014. */
	SOULBOUNDCORE_API EEffectResult SealAgainstRival(FMatchState& State, int32 Rival, double NowSeconds);

	/**
	 * Si se puede marcar a Seat en un evento que dirige Director (R-EFE-8): InvalidSeat, InVoid, SelfTarget (nadie se marca a
	 * sí mismo); Applied si vale. S-014.
	 */
	SOULBOUNDCORE_API EEffectResult CanMark(const FMatchState& State, int32 Seat, int32 Director);

	/**
	 * Marcado (glosario `Marked`, R-EFE-8): Seat queda Marcado FMatchParams::MarkRounds rondas desde la actual; marcar a un
	 * Marcado reinicia la duración; varias marcas conviven, cada una con la suya. Registra Marked (OtherSeat: Pointer, quien
	 * lo señaló, o INDEX_NONE). Devuelve CanMark; si no es Applied, no cambia nada. S-014.
	 */
	SOULBOUNDCORE_API EEffectResult Mark(FMatchState& State, int32 Seat, int32 Director, int32 Pointer, double NowSeconds);

	/**
	 * Cobro de la marca (R-EFE-8): si Marked está Marcado y Collector es otro asiento, la marca termina, Collector gana un turno
	 * extra (GrantExtraTurns) y se registra MarkCollected. Lo llaman las acciones dirigidas que perjudican: la expulsión, el
	 * robo con runa robada, ganar un duelo y la Vuelta al hogar o el Vacío de un efecto que Collector señaló. Devuelve si cobró.
	 * S-014.
	 */
	SOULBOUNDCORE_API bool CollectMark(FMatchState& State, int32 Collector, int32 Marked, double NowSeconds);

	/**
	 * Si vale la Ley de Sombras (R-EFE-9): con la Noche activa, o con algún Eco fuera del Vacío y fuera de su hogar; si no,
	 * NoEchoes. S-014.
	 */
	SOULBOUNDCORE_API EEffectResult CanShadowLaw(const FMatchState& State);

	/**
	 * Ley de Sombras (glosario `ShadowLaw`, R-EFE-9): todos los Ecos fuera del Vacío vuelven a su hogar (SentHome, los que no
	 * están ya en él) y, si la Noche estaba activa, termina (EchoNightEnded con ShadowLaw). Devuelve CanShadowLaw; si no es
	 * Applied, no cambia nada. S-014.
	 */
	SOULBOUNDCORE_API EEffectResult ShadowLaw(FMatchState& State, double NowSeconds);

	/**
	 * Si vale la Noche de los Ecos (R-EFE-18): BondRace durante la carrera; NoEchoes sin ningún Eco; AlreadyActive con otra
	 * Noche activa (decisión del autor del 14/10/2026: el motor la rechaza y el cerebro del Warden no la decreta); Applied si vale. S-014.
	 */
	SOULBOUNDCORE_API EEffectResult CanEchoNight(const FMatchState& State);

	/**
	 * Noche de los Ecos (glosario `EchoNight`, R-EFE-18): los Ecos transitan por cualquier Dominio (permiso temporal de
	 * R-TAB-20, en MakeMoverTraits) y pueden robar allí, pero no obtienen runas en Fuentes con dueño (R-ECO-3). Dura hasta la
	 * Ley de Sombras o FMatchParams::EchoNightMaxRounds rondas; con la Noche activa no vale (AlreadyActive). Registra
	 * EchoNightBegan. Devuelve CanEchoNight; si no es Applied, no cambia nada. S-014.
	 */
	SOULBOUNDCORE_API EEffectResult EchoNight(FMatchState& State, double NowSeconds);

	/** Tipos de runa que tiene Seat (los que puede perder, R-EFE-11), en orden de tipo. S-014. */
	SOULBOUNDCORE_API TArray<int32> LoseRuneOptions(const FMatchState& State, int32 Seat);

	/** Si Seat puede perder una runa (R-EFE-11, R-DIR-3): InvalidSeat, InVoid, NoRunes; Applied si vale. S-014. */
	SOULBOUNDCORE_API EEffectResult CanLoseRune(const FMatchState& State, int32 Seat);

	/**
	 * Perder una runa (glosario `LoseRune`, R-EFE-11): una runa de Seat vuelve a la reserva de su Fuente (ReturnRuneToSource,
	 * R-RUN-6). RuneType: la elegida (una de LoseRuneOptions), o INDEX_NONE para una al azar con Random, cada runa del Brazal
	 * con la misma probabilidad (los duplicados cuentan). No es un robo: la afinidad de Koschei no aparta nada. Devuelve
	 * CanLoseRune, o InvalidRune si RuneType no es una opción; si no es Applied, no cambia nada. S-014.
	 */
	SOULBOUNDCORE_API EEffectResult LoseRune(FMatchState& State, int32 Seat, int32 RuneType, double NowSeconds, FRandomStream& Random);

	/** Tipos de runa con alguna Fuente con reserva (los que se pueden regalar, R-EFE-16), en orden de tipo. S-014. */
	SOULBOUNDCORE_API TArray<int32> GiftRuneOptions(const FMatchState& State);

	/** Si Seat puede recibir una Runa regalada (R-EFE-16): InvalidSeat, InVoid, NoReserve; Applied si vale. S-014. */
	SOULBOUNDCORE_API EEffectResult CanGiftRune(const FMatchState& State, int32 Seat);

	/**
	 * Runa regalada (glosario `GiftRune`, R-EFE-16): Seat recibe una runa del tipo RuneType (uno de GiftRuneOptions) de la
	 * primera Fuente de ese Dominio con reserva, que baja en uno (RuneObtained). Un Eco la recibe de cualquier Dominio.
	 * Devuelve CanGiftRune, o InvalidRune; si no es Applied, no cambia nada. S-014.
	 */
	SOULBOUNDCORE_API EEffectResult GiftRune(FMatchState& State, int32 Seat, int32 RuneType, double NowSeconds);

	/**
	 * Empieza una ronda (decisión del autor del 10/10/2026, S-014): Round sube uno y caducan, en este orden, las marcas
	 * (MarkExpired), los Brazales revelados (BracerHidden) y la Noche que llega a su tope: todos los Ecos fuera del Vacío a
	 * su hogar, como con la Ley de Sombras, y EchoNightEnded con RoundLimit, sin evento de Ley de Sombras (decisión del autor
	 * del 12/10/2026). Lo llama la máquina de turnos cuando el orden pasa por su primer asiento.
	 */
	SOULBOUNDCORE_API void StartRound(FMatchState& State, double NowSeconds);
}
