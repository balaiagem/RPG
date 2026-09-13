$ErrorActionPreference = 'Stop'
$projectFolder = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$workspaceFolder = [IO.Path]::GetFullPath((Join-Path $projectFolder '..\..'))
$descriptor = Get-Content -LiteralPath (Join-Path $projectFolder 'AshenHollow.uproject') -Raw | ConvertFrom-Json
if ($descriptor.EngineAssociation -ne '5.8') { throw 'Unexpected engine version.' }
$classes = Get-Content -LiteralPath (Join-Path $projectFolder 'Content\Data\classes_5e.json') -Raw | ConvertFrom-Json
$abilities = Get-Content -LiteralPath (Join-Path $projectFolder 'Content\Data\abilities_5e.json') -Raw | ConvertFrom-Json
$ancestries = Get-Content -LiteralPath (Join-Path $projectFolder 'Content\Data\ancestries.json') -Raw | ConvertFrom-Json
if (@($classes.PSObject.Properties).Count -ne 12) { throw 'Expected 12 classes.' }
if (@($ancestries.PSObject.Properties).Count -ne 7) { throw 'Expected 7 ancestries.' }
foreach ($classEntry in $classes.PSObject.Properties) {
    foreach ($abilityId in @($classEntry.Value.abilities) + @($classEntry.Value.spellChoices)) {
        if (-not $abilities.PSObject.Properties[$abilityId]) { throw "Missing ability: $abilityId" }
    }
}
foreach ($name in @('classes_5e.json', 'abilities_5e.json', 'ancestries.json')) {
    $source = Join-Path $workspaceFolder "data\$name"
    $target = Join-Path $projectFolder "Content\Data\$name"
    if ((Get-FileHash -LiteralPath $source).Hash -ne (Get-FileHash -LiteralPath $target).Hash) {
        throw "Migration snapshot differs from the validated Godot catalog: $name"
    }
}
Write-Output 'PASS: UE 5.8 descriptor, 12 classes, 7 ancestries, all ability references, byte-identical data snapshots.'
Write-Output 'This validates files and data only. It does NOT compile Unreal C++ or validate gameplay.'
