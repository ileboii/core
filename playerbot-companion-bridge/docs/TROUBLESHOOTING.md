# Troubleshooting

## Setup says it cannot connect

Confirm the active runtime `aiplayerbot.conf` contains:

```ini
AiPlayerbot.CommandServerPort = 8765
```

Then restart `mangosd`.

Also confirm the source build contains the companion bridge patch.

## Setup connects but lists no bots

Log/summon the playerbot first, then rerun setup.

## Commands return `rejected:no-active-player-master`

The selected bot is online, but it does not currently have an active real player master.

Bring the bot under your character's control/group using the normal playerbot workflow, then try again.

## `snapshot` works but `cmd` appears to do nothing

`queued` only means the command entered the bot command queue.

Possible causes include:

- the bot's normal strategy/action priorities overriding the requested behavior
- the command not existing on that playerbot build
- the bot no longer having an active real-player master
- command prefix/configuration differences

Check the next snapshot and test the same command manually in WoW.

## Codex does not operate the bot automatically

Open the **bridge folder itself** as the Codex local project so Codex can see `AGENTS.md`.

Then say:

> Join me in WoW. Use the playerbot bridge in this project.

## Windows blocks PowerShell scripts

Use `bot.bat` / `SETUP-WINDOWS.bat`; they invoke PowerShell with a process-local execution-policy bypass.

Alternatively use `bridge.py` with Python 3.
