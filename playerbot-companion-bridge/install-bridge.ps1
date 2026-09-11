param(
    [string]$RepoPath = ""
)

$ErrorActionPreference = "Stop"

function Write-Utf8NoBom([string]$Path, [string]$Text) {
    $utf8 = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllText($Path, $Text, $utf8)
}

function Read-AllText([string]$Path) {
    return [System.IO.File]::ReadAllText($Path)
}

function To-FileNewlines([string]$Block, [string]$ExistingText) {
    $normalized = $Block -replace "`r`n", "`n"
    if ($ExistingText.Contains("`r`n")) {
        return ($normalized -replace "`n", "`r`n")
    }
    return $normalized
}

function Insert-AfterFunctionOpen(
    [string]$Text,
    [string]$Signature,
    [string]$Block,
    [string]$Marker
) {
    if ($Text.Contains($Marker)) {
        return $Text
    }

    $sigPos = $Text.IndexOf($Signature, [System.StringComparison]::Ordinal)
    if ($sigPos -lt 0) {
        throw "Could not find function signature: $Signature"
    }

    $bracePos = $Text.IndexOf("{", $sigPos + $Signature.Length)
    if ($bracePos -lt 0) {
        throw "Could not find opening brace for: $Signature"
    }

    $blockText = To-FileNewlines $Block $Text
    return $Text.Insert($bracePos + 1, $blockText)
}

function Insert-AfterAnchorInFunction(
    [string]$Text,
    [string]$FunctionSignature,
    [string]$NextFunctionSignature,
    [string]$Anchor,
    [string]$Block,
    [string]$Marker
) {
    if ($Text.Contains($Marker)) {
        return $Text
    }

    $funcPos = $Text.IndexOf($FunctionSignature, [System.StringComparison]::Ordinal)
    if ($funcPos -lt 0) {
        throw "Could not find function signature: $FunctionSignature"
    }

    $endPos = $Text.IndexOf($NextFunctionSignature, $funcPos + $FunctionSignature.Length, [System.StringComparison]::Ordinal)
    if ($endPos -lt 0) {
        throw "Could not find the end anchor for: $FunctionSignature"
    }

    $anchorPos = $Text.IndexOf($Anchor, $funcPos, $endPos - $funcPos, [System.StringComparison]::Ordinal)
    if ($anchorPos -lt 0) {
        throw "Could not find expected anchor inside $FunctionSignature : $Anchor"
    }

    $insertPos = $anchorPos + $Anchor.Length
    $blockText = To-FileNewlines $Block $Text
    return $Text.Insert($insertPos, $blockText)
}

function Resolve-RepoPath([string]$Requested) {
    if (-not [string]::IsNullOrWhiteSpace($Requested)) {
        return (Resolve-Path $Requested).Path
    }

    $current = (Get-Location).Path
    $probe = Join-Path $current "src\game\PlayerBots\playerbot\PlayerbotAI.cpp"
    if (Test-Path $probe) {
        return $current
    }

    $common = "C:\VMaNGOS-core"
    $probe = Join-Path $common "src\game\PlayerBots\playerbot\PlayerbotAI.cpp"
    if (Test-Path $probe) {
        return $common
    }

    $typed = Read-Host "VMaNGOS source folder (example: C:\VMaNGOS-core)"
    if ([string]::IsNullOrWhiteSpace($typed)) {
        throw "No VMaNGOS source folder was provided."
    }
    return (Resolve-Path $typed).Path
}

$backupRoot = ""
$filesWritten = $false

