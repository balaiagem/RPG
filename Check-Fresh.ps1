<#
  Diz se o jogo compilado corresponde ao codigo-fonte atual.

  Existe por causa de 26/09/2026: um build falhou com um erro de uma linha, a
  Unreal continuou carregando a DLL de treze horas antes sem avisar, e um dia
  inteiro de bugs foi diagnosticado num jogo que nao era o jogo escrito.

  Codigos de saida:  0 = em dia   1 = DLL mais velha que o codigo   2 = sem DLL
#>
param([Parameter(Mandatory = $true)][string]$Root)

$dll = Join-Path $Root 'Binaries\Win64\UnrealEditor-AshenHollow.dll'
$binary = Get-Item -LiteralPath $dll -ErrorAction SilentlyContinue
if (-not $binary) {
    Write-Host '  O jogo nunca foi compilado (nao existe UnrealEditor-AshenHollow.dll).'
    exit 2
}

$newest = Get-ChildItem -LiteralPath (Join-Path $Root 'Source') -Recurse `
    -Include *.cpp, *.h -ErrorAction SilentlyContinue |
    Sort-Object LastWriteTime -Descending | Select-Object -First 1

if ($newest -and $newest.LastWriteTime -gt $binary.LastWriteTime) {
    Write-Host ''
    Write-Host '  ================================================================'
    Write-Host '   O JOGO COMPILADO ESTA MAIS VELHO QUE O CODIGO.'
    Write-Host ''
    Write-Host ('   DLL do jogo   : ' + $binary.LastWriteTime.ToString('dd/MM HH:mm'))
    Write-Host ('   Codigo mais novo: ' + $newest.Name + '  ' + $newest.LastWriteTime.ToString('dd/MM HH:mm'))
    Write-Host ''
    Write-Host '   Abrir agora mostra a versao ANTIGA do jogo -- e nenhuma'
    Write-Host '   mudanca recente vai aparecer. Rode Build-Now.bat primeiro.'
    Write-Host '  ================================================================'
    Write-Host ''
    exit 1
}

exit 0
