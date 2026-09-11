param(
    [Parameter(Position = 0)]
    [string]$Action = "help",

    [Parameter(Position = 1, ValueFromRemainingArguments = $true)]
    [string[]]$Rest
)

$ErrorActionPreference = "Stop"

$Root = $PSScriptRoot
$ConfigPath = Join-Path $Root "config.json"
$ProfilePath = Join-Path $Root "PROFILE.md"
$DefaultHost = "127.0.0.1"
$DefaultPorts = @(8765, 8888)
$TimeoutMs = 2500

$ClassNames = @{
    1 = "Warrior"
    2 = "Paladin"
    3 = "Hunter"
    4 = "Rogue"
    5 = "Priest"
    7 = "Shaman"
    8 = "Mage"
    9 = "Warlock"
    11 = "Druid"
}

function Convert-ToOneLine([string]$Text) {
    if ($null -eq $Text) { return "" }
    return (($Text -replace "`r", " ") -replace "`n", " ").Trim()
}

function Send-BridgeRequest {
    param(
        [string]$HostName,
        [int]$Port,
        [string]$Command,
        [string]$Selector = "0"
    )

    $Command = Convert-ToOneLine $Command
    $Selector = Convert-ToOneLine $Selector

    $client = New-Object System.Net.Sockets.TcpClient
    try {
        $async = $client.BeginConnect($HostName, $Port, $null, $null)
        if (-not $async.AsyncWaitHandle.WaitOne($TimeoutMs, $false)) {
            $client.Close()
            throw "Timed out connecting to ${HostName}:${Port}"
        }
        $client.EndConnect($async)
        $client.ReceiveTimeout = $TimeoutMs
        $client.SendTimeout = $TimeoutMs

        $stream = $client.GetStream()
        $utf8 = New-Object System.Text.UTF8Encoding($false)
        $writer = New-Object System.IO.StreamWriter($stream, $utf8, 1024, $true)
        $reader = New-Object System.IO.StreamReader($stream, $utf8, $false, 1024, $true)

        $writer.NewLine = "`n"
        $writer.WriteLine("${Command},${Selector}")
        $writer.Flush()

        $response = $reader.ReadLine()
        if ($null -eq $response) {
            throw "Bridge closed the connection without a response."
        }
        return $response
    }
    finally {
        if ($null -ne $client) {
            $client.Close()
        }
    }
}

function Get-Config {
    if (-not (Test-Path $ConfigPath)) {
        throw "Not configured. Double-click SETUP-WINDOWS.bat first."
    }
    return (Get-Content -Raw -Path $ConfigPath | ConvertFrom-Json)
}

function Convert-Snapshot {
    param([string]$Raw)

    $obj = [ordered]@{}
    foreach ($field in ($Raw -split "`t")) {
        $idx = $field.IndexOf("=")
        if ($idx -lt 1) { continue }
        $key = $field.Substring(0, $idx)
        $value = $field.Substring($idx + 1)
        $obj[$key] = $value
    }
    return [pscustomobject]$obj
}

function Convert-BotList {
    param([string]$Raw)

    $bots = @()
    if ([string]::IsNullOrWhiteSpace($Raw)) {
        return $bots
    }

    foreach ($entry in ($Raw -split ";")) {
        $parts = $entry -split "\|", 5
        if ($parts.Count -lt 5) { continue }

        $classId = 0
        [void][int]::TryParse($parts[3], [ref]$classId)
        $className = if ($ClassNames.ContainsKey($classId)) { $ClassNames[$classId] } else { "Class $classId" }

        $bots += [pscustomobject]@{
            guid = $parts[0]
            name = $parts[1]
            level = $parts[2]
            class_id = $classId
            class = $className
            master = $parts[4]
        }
    }

    return $bots
}

function Find-Bridge {
    foreach ($port in $DefaultPorts) {
        try {
            $version = Send-BridgeRequest -HostName $DefaultHost -Port $port -Command "bridge-version" -Selector "0"
            if ($version.StartsWith("playerbot-companion-bridge/")) {
                return [pscustomobject]@{ host = $DefaultHost; port = $port; version = $version }
            }
        }
        catch {
        }
    }

    $rawPort = Read-Host "Bridge port [$($DefaultPorts[0])]"
    if ([string]::IsNullOrWhiteSpace($rawPort)) {
        $port = $DefaultPorts[0]
    } else {
        $port = [int]$rawPort
    }

    $version = Send-BridgeRequest -HostName $DefaultHost -Port $port -Command "bridge-version" -Selector "0"
    if (-not $version.StartsWith("playerbot-companion-bridge/")) {
        throw "Unexpected bridge response: $version"
    }

    return [pscustomobject]@{ host = $DefaultHost; port = $port; version = $version }
}

function Update-ProfileName {
    param([string]$Name)

    if (-not (Test-Path $ProfilePath)) { return }

    $text = Get-Content -Raw -Path $ProfilePath
    $placeholder = 'Name: (set during setup)'
    $text = $text.Replace($placeholder, "Name: $Name")
    Set-Content -Path $ProfilePath -Value $text -Encoding UTF8
}

