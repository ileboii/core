param(
    [string]$RepoPath = ""
)

$ErrorActionPreference = "Stop"

function Resolve-RepoPath([string]$Requested) {
    if (-not [string]::IsNullOrWhiteSpace($Requested)) {
        return (Resolve-Path $Requested).Path
    }
    if (Test-Path "C:\VMaNGOS-core\src\game\PlayerBots\playerbot\PlayerbotAI.cpp") {
        return "C:\VMaNGOS-core"
    }
    $typed = Read-Host "VMaNGOS source folder"
    return (Resolve-Path $typed).Path
}

try {
    $RepoPath = Resolve-RepoPath $RepoPath
    $pointer = Join-Path $RepoPath ".playerbot-companion-last-backup.txt"
    if (-not (Test-Path $pointer)) {
        throw "No Playerbot Companion Bridge backup pointer was found."
    }

    $backupRoot = ([System.IO.File]::ReadAllText($pointer)).Trim()
    if (-not (Test-Path $backupRoot)) {
        throw "Backup folder no longer exists: $backupRoot"
    }

    $files = @(
        "src\game\PlayerBots\playerbot\PlayerbotCommandServer.cpp",
        "src\game\PlayerBots\playerbot\RandomPlayerbotMgr.cpp",
        "src\game\PlayerBots\playerbot\PlayerbotAI.cpp"
    )

    Write-Host "This restores the exact files saved immediately before the bridge installer ran."
    Write-Host "Any edits made to those three files AFTER installation would be overwritten."
    $answer = Read-Host "Type RESTORE to continue"
    if ($answer -ne "RESTORE") {
        Write-Host "Cancelled."
        exit 0
    }

    foreach ($rel in $files) {
        $src = Join-Path $backupRoot $rel
        $dst = Join-Path $RepoPath $rel
        if (-not (Test-Path $src)) {
            throw "Missing backup file: $src"
        }
        Copy-Item -LiteralPath $src -Destination $dst -Force
    }

    Remove-Item $pointer -Force
    Write-Host "Pre-install source files restored."
}
catch {
    Write-Host $_.Exception.Message -ForegroundColor Red
    exit 1
}
