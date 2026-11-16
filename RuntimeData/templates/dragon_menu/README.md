# Compact Dragon menu assets

The live assembly is `config/ui/menu_bar.json` (`openkal.ui/v1`), at 472 × 42
logical pixels. It reuses the Dragon HUD's dark backing and adds small bronze
end caps, twelve independent buttons and separate relief icons. There are no dragon
heads on this widget.

`bmp/` contains twenty native, uncompressed 32-bit BI_RGB textures with straight
BGRA alpha: twelve 24 × 24 menu icons, two collection emblem sizes, one 64 × 64 balance,
three 34 × 34 button states and two 20 × 42 end
caps. Preserve all four channels when exporting. The build copies them unchanged
to `RuntimeData/UI/dragon_menu` and includes them in `config.pak` and installed
loose resources. All tooltip text is FreeType, not part of the artwork.

The left jade grip, right cap and narrow rail drag the panel. Buttons consume
their own clicks. Hover uses the brighter bronze frame; pressed/open buttons use
the recessed frame with a small jade inset. Actual window visibility supplies
open state, including keyboard toggles and closing a window directly. Several
windows can be marked open simultaneously. The tooltip shows only existing
shortcuts: H, E, C, I, K, Q, L and Esc. Party, Community and Map retain the preexisting
reserved actions.

## Artwork provenance

Generated with the built-in image generation tool from the project-created compact
Dragon menu concept. The sheet was mechanically cropped, resized and exported
to native RGBA BMPs. Only the runtime sprites are needed by the client.

Generation prompt:

> Create a production sprite sheet derived from this approved OpenKal menu
> concept, strictly matching its worn bronze / charcoal stone / tiny jade accent
> look. Transparent RGBA background, no text, no shadow outside sprites,
> orthographic front view. Output exactly 5 equal columns and 3 equal rows of
> isolated assets in a precise evenly spaced grid, one sprite per cell. First row
> five isolated bronze relief ICONS ONLY with transparent background, no button
> frames: ring flame, horse head profile, single person bust, tied inventory pouch,
> open skill book. Second row five isolated bronze relief ICONS ONLY transparent
> background: rolled quest scroll, two person busts, two chat bubbles, folded map,
> gear. Each of these ten icons simplified to a bold recognizable 24-pixel
> silhouette, same visual weight and same warm bronze tint as reference. Third
> row: cell 1 empty SQUARE button with very thin antique bronze frame and opaque
> dark stone center (idle); cell 2 identical empty SQUARE button with slightly
> brighter gold bronze rim, opaque dark stone center (hover); cell 3 identical
> empty SQUARE button with recessed darker stone center and tiny jade horizontal
> inset near its bottom border (open/pressed); cell 4 narrow vertical toolbar LEFT
> endcap, opaque bronze relief backing and single tiny jade round inset, no
> dragon; cell 5 narrow vertical toolbar RIGHT endcap, opaque bronze relief
> backing, no jewel and no dragon. All squares have identical dimensions, icons
> fully isolated. Each sprite centered in its equal grid cell, with large
> transparent gutters, never touching neighboring cells. Uniform sprite scale
> first ten cells. End caps should be approximately half as wide as tall. No
> assembled toolbar, no labels, no titles, no guide grid lines, no scenery. The
> reference image is style guidance; deliver only the 15 production sprites
> arranged in this exact order on transparency.

## Verification

`menu_bar_hud` checks each action at 75%, 100% and 150% scale, drag behavior,
saved logical positions and viewport clamping. `ui_document` checks that an
editor roundtrip preserves button states and tooltip tokens. To capture the
actual client renderer at native/scaled sizes, with hover, several open windows,
and a tooltip at the bottom-right screen edge:

```sh
build-ninja/tests/openkal_test_menu_bar_hud --capture /tmp/dragon-menu.ppm
```

## Collection emblem

`source/collection.png` is an open collector's casket with a medallion and gem,
generated with the built-in image tool in the menu's antique bronze relief style.
The exact prompt is retained in `source/collection.prompt.txt`. The collection
window uses `bmp/collection_dialog.bmp`, a 64 x 64 texture, in its standard
32-pixel `dialog_icon` slot to retain detail at enlarged UI scales. Export both
straight-BGRA versions with `python3 tools/export_collection_icon.py`. Each is
resampled directly from the full-resolution source: the dialog artwork fits
within 59 x 59 pixels, while `bmp/collection.bmp` keeps the menu's 24 x 24 canvas
and 22 x 22 artwork bounds. Transparent margins are trimmed before scaling.

## Messages icon

`source/messages.png` is the full-resolution bronze envelope generated with the
built-in image generation tool. `bmp/messages.bmp` is its 24 x 24 straight-BGRA
runtime export. The same asset is used by the menu, inbox emblem and attachment
notification. Export trims transparent margins, fits inside 22 x 22 pixels and
centers it in a transparent 24 x 24 canvas without changing the artwork.

Generation prompt (built-in tool; quest and community icons supplied as style references):

> Create one production game UI icon for OpenKal Dragon UI: a closed mail envelope as a warm antique bronze bas-relief, matching the attached small quest-scroll and conversation icons exactly in material, restrained warm bronze color and orthographic front lighting. References are style only. The envelope should have a clear triangular folded flap and tiny bronze circular wax-seal shape, readable at 24 x 24 pixels, chunky simplified silhouette with subtle worn edges and dark recessed seams. A single isolated icon centered, front view, filling about 85 percent of a square image. Transparent RGBA background, no panel, no button frame, no outer shadow, no glow, no red/white paper, no arrow, no text or number. Final intended use is a small menu icon and a 32 pixel window emblem.


## Trading Post balance

`source/trading_post.png` is an OpenKal antique bronze balance,
generated on 2026-10-31 with the built-in image generation tool. The existing
OpenKal chest, scroll and conversation artwork supplied the material/lighting
references, and the earlier balance silhouette supplied its subject. It uses
rounded bevels, recessed seams, sculpted bowls and restrained upper-left
highlights to match the menu's three-dimensional relief style. No external game
asset was copied or imported. The exact generation prompt is retained in
`source/trading_post.prompt.txt`. The earlier flat vector design remains in
`source/trading_post.svg` as an unused design source. These project-owned assets
are distributed under GPL-3.0-or-later.

`tools/export_trading_post_icon.py` trims transparent margins and exports
`bmp/trading_post.bmp` (24 px, 22 px artwork bounds) and
`bmp/trading_post_dialog.bmp` (64 px, 59 px artwork bounds), using the same native
RGBA BMP writer as the other Dragon UI templates.
