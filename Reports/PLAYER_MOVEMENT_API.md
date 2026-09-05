# Player Movement Configuration API

The version 18 Linux render boundary adds
`geLinuxRender_SetPlayerMovement`. Applications may provide finite, bounded
gravity, jump-speed, and stair-step values in native world units without
placing map-specific policy in the renderer.

The engine retains these coefficients when transient physics state is reset
after a game-layer save restore. Loading another world restores safe engine
defaults so the application must deliberately apply the new map's settings.
