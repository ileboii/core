# Playerbot Companion Bridge

Connect a ChatGPT/Codex session to a playerbot character it can observe, command, and speak through on a VMaNGOS + ike3-playerbots server.

The public package intentionally ships with generic, neutral defaults. Personal identities, relationships, private conversation history, and custom roleplay settings are not included.

## Easiest Windows installation

### Server source integration

1. Extract this ZIP.
2. Double-click `INSTALL-BRIDGE-WINDOWS.bat`.
3. The installer automatically looks for `C:\VMaNGOS-core` (or asks for the source folder).
4. It backs up the three files it edits.
5. It injects the bridge at stable function anchors instead of applying a line-number-sensitive patch.
6. Rebuild VMaNGOS/PlayerBots normally.

The installer preserves the existing bodies of `HandleRemoteCommand()` and `HandleCommands()` and only adds marked bridge blocks. It refuses to continue if required anchors/APIs are missing.

### Enable the port

In the active `aiplayerbot.conf`:

```ini
AiPlayerbot.CommandServerPort = 8765
```

Restart `mangosd`.

### Configure a bot

Summon/log in the playerbot you want to use, then double-click:

`SETUP-WINDOWS.bat`

Choose the bot from the numbered list.

### Use with ChatGPT/Codex

Open this folder as a local project in ChatGPT Desktop → Codex and say:

> Join me in WoW and control my companion.

Codex reads `AGENTS.md` for the bridge commands and operating rules.

## Manual bridge commands

```bat
bot.bat snapshot
bot.bat cmd "follow"
bot.bat cmd "stay"
bot.bat cmd "do attack"
bot.bat party "Coming."
bot.bat say "Hello."
bot.bat list
bot.bat test
```

## Safety

- The server bridge binds to `127.0.0.1` by default.
- Write commands are queued into the existing playerbot command queue and executed on the bot AI update path.
- The installer makes timestamped backups before editing.
- `UNINSTALL-BRIDGE-WINDOWS.bat` can restore the exact pre-install versions of the edited files. Do this before making later edits to those same files.

## Compatibility

The installer targets the VMaNGOS + ike3-playerbots layout used by the `vmangos-ike3-playerbots` branch and checks for the APIs it needs before editing.

A legacy `.patch` file may still be included as reference, but the Windows installer is the recommended public installation method because local forks commonly have unrelated edits that make unified diffs fail.