try {
    $RepoPath = Resolve-RepoPath $RepoPath

    $commandRel = "src\game\PlayerBots\playerbot\PlayerbotCommandServer.cpp"
    $randomRel  = "src\game\PlayerBots\playerbot\RandomPlayerbotMgr.cpp"
    $randomHdrRel = "src\game\PlayerBots\playerbot\RandomPlayerbotMgr.h"
    $aiRel      = "src\game\PlayerBots\playerbot\PlayerbotAI.cpp"

    $commandPath = Join-Path $RepoPath $commandRel
    $randomPath = Join-Path $RepoPath $randomRel
    $randomHdrPath = Join-Path $RepoPath $randomHdrRel
    $aiPath = Join-Path $RepoPath $aiRel

    foreach ($path in @($commandPath, $randomPath, $randomHdrPath, $aiPath)) {
        if (-not (Test-Path $path)) {
            throw "Required source file not found: $path"
        }
    }

    $commandText = Read-AllText $commandPath
    $randomText = Read-AllText $randomPath
    $randomHdrText = Read-AllText $randomHdrPath
    $aiText = Read-AllText $aiPath

    if (-not $randomHdrText.Contains("GetPlayersSnapshot()")) {
        throw @"
This branch does not expose RandomPlayerbotMgr::GetPlayersSnapshot().
The installer is refusing to use an unsafe cross-thread player map fallback.
Please use a bridge adapter for this fork/version.
"@
    }

    # --- PlayerbotCommandServer.cpp: localhost-only bind ---
    $bindMarker = "// PBCB LOCALHOST BIND"
    if (-not $commandText.Contains($bindMarker)) {
        $oldBind = "addr.sin_addr.s_addr = INADDR_ANY;"
        if ($commandText.Contains($oldBind)) {
            $replacement = @'
// PBCB LOCALHOST BIND
    // The companion bridge is a local control surface. Do not expose it by default.
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
'@
            $replacement = To-FileNewlines $replacement $commandText
            $commandText = $commandText.Replace("    " + $oldBind, "    " + $replacement.TrimEnd("`r","`n"))
        }
        elseif (-not $commandText.Contains("INADDR_LOOPBACK")) {
            throw "Could not find the command-server bind address."
        }
    }

    # --- RandomPlayerbotMgr.cpp: bridge router injected without replacing legacy handler ---
    $routerMarker = "// PBCB BEGIN REMOTE ROUTER"
    $routerBlock = @'

    // PBCB BEGIN REMOTE ROUTER
    // Handle bridge traffic before the legacy first-comma parser below.
    {
        size_t bridgePos = request.rfind(',');
        if (bridgePos != std::string::npos)
        {
            std::string bridgeCommand = request.substr(0, bridgePos);
            std::string bridgeSelector = request.substr(bridgePos + 1);

            if (bridgeCommand == "bridge-version")
                return "playerbot-companion-bridge/2";

            if (bridgeCommand == "list")
            {
                std::map<uint32, Player*> bridgeBots;

                ForEachPlayerbot([&bridgeBots](Player* candidate)
                {
                    if (!candidate)
                        return;

                    PlayerbotAI* candidateAi = candidate->GetPlayerbotAI();
                    if (!candidateAi || candidateAi->IsRealPlayer())
                        return;

                    bridgeBots[candidate->GetGUIDLow()] = candidate;
                });

                PlayerBotMap bridgePlayers = GetPlayersSnapshot();
                for (PlayerBotMap::iterator i = bridgePlayers.begin(); i != bridgePlayers.end(); ++i)
                {
                    Player* candidate = i->second;
                    if (!candidate)
                        continue;

                    PlayerbotAI* candidateAi = candidate->GetPlayerbotAI();
                    if (!candidateAi || candidateAi->IsRealPlayer())
                        continue;

                    bridgeBots[candidate->GetGUIDLow()] = candidate;
                }

                std::ostringstream out;
                bool first = true;

                for (std::map<uint32, Player*>::iterator i = bridgeBots.begin(); i != bridgeBots.end(); ++i)
                {
                    Player* candidate = i->second;
                    PlayerbotAI* candidateAi = candidate->GetPlayerbotAI();

                    if (!first)
                        out << ";";
                    first = false;

                    out << candidate->GetGUIDLow() << "|"
                        << candidate->GetName() << "|"
                        << candidate->GetLevel() << "|"
                        << uint32(candidate->GetClass()) << "|";

                    Player* candidateMaster = candidateAi ? candidateAi->GetMaster() : NULL;
                    if (candidateMaster)
                        out << candidateMaster->GetName();
                }

                return out.str();
            }

            bool bridgeForward =
                bridgeCommand == "snapshot" ||
                bridgeCommand.compare(0, 4, "cmd:") == 0 ||
                bridgeCommand.compare(0, 6, "party:") == 0 ||
                bridgeCommand.compare(0, 4, "say:") == 0;

            if (bridgeForward)
            {
                uint32 bridgeGuid = std::atoi(bridgeSelector.c_str());
                Player* bridgeBot = bridgeGuid ? GetPlayerBot(bridgeGuid) : NULL;

                if (!bridgeBot && bridgeGuid)
                    bridgeBot = GetPlayer(bridgeGuid);

                if (!bridgeBot && !bridgeSelector.empty())
                {
                    ForEachPlayerbot([&bridgeBot, &bridgeSelector](Player* candidate)
                    {
                        if (!bridgeBot && candidate && candidate->GetName() == bridgeSelector)
                            bridgeBot = candidate;
                    });

                    if (!bridgeBot)
                    {
                        PlayerBotMap bridgePlayers = GetPlayersSnapshot();
                        for (PlayerBotMap::iterator i = bridgePlayers.begin(); i != bridgePlayers.end(); ++i)
                        {
                            Player* candidate = i->second;
                            if (candidate && candidate->GetName() == bridgeSelector)
                            {
                                bridgeBot = candidate;
                                break;
                            }
                        }
                    }
                }

                if (!bridgeBot)
                    return "bot offline";

                PlayerbotAI* bridgeAi = bridgeBot->GetPlayerbotAI();
                if (!bridgeAi || bridgeAi->IsRealPlayer())
                    return "not a playerbot";

                return bridgeAi->HandleRemoteCommand(bridgeCommand);
            }
        }
    }
    // PBCB END REMOTE ROUTER
'@

    $randomText = Insert-AfterFunctionOpen `
        $randomText `
        "std::string RandomPlayerbotMgr::HandleRemoteCommand(std::string request)" `
        $routerBlock `
        $routerMarker

    # --- PlayerbotAI.cpp: execute queued bridge commands on the AI update path ---
    $queueMarker = "// PBCB BEGIN QUEUED COMMAND EXECUTION"
    $queueBlock = @'

        // PBCB BEGIN QUEUED COMMAND EXECUTION
        // The TCP thread only queues bridge work. Execution happens here.
        static const std::string bridgeCommandPrefix = "__pb_bridge_command ";
        static const std::string bridgePartyPrefix = "__pb_bridge_party ";
        static const std::string bridgeSayPrefix = "__pb_bridge_say ";

        if (command.compare(0, bridgeCommandPrefix.size(), bridgeCommandPrefix) == 0)
        {
            Player* remoteOwner = GetMaster();
            if (remoteOwner && HasActivePlayerMaster() && IsSafe(remoteOwner))
            {
                std::string playerCommand = command.substr(bridgeCommandPrefix.size());

                if (!sPlayerbotAIConfig.commandPrefix.empty() &&
                    playerCommand.compare(0, sPlayerbotAIConfig.commandPrefix.size(), sPlayerbotAIConfig.commandPrefix) != 0)
                {
                    playerCommand = sPlayerbotAIConfig.commandPrefix + playerCommand;
                }

                HandleCommand(CHAT_MSG_WHISPER, playerCommand, *remoteOwner);
            }
            continue;
        }

        if (command.compare(0, bridgePartyPrefix.size(), bridgePartyPrefix) == 0)
        {
            Player* remoteOwner = GetMaster();
            if (remoteOwner && HasActivePlayerMaster() && IsSafe(remoteOwner))
            {
                std::string message = command.substr(bridgePartyPrefix.size());
                if (!message.empty())
                    SayToParty(message, true);
            }
            continue;
        }

        if (command.compare(0, bridgeSayPrefix.size(), bridgeSayPrefix) == 0)
        {
            Player* remoteOwner = GetMaster();
            if (remoteOwner && HasActivePlayerMaster() && IsSafe(remoteOwner))
            {
                std::string message = command.substr(bridgeSayPrefix.size());
                if (!message.empty())
                    Say(message, true);
            }
            continue;
        }
        // PBCB END QUEUED COMMAND EXECUTION
'@

    $aiText = Insert-AfterAnchorInFunction `
        $aiText `
        "void PlayerbotAI::HandleCommands()" `
        "void PlayerbotAI::UpdateAIInternal" `
        "        Player* owner = holder.GetOwner();" `
        $queueBlock `
        $queueMarker

    # --- PlayerbotAI.cpp: bridge endpoints injected before existing remote/debug commands ---
    $endpointMarker = "// PBCB BEGIN REMOTE ENDPOINTS"
    $endpointBlock = @'

    // PBCB BEGIN REMOTE ENDPOINTS
    // Write operations are queued; they are never executed on the TCP socket thread.
    if (command.compare(0, 4, "cmd:") == 0)
    {
        std::string playerCommand = command.substr(4);
        if (playerCommand.empty() || playerCommand.size() > 1024 ||
            playerCommand.find('\n') != std::string::npos ||
            playerCommand.find('\r') != std::string::npos)
        {
            return "rejected:invalid-command";
        }

        if (!HasActivePlayerMaster())
            return "rejected:no-active-player-master";

        {
            std::lock_guard<std::mutex> lock(m_chatQueuesMutex);
            chatCommands.push(ChatCommandHolder("__pb_bridge_command " + playerCommand));
        }

        return "queued";
    }
    else if (command.compare(0, 6, "party:") == 0)
    {
        std::string message = command.substr(6);
        if (message.empty() || message.size() > 255 ||
            message.find('\n') != std::string::npos ||
            message.find('\r') != std::string::npos)
        {
            return "rejected:invalid-message";
        }

        if (!HasActivePlayerMaster())
            return "rejected:no-active-player-master";

        {
            std::lock_guard<std::mutex> lock(m_chatQueuesMutex);
            chatCommands.push(ChatCommandHolder("__pb_bridge_party " + message));
        }

        return "queued";
    }
    else if (command.compare(0, 4, "say:") == 0)
    {
        std::string message = command.substr(4);
        if (message.empty() || message.size() > 255 ||
            message.find('\n') != std::string::npos ||
            message.find('\r') != std::string::npos)
        {
            return "rejected:invalid-message";
        }

        if (!HasActivePlayerMaster())
            return "rejected:no-active-player-master";

        {
            std::lock_guard<std::mutex> lock(m_chatQueuesMutex);
            chatCommands.push(ChatCommandHolder("__pb_bridge_say " + message));
        }

        return "queued";
    }
    else if (command == "snapshot")
    {
        auto cleanField = [](std::string value)
        {
            for (std::string::iterator i = value.begin(); i != value.end(); ++i)
            {
                if (*i == '\t' || *i == '\r' || *i == '\n')
                    *i = ' ';
            }
            return value;
        };

        std::ostringstream out;
        out << "name=" << cleanField(bot->GetName());
        out << "\tguid=" << bot->GetGUIDLow();
        out << "\tlevel=" << uint32(bot->GetLevel());
        out << "\tclass=" << uint32(bot->GetClass());

        switch (currentState)
        {
        case BotState::BOT_STATE_COMBAT: out << "\tstate=combat"; break;
        case BotState::BOT_STATE_DEAD: out << "\tstate=dead"; break;
        case BotState::BOT_STATE_NON_COMBAT: out << "\tstate=non-combat"; break;
        case BotState::BOT_STATE_REACTION: out << "\tstate=reaction"; break;
        default: out << "\tstate=unknown"; break;
        }

        int hpPct = bot->GetMaxHealth()
            ? int((static_cast<float>(bot->GetHealth()) / bot->GetMaxHealth()) * 100.0f)
            : 0;

        out << "\thp=" << hpPct;
        out << "\tpower=" << uint32(bot->GetPowerPercent(bot->GetPowerType()));
        out << "\tmap=" << bot->GetMapId();
        out << "\tx=" << bot->GetPositionX();
        out << "\ty=" << bot->GetPositionY();
        out << "\tz=" << bot->GetPositionZ();

        Unit* target = *GetAiObjectContext()->GetValue<Unit*>("current target");
        if (target)
        {
            out << "\ttarget=" << cleanField(target->GetName());
            int targetPct = target->GetMaxHealth()
                ? int((static_cast<float>(target->GetHealth()) / target->GetMaxHealth()) * 100.0f)
                : 0;
            out << "\ttarget_hp=" << targetPct;
        }
        else
        {
            out << "\ttarget=";
            out << "\ttarget_hp=";
        }

        Player* currentMaster = GetMaster();
        if (currentMaster && HasActivePlayerMaster() && IsSafe(currentMaster))
        {
            out << "\tmaster=" << cleanField(currentMaster->GetName());
            int masterPct = currentMaster->GetMaxHealth()
                ? int((static_cast<float>(currentMaster->GetHealth()) / currentMaster->GetMaxHealth()) * 100.0f)
                : 0;
            out << "\tmaster_hp=" << masterPct;
        }
        else
        {
            out << "\tmaster=";
            out << "\tmaster_hp=";
        }

        out << "\tstrategies=" << cleanField(currentEngine ? currentEngine->ListStrategies() : "");
        out << "\taction=" << cleanField(currentEngine ? currentEngine->GetLastAction() : "");
        return out.str();
    }
    // PBCB END REMOTE ENDPOINTS
'@

    $aiText = Insert-AfterFunctionOpen `
        $aiText `
        "std::string PlayerbotAI::HandleRemoteCommand(std::string command)" `
        $endpointBlock `
        $endpointMarker

    # Validate every marker before touching files.
    foreach ($check in @(
        @($commandText, "PBCB LOCALHOST BIND"),
        @($randomText, "PBCB BEGIN REMOTE ROUTER"),
        @($aiText, "PBCB BEGIN QUEUED COMMAND EXECUTION"),
        @($aiText, "PBCB BEGIN REMOTE ENDPOINTS")
    )) {
        if (-not $check[0].Contains($check[1])) {
            throw "Internal installer validation failed: $($check[1])"
        }
    }

    # Backup exact local files before writing.
    $stamp = Get-Date -Format "yyyyMMdd-HHmmss"
    $backupRoot = Join-Path $RepoPath ".playerbot-companion-backup\$stamp"
    foreach ($rel in @($commandRel, $randomRel, $aiRel)) {
        $src = Join-Path $RepoPath $rel
        $dst = Join-Path $backupRoot $rel
        $dstDir = Split-Path $dst -Parent
        New-Item -ItemType Directory -Force -Path $dstDir | Out-Null
        Copy-Item -LiteralPath $src -Destination $dst -Force
    }

    Write-Utf8NoBom $commandPath $commandText
    Write-Utf8NoBom $randomPath $randomText
    Write-Utf8NoBom $aiPath $aiText
    $filesWritten = $true

    Write-Utf8NoBom (Join-Path $RepoPath ".playerbot-companion-last-backup.txt") $backupRoot

    # Cheap source-diff sanity check when Git is available.
    $git = Get-Command git -ErrorAction SilentlyContinue
    if ($null -ne $git) {
        Push-Location $RepoPath
        try {
            & git diff --check -- $commandRel $randomRel $aiRel
            if ($LASTEXITCODE -ne 0) {
                throw "git diff --check reported an error."
            }
        }
        finally {
            Pop-Location
        }
    }

    Write-Host ""
    Write-Host "Playerbot Companion Bridge source integration installed successfully."
    Write-Host ""
    Write-Host "Repository: $RepoPath"
    Write-Host "Backup:     $backupRoot"
    Write-Host ""
    Write-Host "Modified:"
    Write-Host "  $commandRel"
    Write-Host "  $randomRel"
    Write-Host "  $aiRel"
    Write-Host ""
    Write-Host "Next step: rebuild your VMaNGOS/PlayerBots solution."
    Write-Host ""
}
catch {
    $errorMessage = $_.Exception.Message

    if ($filesWritten -and -not [string]::IsNullOrWhiteSpace($backupRoot) -and (Test-Path $backupRoot)) {
        try {
            foreach ($rel in @(
                "src\game\PlayerBots\playerbot\PlayerbotCommandServer.cpp",
                "src\game\PlayerBots\playerbot\RandomPlayerbotMgr.cpp",
                "src\game\PlayerBots\playerbot\PlayerbotAI.cpp"
            )) {
                $src = Join-Path $backupRoot $rel
                $dst = Join-Path $RepoPath $rel
                if (Test-Path $src) {
                    Copy-Item -LiteralPath $src -Destination $dst -Force
                }
            }

            $pointer = Join-Path $RepoPath ".playerbot-companion-last-backup.txt"
            if (Test-Path $pointer) {
                Remove-Item $pointer -Force
            }

            Write-Host ""
            Write-Host "A post-write check failed, so the installer automatically restored the original files." -ForegroundColor Yellow
        }
        catch {
            Write-Host ""
            Write-Host "WARNING: automatic rollback also encountered an error. The backup is at:" -ForegroundColor Red
            Write-Host "  $backupRoot" -ForegroundColor Red
        }
    }

    Write-Host ""
    Write-Host "INSTALLATION STOPPED - no forced/fuzzy patching was attempted." -ForegroundColor Red
    Write-Host $errorMessage -ForegroundColor Red
    Write-Host ""
    exit 1
}
