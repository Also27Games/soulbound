<#
.SYNOPSIS
  Lanza los tests de automatización de Soulbound por línea de comandos, sin abrir la ventana del editor.

.DESCRIPTION
  Uso:
    .\Scripts\test.ps1                      # todos los tests cuyo nombre empieza por "Soulbound."
    .\Scripts\test.ps1 -Filtro Soulbound.Core
  Arranca el editor en modo comandlet (-nullrhi, sin ventana), ejecuta los tests y resume el resultado.
  Devuelve código 1 si algún test falla o si no se ejecutó ninguno. Compila antes con Scripts\build.ps1 -Que editor.
  Motor: variable de entorno UE_ROOT o, si no existe, C:\UE_Engines\UE_5.8.
#>
param(
    [string]$Filtro = 'Soulbound.',
    [string]$Engine = $env:UE_ROOT
)

$ErrorActionPreference = 'Stop'
if (-not $Engine) { $Engine = 'C:\UE_Engines\UE_5.8' }

$Raiz = Split-Path -Parent $PSScriptRoot
$Proyecto = Join-Path $Raiz 'Soulbound.uproject'
$EditorCmd = Join-Path $Engine 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$Informe = Join-Path $Raiz 'Saved\Automation\Informe'

foreach ($Ruta in @($Proyecto, $EditorCmd)) {
    if (-not (Test-Path $Ruta)) { throw "No existe: $Ruta" }
}

Write-Host "== Tests '$Filtro' (comandlet, sin ventana)" -ForegroundColor Cyan
$Tiempo = Measure-Command {
    $script:Salida = & $EditorCmd $Proyecto `
        "-ExecCmds=Automation RunTests $Filtro; Quit" `
        '-unattended' '-nopause' '-nullrhi' '-nosplash' '-nosound' '-log' '-stdout' '-FullStdOutLogOutput' `
        "-ReportExportPath=$Informe" 2>&1 | ForEach-Object { "$_" }
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
