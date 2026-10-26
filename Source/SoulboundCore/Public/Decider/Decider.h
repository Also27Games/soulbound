#pragma once

#include "CoreMinimal.h"
#include "Match/MatchState.h"
#include "Math/RandomStream.h"

namespace Soulbound::Decider
{
	/**
	 * Probabilidad con la que un dummy supera el minijuego de habilidad (decisión del autor, 02/10/2026). Valor inicial
	 * 0,5; es el comportamiento de un doble de prueba hasta que exista el minijuego real, no una regla de juego.
	 */
	inline constexpr double DummySkillSuccessChance = 0.5;
}

/**
 * Decisor (glosario "Decisor"): quien decide por un asiento. Humano, bot básico, dummy o dummy insolente
 * implementan la misma interfaz; el motor no distingue. Spike: solo la elección de destino.
 */
class SOULBOUNDCORE_API IDecider
{
public:
	virtual ~IDecider() = default;

	/** Elige una casilla de destino entre las alcanzables. Devuelve INDEX_NONE si no hay opciones. */
	virtual int32 ChooseDestination(const FMatchState& State, int32 Seat, const TArray<int32>& Options, FRandomStream& Rng) = 0;

	/**
	 * Resultado del minijuego de habilidad (glosario "Minijuego de habilidad"): true si el asiento lo supera. Lo pregunta
	 * quien avanza la partida cuando el motor espera en SkillTrial, y entrega la respuesta con SubmitSkillTrialResult,
	 * igual que hace con el destino (S-008). Trial y Difficulty son los datos de la prueba (FMatchState::PendingTrial y
	 * PendingDifficulty). El minijuego real de un humano llegará con su story; hasta entonces los dummies responden con
	 * una probabilidad fija (0,5) sacada del generador que les pasan.
	 */
	virtual bool PassSkillMinigame(const FMatchState& State, int32 Seat, ESkillTrial Trial, ESkillDifficulty Difficulty, FRandomStream& Rng) = 0;

	/**
	 * Contesta la elección pendiente Choice del asiento Seat (S-009): una respuesta válida (FPendingChoice::AnswerAt, un
	 * valor de Options o, en la acción contra un jugador, el índice de una opción), o INDEX_NONE para "no" si la elección
	 * lo admite (FPendingChoice::CanDecline). Quien avanza la partida la pregunta cuando la máquina espera una elección y
	 * entrega la respuesta con SubmitChoiceIntent, igual que el destino. Desde S-010 se pregunta al decisor del asiento al
	 * que se le pide (Choice.Seat), que puede no tener el turno: Koschei víctima aparta una runa. Nombre propuesto en S-009.
	 */
	virtual int32 ChooseOption(const FMatchState& State, int32 Seat, const FPendingChoice& Choice, FRandomStream& Rng) = 0;

	/**
	 * Rendimiento del asiento Seat en el duelo en curso (S-012, R-DUE-1; FMatchState::Duel), de 0 a 1: gana el mayor, con el
	 * margen de Shuten-dōji. Lo pregunta quien avanza la partida a cada participante cuando el motor espera en la fase Duel
	 * y lo entrega con SubmitDuelPerformance, como el minijuego. Es el doble de prueba del minijuego de duelo, que llegará
	 * con su story; hasta entonces los dummies rinden al azar con el generador que les pasan. Nombre propuesto en S-012.
	 */
	virtual double DuelPerformance(const FMatchState& State, int32 Seat, FRandomStream& Rng) = 0;

	/**
	 * Si el asiento Seat rechaza la sentencia del Oráculo que tiene delante (R-ORA-5, S-017): solo cuenta si puede (Tifón, la
	 * primera). Quien avanza la partida lo pregunta al abrirse la consulta y entrega SubmitOracleRefuse o SubmitOracleConsult.
	 * Los dummies nunca rechazan y consultan en el acto (R-VOT-5, decisión del autor del 23/10/2026): nunca son insolentes.
	 * Nombre propuesto en S-017.
	 */
	virtual bool RefusesOracleSentence(const FMatchState& State, int32 Seat, FRandomStream& Rng) = 0;

	/**
	 * A quién vota el asiento Seat en la votación abierta (R-VOT-5, S-017): uno de Options (VoteOptions), o INDEX_NONE si no hay. Quien
	 * avanza la partida lo pregunta y lo entrega con SubmitVote. Nombre propuesto en S-017.
	 */
	virtual int32 ChooseVote(const FMatchState& State, int32 Seat, const TArray<int32>& Options, FRandomStream& Rng) = 0;
};

/**
 * Dummy: elige al azar con el generador que le pasan, así la simulación sigue siendo determinista por semilla. En las
 * elecciones, al azar entre las opciones y "no" si la elección lo admite, todas con la misma probabilidad (S-009): roba
 * a cualquiera de los objetivos o a ninguno, elige cualquier esfera y, si es Koschei, aparta cualquier runa (S-010). Reta a
 * duelo como cualquier otra acción y rinde un número al azar de 0 a 1 (S-012). Desde S-017, en un rival señalado sigue al rival
 * sugerido por un disparador (FCurrentWardenEvent::SuggestedRival), y consulta el Oráculo sin rechazar nunca.
 */
class SOULBOUNDCORE_API FDummyDecider : public IDecider
{
public:
	virtual int32 ChooseDestination(const FMatchState& State, int32 Seat, const TArray<int32>& Options, FRandomStream& Rng) override;
	virtual bool PassSkillMinigame(const FMatchState& State, int32 Seat, ESkillTrial Trial, ESkillDifficulty Difficulty, FRandomStream& Rng) override;
	virtual int32 ChooseOption(const FMatchState& State, int32 Seat, const FPendingChoice& Choice, FRandomStream& Rng) override;
	virtual double DuelPerformance(const FMatchState& State, int32 Seat, FRandomStream& Rng) override;
	virtual bool RefusesOracleSentence(const FMatchState& State, int32 Seat, FRandomStream& Rng) override;
	virtual int32 ChooseVote(const FMatchState& State, int32 Seat, const TArray<int32>& Options, FRandomStream& Rng) override;
};

/**
 * Dummy de carrera (S-006, nombre propuesto): doble de prueba para la simulación y la vista de depuración, no el bot
 * básico del GDD 08. Si es Eco y la carrera sigue, elige la opción que deja más cerca, en pasos del grafo (sin mirar
 * Sellos ni el Altar), el Nicho Libre más cercano; los empates, al azar. Si es Elegido, la carrera acabó o no queda
 * ningún Nicho Libre al que llegar, elige al azar como FDummyDecider. Siempre con el generador que le pasan. Las
 * elecciones (S-009) y el rendimiento del duelo (S-012), como FDummyDecider.
 */
class SOULBOUNDCORE_API FRaceDummyDecider : public IDecider
{
public:
	virtual int32 ChooseDestination(const FMatchState& State, int32 Seat, const TArray<int32>& Options, FRandomStream& Rng) override;
	virtual bool PassSkillMinigame(const FMatchState& State, int32 Seat, ESkillTrial Trial, ESkillDifficulty Difficulty, FRandomStream& Rng) override;
	virtual int32 ChooseOption(const FMatchState& State, int32 Seat, const FPendingChoice& Choice, FRandomStream& Rng) override;
	virtual double DuelPerformance(const FMatchState& State, int32 Seat, FRandomStream& Rng) override;
	virtual bool RefusesOracleSentence(const FMatchState& State, int32 Seat, FRandomStream& Rng) override;
	virtual int32 ChooseVote(const FMatchState& State, int32 Seat, const TArray<int32>& Options, FRandomStream& Rng) override;
};
