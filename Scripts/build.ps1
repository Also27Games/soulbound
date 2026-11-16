<#
.SYNOPSIS
  Compila y empaqueta Soulbound por línea de comandos, sin abrir el editor.

.DESCRIPTION
  Uso (PowerShell, desde cualquier carpeta):
    .\Scripts\build.ps1                 # editor + juego + empaquetado Win64
    .\Scripts\build.ps1 -Que editor     # solo el editor (Development Editor)
    .\Scripts\build.ps1 -Que game       # solo el ejecutable del juego
    .\Scripts\build.ps1 -Que package    # solo empaquetar (cook + pak) en Packaged\Windows
  El paquete lleva el instalador de requisitos de Unreal (-prereqs: Visual C++ y demás), para que arranque en un PC que no los tenga.
  Motor: variable de entorno UE_ROOT o, si no existe, C:\UE_Engines\UE_5.8.
  Imprime el tiempo de cada paso; falla al primer error.
#>
param(
    [ValidateSet('editor', 'game', 'package', 'all')]
    [string]$Que = 'all',
    [string]$Engine = $env:UE_ROOT,
    [ValidateSet('Debug', 'DebugGame', 'Development', 'Test', 'Shipping')]
    [string]$Config = 'Development'
)

$ErrorActionPreference = 'Stop'
if (-not $Engine) { $Engine = 'C:\UE_Engines\UE_5.8' }

$Raiz = Split-Path -Parent $PSScriptRoot
$Proyecto = Join-Path $Raiz 'Soulbound.uproject'
$BuildBat = Join-Path $Engine 'Engine\Build\BatchFiles\Build.bat'
$RunUAT = Join-Path $Engine 'Engine\Build\BatchFiles\RunUAT.bat'
$Salida = Join-Path $Raiz 'Packaged'

foreach ($Ruta in @($Proyecto, $BuildBat, $RunUAT)) {
    if (-not (Test-Path $Ruta)) { throw "No existe: $Ruta" }
}

function Invoke-Paso {
    param([string]$Nombre, [string]$Exe, [string[]]$Argumentos)
    Write-Host "== $Nombre" -ForegroundColor Cyan
    $Tiempo = Measure-Command {
        & $Exe @Argumentos | Out-Host
        if ($LASTEXITCODE -ne 0) { throw "$Nombre ha fallado (codigo $LASTEXITCODE)" }
    }
    Write-Host ("== {0}: {1:n0} s" -f $Nombre, $Tiempo.TotalSeconds) -ForegroundColor Green
}

if ($Que -in 'editor', 'all') {
    Invoke-Paso 'Compilar editor' $BuildBat @('SoulboundEditor', 'Win64', $Config, "-Project=$Proyecto", '-WaitMutex')
}
if ($Que -in 'game', 'all') {
    Invoke-Paso 'Compilar juego' $BuildBat @('Soulbound', 'Win64', $Config, "-Project=$Proyecto", '-WaitMutex')
}
if ($Que -in 'package', 'all') {
    Invoke-Paso 'Empaquetar Win64' $RunUAT @(
        'BuildCookRun', "-project=$Proyecto", '-platform=Win64', "-clientconfig=$Config",
        '-build', '-cook', '-stage', '-pak', '-prereqs', '-archive', "-archivedirectory=$Salida",
        '-unattended', '-utf8output', '-nop4'
    )
    Write-Host "Build en $Salida\Windows" -ForegroundColor Green
}
