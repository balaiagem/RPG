param(
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8',
    [ValidateRange(1, 64)][int]$MaxParallelActions = 2,
    [switch]$CleanRebuild,
    [switch]$SkipShadowScan
)
$ErrorActionPreference = 'Stop'
$projectFile = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\AshenHollow.uproject'))
$buildFile = Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat'
$versionFile = Join-Path $EngineRoot 'Engine\Build\Build.version'
if (-not (Test-Path -LiteralPath $buildFile) -or -not (Test-Path -LiteralPath $versionFile)) {
    Write-Error 'Unreal 5.8 nao encontrado. Instale o motor ou informe -EngineRoot com a pasta correta.'
    exit 2
}
$engineVersion = Get-Content -LiteralPath $versionFile -Raw | ConvertFrom-Json
if ($engineVersion.MajorVersion -ne 5 -or $engineVersion.MinorVersion -ne 8) {
    Write-Error 'Este projeto esta fixado no Unreal 5.8. Selecione essa instalacao.'
    exit 3
}
# Sombreamento de variavel (C4456) e erro de compilacao neste projeto e ja custou
# duas rodadas, entao vale varrer antes -- leva menos de um segundo.
#
# Duas regras aprendidas na marra aqui. A primeira: usar o Python que o Unreal ja
# traz, e nao o do sistema. Em Windows sem Python instalado, "python" resolve para
# o atalho da Microsoft Store, que existe como comando e nao executa nada -- entao
# testar se o comando existe nao prova nada; so rodar prova.
#
# A segunda, e mais importante: um passo de conferencia **nunca** derruba o build
# por problema dele mesmo. Se o interpretador nao aparece, ou a varredura quebra,
# isso vira um aviso e a compilacao segue. So achar sombreamento de verdade para,
# e mesmo assim so porque o compilador pararia dois minutos depois de qualquer jeito.
$python = $null
$candidates = if ($SkipShadowScan) { @() } else { @(
    (Join-Path $EngineRoot 'Engine\Binaries\ThirdParty\Python3\Win64\python.exe'),
    'py',
    'python3'
) }
foreach ($candidate in $candidates) {
    try {
        $probe = & $candidate '--version' 2>&1
        if ($LASTEXITCODE -eq 0 -and "$probe" -match 'Python 3') { $python = $candidate; break }
    } catch { }
}

$shadowScript = Join-Path $PSScriptRoot 'Find-Shadowing.py'
if ($SkipShadowScan) {
    Write-Host 'Varredura de sombreamento desligada por -SkipShadowScan.' -ForegroundColor DarkGray
} elseif (-not $python) {
    Write-Host 'Sem Python utilizavel; pulando a varredura de sombreamento.' -ForegroundColor DarkGray
} elseif (-not (Test-Path -LiteralPath $shadowScript)) {
    Write-Host 'Find-Shadowing.py nao encontrado; pulando a varredura.' -ForegroundColor DarkGray
} else {
    try {
        $sources = Get-ChildItem -Path (Join-Path $PSScriptRoot '..\Source') -Recurse -Filter *.cpp |
                   ForEach-Object { $_.FullName }
        $shadow = & $python $shadowScript @sources 2>&1
        if ("$shadow" -match 'total: 0') {
            Write-Host 'Sem variaveis sombreadas.' -ForegroundColor Green
        } elseif ("$shadow" -match 'total: \d+') {
            $shadow | ForEach-Object { Write-Warning $_ }
            throw 'Variaveis sombreadas encontradas (C4456). Corrija antes de compilar.'
        } else {
            Write-Host 'A varredura de sombreamento nao concluiu; seguindo assim mesmo.' -ForegroundColor DarkGray
        }
    } catch {
        if ($_.Exception.Message -like '*sombreadas*') { throw }
        Write-Host "Varredura de sombreamento falhou ($($_.Exception.Message)); seguindo assim mesmo." -ForegroundColor DarkGray
    }
}

$extraBuildArgs = @('-gather')
if ($CleanRebuild) { $extraBuildArgs += @('-Rebuild', '-NoUBA') }
& $buildFile 'AshenHollowEditor' 'Win64' 'Development' "-Project=$projectFile" '-WaitMutex' "-MaxParallelActions=$MaxParallelActions" @extraBuildArgs
exit $LASTEXITCODE
