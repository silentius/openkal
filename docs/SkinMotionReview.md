# Skin rendering findings

Runtime composition is documented in [Skins.md](Skins.md); catalog inspection
and preview commands are in [SkinCatalogAudit.md](SkinCatalogAudit.md).
This page records the useful rendering results and the unresolved defect.

## Open: Knight skirt clipping, 25003 / 25004

Legs protrude through the blue and black formal costumes while standing and
walking. Sampled attack geometry also intersects. The pants and skirt belong to
one costume mesh; ordinary equipped pants are already excluded. Both materials
are opaque and write depth, so transparent triangle sorting does not fix this.

The two variants have identical decoded geometry, indices and weights. Their
weighted bone references are valid. Both declare 86 bones while the available
Knight rig has 87, but weighted references stop at 84; this count difference
alone does not establish a skeleton defect. Skirt helpers mostly follow the
pelvis while the thighs rotate relative to it.

A bounded probe found strict leg/skirt crossings in 111 sampled keyframes across
six current/archived idle, walk and attack files, with none in the selected bind
regions. Simple weight transfer and radial expansion failed across motions.
Post-skin contact correction removed the initial selected intersections but
introduced stretching, abrupt displacement or crossings elsewhere. Even variants
with edge/area constraints and 237 dense samples introduced new self/other-surface
contacts. An outward-normal contact solver using the complete 556-triangle pants
surface also failed broader checks.

No experimental correction is enabled. A usable fix must cover the full garment,
fixed boundaries, deformation, self-contact, intermediate times and temporal
continuity. Zero intersections in one selected region is insufficient. Preserve
visible pants/skirt geometry; an idle-only correction or an identical archived
skeleton is not a solution. Local scripts, rejected weights, pair lists and
renders remain in ignored `build/skin-skirt-review/`.

Reproduce with `/getitemid 25003 0 1` and `/getitemid 25004 0 1`, inspecting front,
back and side views through idle, walking and attacks.

## Resolved behavior

- Head-only Knight skins 25005, 25011 and 25012 use the helmet skin slot, retain
  clothing and restore the ordinary head on removal.
- Whole-body costumes omit ordinary hair and normally retain the face. Families
  7/8/29/30 supply complete faces and use `skin.replace_face`. Knight 25006 and
  Magician 25161/25174 have correct standing/walking heads and removal restoration.
- Thief 25418 supplies gold headwear/hair, requiring the ordinary face. Its
  `replace_face` exception was removed; the body, face and removal work.
- Knight 25037 uses visible blue group-14 boots. Archer 25283 is a full-body
  appearance with correct hat/hair. Separate removal results for these two
  representatives are not recorded.
- Whole-body/part replacement works in both directions and preserves ordinary
  equipment. 25006 displaces other body skins; equipping 25037 returns it to
  General and unlocks the other body slots. Weapon/shield skins remain independent.
- Character selection retains worn equipment/skins through the protocol's
  255-entry limit, fixing the old thirteen-row truncation that omitted shields.
  Shield skin 25042 uses the distinct gold-trimmed 06-01 model. Selection,
  re-entry persistence and removal restore the correct shield without item loss.
- Weapon 25062 requires a real one-hand sword and 25063 a real two-hand sword.
  Incompatible real-weapon changes are rejected. 25135/25143 render correctly
  through idle/walk/attacks and restore the real weapon on removal.
- The red sphere of staff 25188 renders correctly during camera rotation.
  Back-to-front triangle ordering prevents a two-sided blended surface without
  depth writes from overwriting its front texture. Per-mesh centroid sorting
  does not solve arbitrary intersecting surfaces or ordering between actors.

The five representative weapon appearances 25062, 25063, 25188, 25316 and 25432
received sampled attachment poses; the dual daggers appear in both hands.
Reference-based Knight categories and the 60 OpenKal choices also
received sampled pose checks. The 25108 one-hand and 25132 two-hand appearance
representatives are accepted. These results cover their stated cases, not every
animation phase, weapon effect or catalog variant. Static head-layering candidates
remain review findings rather than defects established for every class.

## Pose renderer

`skin_preview_renderer MANIFEST.json` accepts static jobs and sampled poses.
For a pose add `skeleton`, `motion` and `time_ms`; both files and evaluation must
succeed, with no silent bind-pose fallback. Optional `attachments` supplies one
integer per model path: `-1` skins it, while a nonnegative value rigidly attaches
it to that bone-world matrix. Invalid bones and mismatched lists are rejected.
Motions loop using runtime sampling rules. Run with an OpenGL context; keep
external models, images and exact local manifests outside the source tree.
