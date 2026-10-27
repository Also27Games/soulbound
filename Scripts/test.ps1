<#
.SYNOPSIS
  Lanza los tests de automatización de Soulbound por línea de comandos, sin abrir la ventana del editor.

.DESCRIPTION
  Uso:
    .\Scripts\test.ps1                      # todos los tests cuyo nombre empieza por "Soulbound."
    .\Scripts\test.ps1 -Filtro Soulbound.Core
    .\Scripts\test.ps1 -Cronica             # además imprime la crónica de una partida simulada
    .\Scripts\test.ps1 -Cronica -Filtro Soulbound.Core.Simulation.CronicaDeUnaPartida   # solo la crónica
    .\Scripts\test.ps1 -Informe -Filtro Soulbound.Core.Simulation.InformeDeCarrera      # informe de 50 carreras
  -Cronica pone la variable de entorno SOULBOUND_CRONICA=1 mientras corre el comandlet y al terminar la deja como
  estaba. Con ella, Soulbound.Core.Simulation.CronicaDeUnaPartida imprime una partida entera turno a turno (quién
  juega, qué tira, adónde va, qué hace el Warden) en las líneas "info"; sin ella, solo una línea de resumen.
  El test comprueba que la crónica está completa en los dos casos.
  -Informe pone SOULBOUND_INFORME=1 del mismo modo: Soulbound.Core.Simulation.InformeDeCarrera imprime, por cada
  tablero de Content/Data/Boards y sobre 50 partidas con dummies de carrera, el % de partidas sin ningún vínculo, los
  Elegidos por partida de media, el % de asientos que quedan Eco y el segundo medio de fin de carrera (S-006), las runas
  y Fuentes (S-008), el % de tiradas con dos dados y los saltos planares y expulsiones por partida (S-009), los robos y
  las runas que cambian de mano (S-010), y los duelos por partida y cuántos gana el retado (S-012). Desde S-018, un bloque
  por tablero y por ritmo de la simulación (rápido y humano), que empieza con el resultado de las partidas, el minuto medio de
  la victoria, los desafíos finales, los aspirantes que no llegan a desafiar y las Expulsiones desde el Altar. Son cifras
  para el autor; el test no las juzga, salvo que cada bloque las imprime y que toda partida termina (CA-SIM-2).
  Arranca el editor en modo comandlet (-nullrhi, sin ventana), ejecuta los tests y resume el resultado.
  Devuelve código 1 si algún test falla o si no se ejecutó ninguno. Compila antes con Scripts\build.ps1 -Que editor.
  Motor: variable de entorno UE_ROOT o, si no existe, C:\UE_Engines\UE_5.8.
#>
param(
    [string]$Filtro = 'Soulbound.',
    [string]$Engine = $env:UE_ROOT,
    [switch]$Cronica,
    [switch]$Informe
)

$ErrorActionPreference = 'Stop'
if (-not $Engine) { $Engine = 'C:\UE_Engines\UE_5.8' }

$Raiz = Split-Path -Parent $PSScriptRoot
$Proyecto = Join-Path $Raiz 'Soulbound.uproject'
$EditorCmd = Join-Path $Engine 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$RutaInforme = Join-Path $Raiz 'Saved\Automation\Informe'

foreach ($Ruta in @($Proyecto, $EditorCmd)) {
    if (-not (Test-Path $Ruta)) { throw "No existe: $Ruta" }
}

Write-Host "== Tests '$Filtro' (comandlet, sin ventana)" -ForegroundColor Cyan
# $env: vive en la sesión de PowerShell que llama al script: se restaura al terminar para que -Cronica no se herede.
$CronicaPrevia = $env:SOULBOUND_CRONICA
$InformePrevio = $env:SOULBOUND_INFORME
if ($Cronica) { $env:SOULBOUND_CRONICA = '1' }
if ($Informe) { $env:SOULBOUND_INFORME = '1' }
try {
    $Tiempo = Measure-Command {
        $script:Salida = & $EditorCmd $Proyecto `
            "-ExecCmds=Automation RunTests $Filtro; Quit" `
            '-unattended' '-nopause' '-nullrhi' '-nosplash' '-nosound' '-log' '-stdout' '-FullStdOutLogOutput' `
            "-ReportExportPath=$RutaInforme" 2>&1 | ForEach-Object { "$_" }
    }
}
finally {
    $env:SOULBOUND_CRONICA = $CronicaPrevia
    $env:SOULBOUND_INFORME = $InformePrevio
}

$Lineas = $script:Salida | Where-Object { $_ -match 'LogAutomationController' -or $_ -match 'LogAutomationCommandLine' }
# Unreal 5.8 escribe Result={Success}/{Fail}; versiones anteriores, {Passed}/{Failed}.
$Superados = @($Lineas | Where-Object { $_ -match 'Test Completed\. Result=\{(Success|Passed)\}' })
$Fallidos = @($Lineas | Where-Object { $_ -match 'Test Completed\. Result=\{(Fail|Failed)\}' })
$Errores = @($script:Salida | Where-Object { $_ -match 'LogAutomationController: Error' })

$Superados | ForEach-Object { Write-Host ("  OK   " + ($_ -replace '.*Name=\{', '' -replace '\}.*', '')) -ForegroundColor Green }
$Fallidos | ForEach-Object { Write-Host ("  FALLO " + ($_ -replace '.*Name=\{', '' -replace '\}.*', '')) -ForegroundColor Red }
$Errores | ForEach-Object { Write-Host "  $_" -ForegroundColor Red }
$script:Salida | Where-Object { $_ -match 'LogSoulboundTest' } | ForEach-Object { Write-Host ("  info  " + ($_ -replace '.*LogSoulboundTest: (Display: )?', '')) -ForegroundColor Gray }

Write-Host ("== {0} superados, {1} fallidos, {2:n0} s (arranque del comandlet incluido)" -f $Superados.Count, $Fallidos.Count, $Tiempo.TotalSeconds) -ForegroundColor Cyan

if ($Fallidos.Count -gt 0 -or $Superados.Count -eq 0) {
    if ($Superados.Count -eq 0 -and $Fallidos.Count -eq 0) {
        Write-Host "No se ejecuto ningun test. Ultimas lineas de la salida:" -ForegroundColor Yellow
        $script:Salida | Select-Object -Last 30 | ForEach-Object { Write-Host "  $_" }
    }
    exit 1
}
exit 0
