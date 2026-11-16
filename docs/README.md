# OpenKal documentation

Start with the [project README](../README.md) for dependencies, builds, profiles,
optional game data and running the client or MainSvr. [Open work](TASKS.md)
links to remaining implementation and verification tasks.

For source publication, use the
[repository rights and content review](OpenSourceReleaseChecklist.md).
Implementation, functional verification, CI and delivery tasks live in
[TODO.md](../TODO.md); their technical status does not determine redistribution rights.

The [release procedure](ReleaseProcedure.md), [0.1.0 release notes](ReleaseNotes-0.1.0.md)
and [security policy](../SECURITY.md) define the project artifacts and supported scope.

## Architecture and operation

- [Architecture](ARCHITECTURE.md): module boundaries, client/server ownership and backends.
- [Configuration package](architecture/CONFIG_PACKAGE_VFS.md): `config.pak`, source precedence and deployment.
- [Engine decision](architecture/ADR-0001-engine-backend.md): OpenGL and the headless backend.
- [Multiplayer](Multiplayer.md): dedicated server, accounts, client connections and player interaction.
- [PostgreSQL](PostgreSql.md): local database setup.
- [TCP security](TcpSecurity.md): TLS configuration, certificates and transport guarantees.
- [Multiplayer load testing](MultiplayerLoadTesting.md): owned regression fixtures and bounded runtime measurements.
- [Remaining refactoring plan](RemainingRefactoringPlan.md): 16 ordered packages, baseline contracts and validation gates.
- [Area map format](area-map-format.md): `.akarea` encoding and collision semantics.

## Gameplay

- [Trading Post](TradingPost.md): anonymous offers, partial purchases and NPC collection.
- [NPCs](Npcs.md): server-coordinate placement, facing and configurable overhead symbols.

- [Skills](Skills.md), [skill effect assets](SkillEffects.md) and [Talent Tree](TalentTree.md).
- [Parties](Party.md), [Guilds](GuildPlan.md) and [Castle War](CastleWar.md).
- [Pets](Pets.md), [Pet mixing design](PetMixing.md), [Common riding](RidingAnimals.md),
  [Battle riding](BattleRidingPets.md) and [Riding Cache / Stablemaster](BattleRidingCachePlan.md).
- [Collection equipment](CollectionEquipment.md), [Weapon enchantment](WeaponEnchantment.md)
  and [Polishing Stone](PolishingStone.md).
- [Gathering](Harvesting.md) and [Consumable crafting](ConsumableCrafting.md).
- [Monster strength](MonsterStrength.md) and [Damage reduction](DamageReduction.md).
- [Community EXP event](world/ExpEventBoard.md).
- [Character emotes](Emotes.md): Action-tab and hotbar controls, motions, and network behavior.
- [Feature requests](FeatureRequests.md): proposals and unresolved product questions.

## Appearance and tools

- [Image provenance](ImageProvenance.md): source chats, generation prompts,
  per-file hashes, artwork derivations and remaining evidence limits.
- [Skins](Skins.md): slots, persistence and Skin Artisan exchange.
- [Skin items](SkinItems.md): allocated IDs and model coordinates.
- [Equipment appearances](EquipmentSkinCatalog.md): grade and Darkness mappings.
- [Skin audit](SkinCatalogAudit.md): commands, reports, naming and model guards.
- [Skin rendering findings](SkinMotionReview.md): resolved defects and open skirt clipping.
- [Model materials](ModelMaterials.md): specular lighting and transparent geometry.
- [Beads of Fire](BOF.md): shader naming, attachment data and preview limits.
- [UI format](ui/TEMPLATE.md), [UI variables](ui/VARIABLES.md) and [UI editor](ui/EDITOR.md).
- [Management console](ManagementConsole.md) and [Test Players](TestPlayers.md).

Gameplay references describe current behavior. Proposals and missing capabilities
are labeled explicitly. Configuration files contain the editable values; catalog
IDs and curated mappings must remain stable. Development rules and source
provenance requirements live in [AGENTS.md](../AGENTS.md) and its linked skills.
