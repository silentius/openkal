# Default openkal profile — single-player embeds MainSvr (unix socket + SQLite).
# Asset roots are relative to the executable; an empty configuration_root selects config.pak.

name=default
data_root=data
map_root=map
maps_subdir=MAPS
server_map_root=RuntimeData/areas
configuration_root=
origin_patch=32

# Default fallback used for new characters and invalid saved positions.
# Custom worlds should supply a profile with their own safe starting position.
spawn_name=Narootuh
spawn_server_x=257285
spawn_server_y=258744
spawn_server_z=16100

# offline = viewer only; single = embed server; multi = remote TCP MainSvr
play_mode=single
store_backend=sqlite
server_unix_path=

network_enabled=0
network_host=127.0.0.1
network_port=30001

# Client display, world streaming, effects, and audio.
client.settings_version=2
client.display.mode=windowed
client.display.width=1920
client.display.height=1080
client.quality.preset=medium
client.world.view_distance_tiles=2
client.world.preload_margin_tiles=0
client.world.resident_tile_limit=25
client.world.upload_budget_ms=1.50
client.world.animation_distance=300
client.world.effect_distance=300
client.render.grass_enabled=1
client.render.bloom=subtle
client.render.shadows=all
client.render.max_rendered_monsters=25
client.audio.effect_volume=5
client.audio.bgm_volume=5
client.audio.bgm_enabled=1
client.audio.listener_follows_camera=0
