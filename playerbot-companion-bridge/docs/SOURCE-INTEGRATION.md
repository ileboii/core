# Source integration

The included patch targets the current `vmangos-ike3-playerbots` branch of:

https://github.com/ileboii/core

## Apply

From the repository root:

```bash
git apply --check path/to/source/vmangos-playerbot-companion.patch
git apply path/to/source/vmangos-playerbot-companion.patch
```

Then rebuild the PlayerBots/server target normally.

## Enable

In the **active runtime** `aiplayerbot.conf`:

```ini
AiPlayerbot.CommandServerPort = 8765
```

Restart `mangosd`.

## Why the patch touches three areas

### PlayerbotCommandServer.cpp

The pre-existing command server binds to all interfaces. The patch changes it to loopback (`127.0.0.1`) because write commands turn the port into a local control surface.

### RandomPlayerbotMgr.cpp

The pre-existing remote handler expects `<command>,<guid>` and resolves through `GetPlayerBot()` only.

The patch:

- splits at the last comma
- adds `bridge-version` and `list`
- accepts bot name or GUID
- falls back to `RandomPlayerbotMgr::GetPlayer()` for playerbots that have been moved under a real player holder
- validates that the selected object is actually a playerbot

### PlayerbotAI.cpp

The patch adds:

- a compact `snapshot`
- `cmd:...`
- `party:...`
- `say:...`

Writes are inserted into the existing `m_chatQueuesMutex`-protected chat command queue.

The actual normal playerbot command is then re-entered through `HandleCommand()` on the AI update path using the active real-player master.

This deliberately avoids directly executing normal playerbot actions from the detached command-server socket thread.

## Important

The patch was prepared from the branch contents retrieved on 2026-09-12. Always run `git apply --check` first if the branch has changed.

This package has not compiled the VMaNGOS C++ target inside the ChatGPT artifact environment. Treat the patch as source-ready but still do your normal local compile/test before publishing a binary release.

## Public defaults

The distributed `PROFILE.md` is intentionally neutral. Personal identities, relationships,
private conversation history, and custom roleplay settings belong only in each user's local
configuration and are not part of the public defaults.
