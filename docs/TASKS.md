# Open work

This index lists remaining work described in the feature references. It is not
an execution order or a record of completed implementation steps.

The canonical engineering queue, with stable task IDs and acceptance criteria,
is in [TODO.md](../TODO.md). Licensing, redistribution and repository-content
review are tracked separately in the
[source-release review](OpenSourceReleaseChecklist.md).

- [Skin rendering](SkinMotionReview.md): fix Knight 25003/25004 skirt clipping
  across motion without introducing garment distortion or self-intersections.
- [Skin catalog](SkinCatalogAudit.md): resolve three missing textures and remaining
  appearance/classification findings; keep static inspection distinct from motion checks.
- [Equipment appearances](EquipmentSkinCatalog.md): complete animation and mixed-outfit verification.
- [Battle riding](BattleRidingPets.md): load remaining JSON rules, replace placeholder
  abilities, make mutations atomic, and complete action/observer coverage.
- [Riding progression](BattleRidingCachePlan.md): persistence rollback/retry safety,
  acquisition balance and broader species support.
- [Pet mixing](PetMixing.md): resolve the recipe/option questions before implementing Crossbreed and Monster Pet Creation.
- [Guilds](GuildPlan.md): broader guild features beyond the current Castle War rules.
- [Castle War](CastleWar.md#follow-up-work-for-the-original-castle): deployment
  layout, gate and statue interactions, and end-to-end verification.
- [Damage reduction](DamageReduction.md): playtest high avoidance with healing
  before changing mitigation constants.
- [EXP noticeboard](world/ExpEventBoard.md): verify placement and clearance in Temporary Fort.
- [BOF](BOF.md): complete class-specific timing, attachment and observer verification.
- [Emotes](Emotes.md): gameplay controls are implemented; chat commands remain optional future work.

[Feature requests](FeatureRequests.md) contains additional proposals, not promised
implementation. General development and release requirements live in
[AGENTS.md](../AGENTS.md); build/test instructions live in the [README](../README.md).