function Run-Setup {
    Write-Host ""
    Write-Host "Playerbot Companion Bridge setup"
    Write-Host "--------------------------------"

    try {
        $bridge = Find-Bridge
    }
    catch {
        Write-Host ""
        Write-Host "Could not reach the companion bridge:"
        Write-Host "  $($_.Exception.Message)"
        Write-Host ""
        Write-Host "In the ACTIVE aiplayerbot.conf set:"
        Write-Host "  AiPlayerbot.CommandServerPort = $($DefaultPorts[0])"
        Write-Host ""
        Write-Host "Then restart mangosd and run SETUP-WINDOWS.bat again."
        exit 1
    }

    Write-Host "Connected to $($bridge.host):$($bridge.port) ($($bridge.version))"

    $raw = Send-BridgeRequest -HostName $bridge.host -Port $bridge.port -Command "list" -Selector "0"
    $bots = @(Convert-BotList $raw)

    if ($bots.Count -eq 0) {
        Write-Host ""
        Write-Host "No online playerbots were found."
        Write-Host "Log/summon the bot you want ChatGPT to use, then run setup again."
        exit 1
    }

    Write-Host ""
    Write-Host "Online playerbots:"
    for ($i = 0; $i -lt $bots.Count; $i++) {
        $bot = $bots[$i]
        $master = if ([string]::IsNullOrWhiteSpace($bot.master)) { "no active master" } else { $bot.master }
        Write-Host ("  {0}. {1} - level {2} {3} (master: {4})" -f ($i + 1), $bot.name, $bot.level, $bot.class, $master)
    }

    Write-Host ""
    while ($true) {
        $choiceRaw = Read-Host "Choose companion number [1]"
        if ([string]::IsNullOrWhiteSpace($choiceRaw)) { $choiceRaw = "1" }

        $choice = 0
        if ([int]::TryParse($choiceRaw, [ref]$choice)) {
            if ($choice -ge 1 -and $choice -le $bots.Count) {
                break
            }
        }
        Write-Host "Please choose one of the listed numbers."
    }

    $chosen = $bots[$choice - 1]
    $config = [ordered]@{
        host = $bridge.host
        port = [int]$bridge.port
        selector = [string]$chosen.guid
        bot_name = [string]$chosen.name
    }

    ($config | ConvertTo-Json) | Set-Content -Path $ConfigPath -Encoding UTF8
    Update-ProfileName $chosen.name

    Write-Host ""
    Write-Host "Selected $($chosen.name) (GUID $($chosen.guid))."
    Write-Host "Saved config.json."
    Write-Host ""

    $snapshotRaw = Send-BridgeRequest -HostName $bridge.host -Port $bridge.port -Command "snapshot" -Selector $chosen.guid
    Convert-Snapshot $snapshotRaw | ConvertTo-Json | Write-Host

    Write-Host ""
    Write-Host "Setup complete."
    Write-Host ""
    Write-Host "Open THIS FOLDER in ChatGPT Desktop -> Codex and say:"
    Write-Host '  "Join me in WoW and control my companion."'
    Write-Host ""
}

switch ($Action.ToLowerInvariant()) {
    "setup" {
        Run-Setup
    }

    "snapshot" {
        $cfg = Get-Config
        $raw = Send-BridgeRequest -HostName $cfg.host -Port ([int]$cfg.port) -Command "snapshot" -Selector ([string]$cfg.selector)
        Convert-Snapshot $raw | ConvertTo-Json
    }

    "list" {
        $cfg = Get-Config
        $raw = Send-BridgeRequest -HostName $cfg.host -Port ([int]$cfg.port) -Command "list" -Selector "0"
        @(Convert-BotList $raw) | ConvertTo-Json
    }

    "test" {
        $cfg = Get-Config
        $version = Send-BridgeRequest -HostName $cfg.host -Port ([int]$cfg.port) -Command "bridge-version" -Selector "0"
        $raw = Send-BridgeRequest -HostName $cfg.host -Port ([int]$cfg.port) -Command "snapshot" -Selector ([string]$cfg.selector)
        $snap = Convert-Snapshot $raw
        Write-Host "bridge=$version"
        Write-Host "bot=$($snap.name)"
        Write-Host "status=ok"
    }

    "cmd" {
        $cfg = Get-Config
        $text = Convert-ToOneLine ($Rest -join " ")
        if ([string]::IsNullOrWhiteSpace($text)) { throw "Usage: bot.bat cmd <playerbot command>" }
        Send-BridgeRequest -HostName $cfg.host -Port ([int]$cfg.port) -Command ("cmd:" + $text) -Selector ([string]$cfg.selector)
    }

    "party" {
        $cfg = Get-Config
        $text = Convert-ToOneLine ($Rest -join " ")
        if ([string]::IsNullOrWhiteSpace($text)) { throw "Usage: bot.bat party <message>" }
        Send-BridgeRequest -HostName $cfg.host -Port ([int]$cfg.port) -Command ("party:" + $text) -Selector ([string]$cfg.selector)
    }

    "say" {
        $cfg = Get-Config
        $text = Convert-ToOneLine ($Rest -join " ")
        if ([string]::IsNullOrWhiteSpace($text)) { throw "Usage: bot.bat say <message>" }
        Send-BridgeRequest -HostName $cfg.host -Port ([int]$cfg.port) -Command ("say:" + $text) -Selector ([string]$cfg.selector)
    }

    "raw" {
        $cfg = Get-Config
        $text = Convert-ToOneLine ($Rest -join " ")
        if ([string]::IsNullOrWhiteSpace($text)) { throw "Usage: bot.bat raw <remote command>" }
        Send-BridgeRequest -HostName $cfg.host -Port ([int]$cfg.port) -Command $text -Selector ([string]$cfg.selector)
    }

    default {
        Write-Host "Playerbot Companion Bridge"
        Write-Host ""
        Write-Host "  SETUP-WINDOWS.bat"
        Write-Host "  bot.bat snapshot"
        Write-Host "  bot.bat cmd `"follow`""
        Write-Host "  bot.bat party `"Hello!`""
        Write-Host "  bot.bat say `"Hello!`""
        Write-Host "  bot.bat list"
        Write-Host "  bot.bat test"
    }
}
