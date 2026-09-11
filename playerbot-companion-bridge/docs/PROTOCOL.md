# Bridge protocol

The server uses a deliberately tiny line-oriented TCP protocol.

Default address:

`127.0.0.1:<AiPlayerbot.CommandServerPort>`

A request is:

```text
<command>,<selector>\n
```

The server splits at the **last comma**, so command/message text may contain commas.

`selector` is normally the selected bot's low GUID. The patched server can also resolve a playerbot by character name.

A response is one line terminated by `\n`.

## Global commands

These do not require a real bot selector; clients send selector `0`.

### `bridge-version,0`

Example response:

```text
playerbot-companion-bridge/1
```

### `list,0`

Returns online playerbots:

```text
guid|name|level|classId|master;guid|name|level|classId|master
```

Example:

```text
123|Elena|60|8|Ile;456|Tankbot|60|1|Ile
```

## Bot read commands

The original command server read commands remain available, including:

- `state`
- `position`
- `target`
- `hp`
- `strategy`
- `action`
- `travel`
- `values`

The patch adds:

### `snapshot`

Returns one tab-delimited line of key/value telemetry.

Example:

```text
name=Elena	guid=123	state=combat	hp=91	power=73	map=533	zone=Naxxramas	target=Patchwerk	target_hp=42	master=Ile	master_hp=61	strategies=...	action=frostbolt
```

The client converts this into JSON.

## Bot write commands

### `cmd:<playerbot command>`

Queues a normal playerbot command.

Example:

```text
cmd:follow,123
```

Response:

```text
queued
```

### `party:<message>`

Queues party chat from the bot.

### `say:<message>`

Queues local /say chat from the bot.

Write operations are consumed by the bot's normal AI command/update path rather than mutating world state directly from the command server socket thread.
