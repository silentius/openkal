/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/ClientSession.h"

#include "openkal/client/AnimatedActor.h"
#include "openkal/client/ArcherFxDispatcher.h"
#include "openkal/client/Audio.h"
#include "openkal/client/BattleMsgBuilder.h"
#include "openkal/client/BgmController.h"
#include "openkal/client/BlacksmithWindow.h"
#include "openkal/client/BofFx.h"
#include "openkal/client/BootShell.h"
#include "openkal/client/BuffSlotBar.h"
#include "openkal/client/CharacterInfoWindow.h"
#include "openkal/client/ChatWidget.h"
#include "openkal/client/ClickMarker.h"
#include "openkal/client/ClientCpuProfiler.h"
#include "openkal/client/ClientInput.h"
#include "openkal/client/ClientRuntimeConfig.h"
#include "openkal/client/ClientShutdownSignal.h"
#include "openkal/client/ClientUiConfiguration.h"
#include "openkal/client/ClientUiResources.h"
#include "openkal/client/CombatFloatText.h"
#include "openkal/client/CombatFx.h"
#include "openkal/client/ConsumableCraftingWindow.h"
#include "openkal/client/DebugOverlay.h"
#include "openkal/client/DialogCatalog.h"
#include "openkal/client/DropItemVisual.h"
#include "openkal/client/EnchantOverlay.h"
#include "openkal/client/Equipment.h"
#include "openkal/client/ExpEventBoard.h"
#include "openkal/client/FishingBar.h"
#include "openkal/client/Floor.h"
#include "openkal/client/FrameProfiler.h"
#include "openkal/client/FreetypeFont.h"
#include "openkal/client/GameHud.h"
#include "openkal/client/GameMenu.h"
#include "openkal/client/GuildWindow.h"
#include "openkal/client/InGameWindowOrder.h"
#include "openkal/client/IndoorMapView.h"
#include "openkal/client/Inventory.h"
#include "openkal/client/ItemDb.h"
#include "openkal/client/ItemTooltip.h"
#include "openkal/client/JobTitles.h"
#include "openkal/client/KnightFxDispatcher.h"
#include "openkal/client/LoadingScreen.h"
#include "openkal/client/MailWindow.h"
#include "openkal/client/MixVisual.h"
#include "openkal/client/MixWindow.h"
#include "openkal/client/MonsterRender.h"
#include "openkal/client/MultiplayerMenu.h"
#include "openkal/client/Nameplate.h"
#include "openkal/client/NetClient.h"
#include "openkal/client/NpcCatalog.h"
#include "openkal/client/NpcTalkWindow.h"
#include "openkal/client/OverheadChat.h"
#include "openkal/client/PartyWindow.h"
#include "openkal/client/PetController.h"
#include "openkal/client/PileSkillBar.h"
#include "openkal/client/Player.h"
#include "openkal/client/PlayerOptionsWindow.h"
#include "openkal/client/PlayerRenderSelection.h"
#include "openkal/client/PowerbookWindow.h"
#include "openkal/client/QuestJournalWindow.h"
#include "openkal/client/QuestMarker.h"
#include "openkal/client/RangeSkillDecal.h"
#include "openkal/client/RemotePlayers.h"
#include "openkal/client/RenderSettingsApplier.h"
#include "openkal/client/RidingCacheOverlay.h"
#include "openkal/client/RidingUiHost.h"
#include "openkal/client/SelectMarker.h"
#include "openkal/client/SettingsWindow.h"
#include "openkal/client/ShinsuDb.h"
#include "openkal/client/ShopWindow.h"
#include "openkal/client/SkillBar.h"
#include "openkal/client/SkillEffectTable.h"
#include "openkal/client/SkillPresenter.h"
#include "openkal/client/SkillWindow.h"
#include "openkal/client/Skills.h"
#include "openkal/client/SkinShopWindow.h"
#include "openkal/client/SkyBox.h"
#include "openkal/client/SoftCursor.h"
#include "openkal/client/SpeedMedicineWindow.h"
#include "openkal/client/SpriteFx.h"
#include "openkal/client/StartupScreen.h"
#include "openkal/client/StorageWindow.h"
#include "openkal/client/TalentTreeWindow.h"
#include "openkal/client/TaskCatalog.h"
#include "openkal/client/TerrainStream.h"
#include "openkal/client/TradeWindow.h"
#include "openkal/client/TradingPostWindow.h"
#include "openkal/client/TransformWindow.h"
#include "openkal/client/UiLayout.h"
#include "openkal/client/UiSettings.h"
#include "openkal/client/UiView.h"
#include "openkal/client/UiWidgets.h"
#include "openkal/client/WorldMapWidget.h"
#include "openkal/client/WorldObjectCatalog.h"
#include "openkal/client/WorldPick.h"
#include "openkal/client/WorldRenderCache.h"
#include "openkal/engine/engine.h"
#include "openkal/map/Coords.h"
#include "openkal/map/IndoorMap.h"
#include "openkal/map/MapCatalog.h"
#include "openkal/map/MapStream.h"
#include "openkal/map/MiniMapSize.h"
#include "openkal/map/Portal.h"
#include "openkal/map/Teleport.h"
#include "openkal/map/WorldGrid.h"
#include "openkal/platform/File.h"
#include "openkal/platform/Json.h"
#include "openkal/platform/Log.h"
#include "openkal/platform/Path.h"
#include "openkal/platform/Profile.h"
#include "openkal/protocol/Attack.h"
#include "openkal/protocol/BuffState.h"
#include "openkal/protocol/ChatAmount.h"
#include "openkal/protocol/Chatting.h"
#include "openkal/protocol/GetItemId.h"
#include "openkal/protocol/HarvestPlant.h"
#include "openkal/protocol/InsertItem.h"
#include "openkal/protocol/ItemInfo.h"
#include "openkal/protocol/ItemMonster.h"
#include "openkal/protocol/LevelUp.h"
#include "openkal/protocol/MageSkill.h"
#include "openkal/protocol/MonsterRace.h"
#include "openkal/protocol/Progression.h"
#include "openkal/protocol/Property.h"
#include "openkal/protocol/Quest.h"
#include "openkal/protocol/Shortcut.h"
#include "openkal/protocol/Skill.h"
#include "openkal/protocol/ThiefSkill.h"
#include "openkal/protocol/Transform.h"
#include "openkal/server/BofCatalog.h"
#include "openkal/server/CollectionEquipment.h"
#include "openkal/server/Combat.h"
#include "openkal/server/FishingCatalog.h"
#include "openkal/server/MixCatalog.h"
#include "openkal/server/PrefixEnchant.h"
#include "openkal/server/ServerHost.h"
#include "openkal/server/SpeedMedicine.h"
#include "openkal/server/WeaponEnchantment.h"
#include "openkal/server/store/DataStore.h"
#include "openkal/server/world/QuestDb.h"
#include "openkal/terrain/EnvLoader.h"
#include "openkal/terrain/GbModel.h"
#include "openkal/terrain/HeightPatch.h"
#include "openkal/terrain/OplLoader.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <future>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace openkal::client {

    class ClientSession::Impl {
      public:
        explicit Impl(std::unique_ptr<engine::IEngine>& engine) : m_engine(engine) {}

        [[nodiscard]] int run(int argc, char** argv, bool skip_startup_screen);

      private:
        std::unique_ptr<engine::IEngine>& m_engine;
        std::optional<ClientRuntimeConfig> m_runtime;
        ClientShutdownSignal m_shutdown_signal;
        std::int32_t m_preferred_character_id = 0;
        std::string m_connection_message;
        bool m_multiplayer = false;
    };

    ClientSession::ClientSession(std::unique_ptr<engine::IEngine>& engine)
        : m_impl(std::make_unique<Impl>(engine)) {}

    ClientSession::~ClientSession() = default;

    int ClientSession::run(int argc, char** argv, bool skip_startup_screen) {
        return m_impl->run(argc, argv, skip_startup_screen);
    }

}  // namespace openkal::client

int openkal::client::ClientSession::Impl::run(int argc, char** argv, bool skip_startup_screen) {
    using namespace openkal;
    constexpr float kPi = 3.14159265358979323846f;

    client::ClientCpuProfiler cpu_profiler;
    const char* test_attachment = std::getenv("OPENKAL_TEST_ATTACH");
    const bool test_attached = test_attachment && test_attachment[0] != '\0';
    const char* read_only_environment = std::getenv("OPENKAL_SESSION_READ_ONLY");
    const bool read_only_session =
        read_only_environment && std::string_view(read_only_environment) == "1";

    m_runtime = client::ClientRuntimeConfig::load(argc, argv);
    if (!m_runtime) {
        return 1;
    }
    client::ClientRuntimeConfig& runtime = *m_runtime;
    const platform::Profile& profile = runtime.profile();
    const std::string& repo_root = runtime.repo_root();
    const std::string& exe_dir = runtime.executable_dir();
    const std::string& save_dir = runtime.save_dir();
    const std::string& maps_dir = runtime.maps_dir();
    const std::string& server_map_dir = runtime.server_map_dir();
    const std::string& configuration_root = runtime.configuration_root();
    const std::string& tex_dir = runtime.texture_dir();
    settings::ClientSettings client_settings = runtime.client_settings();

    auto& engine = m_engine;
    if (!engine) {
        engine.reset(openkal::engine::create_engine());
        if (!engine || !engine->init("openkal",
                                     client_settings.display.width,
                                     client_settings.display.height)) {
            log::error("failed to init engine");
            return 1;
        }
    }
    auto apply_display_settings = [&](const settings::DisplaySettings& display) {
        engine::WindowMode window_mode = engine::WindowMode::Windowed;
        if (display.mode == settings::DisplayMode::Maximized) {
            window_mode = engine::WindowMode::Maximized;
        } else if (display.mode == settings::DisplayMode::Borderless) {
            window_mode = engine::WindowMode::Borderless;
        }
        return engine->set_window_mode(window_mode, display.width, display.height);
    };
    if (!apply_display_settings(client_settings.display)) {
        log::warn("failed to apply configured window mode");
    }
    client::RenderSettingsApplier render_settings_applier(*engine);
    engine->set_bloom_strength(0.0f);
    m_shutdown_signal.install();
    engine->set_cursor_mode(engine::CursorMode::Free);

    client::LoginMapStagePaths stage_paths;
    stage_paths.map_root = profile.map_root();
    stage_paths.world_objects_path = platform::join_path(configuration_root, "world_objects.json");
    bool wait_for_startup_input = true;
#if defined(OPENKAL_USE_GPERFTOOLS)
    const char* auto_boot = std::getenv("OPENKAL_AUTO_BOOT");
    wait_for_startup_input = !(auto_boot && auto_boot[0] == '1');
#endif
    client::ClientUiConfiguration ui_configuration;
    ui_configuration.initialize(save_dir, configuration_root);
    const std::string& ui_settings_path = ui_configuration.settings_path();
    client::UiSettings& ui_settings = ui_configuration.settings();
    client::UiLayoutStore& ui_layouts = ui_configuration.layouts();
    bool startup_multiplayer = false;
    bool continue_startup = skip_startup_screen || test_attached;
    if (!skip_startup_screen && !test_attached) {
        client::StartupScreen startup_screen(*engine);
        continue_startup = startup_screen.run(stage_paths.map_root,
                                              runtime,
                                              ui_settings,
                                              m_shutdown_signal,
                                              wait_for_startup_input,
                                              &startup_multiplayer);
    }
    if (!continue_startup) {
        engine->shutdown();
        return 0;
    }

    client_settings = runtime.client_settings();

    client::ClientUiResources ui_resources;
    ui_resources.initialize(*engine, profile.data_root());
    client::WidgetTools& ui = ui_resources.ui();
    client::FreetypeFont& name_font = ui_resources.name_font();
    client::OverheadChat overhead_chat;
    client::FreetypeFont& tip_font = ui_resources.tip_font();
    client::FreetypeFont& banner_font = ui_resources.banner_font();
    client::FreetypeFont& combat_font = ui_resources.combat_font();
    client::CombatFloatText& combat_floats = ui_resources.combat_floats();
    client::SoftCursor soft_cursor;
    if (!soft_cursor.load(*engine, configuration_root)) {
        log::warn("soft cursors missing under config/ui/cursors");
    }
    client::ClickMarker click_marker;
    if (!click_marker.init(*engine)) {
        log::warn("click destination marker mesh failed");
    }
    client::RangeSkillDecal range_skill_decal;
    if (!range_skill_decal.init(*engine)) {
        log::warn("range skill marker mesh failed");
    }
    client::QuestMarker quest_marker;
    if (!quest_marker.init(*engine)) {
        log::warn("quest NPC marker mesh failed");
    }
    client::SelectMarker select_marker;
    if (!select_marker.init(*engine, profile.data_root())) {
        log::warn("select marker (Effect/Tex/circle.GTX) failed");
    }
    client::DropItemVisual drop_item_visual;
    if (!drop_item_visual.init(*engine)) {
        log::warn("drop item pouch glow mesh failed");
    }
    if (!client::DebugOverlay::init(*engine)) {
        log::warn("debug monster hitbox meshes failed");
    }
    client::WorldDebugOverlay world_debug;
    client::SkyBox sky_box;
    if (!sky_box.load(*engine, profile.data_root())) {
        log::warn("sky box missing (OBJECTS/common/tex/sample_sky_*)");
    }

    client::Audio sfx;
    if (!sfx.init(profile.data_root(), platform::join_path(repo_root, "RuntimeData"))) {
        log::warn("audio mixer has no playback device — sounds silent");
    }
    sfx.set_effect_volume(static_cast<float>(client_settings.audio.effect_volume) / 5.0f);
    client::BgmController bgm(sfx);
    bgm.configure(client_settings.audio.bgm_enabled, client_settings.audio.bgm_volume);
    bgm.enter(client::BgmScene::CharacterSelection);
    if (!client::JobTitles::load(platform::join_path(configuration_root, "jobs"))) {
        log::warn("JobTitles: failed to load config/jobs (G.P. titles unavailable)");
    }
    client::MultiplayerChoice choice;
    if (test_attached) {
        choice.lobby = std::make_unique<server::LobbyClient>();
        if (!choice.lobby->connect_local(test_attachment) ||
            !choice.lobby->login("managed", "managed")) {
            log::error("Could not attach to the test-player tool: " + choice.lobby->error());
            ui_resources.shutdown(*engine);
            engine->shutdown();
            return 1;
        }
        choice.quit = false;
        choice.multiplayer = true;
        choice.server_key = "test-control";
    } else if (!(skip_startup_screen ? m_multiplayer : startup_multiplayer)) {
        choice.quit = false;
    } else {
        choice = client::MultiplayerMenu::run(
            *engine, ui, name_font, ui_settings, configuration_root, m_connection_message, true);
    }
    m_connection_message.clear();
    if (choice.quit) {
        ui_resources.shutdown(*engine);
        if (choice.startup) {
            return client::ClientSession::kStartupExitCode;
        }
        engine->shutdown();
        return 0;
    }
    const bool multiplayer = choice.multiplayer;
    m_multiplayer = multiplayer;
    std::unique_ptr<server::IDataStore> store;
    server::ICharacterRoster* roster = nullptr;
    std::int32_t account_id = 0;
    const std::string starters_dir = platform::join_path(configuration_root, "starters");
    if (multiplayer) {
        account_id = choice.lobby->account_id();
        roster = choice.lobby.get();
        const std::string online_save = platform::join_path(
            save_dir, "online/" + choice.server_key + "/" + std::to_string(account_id));
        (void) platform::ensure_directory(online_save);
        ui_configuration.initialize(online_save, configuration_root);
        choice.lobby->set_wait_callback([&] {
            engine::FrameInput waiting;
            if (!engine->poll(waiting) || waiting.quit || waiting.key_escape_pressed ||
                m_shutdown_signal.requested()) {
                return false;
            }
            engine->begin_frame(0.035f, 0.045f, 0.055f);
            name_font.draw_centered(*engine,
                                    static_cast<float>(waiting.width) * 0.5f,
                                    static_cast<float>(waiting.height) * 0.5f,
                                    "Contacting server... (Escape to cancel)",
                                    .9f,
                                    .85f,
                                    .7f);
            engine->end_frame();
            return true;
        });
    } else {
        store = server::create_data_store(server::StoreBackend::Sqlite);
        if (!store || !store->open(save_dir) ||
            !store->ensure_default_account(server::kDefaultAccountName,
                                           server::kDefaultAccountPassword)) {
            log::error("failed to open the local single-player store");
            ui_resources.shutdown(*engine);
            engine->shutdown();
            return 1;
        }
        const auto account = store->find_account(server::kDefaultAccountName);
        if (!account) {
            ui_resources.shutdown(*engine);
            engine->shutdown();
            return 1;
        }
        account_id = account->id;
        roster = store.get();
    }

    client::ShellResult boot;
    if (test_attached) {
        const auto characters = roster->list_characters(account_id);
        if (characters.size() != 1) {
            log::error("The test bridge must expose exactly one character.");
            ui_resources.shutdown(*engine);
            engine->shutdown();
            return 1;
        }
        boot.phase = client::ShellPhase::EnterWorld;
        boot.multiplayer = true;
        boot.selected = characters.front();
    } else
#if defined(OPENKAL_USE_GPERFTOOLS)
        if (auto_boot && auto_boot[0] == '1') {
        const auto chars = store->list_characters(account_id);
        if (chars.empty()) {
            log::error("OPENKAL_AUTO_BOOT=1 but no characters on DefaultAccount");
            if (store) {
                store->close();
            }
            ui.destroy();
            engine->shutdown();
            return 1;
        }
        boot.phase = client::ShellPhase::EnterWorld;
        boot.multiplayer = false;
        boot.selected = chars.front();
        log::info("OPENKAL_AUTO_BOOT: entering as " + boot.selected.name);
    } else
#endif
    {
        boot = client::BootShell::run(*engine,
                                      ui,
                                      *roster,
                                      account_id,
                                      multiplayer ? 0 : m_preferred_character_id,
                                      profile.data_root(),
                                      configuration_root,
                                      starters_dir,
                                      stage_paths,
                                      name_font,
                                      ui_settings,
                                      multiplayer);
    }
    if (boot.phase != client::ShellPhase::EnterWorld) {
        if (multiplayer && !choice.lobby->connected()) {
            m_connection_message = choice.lobby->error();
        }
        if (store) {
            store->close();
        }
        ui_resources.shutdown(*engine);
        if (boot.phase == client::ShellPhase::Startup) {
            return client::ClientSession::kStartupExitCode;
        }
        if (multiplayer && !choice.lobby->connected()) {
            return client::ClientSession::kSelectCharacterExitCode;
        }
        engine->shutdown();
        return 0;
    }
    if (multiplayer) {
        const auto selected = choice.lobby->select(boot.selected.id);
        if (!selected) {
            m_connection_message = choice.lobby->error();
            ui_resources.shutdown(*engine);
            return client::ClientSession::kSelectCharacterExitCode;
        }
        boot.selected = *selected;
    }
    log::info("entering world as " + boot.selected.name +
              " class=" + std::to_string(boot.selected.clazz));
    bgm.enter(client::BgmScene::World);

    // KalOnline KLoadingSystem::loading-texture initialization picks Loadbg0..3 at random.
    const int load_bg_index = std::rand() % 4;
    const std::string load_bg_file = "Loadbg" + std::to_string(load_bg_index) + ".gtx";
    log::info("loading screen: " + load_bg_file);

    client::LoadingScreen loading_screen(*engine, ui, tip_font, load_bg_file);
    // Seven completed stages: environment, map resources, map sections, NPC/effects,
    // server/catalogs, player/UI, and connection. Map work reports its actual section count.
    constexpr float kWorldLoadingStages = 7.0f;
    bool entering_world = true;
    float boot_map_progress = 0.0f;
    if (!loading_screen.present("Preparing world", 0.0f)) {
        if (store) {
            store->close();
        }
        ui.destroy();
        engine->shutdown();
        return 0;
    }

    // The selected character's persisted pose is authoritative, including zero coordinates.
    int spawn_sx = boot.selected.map_x;
    int spawn_sy = boot.selected.map_y;
    int spawn_sz = boot.selected.map_z;

    const std::string& indoor_root = profile.map_root();
    map::MapCatalog indoor_maps;
    const std::string indoor_info_path = platform::join_path(configuration_root, "mapinfo.json");
    if (!indoor_maps.load(indoor_info_path)) {
        log::warn("indoor mapinfo missing: " + indoor_info_path);
    }
    int local_map = boot.selected.map;
    auto uses_indoor_visuals = [&]() -> bool { return indoor_maps.has_indoor_mapname(local_map); };

    std::vector<terrain::LayerInfo> env_layers;
    std::vector<terrain::DecoInfo> env_decorations;
    if (!terrain::load_environment(
            platform::join_path(maps_dir, "n.env"), env_layers, env_decorations)) {
        log::error("need MAPS/n.env");
        if (store) {
            store->close();
        }
        ui.destroy();
        engine->shutdown();
        return 1;
    }
    if (!loading_screen.present("Terrain environment", 1.0f / kWorldLoadingStages)) {
        if (store) {
            store->close();
        }
        ui.destroy();
        engine->shutdown();
        return 0;
    }

    // Load JSON catalogs and start MainSvr off the UI thread while maps upload.
    const std::string monsters_dir = platform::join_path(configuration_root, "monsters");
    const std::string npcs_dir = platform::join_path(configuration_root, "npcs");
    const std::string shops_dir = platform::join_path(configuration_root, "server/shops");
    const std::string items_dir = platform::join_path(configuration_root, "items");
    const std::string groups_dir = platform::join_path(configuration_root, "groups");
    const std::string itemgroups_dir = platform::join_path(configuration_root, "itemgroups");
    const std::string skills_dir = platform::join_path(configuration_root, "skills");
    const std::string quests_dir = platform::join_path(configuration_root, "quests");
    const std::string portals_path = platform::join_path(configuration_root, "portals.json");
    const std::string dungeon_minimaps_path =
        platform::join_path(configuration_root, "ui/dungeon_minimaps.json");
    const std::string mixing_dir = platform::join_path(configuration_root, "mixing");
    const std::string dialogs_dir = platform::join_path(configuration_root, "dialogs");
    const std::string tasks_dir = platform::join_path(configuration_root, "tasks");
    const std::string shinsu_dir = platform::join_path(configuration_root, "shinsu");
    const std::string world_objects_path =
        platform::join_path(configuration_root, "world_objects.json");
    client::WorldObjectCatalog world_objects;
    std::uint32_t world_prop_fx_serial = 1;
    if (world_objects.load_json(world_objects_path)) {
        log::info("world objects=" + std::to_string(world_objects.size()) + " " +
                  world_objects_path);
    } else {
        log::warn("world object particle table missing: " + world_objects_path);
    }
    auto item_catalog_fut = std::async(
        std::launch::async, [configuration_root, data_root = profile.data_root(), items_dir]() {
            client::ItemDb catalog;
            if (!catalog.load(configuration_root, data_root, items_dir)) {
                log::warn("item definition catalog empty ? inventory icons may be missing");
            }
            return catalog;
        });
    auto skill_catalog_fut = std::async(std::launch::async, [skills_dir]() {
        client::Catalog catalog;
        if (!catalog.load(skills_dir)) {
            log::warn("skill catalog empty after JSON load (dir=" + skills_dir + ")");
        }
        return catalog;
    });

    server::ServerHost embedded_server;
    server::ServerConfig scfg;
    scfg.maps_dir = server_map_dir;
    scfg.indoor_geometry_root = profile.map_root();
    scfg.save_dir = save_dir;
    scfg.monsters_dir = platform::is_directory(monsters_dir) ? monsters_dir : std::string{};
    const std::string monster_strength_path =
        platform::join_path(configuration_root, "server/monster_strength.json");
    scfg.monster_strength_path =
        platform::file_exists(monster_strength_path) ? monster_strength_path : std::string{};
    scfg.items_dir = platform::is_directory(items_dir) ? items_dir : std::string{};
    const std::string pets_dir = platform::join_path(configuration_root, "pets");
    scfg.pets_dir = platform::is_directory(pets_dir) ? pets_dir : std::string{};
    const std::string riding_dir = platform::join_path(configuration_root, "riding");
    scfg.riding_dir = platform::is_directory(riding_dir) ? riding_dir : std::string{};
    const std::string riding_cache_path = platform::join_path(riding_dir, "cache.json");
    scfg.riding_cache_path =
        platform::file_exists(riding_cache_path) ? riding_cache_path : std::string{};
    const std::string riding_progression_path = platform::join_path(riding_dir, "progression.json");
    scfg.riding_progression_path =
        platform::file_exists(riding_progression_path) ? riding_progression_path : std::string{};
    struct PetPresentation {
        struct MeshAsset {
            std::string mesh_name;
            std::string texture_name;
        };

        int visual_key = 0;
        int mesh_key = 0;
        int bone = 0;
        int part_count = 0;
        float scale = 1.0f;
        int monster_type = 0;
        std::string custom_model;
        std::vector<MeshAsset> mesh_assets;
        std::uint64_t duration_seconds = 0;
        int heal_per_two_seconds = 0;
        bool auto_pickup = false;
    };
    std::unordered_map<int, PetPresentation> pet_presentations;
    if (const auto catalog_bytes =
            platform::read_file(platform::join_path(pets_dir, "catalog.json"))) {
        platform::Json catalog_json;
        std::string catalog_error;
        if (platform::json_parse(std::string(catalog_bytes->begin(), catalog_bytes->end()),
                                 catalog_json,
                                 &catalog_error) &&
            catalog_json.at("pets").is_array()) {
            for (const platform::Json& entry : catalog_json.at("pets").array()) {
                if (!entry.is_object()) {
                    continue;
                }
                const int item_index = entry.at("itemIndex").as_int(0);
                const int visual_key = entry.at("visualKey").as_int(0);
                if (item_index <= 0) {
                    continue;
                }
                PetPresentation presentation;
                presentation.visual_key = visual_key;
                // A catalog entry may render as a normal monster rather than a
                // Pet/ mesh. This keeps each companion mapping explicit in JSON.
                presentation.monster_type = entry.at("monsterType").as_int(0);
                presentation.scale = entry.at("modelScale").as_float(1.0f);
                presentation.duration_seconds =
                    static_cast<std::uint64_t>(std::max(0, entry.at("durationSeconds").as_int(0)));
                presentation.heal_per_two_seconds = entry.at("healPerTwoSeconds").as_int(0);
                const platform::Json& auto_pickup = entry.at("autoPickup");
                presentation.auto_pickup =
                    auto_pickup.type() == platform::Json::Type::Bool && auto_pickup.boolean();
                pet_presentations.emplace(item_index, presentation);
            }
        } else {
            log::warn("pet presentation catalog invalid: " + catalog_error);
        }
    } else {
        log::warn("pet presentation catalog missing: " + pets_dir);
    }
    struct PetVisualInfo {
        int mesh_key = 0;
        int bone = 0;
        int part_count = 0;
        float scale = 1.0f;
        int monster_type = 0;
        std::string custom_model;
    };
    struct PetSwitchModelInfo {
        std::vector<std::string> meshes;
        std::vector<std::string> textures;
    };
    std::unordered_map<int, PetVisualInfo> pet_visual_info;
    std::unordered_map<std::string, PetSwitchModelInfo> pet_switch_models;
    if (const auto visuals_bytes =
            platform::read_file(platform::join_path(pets_dir, "visuals.json"))) {
        platform::Json visuals_json;
        std::string visuals_error;
        if (platform::json_parse(std::string(visuals_bytes->begin(), visuals_bytes->end()),
                                 visuals_json,
                                 &visuals_error) &&
            visuals_json.at("visuals").is_array()) {
            for (const platform::Json& entry : visuals_json.at("visuals").array()) {
                if (!entry.is_object()) {
                    continue;
                }
                const int key = entry.at("key").as_int(0);
                const int monster_type = entry.at("monsterType").as_int(0);
                if (key > 0) {
                    PetVisualInfo info;
                    const int variant_key = entry.at("variantKey").as_int(0);
                    info.mesh_key = variant_key > 0 ? variant_key : key;
                    info.bone = entry.at("bone").as_int(0);
                    info.scale = entry.at("scale").as_float(1.0f);
                    info.monster_type = monster_type;
                    info.custom_model = entry.at("customModel").as_string();
                    if (const platform::Json* part = entry.get("part"); part && part->is_object()) {
                        info.part_count = part->at("index").as_int(0);
                    }
                    pet_visual_info.emplace(key, info);
                }
            }
            if (visuals_json.at("switchModels").is_array()) {
                for (const platform::Json& entry : visuals_json.at("switchModels").array()) {
                    if (!entry.is_object()) {
                        continue;
                    }
                    const std::string name = entry.at("name").as_string();
                    if (name.empty()) {
                        continue;
                    }
                    PetSwitchModelInfo info;
                    if (entry.at("meshes").is_array()) {
                        for (const platform::Json& mesh : entry.at("meshes").array()) {
                            if (mesh.is_string()) {
                                info.meshes.push_back(mesh.as_string());
                            }
                        }
                    }
                    if (entry.at("textures").is_array()) {
                        for (const platform::Json& texture : entry.at("textures").array()) {
                            if (texture.is_string()) {
                                info.textures.push_back(texture.as_string());
                            }
                        }
                    }
                    if (!info.meshes.empty()) {
                        pet_switch_models.emplace(name, std::move(info));
                    }
                }
            }
        } else {
            log::warn("pet visual catalog invalid: " + visuals_error);
        }
    }
    for (auto& [item_index, pet] : pet_presentations) {
        const auto visual = pet_visual_info.find(pet.visual_key);
        if (visual != pet_visual_info.end()) {
            pet.mesh_key = visual->second.mesh_key;
            pet.bone = visual->second.bone;
            pet.part_count = visual->second.part_count;
            pet.scale = visual->second.scale;
            pet.monster_type = visual->second.monster_type;
            pet.custom_model = visual->second.custom_model;
            const auto custom_model = pet_switch_models.find(pet.custom_model);
            if (custom_model != pet_switch_models.end()) {
                for (const std::string& mesh_name : custom_model->second.meshes) {
                    PetPresentation::MeshAsset asset;
                    asset.mesh_name = mesh_name;
                    const std::size_t dot = mesh_name.rfind('.');
                    const std::string stem = mesh_name.substr(0, dot);
                    for (const std::string& texture_name : custom_model->second.textures) {
                        if (texture_name.rfind(stem + "_mix_", 0) == 0) {
                            asset.texture_name = texture_name;
                            break;
                        }
                        if (asset.texture_name.empty() && texture_name.rfind(stem + ".", 0) == 0) {
                            asset.texture_name = texture_name;
                        }
                    }
                    pet.mesh_assets.push_back(std::move(asset));
                }
            }
        }
    }
    struct RidingPresentation {
        int visual_key = 0;
        int bone = 0;
        int part_count = 0;
        int seat_bone = 0;
        float scale = 1.0f;
        float rider_offset_y = 18.0f;
        std::uint64_t duration_seconds = 0;
        int movement_speed_percent = 0;
    };
    std::unordered_map<int, RidingPresentation> riding_presentations;
    if (const auto bytes = platform::read_file(platform::join_path(riding_dir, "catalog.json"))) {
        platform::Json root;
        std::string error;
        if (platform::json_parse(std::string(bytes->begin(), bytes->end()), root, &error) &&
            root.at("schema").as_string() == "openkal.riding/v1" && root.at("mounts").is_array()) {
            for (const platform::Json& entry : root.at("mounts").array()) {
                if (!entry.is_object()) {
                    continue;
                }
                const int item_index = entry.at("itemIndex").as_int(0);
                RidingPresentation riding;
                riding.visual_key = entry.at("visualKey").as_int(0);
                riding.bone = entry.at("bone").as_int(0);
                riding.part_count = entry.at("partCount").as_int(0);
                riding.seat_bone = entry.at("seatBone").as_int(0);
                riding.scale = entry.at("modelScale").as_float(1.0f);
                riding.rider_offset_y = entry.at("riderOffsetY").as_float(18.0f);
                riding.duration_seconds =
                    static_cast<std::uint64_t>(std::max(0, entry.at("durationSeconds").as_int(0)));
                riding.movement_speed_percent = entry.at("movementSpeedPercent").as_int(0);
                if (item_index > 0 && riding.visual_key > 0 && riding.bone > 0 &&
                    riding.part_count > 0) {
                    riding_presentations.emplace(item_index, riding);
                }
            }
        } else {
            log::warn("riding presentation catalog invalid: " + error);
        }
    } else {
        log::warn("riding presentation catalog missing: " + riding_dir);
    }
    const std::string prefixes_dir = platform::join_path(configuration_root, "prefixes");
    scfg.prefixes_dir = platform::is_directory(prefixes_dir) ? prefixes_dir : std::string{};
    scfg.groups_dir = platform::is_directory(groups_dir) ? groups_dir : std::string{};
    scfg.itemgroups_dir = platform::is_directory(itemgroups_dir) ? itemgroups_dir : std::string{};
    scfg.skills_dir = platform::is_directory(skills_dir) ? skills_dir : std::string{};
    scfg.quests_dir = platform::is_directory(quests_dir) ? quests_dir : std::string{};
    scfg.tasks_dir = platform::is_directory(tasks_dir) ? tasks_dir : std::string{};
    scfg.npcs_dir = platform::is_directory(npcs_dir) ? npcs_dir : std::string{};
    scfg.shops_dir = platform::is_directory(shops_dir) ? shops_dir : std::string{};
    scfg.portals_path = platform::file_exists(portals_path) ? portals_path : std::string{};
    scfg.mapinfo_path = platform::file_exists(indoor_info_path) ? indoor_info_path : std::string{};
    scfg.starters_dir = platform::is_directory(starters_dir) ? starters_dir : std::string{};
    scfg.mixing_dir = platform::is_directory(mixing_dir) ? mixing_dir : std::string{};
    const std::string bof_path = platform::join_path(configuration_root, "bof.json");
    scfg.bof_path = platform::file_exists(bof_path) ? bof_path : std::string{};
    const std::string fishing_path = platform::join_path(configuration_root, "fishing.json");
    scfg.fishing_path = platform::file_exists(fishing_path) ? fishing_path : std::string{};
    const std::string level_mail_rewards_path =
        platform::join_path(configuration_root, "level_mail_rewards.json");
    scfg.level_mail_rewards_path =
        platform::file_exists(level_mail_rewards_path) ? level_mail_rewards_path : std::string{};
    const std::string server_events_path =
        platform::join_path(configuration_root, "server_events.json");
    scfg.server_events_path =
        platform::file_exists(server_events_path) ? server_events_path : std::string{};
    server::FishingCatalog fishing_catalog;
    std::uint64_t fishing_base_cycle_duration_ms = 600000;
    std::uint64_t fishing_accelerated_cycle_duration_ms = 300000;
    std::uint64_t fishing_cycle_duration_ms = fishing_base_cycle_duration_ms;
    if (!scfg.fishing_path.empty() && fishing_catalog.load_file(scfg.fishing_path)) {
        fishing_base_cycle_duration_ms = fishing_catalog.base_interval_ms();
        fishing_accelerated_cycle_duration_ms = fishing_catalog.accelerated_interval_ms();
        fishing_cycle_duration_ms = fishing_base_cycle_duration_ms;
    }
    scfg.store = server::StoreBackend::Sqlite;
    scfg.character_id = boot.selected.id;
    scfg.transport = server::TransportKind::Unix;
    scfg.unix_path = profile.server_unix_path().empty()
                         ? platform::join_path(scfg.save_dir, "openkal.sock")
                         : profile.server_unix_path();
    scfg.spawn_x = profile.spawn_server_x();
    scfg.spawn_y = profile.spawn_server_y();
    scfg.spawn_z = profile.spawn_server_z();
    scfg.player_clazz = static_cast<int>(boot.selected.clazz);
    scfg.player_level = std::max(1, boot.selected.level);
    scfg.player_name = boot.selected.name.empty() ? "Hero" : boot.selected.name;
    scfg.map_radius_cells = 2;
    scfg.spawn_load_radius_px = 16384;
    scfg.max_monsters = 256;
    const auto plants =
        server::load_harvest_config(platform::join_path(configuration_root, "server/plants.json"));
    if (plants) {
        scfg.plants = *plants;
    }
    if (store) {
        store->close();
        store.reset();
    }
    std::promise<bool> server_promise;
    auto server_fut = server_promise.get_future();
    std::thread server_loader;
    if (multiplayer) {
        server_promise.set_value(true);
    } else {
        server_loader =
            std::thread([&embedded_server, &scfg, p = std::move(server_promise)]() mutable {
                p.set_value(embedded_server.start(scfg));
            });
    }

    float spawn_x = coords::to_client(spawn_sx, profile.origin_patch());
    float spawn_z = coords::to_client(spawn_sy, profile.origin_patch());
    const float spawn_y = coords::to_client_y(spawn_sz);
    const int center_px = coords::patch_index_for_client(spawn_x);
    const int center_py = coords::patch_index_for_client(spawn_z);
    log::info(boot.selected.name + " client=(" + std::to_string(spawn_x) + ", " +
              std::to_string(spawn_y) + ", " + std::to_string(spawn_z) + ") patches around (" +
              std::to_string(center_px) + "," + std::to_string(center_py) + ")");
    log::info("controls: WASD move (+auto-select nearest), Space attack / "
              "hold-Space loot, "
              "LMB walk/attack, RMB or Arrows look, F skill (selected/nearest), "
              "wheel zoom, C info, I "
              "inventory, K skills, Esc clears");
    log::info("hotbar: 1-0 = lower row, Shift+1-0 = upper row");
    log::info("skills: S/K opens class skill window; drag learned icons onto the "
              "skill bar");
    render_settings_applier.apply(client_settings.render);

    std::unordered_map<std::string, engine::TextureHandle> detail_cache;
    std::vector<client::WorldRenderCache::DrawnPatch> drawn;
    std::unordered_set<std::uint64_t> loaded_patches;
    client::TerrainStream terrain_stream(maps_dir, tex_dir, env_layers);
    auto patch_key = [](int px, int py) -> std::uint64_t {
        return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(px)) << 32) |
               static_cast<std::uint32_t>(py);
    };
    client::FloorField floor;
    const int stream_search_radius =
        client_settings.world.view_distance_tiles + client_settings.world.preload_margin_tiles;
    const int resident_tile_limit = client_settings.world.resident_tile_limit;
    const double terrain_upload_budget_ms = client_settings.world.upload_budget_ms;
    const int stream_request_batch =
        (stream_search_radius * 2 + 1) * (stream_search_radius * 2 + 1);
    constexpr float kPatchWorld = static_cast<float>(map::kMapSize * map::kTerrainCellSize);
    constexpr int kWaterRadius = 8;

    auto patch_center_xz = [](int px, int py, float& cx, float& cz) {
        const float origin = static_cast<float>(map::kPatchGrid / 2);
        cx = (static_cast<float>(px) - origin + 0.5f) * kPatchWorld;
        cz = (static_cast<float>(py) - origin + 0.5f) * kPatchWorld;
    };

    // OPL static props with GB meshes + TEX textures.
    std::unordered_map<std::string, client::WorldRenderCache::GpuModel> model_cache;
    std::unordered_map<std::string, engine::TextureHandle> prop_tex_cache;
    std::unordered_map<int, const client::WorldRenderCache::GpuModel*> decoration_models;
    for (const auto& decoration : env_decorations) {
        if (decoration.model_name.empty()) {
            continue;
        }
        const std::string path = client::WorldRenderCache::opl_name_to_gb_path(
            profile.data_root(), decoration.model_name);
        const auto* gpu =
            client::WorldRenderCache::get_or_load_gb(*engine, path, model_cache, prop_tex_cache);
        if (gpu != nullptr) {
            decoration_models[decoration.index] = gpu;
        }
    }
    std::vector<client::WorldRenderCache::DrawnProp> props;
    std::unordered_set<std::uint64_t> loaded_opl;
    int opl_ok = 0;
    int opl_missing_gb = 0;
    auto load_opl_patch = [&](int px, int py) {
        if (loaded_opl.count(patch_key(px, py))) {
            return;
        }
        loaded_opl.insert(patch_key(px, py));
        std::array<char, 64> name{};
        std::snprintf(name.data(), name.size(), "n_%03d_%03d.opl", px, py);
        std::string path = platform::join_path(maps_dir, name.data());
        if (!platform::file_exists(path)) {
            path.replace(path.size() - 4, 4, ".OPL");
            if (!platform::file_exists(path)) {
                return;
            }
        }
        terrain::OplFile opl;
        if (!terrain::load_opl(path, opl)) {
            return;
        }
        for (const auto& inst : opl.instances) {
            if (inst.name.empty()) {
                ++opl_missing_gb;
                continue;
            }
            const auto* object_particles = world_objects.find(inst.name);
            const std::string gb_path =
                client::WorldRenderCache::opl_name_to_gb_path(profile.data_root(), inst.name);
            if (gb_path.empty()) {
                ++opl_missing_gb;
                continue;
            }
            const client::WorldRenderCache::GpuModel* gpu =
                client::WorldRenderCache::get_or_load_gb(
                    *engine, gb_path, model_cache, prop_tex_cache);
            float wx = 0.0f;
            float wy = 0.0f;
            float wz = 0.0f;
            terrain::opl_world_translation(inst, px, py, wx, wy, wz);
            const auto S = engine::Mat4::scaling(inst.sx, inst.sy, inst.sz);
            const auto R = engine::Mat4::from_quaternion(inst.qx, inst.qy, inst.qz, inst.qw);
            const auto T = engine::Mat4::translation(wx, wy, wz);
            const auto model = engine::Mat4::multiply(engine::Mat4::multiply(T, R), S);
            // Collision-only GBs (bridge_02h*) supply standing surfaces without a GPU mesh.
            // Register their wall hulls too so arches and towers block side movement.
            floor.add_prop(gb_path, model, px, py);
            floor.add_obstacle(gb_path, model, px, py);
            ++opl_ok;
            // Object.dat particles belong to the OPL instance, rather than to its
            // render mesh. Keep them alive when a legacy GB cannot be drawn: this
            // is required for the D4 gate's 8-door2 / worpgate_01 attachment.
            if (!gpu && object_particles == nullptr) {
                continue;
            }
            client::WorldRenderCache::DrawnProp p;
            p.gpu = gpu;
            p.patch_x = px;
            p.patch_y = py;
            p.wx = wx;
            p.wy = wy;
            p.wz = wz;
            p.model = model;
            p.asset_path = gb_path;
            client::WorldRenderCache::apply_instance_bounds(p);
            if (object_particles != nullptr) {
                p.catalog_key = inst.name;
                p.fx_entity = client::WorldObjectCatalog::take_fx_entity(world_prop_fx_serial);
            }
            props.push_back(p);
        }
    };

    auto release_drawn_patch = [&](client::WorldRenderCache::DrawnPatch& d) {
        if (d.mesh) {
            engine->destroy_mesh(d.mesh);
            d.mesh = {};
        }
        if (d.shadow_height_map) {
            engine->destroy_texture(d.shadow_height_map);
            d.shadow_height_map = {};
        }
        std::uint32_t last_color = 0;
        for (auto& layer : d.layers) {
            if (layer.color && layer.color.id != last_color) {
                last_color = layer.color.id;
                engine->destroy_texture(layer.color);
            }
            layer.color = {};
            layer.detail = {};
        }
        d.layers.clear();
    };
    auto drop_streamed_patch = [&](int px, int py) {
        const auto key = patch_key(px, py);
        for (auto it = drawn.begin(); it != drawn.end();) {
            if (it->patch_x == px && it->patch_y == py) {
                release_drawn_patch(*it);
                it = drawn.erase(it);
            } else {
                ++it;
            }
        }
        floor.remove_patch(px, py);
        props.erase(std::remove_if(props.begin(),
                                   props.end(),
                                   [px, py](const client::WorldRenderCache::DrawnProp& p) {
                                       return p.patch_x == px && p.patch_y == py;
                                   }),
                    props.end());
        loaded_patches.erase(key);
        loaded_opl.erase(key);
    };

    auto ensure_world_around =
        [&](float wx, float wz, int max_new = -1, bool show_loading = false) -> bool {
        const int cpx = coords::patch_index_for_client(wx);
        const int cpy = coords::patch_index_for_client(wz);
        while (static_cast<int>(drawn.size()) > resident_tile_limit) {
            auto victim = drawn.end();
            int victim_distance = -1;
            for (auto it = drawn.begin(); it != drawn.end(); ++it) {
                const int distance =
                    std::max(std::abs(it->patch_x - cpx), std::abs(it->patch_y - cpy));
                if (distance > stream_search_radius && distance > victim_distance) {
                    victim = it;
                    victim_distance = distance;
                }
            }
            if (victim == drawn.end()) {
                break;
            }
            drop_streamed_patch(victim->patch_x, victim->patch_y);
        }
        const auto patch_is_needed = [&](int px, int py) {
            return std::max(std::abs(px - cpx), std::abs(py - cpy)) <= stream_search_radius;
        };
        if (max_new >= 0) {
            const auto upload_started = std::chrono::steady_clock::now();
            bool uploaded_one = false;
            for (;;) {
                const double elapsed_ms = std::chrono::duration<double, std::milli>(
                                              std::chrono::steady_clock::now() - upload_started)
                                              .count();
                if (uploaded_one && elapsed_ms >= terrain_upload_budget_ms) {
                    break;
                }
                auto ready = terrain_stream.take_ready();
                if (!ready) {
                    break;
                }
                const auto key = patch_key(ready->patch_x, ready->patch_y);
                if (!patch_is_needed(ready->patch_x, ready->patch_y) || loaded_patches.count(key)) {
                    continue;
                }
                loaded_patches.insert(key);
                if (!ready->prepared) {
                    load_opl_patch(ready->patch_x, ready->patch_y);
                    continue;
                }
                auto drawn_patch = client::WorldRenderCache::upload_prepared_patch(
                    *engine, *ready->prepared, detail_cache);
                if (drawn_patch.mesh && !drawn_patch.layers.empty()) {
                    drawn_patch.patch_x = ready->patch_x;
                    drawn_patch.patch_y = ready->patch_y;
                    drawn.push_back(std::move(drawn_patch));
                    floor.add_patch(std::move(ready->prepared->patch));
                    uploaded_one = true;
                }
                load_opl_patch(ready->patch_x, ready->patch_y);
            }
        }
        int added = 0;
        int scheduled = 0;
        const int search_radius = stream_search_radius;
        const int total = (search_radius * 2 + 1) * (search_radius * 2 + 1);
        int processed = 0;
        for (int dy = -search_radius; dy <= search_radius; ++dy) {
            for (int dx = -search_radius; dx <= search_radius; ++dx) {
                const int px = cpx + dx;
                const int py = cpy + dy;
                const bool already_loaded = loaded_patches.count(patch_key(px, py)) != 0;
                if (show_loading && processed % 4 == 0) {
                    const float map_progress = static_cast<float>(processed) / total;
                    boot_map_progress = std::max(boot_map_progress, map_progress);
                    const float progress = entering_world
                                               ? (2.0f + boot_map_progress) / kWorldLoadingStages
                                               : map_progress;
                    if (!loading_screen.present("Map", progress)) {
                        return false;
                    }
                }
                ++processed;
                if (already_loaded) {
                    load_opl_patch(px, py);
                    continue;
                }
                if (max_new >= 0 && scheduled >= max_new) {
                    continue;
                }
                if (max_new >= 0) {
                    const int distance = std::abs(dx) + std::abs(dy);
                    if (terrain_stream.request(px, py, 100 - distance)) {
                        ++scheduled;
                    }
                    continue;
                }
                const std::string path = client::WorldRenderCache::patch_path(maps_dir, px, py);
                if (!platform::file_exists(path)) {
                    loaded_patches.insert(patch_key(px, py));
                    load_opl_patch(px, py);
                    continue;
                }
                terrain::HeightPatch patch;
                if (!terrain::load_kcm(path, patch)) {
                    loaded_patches.insert(patch_key(px, py));
                    load_opl_patch(px, py);
                    continue;
                }
                patch.patch_x = px;
                patch.patch_y = py;
                client::WorldRenderCache::DrawnPatch d = client::WorldRenderCache::upload_patch(
                    *engine, patch, tex_dir, env_layers, detail_cache);
                if (d.mesh && !d.layers.empty()) {
                    d.patch_x = px;
                    d.patch_y = py;
                    drawn.push_back(std::move(d));
                    floor.add_patch(std::move(patch));
                    ++added;
                }
                loaded_patches.insert(patch_key(px, py));
                load_opl_patch(px, py);
            }
        }
        if (added > 0) {
            log::info("streamed " + std::to_string(added) + " patches around (" +
                      std::to_string(cpx) + "," + std::to_string(cpy) +
                      ") keep=" + std::to_string(drawn.size()));
        }
        if (show_loading) {
            boot_map_progress = 1.0f;
            if (!loading_screen.present("Map",
                                        entering_world ? 3.0f / kWorldLoadingStages : 1.0f)) {
                return false;
            }
        }
        return true;
    };

    if (!loading_screen.present("Map", 2.0f / kWorldLoadingStages)) {
        if (server_loader.joinable()) {
            server_loader.join();
        }
        embedded_server.stop();
        if (store) {
            store->close();
        }
        ui.destroy();
        engine->shutdown();
        return 0;
    }
    auto reset_to_profile_spawn = [&]() {
        spawn_sx = profile.spawn_server_x();
        spawn_sy = profile.spawn_server_y();
        spawn_sz = profile.spawn_server_z();
        local_map = 0;
        spawn_x = coords::to_client(spawn_sx, profile.origin_patch());
        spawn_z = coords::to_client(spawn_sy, profile.origin_patch());
    };
    if (!uses_indoor_visuals()) {
        if (!ensure_world_around(spawn_x, spawn_z, -1, true)) {
            if (server_loader.joinable()) {
                server_loader.join();
            }
            embedded_server.stop();
            if (store) {
                store->close();
            }
            ui.destroy();
            engine->shutdown();
            return 0;
        }
        if (drawn.empty()) {
            log::warn("no patches at saved pose; falling back to spawn");
            reset_to_profile_spawn();
            if (!ensure_world_around(spawn_x, spawn_z, -1, true)) {
                if (server_loader.joinable()) {
                    server_loader.join();
                }
                embedded_server.stop();
                if (store) {
                    store->close();
                }
                ui.destroy();
                engine->shutdown();
                return 0;
            }
            if (drawn.empty()) {
                log::error("no patches near Narootuh ? check MAPS data");
                if (server_loader.joinable()) {
                    server_loader.join();
                }
                embedded_server.stop();
                if (store) {
                    store->close();
                }
                ui.destroy();
                engine->shutdown();
                return 1;
            }
        }
    } else {
        log::info("indoor spawn map=" + std::to_string(local_map) + " at (" +
                  std::to_string(spawn_sx) + "," + std::to_string(spawn_sy) + ")");
    }
    log::info("OPL textured draws=" + std::to_string(opl_ok) +
              " missing_gb=" + std::to_string(opl_missing_gb) +
              " unique_models=" + std::to_string(model_cache.size()) + " floor_props=" +
              std::to_string(floor.props().size()) + " patches=" + std::to_string(drawn.size()));

    // GenNPC list — drawn only while inside compatibility behavior 25-tile sight (like monsters).
    // Load names, spawns, and appearance from the JSON catalog.
    client::ExpEventBoard exp_event_board(*engine);
    client::NpcCatalog npc_catalog;
    std::vector<client::NpcSpawn> npc_spawns;
    if (!npc_catalog.load_json_dir(npcs_dir)) {
        log::warn("npc json load failed: " + npcs_dir);
    } else {
        log::info("NPC total=" + std::to_string(npc_catalog.spawns().size()) + " " + npcs_dir);
    }
    std::array<const client::NpcSpawn*, 8> doggebi_mirrors{};
    const client::NpcSpawn* doggebi_throne = nullptr;
    for (const client::NpcSpawn& spawn : npc_catalog.spawns()) {
        if (spawn.map() != 3) {
            continue;
        }
        if (spawn.index() == 383) {
            doggebi_throne = &spawn;
        }
        const int slot = spawn.scenario_slot();
        if (slot >= 0 && slot < static_cast<int>(doggebi_mirrors.size())) {
            doggebi_mirrors[static_cast<std::size_t>(slot)] = &spawn;
        }
    }
    auto refresh_npc_candidates = [&](int around_sx, int around_sy) {
        constexpr int kNpcLoadPx = 16384;
        npc_spawns.clear();
        for (const auto& s : npc_catalog.spawns()) {
            if (std::abs(s.server_x() - around_sx) > kNpcLoadPx ||
                std::abs(s.server_y() - around_sy) > kNpcLoadPx) {
                continue;
            }
            npc_spawns.push_back(s);
        }
    };
    refresh_npc_candidates(spawn_sx, spawn_sy);
    log::info("NPC candidates=" + std::to_string(npc_spawns.size()));

    // Global water plane (compatibility behavior): flat quad at WATER_POS_Y around the
    // currently active outdoor area.
    engine::MeshHandle water_mesh{};
    engine::TextureHandle water_tex{};
    {
        terrain::ImageRgba water_img;
        const std::string water_path = platform::join_path(
            platform::join_path(profile.data_root(), "OBJECTS/common/tex"), "water_001.dds");
        if (terrain::load_gtx_rgba(water_path, water_img) && !water_img.rgba.empty()) {
            for (std::size_t i = 3; i < water_img.rgba.size(); i += 4) {
                water_img.rgba[i] = 102;
            }
            water_tex = engine->create_texture_rgba8(
                water_img.rgba.data(), water_img.width, water_img.height, true);
            log::info("water texture loaded: " + water_path);
        } else {
            log::warn("water texture missing: " + water_path);
        }
    }
    auto rebuild_water_mesh = [&](float center_x, float center_z) {
        if (water_mesh) {
            engine->destroy_mesh(water_mesh);
            water_mesh = {};
        }
        const int center_patch_x = coords::patch_index_for_client(center_x);
        const int center_patch_y = coords::patch_index_for_client(center_z);
        float min_x = 1e30f, max_x = -1e30f, min_z = 1e30f, max_z = -1e30f;
        for (int py = center_patch_y - kWaterRadius; py <= center_patch_y + kWaterRadius; ++py) {
            for (int px = center_patch_x - kWaterRadius; px <= center_patch_x + kWaterRadius;
                 ++px) {
                const float x0 = static_cast<float>((px - map::kPatchGrid / 2) * map::kMapSize *
                                                    map::kTerrainCellSize);
                const float z0 = static_cast<float>((py - map::kPatchGrid / 2) * map::kMapSize *
                                                    map::kTerrainCellSize);
                const float extent = static_cast<float>(map::kMapSize * map::kTerrainCellSize);
                min_x = std::min(min_x, x0);
                max_x = std::max(max_x, x0 + extent);
                min_z = std::min(min_z, z0);
                max_z = std::max(max_z, z0 + extent);
            }
        }
        const float uv_scale = 1.0f / static_cast<float>(map::kTerrainCellSize * 4);
        const float y = map::kWaterPosY;
        const std::vector<engine::VertexP3T2> vertices = {
            {min_x, y, min_z, min_x * uv_scale, min_z * uv_scale},
            {max_x, y, min_z, max_x * uv_scale, min_z * uv_scale},
            {max_x, y, max_z, max_x * uv_scale, max_z * uv_scale},
            {min_x, y, max_z, min_x * uv_scale, max_z * uv_scale},
        };
        const std::uint32_t indices[6] = {0, 1, 2, 0, 2, 3};
        water_mesh = engine->create_mesh(vertices.data(), vertices.size(), indices, 6);
        log::info("water plane centered at patch " + std::to_string(center_patch_x) + "," +
                  std::to_string(center_patch_y));
    };

    // Offline local player from selected character.
    client::LocalPlayer local_player;
    local_player.set_clazz(
        client::BootShell::clazz_from_job(static_cast<std::uint8_t>(boot.selected.clazz)));
    local_player.set_pos(spawn_x, floor.sample(spawn_x, spawn_z, spawn_y), spawn_z);

    // Authoritative walk masks from OpenKal area maps (not visual data/MAPS).
    map::WorldGrid walk_grid;
    const int walk_cells =
        map::MapStream::ensure_around(walk_grid, server_map_dir, spawn_sx, spawn_sy, 2);
    log::info("walk area cells loaded=" + std::to_string(walk_cells) + " from " + server_map_dir);
    std::int32_t player_max_hp = boot.selected.max_hp > 0 ? boot.selected.max_hp : 100;
    std::int32_t player_hp = std::clamp(boot.selected.hp, 0, player_max_hp);
    std::int32_t player_max_mp = boot.selected.max_mp > 0 ? boot.selected.max_mp : 50;
    std::int32_t player_mp = std::clamp(boot.selected.mp, 0, player_max_mp);
    std::chrono::steady_clock::time_point village_revival_deadline{};
    std::int64_t player_exp_current = 0;
    std::int64_t player_exp_total = 5;
    bool local_riding_mounted = false;
    float local_riding_offset_y = 0.0f;
    client::AnimatedActor player_actor;
    player_actor.set_appearance(boot.selected.face, boot.selected.hair);
    if (!player_actor.load(*engine, profile.data_root(), local_player.clazz(), prop_tex_cache)) {
        log::warn(std::string("failed to load animated player for ") +
                  client::LocalPlayer::class_name(local_player.clazz()));
    }
    auto player_world_matrix = [&]() {
        auto model = local_player.model_matrix();
        if (local_riding_mounted) {
            model.m[13] += local_riding_offset_y;
        }
        const float scale = player_actor.metamo_scale();
        if (player_actor.metamo_type() >= 0 && std::fabs(scale - 1.0f) > 0.001f) {
            model = engine::Mat4::multiply(model, engine::Mat4::scaling(scale, scale, scale));
        }
        return model;
    };
    client::CombatFx combat_fx;
    if (!combat_fx.init(*engine, profile.data_root())) {
        log::warn("combat FX init failed — lightning/arrow effects may be missing");
    }
    client::SkillPresenter skill_presenter;
    skill_presenter.set_combat_fx(&combat_fx);
    client::SpriteFx sprite_fx;
    if (!sprite_fx.init(*engine, profile.data_root())) {
        log::warn("sprite FX init failed — monster particles may be missing");
    }
    sprite_fx.set_cull_distance_scale(static_cast<float>(client_settings.world.effect_distance) /
                                      300.0f);
    client::KnightFxDispatcher knight_fx_dispatcher(sprite_fx, combat_fx);
    skill_presenter.set_knight_fx_dispatcher(&knight_fx_dispatcher);
    client::ArcherFxDispatcher archer_fx_dispatcher(sprite_fx, combat_fx);
    skill_presenter.set_archer_fx_dispatcher(&archer_fx_dispatcher);
    std::unordered_set<std::uint32_t> npc_fx_live;
    std::unordered_set<std::uint32_t> world_prop_fx_live;
    combat_fx.set_audio(&sfx);
    combat_fx.set_sprite_fx(&sprite_fx);
    client::SoundHandle skill_loop;
    std::unordered_map<std::uint32_t, client::SoundHandle> remote_skill_loops;

    client::FollowCamera cam;
    cam.set_yaw_deg(45.0f);
    std::uint8_t doggebi_ritual_phase = 0;
    std::uint8_t doggebi_ritual_active_mask = 0;
    bool doggebi_ritual_state_received = false;
    std::string doggebi_ritual_overlay;
    std::chrono::steady_clock::time_point doggebi_ritual_overlay_until{};
    protocol::MapStateTimer doggebi_countdown_timer = protocol::MapStateTimer::None;
    std::uint32_t doggebi_countdown_seconds = 0;
    std::chrono::steady_clock::time_point doggebi_countdown_received{};
    constexpr std::uint32_t kDoggebiMirrorFxKey = 0x30000000u;
    constexpr std::uint32_t kDoggebiThroneFxKey = 0x30000010u;
    constexpr std::uint32_t kDoggebiQuakeFxKey = 0x30000020u;
    // compatibility behavior places the Lord just in front of the throne NPC.
    constexpr int kDoggebiLordSpawnServerX = 57367;
    constexpr int kDoggebiLordSpawnServerY = 8931;
    std::uint8_t doggebi_quake_fx_phase = 0;
    auto sync_doggebi_ritual_fx = [&]() {
        if (!doggebi_throne) {
            return;
        }
        const engine::Vec3 lord_spawn{
            coords::to_client(kDoggebiLordSpawnServerX, profile.origin_patch()),
            coords::to_client_y(doggebi_throne->server_z()),
            coords::to_client(kDoggebiLordSpawnServerY, profile.origin_patch())};
        // Doggaebi_54 is authored with a 160-unit vertical offset, so the beams
        // converge at the same point as the Lord's persistent summoning effect.
        engine::Vec3 beam_target = lord_spawn;
        beam_target.y += 160.0f;
        for (std::size_t slot = 0; slot < doggebi_mirrors.size(); ++slot) {
            const std::uint32_t key = kDoggebiMirrorFxKey + static_cast<std::uint32_t>(slot);
            const bool active =
                (doggebi_ritual_active_mask & static_cast<std::uint8_t>(1u << slot)) != 0;
            const client::NpcSpawn* mirror = doggebi_mirrors[slot];
            if (!active || !mirror) {
                sprite_fx.detach(key);
                continue;
            }
            if (!sprite_fx.has(key)) {
                sprite_fx.attach_particle(*engine, key, "tm9_mirrorlight2", -1, 0.0f, 0.0f, 0.0f);
            }
            const engine::Vec3 source{
                coords::to_client(mirror->server_x(), profile.origin_patch()),
                coords::to_client_y(mirror->server_z()) + 20.0f,
                coords::to_client(mirror->server_y(), profile.origin_patch())};
            sprite_fx.follow_beam(key, -1, source, beam_target);
        }
        if (doggebi_ritual_active_mask == 0xffu) {
            if (!sprite_fx.has(kDoggebiThroneFxKey)) {
                sprite_fx.attach_fx(*engine, kDoggebiThroneFxKey, "Doggaebi_54", -1);
                sprite_fx.attach_fx(*engine, kDoggebiThroneFxKey, "Doggaebi_55", -1);
                sprite_fx.persist_attached(kDoggebiThroneFxKey);
            }
            sprite_fx.follow_all(kDoggebiThroneFxKey, lord_spawn.x, lord_spawn.y, lord_spawn.z);
        } else {
            sprite_fx.detach(kDoggebiThroneFxKey);
        }
    };
    auto sync_doggebi_quake_fx = [&](const engine::Vec3& camera_eye) {
        std::string_view particle;
        if (doggebi_ritual_active_mask == 0xffu) {
            switch (doggebi_ritual_phase) {
            case 1:
            case 2:
                particle = "quakedust1";
                break;
            case 3:
            case 4:
                particle = "quakedust2";
                break;
            default:
                break;
            }
        }
        if (particle.empty()) {
            sprite_fx.detach(kDoggebiQuakeFxKey);
            doggebi_quake_fx_phase = 0;
            return;
        }
        if (doggebi_quake_fx_phase != doggebi_ritual_phase) {
            sprite_fx.detach(kDoggebiQuakeFxKey);
            doggebi_quake_fx_phase = doggebi_ritual_phase;
        }
        if (!sprite_fx.has(kDoggebiQuakeFxKey)) {
            sprite_fx.attach_particle(*engine, kDoggebiQuakeFxKey, particle, -1, 0.0f, 0.0f, 0.0f);
        }
        sprite_fx.follow_all(kDoggebiQuakeFxKey, camera_eye.x, camera_eye.y, camera_eye.z);
    };
    engine->set_cursor_mode(engine::CursorMode::Free);

    // Finish background MainSvr + catalog parses (started during map upload).
    client::NetClient net;
    net.set_read_only(read_only_session);
    const bool want_net = true;
    while (server_fut.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready ||
           item_catalog_fut.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready ||
           skill_catalog_fut.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
        if (!loading_screen.present("MainSvr / configs", 4.0f / kWorldLoadingStages)) {
            if (server_loader.joinable()) {
                server_loader.join();
            }
            embedded_server.stop();
            if (store) {
                store->close();
            }
            ui.destroy();
            engine->shutdown();
            return 0;
        }
    }
    if (server_loader.joinable()) {
        server_loader.join();
    }
    const bool embedded_server_ready = server_fut.get();
    if (!embedded_server_ready) {
        log::error("failed to start embedded MainSvr");
    }

    // Monster GB rigs (shared by index) + per-entity skinned instances.
    // Load appearance and shared bones from the JSON catalog.
    client::MonsterCatalog monster_catalog;
    if (!monster_catalog.load_json_dir(monsters_dir)) {
        log::warn("monster appearance JSON load failed: " + monsters_dir);
    } else {
        log::info("monster appearance from " + monsters_dir);
    }
    std::unordered_map<int, client::MonsterRigGpu> monster_rig_cache;
    std::unordered_map<int, client::MonsterRigGpu> monster_behead_rig_cache;
    std::unordered_map<std::uint32_t, client::MonsterInstance> monster_instances;
    std::unordered_map<int, client::MonsterRigGpu> pet_rig_cache;
    client::MonsterInstance pet_instance;
    client::PetController pet_controller;
    std::vector<client::PetPickupTarget> pet_pickup_targets;
    int pet_controller_item_index = 0;
    int pet_rendered_item_index = 0;
    std::uint64_t equipped_pet_expire_at = 0;
    std::unordered_map<int, client::MonsterRigGpu> riding_rig_cache;
    client::MonsterInstance riding_instance;
    int riding_rendered_item_index = 0;
    std::uint64_t registered_ride_expire_at = 0;
    std::uint32_t registered_ride_iid = 0;
    protocol::RidingKind registered_riding_kind = protocol::RidingKind::Common;
    protocol::BattleRidingGrade registered_battle_grade = protocol::BattleRidingGrade::Normal;
    std::uint8_t registered_battle_rarity = 0;
    std::uint16_t registered_battle_satiety = 0;
    std::uint16_t registered_battle_max_satiety = 0;
    std::array<std::uint32_t, 5> registered_battle_equipment_iids{};
    std::array<std::uint16_t, 5> registered_battle_equipment_indices{};
    std::array<std::uint8_t, 5> registered_battle_equipment_enchants{};
    std::uint32_t next_battle_riding_operation_id = 1;
    std::unordered_map<std::uint32_t, std::vector<client::SoundHandle>> monster_loop_sfx;
    std::unordered_set<std::uint32_t> monster_loop_armed;
    std::unordered_set<std::uint32_t> freeze_fx_armed;

    std::uint32_t selected_mob_id = 0;
    std::uint32_t selected_npc_id = 0;
    std::uint32_t selected_player_id = 0;
    client::RemotePlayers remote_players;
    std::unordered_set<std::uint32_t> gathering_players;
    std::unordered_set<std::uint32_t> defeated_castle_players;
    std::uint32_t pending_arrow_target = 0;
    struct LatchedBoltFloat {
        client::CombatFloatKind kind = client::CombatFloatKind::DamageDealt;
        std::string text;
        std::uint32_t anchor_id = 0;
        float age = 0.0f;
    };
    std::vector<LatchedBoltFloat> latched_bolt_floats;
    struct DelayDamageHud {
        std::uint32_t target_id = 0;
        int saved_damage = 0;
        std::chrono::steady_clock::time_point start{};
        bool active = false;
    };
    DelayDamageHud delay_damage_hud;
    std::chrono::steady_clock::time_point player_overhead_hp_until{};
    std::uint32_t selected_item_id = 0;
    /** Set by Space: walk to this pouch and pick automatically on arrival. */
    std::uint32_t pending_pickup_id = 0;
    /** Walk-then-talk NPC target after LMB out of range. */
    std::uint32_t pending_ask_npc_id = 0;
    /** Chest currently being opened (set only after MSG_ITEMMONSTEROPEN). */
    std::uint32_t opening_chest_id = 0;
    /** Chest we sent GETITEMMONSTER for while waiting for Open / rejection. */
    std::uint32_t pending_chest_id = 0;
    /** Cooldown before re-sending GETITEMMONSTER for the same chest. */
    float chest_open_retry = 0.0f;
    /** True while the server-authoritative compatibility behavior flag is active. */
    bool fishing_active = false;
    /** Start of the currently displayed fishing cycle. */
    std::chrono::steady_clock::time_point fishing_cycle_started{};
    /** True while the server-authoritative compatibility behavior flag is active. */
    bool cooking_active = false;
    /** Rising-edge guard for the item-sourced ET_HASTE activation model. */
    bool item_haste_fx_active = false;
    /** Start of the currently displayed Speed-up Medicine production cycle. */
    std::chrono::steady_clock::time_point cooking_started{};
    /** Locally submitted recipe awaiting authoritative inventory changes. */
    std::optional<protocol::CookingOrder> pending_cooking_order;
    /** Last chest-open attempt summary for DEBUG HUD (persists across frames). */
    std::string chest_debug_last;
    bool auto_attack = false;
    /** True while auto-attack should recast the selected combat skill (RMB / mage). */
    bool auto_skill = false;
    if (!loading_screen.present("Items / skills", 5.0f / kWorldLoadingStages)) {
        embedded_server.stop();
        if (store) {
            store->close();
        }
        ui.destroy();
        engine->shutdown();
        return 0;
    }
    client::ItemDb item_catalog = item_catalog_fut.get();
    client::Catalog skill_catalog = skill_catalog_fut.get();
    net.set_skill_catalog(&skill_catalog);
    net.set_item_catalog(&item_catalog);
    client::Bag player_bag;
    client::Wear player_wear;
    if (boot.selected.weapon_index > 0) {
        player_wear.set_weapon_index(boot.selected.weapon_index);
    }
    if (boot.selected.armor_index > 0) {
        player_wear.set_cloth_index(client::ClothPart::Upper, boot.selected.armor_index);
    }
    if (boot.selected.transform_index > 0) {
        player_wear.set_transform_index(boot.selected.transform_index);
    }
    client::Window inv_win;
    client::EnchantOverlay enchant_overlay;
    std::uint32_t enchantment_result_iid = 0;
    client::RidingCacheOverlay riding_cache_overlay;
    inv_win.set_catalog(&item_catalog);
    inv_win.set_wear(&player_wear);
    inv_win.set_riding_equipment(&registered_battle_equipment_iids);
    const std::unordered_map<int, client::PetTooltipInfo> pet_tooltip_info = [&pet_presentations] {
        std::unordered_map<int, client::PetTooltipInfo> result;
        result.reserve(pet_presentations.size());
        for (const auto& [item_index, pet] : pet_presentations) {
            result.emplace(item_index,
                           client::PetTooltipInfo{
                               pet.heal_per_two_seconds, pet.auto_pickup, pet.duration_seconds});
        }
        return result;
    }();
    inv_win.set_pet_tooltip_info(pet_tooltip_info);
    inv_win.set_layout(&ui_layouts.get("inventory"));
    inv_win.set_player_clazz(static_cast<int>(local_player.clazz()));
    inv_win.set_player_level(std::max(1, static_cast<int>(boot.selected.level)));
    inv_win.sync_from_layout();
    player_bag.set_compact_register_enabled(inv_win.register_enabled());
    client::PlayerSkills player_skills;
    player_skills.set_multiplayer(multiplayer);
    client::SkillWindow skill_win;
    skill_win.set_catalog(&skill_catalog);
    skill_win.set_multiplayer(multiplayer);
    skill_win.set_player(&player_skills);
    skill_win.set_player_clazz(inv_win.player_clazz());
    skill_win.set_player_level(inv_win.player_level());
    skill_win.set_player_specialty(static_cast<int>(protocol::kStartingSpecialty));
    skill_win.set_layout(&ui_layouts.get("skill_window"));
    skill_win.sync_from_layout();
    player_skills.seed_for_class(skill_win.player_clazz(), skill_win.player_level(), skill_catalog);

    client::ShinsuDb shinsu_db;
    if (!shinsu_db.load_dir(shinsu_dir)) {
        log::warn("shinsu catalog empty: " + shinsu_dir);
    }
    inv_win.set_shinsu(&shinsu_db);
    client::TransformWindow transform_win;
    transform_win.set_catalog(&skill_catalog);
    transform_win.set_items(&item_catalog);
    transform_win.set_shinsu(&shinsu_db);
    transform_win.set_layout(&ui_layouts.get("transform_window"));
    transform_win.sync_from_layout();
    client::UiDocument riding_view;
    client::UiView::reload(riding_view, &ui_layouts.get("riding"));
    client::RidingUiHost riding_host(item_catalog);
    client::FreetypeFont riding_body_font;
    client::FreetypeFont riding_title_font;
    client::InGameWindowOrder in_game_window_order;
    std::string riding_feedback;
    int riding_pressed_slot = -1;
    std::uint32_t riding_pressed_iid = 0;
    bool riding_equipment_dragging = false;
    float riding_press_x = 0.0f;
    float riding_press_y = 0.0f;
    bool riding_window_open = false;
    bool world_map_open = false;
    client::TalentTreeWindow talent_tree;
    client::PowerbookWindow powerbook;
    powerbook.set_layout(&ui_layouts.get("powerbook"));
    talent_tree.set_class(boot.selected.clazz);
    talent_tree.set_layout(&ui_layouts.get("talent_tree"));
    client::MinimapWidgetData world_map_data;
    client::WorldMapWidget::TileLoadState world_map_tiles;
    bool riding_window_positioned = false;
    bool riding_window_dragging = false;
    std::uint32_t stablemaster_npc_id = 0;
    std::uint32_t stablemaster_duplicate_iid = 0;
    float riding_window_x = 0.0f;
    float riding_window_y = 0.0f;
    float riding_window_drag_dx = 0.0f;
    float riding_window_drag_dy = 0.0f;
    const auto riding_hit_at = [&](float mx, float my, float scale) {
        if (!riding_window_open) {
            return client::UiHit{};
        }
        client::UiVarMap vars;
        vars["riding.battle"] =
            registered_riding_kind == protocol::RidingKind::Battle && registered_ride_iid != 0
                ? "true"
                : "false";
        vars["stablemaster.open"] = stablemaster_npc_id != 0 ? "true" : "false";
        return client::UiView::hit(riding_view,
                                   &ui,
                                   mx,
                                   my,
                                   riding_window_x,
                                   riding_window_y,
                                   riding_view.window_w() * scale,
                                   riding_view.window_h() * scale,
                                   scale,
                                   vars,
                                   &riding_host,
                                   true);
    };
    const auto close_riding_window = [&]() {
        riding_window_open = false;
        stablemaster_npc_id = 0;
        riding_window_dragging = false;
        riding_equipment_dragging = false;
        riding_pressed_slot = -1;
        riding_pressed_iid = 0;
        riding_host.set_pointer({}, false);
        in_game_window_order.closed(client::InGameWindowId::Riding);
    };
    const auto toggle_riding_window = [&](float available_w, float available_h, float scale) {
        if (riding_window_open) {
            close_riding_window();
            return;
        }
        riding_window_open = true;
        stablemaster_npc_id = 0;
        riding_feedback.clear();
        riding_window_dragging = false;
        in_game_window_order.opened(client::InGameWindowId::Riding);
        if (!riding_window_positioned) {
            const float riding_w = riding_view.window_w() * scale;
            const float riding_h = riding_view.window_h() * scale;
            riding_window_x = std::max(0.0f, (available_w - riding_w) * 0.5f);
            riding_window_y = std::max(0.0f, (available_h - riding_h) * 0.35f);
            riding_window_positioned = true;
        }
    };
    const auto send_riding_equipment = [&](protocol::BattleRidingOperation operation,
                                           std::uint32_t primary_iid) {
        if (!want_net || !net.connected() || registered_ride_iid == 0 ||
            registered_riding_kind != protocol::RidingKind::Battle) {
            return;
        }
        protocol::BattleRidingRequest request;
        request.operation = operation;
        request.operation_id = next_battle_riding_operation_id++;
        if (next_battle_riding_operation_id == 0) {
            next_battle_riding_operation_id = 1;
        }
        request.mount_iid = registered_ride_iid;
        request.primary_iid = primary_iid;
        if (net.send_battle_riding_request(request)) {
            riding_feedback.clear();
        }
    };
    int pending_transform_execute_stage = 0;
    float pending_transform_execute_left = 0.0f;
    constexpr std::uint32_t kShinsuPrtBase = 0xD0000200u;
    constexpr int kShinsuPrtMax = 8;

    client::CharacterInfoWindow char_info_win;
    char_info_win.set_layout(&ui_layouts.get("character_info"));
    char_info_win.sync_from_layout();
    char_info_win.set_apply_property([&](std::uint8_t property, std::uint8_t count) {
        if (!want_net || !net.connected()) {
            return false;
        }
        return net.send_update_property(property, count);
    });
    auto update_character_info_equipment_bonuses = [&]() {
        auto bonuses = client::collection_primary_bonuses(player_bag);
        for (const client::ItemStack& stack : player_bag.equipment) {
            if (!stack.equipped || stack.count <= 0 || stack.index <= 0) {
                continue;
            }
            const client::ItemDef* item = item_catalog.find(stack.index);
            if (!item) {
                continue;
            }

            bonuses[0] += item->str();
            bonuses[1] += item->hth();
            bonuses[2] += item->intel();
            bonuses[3] += item->wis();
            bonuses[4] += item->dex();

            if (const client::PrefixDef* prefix = item_catalog.prefixes().find(stack.prefix)) {
                bonuses[0] += prefix->str();
                bonuses[1] += prefix->hth();
                bonuses[2] += prefix->intel();
                bonuses[3] += prefix->wis();
                bonuses[4] += prefix->dex();
            }

            if (!item->is_defense()) {
                continue;
            }
            const int value = std::clamp(stack.random_stat_value, 0, 5);
            switch (static_cast<protocol::RandomItemStatType>(stack.random_stat_type)) {
            case protocol::RandomItemStatType::ArmorStrength:
            case protocol::RandomItemStatType::ArmorDefensePercentStrength:
                bonuses[0] += value;
                break;
            case protocol::RandomItemStatType::ArmorHealth:
            case protocol::RandomItemStatType::ArmorDefensePercentHealth:
                bonuses[1] += value;
                break;
            case protocol::RandomItemStatType::ArmorIntelligence:
            case protocol::RandomItemStatType::ArmorDefensePercentIntelligence:
                bonuses[2] += value;
                break;
            case protocol::RandomItemStatType::ArmorWisdom:
            case protocol::RandomItemStatType::ArmorDefensePercentWisdom:
                bonuses[3] += value;
                break;
            case protocol::RandomItemStatType::ArmorDexterity:
            case protocol::RandomItemStatType::ArmorDefensePercentDexterity:
                bonuses[4] += value;
                break;
            default:
                break;
            }
        }
        char_info_win.set_equipment_primary_bonuses(bonuses);
    };

    client::DialogCatalog dialog_catalog;
    if (platform::is_directory(dialogs_dir)) {
        if (!dialog_catalog.load_dir(dialogs_dir)) {
            log::warn("dialog catalog empty: " + dialogs_dir);
        }
    }
    client::TaskCatalog task_catalog;
    if (platform::is_directory(tasks_dir)) {
        if (!task_catalog.load_dir(tasks_dir)) {
            log::warn("task catalog empty: " + tasks_dir);
        }
    }
    server::QuestDb quest_db;
    if (platform::is_directory(quests_dir)) {
        if (!quest_db.load_dir(quests_dir)) {
            log::warn("quest db empty: " + quests_dir);
        }
    }
    if (platform::is_directory(tasks_dir) && !quest_db.load_task_levels_dir(tasks_dir)) {
        log::warn("quest level requirements empty: " + tasks_dir);
    }
    map::PortalCatalog portal_catalog;
    if (platform::file_exists(portals_path)) {
        if (!portal_catalog.load_json(portals_path)) {
            log::warn("portal catalog empty: " + portals_path);
        }
    }
    map::MiniMapSizeCatalog dungeon_minimaps;
    if (platform::file_exists(dungeon_minimaps_path)) {
        if (!dungeon_minimaps.load_json(dungeon_minimaps_path)) {
            log::warn("dungeon minimap sizes empty: " + dungeon_minimaps_path);
        }
    }
    client::IndoorMapView indoor_view;
    client::NpcTalkWindow npc_talk_win;
    npc_talk_win.set_layout(&ui_layouts.get("npc_talk"));
    client::SpeedMedicineWindow speed_medicine_win;
    speed_medicine_win.set_layout(&ui_layouts.get("speed_medicine"));
    speed_medicine_win.sync_from_layout();
    client::ConsumableCraftingWindow consumable_crafting_win;
    consumable_crafting_win.set_layout(&ui_layouts.get("consumable_crafting"));
    consumable_crafting_win.sync_from_layout();
    client::BlacksmithWindow blacksmith_win;
    blacksmith_win.set_layout(&ui_layouts.get("blacksmith"));
    blacksmith_win.sync_from_layout();
    client::MixWindow mix_win;
    mix_win.set_layout(&ui_layouts.get("mix_master_window"));
    mix_win.set_material_layout(&ui_layouts.get("mixing_item_list"));
    mix_win.sync_from_layout();
    server::MixCatalog mix_catalog;
    if (!mix_catalog.load_dir(mixing_dir)) {
        log::warn("mix catalog empty: " + mixing_dir);
    }
    mix_win.set_mix_catalog(&mix_catalog);
    client::SkinShopWindow skin_shop_win;
    skin_shop_win.set_layout(&ui_layouts.get("skin_shop"));
    skin_shop_win.sync_from_layout();
    auto pending_skin_category = client::SkinShopCategory::Weapons;
    client::ShopWindow shop_win;
    shop_win.set_layout(&ui_layouts.get("shop"));
    shop_win.set_pet_tooltip_info(pet_tooltip_info);
    shop_win.sync_from_layout();
    client::StorageWindow storage_win;
    storage_win.set_layout(&ui_layouts.get("storage"));
    storage_win.sync_from_layout();
    client::QuestJournalWindow quest_journal_win;
    quest_journal_win.set_layout(&ui_layouts.get("quest_journal"));
    quest_journal_win.set_snitch_context(
        &ui_layouts.get("quest_snitch"), configuration_root, npc_catalog.spawns());
    // Server CanReply stays at 64. SendAskNpc uses METER*6 (60), which
    // still opens the dialog a few body-lengths away. Walk in to melee-like
    // range (METER*2.4) first so the NPC only reacts up close.
    constexpr int kNpcTalkEngageRange = protocol::kNpcInteractionRange;
    {

        net.set_char_identity(boot.selected.name.empty() ? "Hero" : boot.selected.name,
                              inv_win.player_clazz(),
                              client::LocalPlayer::class_name(local_player.clazz()));
        if (!(want_net && net.handshook())) {
            const auto base =
                openkal::server::combat::class_base_plus_bonus(inv_win.player_clazz());
            const int pu = protocol::kPuPerLevel * std::max(1, inv_win.player_level());
            net.seed_char_primaries(base.str, base.hth, base.intl, base.wis, base.dex, pu);
        } else {
            net.update(0.0f, profile.origin_patch());
        }
        char_info_win.set_stats(net.char_info_stats());
    }

    client::ItemCooldowns item_cooldowns(item_catalog);
    client::SkillBar skill_bar;
    skill_bar.set_catalog(&skill_catalog);
    skill_bar.set_items(&item_catalog);
    skill_bar.set_item_cooldowns(&item_cooldowns);
    skill_bar.set_bag(&player_bag);
    skill_bar.set_player(&player_skills);
    skill_bar.set_skill_window(&skill_win);
    skill_bar.set_player_clazz(skill_win.player_clazz());
    skill_bar.set_layout(&ui_layouts.get("dragon_hud"));
    const auto& saved_auto_hp = ui_settings.auto_hp(boot.selected.id);
    skill_bar.restore_auto_hp(saved_auto_hp.item_index, saved_auto_hp.percent);

    client::GameHud game_hud;
    game_hud.set_layouts(&ui_layouts);
    game_hud.set_server_clock(&net.server_clock());
    game_hud.set_clock_visible(client_settings.clock_enabled);
    game_hud.chat().set_battle_feed(!boot.multiplayer);
    client::GameMenu game_menu;
    game_menu.set_layout(&ui_layouts.get("game_menu"));
    client::TradingPostWindow trading_post(ui_layouts);
    trading_post.set_pet_tooltip_info(pet_tooltip_info);
    engine::FrameInput trading_post_input;
    client::TradeWindow trade_window(ui_layouts);
    engine::FrameInput trade_input;
    client::PartyWindow party_window;
    party_window.set_layouts(&ui_layouts);
    if (const auto& saved = ui_settings.party_position(boot.selected.id); saved.has_pos()) {
        party_window.set_logical_position(saved.x(), saved.y());
    }
    bool party_preferences_sent = false;
    client::SettingsWindow settings_window;
    settings_window.set_layout(&ui_layouts.get("settings"));
    client::GuildWindow guild_window;
    guild_window.set_layout(&ui_layouts.get("guild"));
    guild_window.set_rank_layout(&ui_layouts.get("guild_ranks"));
    std::uint64_t guild_view_serial = 0;
    client::PlayerOptionsWindow player_options;
    player_options.set_layouts(&ui_layouts);
    client::MailWindow mail_window;
    mail_window.set_layout(&ui_layouts.get("mail"));
    mail_window.set_read_ids(ui_settings.read_mail_ids(boot.selected.id));
    mail_window.set_entries(net.mail());
    auto update_window_order = [&](client::InGameWindowId window, bool was_open, bool is_open) {
        if (!was_open && is_open) {
            in_game_window_order.opened(window);
        } else if (was_open && !is_open) {
            in_game_window_order.closed(window);
        }
    };
    auto is_window_open = [&](client::InGameWindowId window) {
        switch (window) {
        case client::InGameWindowId::Inventory:
            return inv_win.open();
        case client::InGameWindowId::Skills:
            return skill_win.open();
        case client::InGameWindowId::CharacterInfo:
            return char_info_win.open();
        case client::InGameWindowId::Transform:
            return transform_win.open();
        case client::InGameWindowId::Riding:
            return riding_window_open;
        case client::InGameWindowId::QuestJournal:
            return quest_journal_win.open();
        case client::InGameWindowId::Guild:
            return guild_window.open();
        case client::InGameWindowId::PlayerOptions:
            return player_options.open();
        case client::InGameWindowId::Mail:
            return mail_window.open();
        case client::InGameWindowId::NpcTalk:
            return npc_talk_win.open();
        case client::InGameWindowId::Blacksmith:
            return blacksmith_win.open();
        case client::InGameWindowId::Mixing:
            return mix_win.open();
        case client::InGameWindowId::Shop:
            return shop_win.open() || skin_shop_win.open();
        case client::InGameWindowId::Storage:
            return storage_win.open();
        case client::InGameWindowId::ConsumableCrafting:
            return consumable_crafting_win.open();
        case client::InGameWindowId::SpeedMedicine:
            return speed_medicine_win.open();
        case client::InGameWindowId::Settings:
            return settings_window.open();
        }
        return false;
    };
    auto close_window = [&](client::InGameWindowId window) {
        switch (window) {
        case client::InGameWindowId::Inventory:
            inv_win.set_open(false);
            break;
        case client::InGameWindowId::Skills:
            skill_win.set_open(false);
            break;
        case client::InGameWindowId::CharacterInfo:
            char_info_win.set_open(false);
            break;
        case client::InGameWindowId::Transform:
            transform_win.set_open(false);
            break;
        case client::InGameWindowId::Riding:
            close_riding_window();
            break;
        case client::InGameWindowId::QuestJournal:
            if (!quest_journal_win.close_snitch()) {
                quest_journal_win.set_open(false);
            }
            break;
        case client::InGameWindowId::Guild:
            if (!guild_window.close_rank_dialog()) {
                guild_window.set_open(false);
            }
            break;
        case client::InGameWindowId::PlayerOptions:
            player_options.close();
            break;
        case client::InGameWindowId::Mail:
            mail_window.set_open(false);
            break;
        case client::InGameWindowId::NpcTalk:
            npc_talk_win.close();
            break;
        case client::InGameWindowId::Blacksmith:
            blacksmith_win.set_open(false);
            break;
        case client::InGameWindowId::Mixing:
            mix_win.set_open(false, mix_win.mode());
            break;
        case client::InGameWindowId::Shop:
            shop_win.close();
            skin_shop_win.close();
            break;
        case client::InGameWindowId::Storage:
            storage_win.close();
            break;
        case client::InGameWindowId::ConsumableCrafting:
            consumable_crafting_win.set_open(false);
            break;
        case client::InGameWindowId::SpeedMedicine:
            speed_medicine_win.set_open(false);
            break;
        case client::InGameWindowId::Settings:
            settings_window.close();
            break;
        }
    };
    auto close_latest_window = [&]() {
        while (const auto latest = in_game_window_order.latest()) {
            if (!is_window_open(*latest)) {
                in_game_window_order.closed(*latest);
                continue;
            }
            close_window(*latest);
            if (!is_window_open(*latest)) {
                in_game_window_order.closed(*latest);
            }
            return true;
        }
        return false;
    };
    bool hud_layout_dirty = false;
    auto persist_hud = [&] {
        ui_settings.set_auto_hp(boot.selected.id,
                                {skill_bar.auto_hp_item_index(), skill_bar.auto_hp_percent()});
        ui_settings.capture_character(
            game_hud, boot.selected.id, boot.selected.name, game_hud.scale());
        if (party_window.positioned()) {
            ui_settings.set_party_position(
                boot.selected.id, party_window.logical_x(), party_window.logical_y());
        }
        if (!ui_settings.save(ui_settings_path)) {
            log::warn("failed to save UI settings: " + ui_settings_path);
        }
        hud_layout_dirty = false;
    };
    float attack_cooldown = 0.0f;
    enum class PendingSkillKind : std::uint8_t {
        None = 0,
        Mage,
        Archer,
        Melee,
        SelfHeal,
        SelfArea,
        Player,
        DeadPlayer,
        Ground,
        Animal
    };
    bool behead_animation_active = false;
    engine::Vec3 pending_skill_start_position{};
    PendingSkillKind pending_skill_kind = PendingSkillKind::None;
    std::uint8_t pending_skill_index = 0;
    std::uint32_t pending_skill_target = 0;
    std::uint32_t pending_skill_item_iid = 0;
    std::int32_t pending_skill_ground_x = 0;
    std::int32_t pending_skill_ground_y = 0;
    /**
     * Compatibility behavior. Targeted and self-area
     * skills queue across runtime state instead of dropping the click.
     */
    int queued_skill_index = -1;
    std::uint32_t queued_skill_target = 0;
    bool queued_skill_is_player = false;
    /** Runtime state: 0 idle, 1 aiming, 2 position locked. */
    int range_skill_cast = 0;
    int range_skill_index = -1;
    float range_skill_wx = 0.0f;
    float range_skill_wz = 0.0f;
    std::int32_t range_skill_server_x = 0;
    std::int32_t range_skill_server_y = 0;
    float pending_skill_left = 0.0f;
    float pending_skill_total = 0.0f;
    float focus_shot_elapsed_sec = 0.0f;
    float rush_elapsed_sec = 0.0f;
    bool pending_face = false;
    float pending_face_x = 0.0f;
    float pending_face_z = 0.0f;

    auto refresh_player_visuals = [&]() {
        if (!player_actor.apply_wear(*engine, prop_tex_cache, player_wear, item_catalog)) {
            log::warn("Failed to refresh player wear visuals");
        }
    };

    auto clear_shinsu_particles = [&]() {
        for (int i = 0; i < kShinsuPrtMax; ++i) {
            sprite_fx.detach(kShinsuPrtBase + static_cast<std::uint32_t>(i));
        }
    };

    auto attach_shinsu_particles = [&](const client::ShinsuInfo* info) {
        clear_shinsu_particles();
        if (!info) {
            return;
        }
        const auto& parts = info->particles();
        for (std::size_t i = 0; i < parts.size() && i < static_cast<std::size_t>(kShinsuPrtMax);
             ++i) {
            const std::uint32_t id = kShinsuPrtBase + static_cast<std::uint32_t>(i);
            sprite_fx.attach_particle(*engine,
                                      id,
                                      parts[i].name(),
                                      parts[i].bone(),
                                      parts[i].ox(),
                                      parts[i].oy(),
                                      parts[i].oz());
            sprite_fx.loop_attached(id);
            sprite_fx.persist_attached(id);
        }
    };

    auto apply_local_metamo = [&](int race, int stage) {
        const client::ShinsuInfo* info =
            (race >= 0 && stage >= 1) ? shinsu_db.find(race, stage) : nullptr;
        const float scale = info ? info->scale() : 1.0f;
        const int type = (race >= 0 && stage >= 1) ? race : -1;
        if (!player_actor.apply_metamo(
                *engine, prop_tex_cache, type, stage, scale, &player_wear, &item_catalog)) {
            log::warn("Failed to apply metamorphose");
        }
        const auto& pos = local_player.pos();
        sprite_fx.spawn_oneshot_particle(
            *engine, "metamo_fog", pos.x, pos.y, pos.z, net.local_player_id());
        if (type < 0) {
            clear_shinsu_particles();
        } else {
            attach_shinsu_particles(info);
        }
    };

    auto request_transform_stage = [&](int stage) {
        if (!want_net || !net.connected()) {
            return;
        }
        pending_transform_execute_stage = 0;
        pending_transform_execute_left = 0.0f;
        (void) net.send_transform(protocol::kTransMPre, static_cast<std::uint8_t>(stage));
    };

    auto request_human_form = [&]() {
        if (!want_net || !net.connected()) {
            return;
        }
        pending_transform_execute_stage = 0;
        pending_transform_execute_left = 0.0f;
        (void) net.send_transform(protocol::kTransMFree, 0);
    };

    // UpdateWeaponEffect: looping mix particle on the weapon bone.
    constexpr std::uint32_t kWeaponMixFxEntity = 0xD0000001u;
    int weapon_mix_fx_magic = 0;
    bool weapon_mix_fx_thief = false;

    auto equipped_weapon_mix_type = [&]() -> int {
        const int weapon_index = player_wear.weapon_index();
        if (weapon_index <= 0) {
            return 0;
        }
        for (const auto& slot : player_bag.equipment) {
            if (slot.equipped && slot.index == weapon_index && slot.mix_type != 0) {
                return slot.mix_type;
            }
        }
        return 0;
    };

    auto detach_weapon_mix_fx = [&]() {
        sprite_fx.detach(kWeaponMixFxEntity);
        weapon_mix_fx_magic = 0;
        weapon_mix_fx_thief = false;
    };

    auto sync_weapon_mix_fx = [&]() {
        if (!engine || !sprite_fx.ready()) {
            return;
        }
        const int mix_type = equipped_weapon_mix_type();
        const bool transformed = player_actor.metamo_type() >= 0;
        const bool emoting = player_actor.clip() == client::AnimatedActor::Clip::Emote;
        const int magic = transformed || emoting ? 0 : client::MixVisual::magic_stone(mix_type);
        const bool thief = local_player.clazz() == client::Class::Thief;
        if (magic <= 0) {
            detach_weapon_mix_fx();
            return;
        }
        if (weapon_mix_fx_magic == magic && weapon_mix_fx_thief == thief &&
            sprite_fx.has(kWeaponMixFxEntity)) {
            return;
        }
        detach_weapon_mix_fx();
        if (thief) {
            const std::string fx =
                client::MixVisual::thief_fx_name(client::MixVisual::stone_type(mix_type));
            if (fx.empty() || !sprite_fx.has_fx(fx)) {
                return;
            }
            sprite_fx.attach_fx(*engine, kWeaponMixFxEntity, fx, -1);
        } else {
            const std::string prt = client::MixVisual::particle_name(
                client::LocalPlayer::job_letter(local_player.clazz()), magic);
            if (prt.empty() || !sprite_fx.has_sprite(prt)) {
                return;
            }
            sprite_fx.attach_particle(*engine, kWeaponMixFxEntity, prt, -1, 0.0f, 0.0f, 0.0f);
        }
        if (sprite_fx.has(kWeaponMixFxEntity)) {
            weapon_mix_fx_magic = magic;
            weapon_mix_fx_thief = thief;
        }
    };

    std::unordered_set<std::uint32_t> notified_quest_item_ready;

    auto notify_ready_quest_items = [&]() {
        std::unordered_set<std::uint32_t> ready_now;
        for (const auto& entry : net.quest_flags()) {
            if (entry.clear != 0 || entry.flag == 0) {
                continue;
            }
            const client::TaskDef* task = task_catalog.find(entry.quest);
            if (!task) {
                continue;
            }
            for (const auto& flag : task->flags) {
                if (flag.key != entry.flag || flag.item_index <= 0 || flag.item_count <= 0) {
                    continue;
                }
                if (player_bag.count_item(flag.item_index) < flag.item_count) {
                    continue;
                }
                const std::uint32_t ready_key =
                    (static_cast<std::uint32_t>(entry.quest) << 16) | flag.key;
                ready_now.insert(ready_key);
                if (notified_quest_item_ready.count(ready_key) != 0) {
                    continue;
                }
                std::string line = "Quest item complete: ";
                line += task->name.empty() ? ("quest #" + std::to_string(entry.quest)) : task->name;
                if (!flag.npc_desc.empty()) {
                    line += ". Return to ";
                    line += flag.npc_desc;
                    line += ".";
                } else {
                    line += ".";
                }
                game_hud.chat().push_info(line, protocol::BattleMsgKind::ItemGet);
                (void) sfx.play_ui("quest_done");
            }
        }
        notified_quest_item_ready = std::move(ready_now);
    };

    auto weapon_attack_speed_sec = [&]() -> float {
        // KalOnline auto-attack gates on runtime state = item aspeed/1000 only —
        // not anim length.
        if (player_wear.weapon_index() != 0) {
            const client::ItemDef* def = item_catalog.find(player_wear.weapon_index());
            if (def && def->attack_speed_ms() > 0) {
                return static_cast<float>(std::max(200, def->attack_speed_ms())) / 1000.0f;
            }
        }
        return 0.7f;
    };

    auto face_world_xz = [&](float wx, float wz) {
        const float dx = wx - local_player.pos().x;
        const float dz = wz - local_player.pos().z;
        if (dx * dx + dz * dz > 1e-4f) {
            local_player.set_yaw(std::atan2(-dx, -dz));
            pending_face = true;
            pending_face_x = wx;
            pending_face_z = wz;
            // Don't keep click-walking away while turning to fight.
            local_player.clear_click_target();
        }
    };

    auto weapon_attack_range = [&]() -> int {
        if (player_wear.weapon_index() == 0) {
            return 16;
        }
        const client::ItemDef* def = item_catalog.find(player_wear.weapon_index());
        if (def && def->range() > 0) {
            return def->range();
        }
        // Fallbacks by class if item definition range missing.
        if (local_player.clazz() == client::Class::Archer) {
            return 160;
        }
        if (local_player.clazz() == client::Class::Magician) {
            return 18;
        }
        return 16;
    };

    auto in_client_range = [&](float tx, float tz, int range) -> bool {
        const float dx = tx - local_player.pos().x;
        const float dz = tz - local_player.pos().z;
        const float r = static_cast<float>(std::max(1, range));
        return dx * dx + dz * dz <= r * r;
    };
    const auto npc_dialog_available = [&](std::uint32_t npc_id) {
        const auto npc = net.npcs().find(npc_id);
        if (npc == net.npcs().end()) {
            return false;
        }
        const auto& position = read_only_session && net.observed_pose_ready()
                                   ? net.observed_motion().position()
                                   : local_player.pos();
        const float dx = npc->second.wx() - position.x;
        const float dz = npc->second.wz() - position.z;
        const int range = read_only_session ? protocol::kNpcReplyRange : kNpcTalkEngageRange;
        return dx * dx + dz * dz <= range * range;
    };
    const auto expire_npc_dialog = [&] {
        if (npc_talk_win.close_if_npc_unavailable(npc_dialog_available(npc_talk_win.npc_id()))) {
            in_game_window_order.closed(client::InGameWindowId::NpcTalk);
        }
    };

    // SendAttack: in weapon range returns TRUE even while waiting on
    // runtime state, so OnLButtonDown never SetMove. The +8 matches try_basic_attack.
    auto in_weapon_engage_range = [&](float tx, float tz) -> bool {
        return in_client_range(tx, tz, weapon_attack_range() + 8);
    };

    // Stand still in bow/melee range; only click-walk when the target is too far.
    auto approach_for_basic_attack = [&](float tx, float tz) {
        if (in_weapon_engage_range(tx, tz)) {
            local_player.clear_click_target();
        } else {
            local_player.set_click_target(tx, tz);
        }
    };

    auto mob_world_y = [&](float wx, float wz) -> float { return floor.get_floor(wx, wz); };

    auto resolve_monster_impact = [&](std::uint32_t mob_id,
                                      const client::NetMonster& nm,
                                      float& hit_x,
                                      float& hit_y,
                                      float& hit_z,
                                      float& impact_scale) {
        const float my = mob_world_y(nm.wx(), nm.wz());
        float model_scale = 1.0f;
        float target_height = 17.0f;
        const auto rig = monster_rig_cache.find(nm.index());
        if (rig != monster_rig_cache.end()) {
            model_scale = rig->second.scale();
            target_height = rig->second.target_height();
        }
        hit_x = nm.wx();
        hit_y = my + target_height * model_scale * 0.45f;
        hit_z = nm.wz();
        auto vit = monster_instances.find(mob_id);
        if (vit != monster_instances.end() && rig != monster_rig_cache.end() &&
            rig->second.spine_bone() >= 0) {
            const float hy = vit->second.yaw() * 0.5f;
            const auto R = engine::Mat4::from_quaternion(0.0f, std::sin(hy), 0.0f, std::cos(hy));
            const auto S = engine::Mat4::scaling(model_scale, model_scale, model_scale);
            const auto T = engine::Mat4::translation(nm.wx(), my, nm.wz());
            const auto model = engine::Mat4::multiply(T, engine::Mat4::multiply(R, S));
            if (!vit->second.bone_world(
                    rig->second, model, rig->second.spine_bone(), hit_x, hit_y, hit_z)) {
                log::debug("monster spine bone world resolve failed; using body midpoint");
            }
        }
        impact_scale = (target_height * model_scale) / 16.0f;
    };

    // compatibility behavior starts attack and skill effects at
    // EFFECTBONE_EFFECT.  Monster spell packets need the same resolved source
    // point; using their target's temporary tip made Thunder Strike descend
    // from above instead of leaving the Demon Worker's weapon/hand.
    auto resolve_monster_effect_tip = [&](std::uint32_t mob_id,
                                          const client::NetMonster& nm,
                                          float& tip_x,
                                          float& tip_y,
                                          float& tip_z) -> bool {
        const auto vit = monster_instances.find(mob_id);
        const auto rig = monster_rig_cache.find(nm.index());
        if (vit == monster_instances.end() || rig == monster_rig_cache.end()) {
            return false;
        }
        const float wy = mob_world_y(nm.wx(), nm.wz());
        const float half_yaw = vit->second.yaw() * 0.5f;
        const auto rotation =
            engine::Mat4::from_quaternion(0.0f, std::sin(half_yaw), 0.0f, std::cos(half_yaw));
        const auto scale =
            engine::Mat4::scaling(rig->second.scale(), rig->second.scale(), rig->second.scale());
        const auto translation = engine::Mat4::translation(nm.wx(), wy, nm.wz());
        const auto model =
            engine::Mat4::multiply(translation, engine::Mat4::multiply(rotation, scale));
        int skeleton_bone = rig->second.skeleton_bone(0);
        if (skeleton_bone < 0) {
            skeleton_bone = rig->second.spine_bone();
        }
        return skeleton_bone >= 0 &&
               vit->second.bone_world(rig->second, model, skeleton_bone, tip_x, tip_y, tip_z);
    };

    // Spine1 sits inside the torso. Pull the crash a short step toward the archer
    // so the arrow (and hit SFX origin) sit on the near skin instead of inside /
    // behind the mesh.
    auto pull_archer_impact_front = [&](float& hit_x, float& hit_z, float impact_scale) {
        const float dx = local_player.pos().x - hit_x;
        const float dz = local_player.pos().z - hit_z;
        const float len = std::sqrt(dx * dx + dz * dz);
        if (len < 0.05f) {
            return;
        }
        const float depth = std::clamp(impact_scale * 5.0f, 5.0f, 9.0f);
        // Do not pull past the archer — at point-blank the remaining gap is the
        // whole offset.
        const float step = std::min(depth, std::max(0.0f, len - 1.0f));
        hit_x += dx / len * step;
        hit_z += dz / len * step;
    };

    auto spawn_archer_shot = [&](std::uint32_t target_id) {
        const auto monster = net.monsters().find(target_id);
        const auto* player =
            monster == net.monsters().end() ? remote_players.find(target_id) : nullptr;
        if (monster == net.monsters().end() && !player) {
            return;
        }
        float tip_x = local_player.pos().x;
        float tip_y = local_player.pos().y + 12.0f;
        float tip_z = local_player.pos().z;
        const auto player_model = player_world_matrix();
        if (!player_actor.projectile_origin_world(player_model, tip_x, tip_y, tip_z)) {
            log::debug("effect tip bone missing; using approximate bow tip");
        }
        float hit_x = 0.0f;
        float hit_y = 0.0f;
        float hit_z = 0.0f;
        float impact_scale = 1.0f;
        if (player) {
            player->spine_world(hit_x, hit_y, hit_z);
        } else {
            resolve_monster_impact(target_id, monster->second, hit_x, hit_y, hit_z, impact_scale);
        }
        combat_fx.spawn_arrow(client::SkillEt::None,
                              tip_x,
                              tip_y,
                              tip_z,
                              hit_x,
                              hit_y,
                              hit_z,
                              0.0f,
                              target_id,
                              1,
                              impact_scale,
                              net.local_player_id());
    };

    auto present_monster_hit = [&](std::uint32_t target,
                                   std::uint8_t flags,
                                   const engine::Vec3& impact) {
        if ((flags & protocol::kAtkHit) == 0) {
            return;
        }
        const auto instance = monster_instances.find(target);
        if (instance != monster_instances.end()) {
            const bool critical = (flags & (protocol::kAtkFatal | protocol::kAtkExplosive)) != 0;
            instance->second.trigger_hit_outline(critical);
        }
        if ((flags & protocol::kAtkExplosive) != 0) {
            const auto sounds =
                sprite_fx.spawn_oneshot_fx(*engine, "palsy dmg2", impact.x, impact.y, impact.z);
            for (const auto& sound : sounds) {
                combat_fx.play_sfx(sound, impact.x, impact.y, impact.z);
            }
        }
    };
    auto present_arrow_feedback = [&] {
        client::ArrowHitFeedback::Impact impact;
        while (combat_fx.take_arrow_feedback(impact)) {
            present_monster_hit(impact.target, impact.flags, impact.position);
        }
    };

    // Auto-select radius for WASD / Space nearest. ~8 tiles (tile=32) — close
    // enough to see and close, not half a patch (~4k). Slightly above weapon
    // range so Space reaches pouches.
    constexpr int kAutoEngageTiles = 8;
    constexpr int kAutoEngageRange = kAutoEngageTiles * 32;  // 256
    // Max leash while chasing an already-selected target (LMB click or Space
    // auto-attack).
    constexpr int kAutoChaseRange = 2000;
    auto auto_engage_range = [&]() -> int {
        return std::max(kAutoEngageRange, weapon_attack_range() + 48);
    };

    // Living mobs, plus Maguni corpses that are still kneeling (beheadable).
    auto is_clickable_mob = [](const client::NetMonster& nm) -> bool {
        if (!protocol::MonsterRaceInfo::blocks_collision(nm.race()) ||
            (nm.gstate() & protocol::kCgsInvincible) != 0) {
            return false;
        }
        return nm.hp() > 0 || nm.kneeling();
    };

    // Nearest living mob within engage range (WASD auto-target / Space fallback).
    // include_kneeling: also consider Maguni corpses (behead / F).
    auto find_nearest_mob_id = [&](bool include_kneeling = false) -> std::uint32_t {
        const float max_r = static_cast<float>(auto_engage_range());
        const float max_d2 = max_r * max_r;
        std::uint32_t best_id = 0;
        float best_d2 = 0.0f;
        for (const auto& [id, nm] : net.monsters()) {
            if (!is_clickable_mob(nm) || (nm.hp() <= 0 && !(include_kneeling && nm.kneeling()))) {
                continue;
            }
            const float dx = nm.wx() - local_player.pos().x;
            const float dz = nm.wz() - local_player.pos().z;
            const float d2 = dx * dx + dz * dz;
            if (d2 > max_d2) {
                continue;
            }
            if (best_id == 0 || d2 < best_d2) {
                best_id = id;
                best_d2 = d2;
            }
        }
        return best_id;
    };

    auto find_nearest_kneeling_id = [&]() -> std::uint32_t {
        const float max_r = static_cast<float>(auto_engage_range());
        const float max_d2 = max_r * max_r;
        std::uint32_t best_id = 0;
        float best_d2 = 0.0f;
        for (const auto& [id, nm] : net.monsters()) {
            if (!nm.kneeling()) {
                continue;
            }
            const float dx = nm.wx() - local_player.pos().x;
            const float dz = nm.wz() - local_player.pos().z;
            const float d2 = dx * dx + dz * dz;
            if (d2 > max_d2) {
                continue;
            }
            if (best_id == 0 || d2 < best_d2) {
                best_id = id;
                best_d2 = d2;
            }
        }
        return best_id;
    };

    auto monster_pick_size = [&](const client::NetMonster& nm, float& half, float& height) {
        half = client::WorldPick::kMonsterHalfExtent;
        height = client::WorldPick::kMonsterBaseHeight;
        const auto rig = monster_rig_cache.find(nm.index());
        if (rig != monster_rig_cache.end()) {
            half = rig->second.pick_half();
            height = rig->second.pick_height();
        }
        // The Doggebi assets contain oversized decorative meshes. Restrict only their
        // horizontal combat-body bounds so nearby floor and NPCs remain selectable;
        // retain the model-derived height for reliable clicks on tall monsters.
        if (nm.index() == 213) {
            half = 20.0f;
        } else if (nm.index() == 232) {
            half = 8.0f;
        }
    };

    auto npc_pick_size = [&](const client::NetNpc& nn, float& half, float& height) {
        half = client::WorldPick::kNpcHalfExtent;
        height = client::WorldPick::kNpcBaseHeight;
        const client::WorldRenderCache::GpuModel* gpu =
            client::WorldRenderCache::get_or_load_npc_shape(
                *engine, profile.data_root(), nn.shape(), model_cache, prop_tex_cache, npc_catalog);
        if (gpu && gpu->has_bounds) {
            client::WorldPick::size_from_aabb(gpu->min_x,
                                              gpu->min_y,
                                              gpu->min_z,
                                              gpu->max_x,
                                              gpu->max_y,
                                              gpu->max_z,
                                              1.0f,
                                              half,
                                              height);
        }
    };

    const auto ground_item_position = [&](const client::NetGroundItem& item) {
        engine::Vec3 position{item.wx, floor.get_floor(item.wx, item.wz), item.wz};
        constexpr float kDropBounceSec = 0.45f;
        constexpr float kDropBounceHeight = 12.0f;
        if (item.age_sec < kDropBounceSec) {
            const float t = item.age_sec / kDropBounceSec;
            position.x = item.origin_wx + (item.wx - item.origin_wx) * t;
            position.z = item.origin_wz + (item.wz - item.origin_wz) * t;
            position.y += kDropBounceHeight * 4.0f * t * (1.0f - t);
        }
        return position;
    };

    auto attach_loop_fx =
        [&](std::uint32_t id, const client::MonsterRigGpu& rig, float wx, float wy, float wz) {
            if (!sprite_fx.has(id)) {
                for (const auto& p : rig.particles()) {
                    sprite_fx.attach_particle(
                        *engine, id, p.name(), p.effect_bone(), p.ox(), p.oy(), p.oz());
                }
                for (const auto& fx : rig.fx()) {
                    sprite_fx.attach_fx(*engine, id, fx.name(), fx.effect_bone());
                }
                if (sprite_fx.has(id)) {
                    sprite_fx.persist_attached(id);
                }
            }
            if (!monster_loop_armed.insert(id).second) {
                return;
            }
            std::unordered_set<std::string> seen_stems;
            auto play_loop_stems = [&](std::string_view fx_name) {
                for (const auto& sound : sprite_fx.fx_sounds(*engine, fx_name)) {
                    const auto cue = client::Audio::parse_name(sound);
                    if (!cue.looping() || cue.stem().empty() ||
                        !seen_stems.insert(cue.stem()).second) {
                        continue;
                    }
                    client::SoundHandle handle = sfx.play(sound, wx, wy, wz);
                    if (handle.valid()) {
                        monster_loop_sfx[id].push_back(handle);
                    }
                }
            };
            for (const auto& fx : rig.fx()) {
                play_loop_stems(fx.name());
            }
            for (const auto& cue : rig.idle_fx_cues()) {
                play_loop_stems(cue.name());
            }
        };

    auto stop_monster_loop_sfx = [&](std::uint32_t id) {
        auto it = monster_loop_sfx.find(id);
        if (it == monster_loop_sfx.end()) {
            return;
        }
        for (const auto& handle : it->second) {
            sfx.stop(handle);
        }
        monster_loop_sfx.erase(it);
    };

    auto detach_monster_visual = [&](std::uint32_t id) {
        monster_loop_armed.erase(id);
        stop_monster_loop_sfx(id);
        sprite_fx.detach(id);
        if (freeze_fx_armed.erase(id)) {
            combat_fx.kill_clip_entity(id);
        }
    };

    auto follow_loop_fx = [&](std::uint32_t id,
                              const client::MonsterInstance& inst,
                              const client::MonsterRigGpu& rig,
                              const engine::Mat4& model) {
        const auto particle_world = [](engine::Mat4 world) {
            // The emitter applies the model scale independently to its particle sizes.
            for (int column = 0; column < 3; ++column) {
                const int offset = column * 4;
                const float length = std::sqrt(world.m[offset] * world.m[offset] +
                                               world.m[offset + 1] * world.m[offset + 1] +
                                               world.m[offset + 2] * world.m[offset + 2]);
                if (length > 0.00001f) {
                    world.m[offset] /= length;
                    world.m[offset + 1] /= length;
                    world.m[offset + 2] /= length;
                }
            }
            return world;
        };

        sprite_fx.set_target_scale(id, rig.scale());
        // Followed FX stages without useEffectBoneRotation inherit this facing;
        // the effect bone supplies only the animated attachment position.
        const engine::Mat4 origin = particle_world(model);
        std::array<engine::Mat4, client::kEffectBoneCount> effect_bones{};
        sprite_fx.follow_oriented(id, -1, origin);
        for (int i = 0; i < client::kEffectBoneCount; ++i) {
            engine::Mat4 bone_world = model;
            const int skel = rig.skeleton_bone(i);
            if (skel >= 0) {
                (void) inst.bone_world_matrix(rig, model, skel, bone_world);
            }
            bone_world = particle_world(bone_world);
            effect_bones[static_cast<std::size_t>(i)] = bone_world;
            sprite_fx.follow_oriented(id, i, bone_world);
            if (i == 1) {
                const float x = bone_world.m[12];
                const float y = bone_world.m[13];
                const float z = bone_world.m[14];
                archer_fx_dispatcher.follow_target(id, x, y, z);
            }
        }
        sprite_fx.follow_oneshots(id, origin, effect_bones.data(), client::kEffectBoneCount);
        engine::Mat4 spine = model;
        const int spine_skel = rig.spine_bone() >= 0 ? rig.spine_bone() : rig.skeleton_bone(1);
        if (spine_skel >= 0) {
            (void) inst.bone_world_matrix(rig, model, spine_skel, spine);
        }
        combat_fx.follow_arrows(id, spine);
    };

    auto player_spine_world = [&](float& x, float& y, float& z) {
        x = local_player.pos().x;
        y = local_player.pos().y + client::kBodyHeight;
        z = local_player.pos().z;
    };

    auto follow_player_fx_bones = [&](std::uint32_t id,
                                      const client::AnimatedActor& actor,
                                      const engine::Mat4& model,
                                      client::Class clazz) {
        engine::Mat4 bones[client::kEffectBoneCount];
        const client::ShinsuInfo* shinsu = nullptr;
        if (actor.metamo_type() >= 0 && actor.metamo_level() >= 1) {
            shinsu = shinsu_db.find(actor.metamo_type(), actor.metamo_level());
        }
        for (int i = 0; i < client::kEffectBoneCount; ++i) {
            bones[i] = model;
            const int skel = shinsu != nullptr
                                 ? shinsu_db.skeleton_bone(shinsu->bone(), i)
                                 : client::EquipmentPaths::effect_slot_skeleton(clazz, i);
            (void) actor.bone_world_matrix(model, skel, bones[i]);
        }
        sprite_fx.follow_oneshots(id, model, bones, client::kEffectBoneCount);
        combat_fx.follow_clip_bones(id, model, bones, client::kEffectBoneCount);
    };

    auto player_impact_world = [&](std::uint32_t player_id, float& x, float& y, float& z) {
        if (player_id != 0 && player_id != net.local_player_id()) {
            if (const client::RemotePlayer* remote = remote_players.find(player_id)) {
                remote->spine_world(x, y, z);
                return;
            }
        }
        player_spine_world(x, y, z);
    };

    auto fill_target_origin =
        [&](std::uint32_t target_id, client::SkillPresentEndpoints& ends, bool ground) {
            const auto mit = net.monsters().find(target_id);
            if (mit != net.monsters().end()) {
                ends.origin_x = mit->second.wx();
                ends.origin_y = mob_world_y(mit->second.wx(), mit->second.wz());
                ends.origin_z = mit->second.wz();
                return;
            }
            if (ground) {
                ends.origin_x = ends.hit_x;
                ends.origin_y = ends.hit_y;
                ends.origin_z = ends.hit_z;
                return;
            }
            if (target_id != 0 && target_id != net.local_player_id()) {
                if (const client::RemotePlayer* remote = remote_players.find(target_id)) {
                    ends.origin_x = remote->wx();
                    ends.origin_y = remote->wy();
                    ends.origin_z = remote->wz();
                    return;
                }
            }
            ends.origin_x = local_player.pos().x;
            ends.origin_y = local_player.pos().y;
            ends.origin_z = local_player.pos().z;
        };

    auto fill_caster_origin = [&](std::uint32_t caster_id, client::SkillPresentEndpoints& ends) {
        if (caster_id != 0 && caster_id != net.local_player_id()) {
            if (const client::RemotePlayer* remote = remote_players.find(caster_id)) {
                ends.caster_origin_x = remote->wx();
                ends.caster_origin_y = remote->wy();
                ends.caster_origin_z = remote->wz();
                return;
            }
        }
        ends.caster_origin_x = local_player.pos().x;
        ends.caster_origin_y = local_player.pos().y;
        ends.caster_origin_z = local_player.pos().z;
    };

    // ActionDamage at `(*EFFECT_DAMAGE)`: floats, player-spine hit FX, evade.
    auto present_monster_action_damage = [&](const client::MonsterActionDamage& ad) {
        float hx = 0.0f;
        float hy = 0.0f;
        float hz = 0.0f;
        player_impact_world(ad.target_id(), hx, hy, hz);
        const std::uint8_t flags = ad.flags();
        if ((flags & protocol::kAtkHit) != 0) {
            combat_fx.spawn_hit(*engine, hx, hy, hz, flags, 1.0f, false);
            if ((flags & protocol::kAtkExplosive) != 0) {
                const auto eb_sounds =
                    sprite_fx.spawn_oneshot_fx(*engine, "palsy dmg2", hx, hy, hz);
                for (const auto& snd : eb_sounds) {
                    combat_fx.play_sfx(snd, hx, hy, hz);
                }
            }
        }
        if (ad.target_id() != net.local_player_id()) {
            return;
        }
        if (ad.damage() > 0) {
            combat_floats.spawn(
                client::CombatFloatKind::DamageTaken, std::to_string(ad.damage()), 0);
        } else if ((flags & protocol::kAtkBlock) != 0) {
            combat_floats.spawn(
                client::CombatFloatKind::Blocked, client::CombatFloatText::blocked_text(), 0);
        } else if ((flags & protocol::kAtkHit) == 0) {
            player_actor.trigger_evade();
            const std::string evade_sys = item_catalog.sys(140);
            game_hud.chat().push_info(client::BattleMsgBuilder::evaded_attack(evade_sys),
                                      protocol::BattleMsgKind::Damage);
            combat_floats.spawn(
                client::CombatFloatKind::Missed, client::CombatFloatText::missed_text(), 0);
        }
    };

    // Attack-GB `(*PROJECTILE)` / `(*EFFECT_ARROW)` — compatibility behavior.
    auto spawn_monster_attack_fx = [&](client::MonsterInstance& inst,
                                       const client::MonsterRigGpu& rig,
                                       const engine::Mat4& model,
                                       float wx,
                                       float wy,
                                       float wz) {
        client::MonsterAttackFx fx;
        if (!inst.take_attack_fx(fx)) {
            return;
        }
        float ox = wx;
        float oy = wy + 8.0f;
        float oz = wz;
        int skel = rig.skeleton_bone(fx.bone());
        if (skel < 0) {
            skel = rig.spine_bone();
        }
        if (skel >= 0) {
            (void) inst.bone_world(rig, model, skel, ox, oy, oz);
        }
        float tx = 0.0f;
        float ty = 0.0f;
        float tz = 0.0f;
        player_spine_world(tx, ty, tz);
        if (fx.kind() == client::MonsterAttackFx::Kind::Arrow) {
            combat_fx.spawn_arrow(
                client::SkillEt::None, ox, oy, oz, tx, ty, tz, 0.0f, net.local_player_id(), 1);
            return;
        }
        if (fx.kind() == client::MonsterAttackFx::Kind::Projectile && fx.type() == 2) {
            combat_fx.spawn_devil_spirit_spit(ox, oy, oz, tx, ty, tz);
        }
    };

    auto pick_world_mob =
        [&](float mx, float my, float fw, float fh, float& out_t) -> std::uint32_t {
        std::uint32_t hit_mob = 0;
        float best = 0.0f;
        out_t = 0.0f;
        for (const auto& [id, nm] : net.monsters()) {
            if (!is_clickable_mob(nm)) {
                continue;
            }
            float half = client::WorldPick::kMonsterHalfExtent;
            float height = client::WorldPick::kMonsterBaseHeight;
            monster_pick_size(nm, half, height);
            const float base_y = floor.get_floor(nm.wx(), nm.wz());
            float hit_t = 0.0f;
            if (!client::WorldPick::pick_monster_box(cam,
                                                     local_player,
                                                     mx,
                                                     my,
                                                     fw,
                                                     fh,
                                                     nm.wx(),
                                                     base_y,
                                                     nm.wz(),
                                                     half,
                                                     height,
                                                     hit_t)) {
                continue;
            }
            if (hit_mob == 0 || hit_t < best) {
                best = hit_t;
                hit_mob = id;
            }
        }
        out_t = best;
        return hit_mob;
    };

    auto try_open_chest = [&](std::uint32_t mob_id) -> bool {
        if (mob_id == 0 || local_player.resting() || local_player.dead()) {
            return false;
        }
        if (opening_chest_id == mob_id || player_actor.opening_box()) {
            return true;
        }
        auto mit = net.monsters().find(mob_id);
        if (mit == net.monsters().end() || mit->second.hp() <= 0 ||
            !protocol::MonsterRaceInfo::is_openable(mit->second.race())) {
            return false;
        }
        constexpr int kOpenRange = protocol::kItemMonsterOpenRange;
        // Float world distance can sit just under range while integer server pixels
        // sit just over. Approach on the same +8 grace as basic attacks; only send
        // when the same squared check the server uses would accept.
        constexpr int kApproachGrace = 8;
        const float cdx = mit->second.wx() - local_player.pos().x;
        const float cdz = mit->second.wz() - local_player.pos().z;
        const float client_dist = std::sqrt(cdx * cdx + cdz * cdz);
        const int pose_sx = coords::to_server(local_player.pos().x, profile.origin_patch());
        const int pose_sy = coords::to_server(local_player.pos().z, profile.origin_patch());
        const int base_px = net.server_px();
        const int base_py = net.server_py();
        const int mx = mit->second.x();
        const int my = mit->second.y();
        const auto dist2 = [](int ax, int ay, int bx, int by) -> double {
            const long long dx = static_cast<long long>(bx) - ax;
            const long long dy = static_cast<long long>(by) - ay;
            return std::sqrt(static_cast<double>(dx * dx + dy * dy));
        };
        const double pose_dist = dist2(pose_sx, pose_sy, mx, my);
        const double base_dist = dist2(base_px, base_py, mx, my);
        if (!in_client_range(mit->second.wx(), mit->second.wz(), kOpenRange + kApproachGrace)) {
            return false;
        }
        // Already sent; stand still until Open / timeout (do not re-chase).
        if (pending_chest_id == mob_id && chest_open_retry > 0.0f) {
            local_player.clear_click_target();
            return true;
        }
        face_world_xz(mit->second.wx(), mit->second.wz());
        // Client pose runs ahead of MovePlayerOn; flush so the server range check
        // sees the same feet position the client used (standing at the chest).
        net.flush_local_move(local_player.pos().x,
                             local_player.pos().y,
                             local_player.pos().z,
                             profile.origin_patch());
        const int after_px = net.server_px();
        const int after_py = net.server_py();
        const double after_dist = dist2(after_px, after_py, mx, my);
        if (!server::combat::in_attack_range(after_px, after_py, mx, my, kOpenRange)) {
            if (client::DebugOverlay::enabled()) {
                char buf[384];
                std::snprintf(buf,
                              sizeof(buf),
                              "chest wait_server_range id=%u client_dist=%.1f pose_dist=%.1f "
                              "after_flush_dist=%.1f range=%d pose=(%d,%d) mob=(%d,%d)",
                              mob_id,
                              static_cast<double>(client_dist),
                              pose_dist,
                              after_dist,
                              kOpenRange,
                              after_px,
                              after_py,
                              mx,
                              my);
                chest_debug_last = buf;
            }
            return false;
        }
        if (!net.send_get_item_monster(mob_id)) {
            if (client::DebugOverlay::enabled()) {
                char buf[384];
                std::snprintf(buf,
                              sizeof(buf),
                              "chest send_fail id=%u client_dist=%.1f pose_dist=%.1f "
                              "base_dist=%.1f "
                              "after_flush_dist=%.1f flush_d=(%d,%d) range=%d",
                              mob_id,
                              static_cast<double>(client_dist),
                              pose_dist,
                              base_dist,
                              after_dist,
                              after_px - base_px,
                              after_py - base_py,
                              kOpenRange);
                chest_debug_last = buf;
                log::info(buf);
            }
            return false;
        }
        if (client::DebugOverlay::enabled()) {
            char buf[384];
            std::snprintf(buf,
                          sizeof(buf),
                          "chest send_get id=%u race=%d client_dist=%.1f pose_dist=%.1f "
                          "base_dist=%.1f "
                          "after_flush_dist=%.1f flush_d=(%d,%d) range=%d pose=(%d,%d) "
                          "base=(%d,%d)->(%d,%d) mob=(%d,%d)",
                          mob_id,
                          mit->second.race(),
                          static_cast<double>(client_dist),
                          pose_dist,
                          base_dist,
                          after_dist,
                          after_px - base_px,
                          after_py - base_py,
                          kOpenRange,
                          pose_sx,
                          pose_sy,
                          base_px,
                          base_py,
                          after_px,
                          after_py,
                          mx,
                          my);
            chest_debug_last = buf;
            log::info(buf);
        }
        pending_chest_id = mob_id;
        chest_open_retry = 0.75f;
        // Stop walking immediately — further MoveOn after GETITEMMONSTER cancels
        // compatibility behavior. Return true so the chase loop does not re-set a click target
        // this frame.
        local_player.clear_click_target();
        return true;
    };

    auto cancel_open_chest = [&](const char* reason) {
        if (opening_chest_id == 0 && pending_chest_id == 0 && !player_actor.opening_box()) {
            return;
        }
        if (client::DebugOverlay::enabled()) {
            log::info(std::string("chest cancel: ") + reason +
                      " pending=" + std::to_string(pending_chest_id) +
                      " open=" + std::to_string(opening_chest_id) +
                      " moving=" + (local_player.moving() ? "1" : "0"));
        }
        const std::uint32_t id = opening_chest_id != 0 ? opening_chest_id : pending_chest_id;
        if (id != 0) {
            (void) net.send_cancel_item_monster(id);
        }
        opening_chest_id = 0;
        pending_chest_id = 0;
        chest_open_retry = 0.0f;
        player_actor.clear_open_box();
    };

    auto request_stand = [&]() -> bool {
        if (!local_player.resting() || local_player.dead()) {
            return true;
        }
        if (want_net && net.connected()) {
            if (!net.send_rest(false)) {
                return false;
            }
        }
        local_player.set_resting(false);
        return true;
    };

    auto try_basic_attack = [&](std::uint32_t target_id, bool player_target = false) -> bool {
        if (target_id == 0 || local_player.dead() || behead_animation_active) {
            return false;
        }
        if (local_player.resting() && !request_stand()) {
            return false;
        }
        float x = 0.0f, z = 0.0f;
        if (player_target) {
            const auto* player = remote_players.find(target_id);
            if (!player || !net.castle_enemy(target_id) || net.castle_defeated(target_id)) {
                return false;
            }
            x = player->wx();
            z = player->wz();
        } else {
            const auto monster = net.monsters().find(target_id);
            if (monster == net.monsters().end()) {
                return false;
            }
            if (protocol::MonsterRaceInfo::is_openable(monster->second.race())) {
                return try_open_chest(target_id);
            }
            if (monster->second.hp() <= 0) {
                return false;
            }
            x = monster->second.wx();
            z = monster->second.wz();
        }
        if (!player_wear.has_weapon() || pending_skill_kind != PendingSkillKind::None ||
            attack_cooldown > 0.0f) {
            return false;
        }
        if (!in_weapon_engage_range(x, z)) {
            return false;
        }
        net.flush_local_move(local_player.pos().x,
                             local_player.pos().y,
                             local_player.pos().z,
                             profile.origin_patch());
        face_world_xz(x, z);
        const bool sent =
            player_target ? net.send_attack_player(target_id) : net.send_attack_monster(target_id);
        if (!sent) {
            return false;
        }
        attack_cooldown = weapon_attack_speed_sec();
        // ModelChange(GO_ATTACK) replaces GO_SKILL; do not send C2S_ATTACK
        // while the skill pose is still held.
        player_actor.end_skill();
        const std::uint64_t now_ms =
            static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                           std::chrono::steady_clock::now().time_since_epoch())
                                           .count());
        const bool dance_of_killing =
            local_player.clazz() == client::Class::Thief &&
            std::any_of(net.buff_slots().begin(),
                        net.buff_slots().end(),
                        [now_ms](const client::BuffSlotEntry& buff) {
                            return (buff.bstate & protocol::kCbsSkillThiefDanceOfKilling) != 0 &&
                                   (buff.expire_ms == 0 || buff.expire_ms > now_ms);
                        });
        if (dance_of_killing) {
            const auto* dance = skill_catalog.find(static_cast<int>(client::Class::Thief), 13);
            player_actor.trigger_skill_motion(dance ? dance->motion_fire() : 8, true);
        } else {
            player_actor.trigger_attack();
        }
        auto_attack = true;
        auto_skill = false;
        if (local_player.clazz() == client::Class::Archer) {
            pending_arrow_target = target_id;
        }
        return true;
    };

    auto stop_skill_cast_sfx = [&]() {
        sfx.stop(skill_loop);
        skill_loop.clear();
    };

    auto play_skill_cast_sfx = [&](const client::SkillDef* def) {
        stop_skill_cast_sfx();
        if (!def || def->sound(0).empty()) {
            return;
        }
        const auto& p = local_player.pos();
        skill_loop = sfx.play(def->sound(0), p.x, p.y, p.z);
    };

    auto mage_effect_tip = [&](float& tip_x, float& tip_y, float& tip_z) {
        tip_x = local_player.pos().x;
        tip_y = local_player.pos().y + 16.0f;
        tip_z = local_player.pos().z;
        const auto player_model = player_world_matrix();
        if (!player_actor.effect_tip_world(player_model, tip_x, tip_y, tip_z)) {
            log::debug("effect tip bone missing; using approximate wand tip");
        }
    };

    auto clear_pending_skill = [&](bool restore_idle = true) {
        stop_skill_cast_sfx();
        combat_fx.clear_cast_fx(net.local_player_id());
        knight_fx_dispatcher.clear_looping(net.local_player_id());
        archer_fx_dispatcher.clear_looping(net.local_player_id());
        if (restore_idle && pending_skill_kind != PendingSkillKind::None) {
            player_actor.end_skill();
            player_actor.cancel_attack();
        }
        pending_skill_kind = PendingSkillKind::None;
        pending_skill_index = 0;
        pending_skill_target = 0;
        pending_skill_item_iid = 0;
        pending_skill_left = 0.0f;
        pending_skill_total = 0.0f;
        focus_shot_elapsed_sec = 0.0f;
        queued_skill_index = -1;
        queued_skill_target = 0;
        queued_skill_is_player = false;
    };

    // Drop an in-flight PRESKILL so LMB / Space can basic-attack immediately.
    auto abort_pending_skill_for_basic_attack = [&]() {
        if (pending_skill_kind == PendingSkillKind::None) {
            return;
        }
        clear_pending_skill(true);
        auto_skill = false;
        // Mage/melee stamped the remaining charge onto attack_cooldown; do not make
        // LMB wait out the rest of the cast. Weapon GCD is the ceiling.
        attack_cooldown = std::min(attack_cooldown, weapon_attack_speed_sec());
    };

    auto archer_skill_prep_ms = [&](const client::SkillDef* def) -> int {
        if (!def) {
            return 0;
        }
        if (protocol::archer_special_kind(def->index()) == protocol::ArcherSpecialKind::FocusShot) {
            return protocol::kArcherFocusShotBarMs;
        }
        // Client wait is runtime state[2] (JSON compare id 3), not delay[0].
        return client::Catalog::preskill_ms(*def, player_skills.level_of(def->index()));
    };

    auto is_focus_shot_charging = [&]() -> bool {
        return pending_skill_kind == PendingSkillKind::Archer &&
               protocol::archer_special_kind(static_cast<int>(pending_skill_index)) ==
                   protocol::ArcherSpecialKind::FocusShot;
    };

    /** Pile type B: second click on a hostile fires after pip 1 (1000 ms). */
    auto try_release_focus_shot = [&](std::uint32_t target_id) -> bool {
        if (!is_focus_shot_charging()) {
            return false;
        }
        const int charged_ms = static_cast<int>(focus_shot_elapsed_sec * 1000.0f);
        if (protocol::archer_focus_shot_bar_count(charged_ms) < 1) {
            return false;
        }
        if (target_id == 0 || target_id == net.local_player_id()) {
            return false;
        }
        float tx = 0.0f;
        float tz = 0.0f;
        auto mit = net.monsters().find(target_id);
        if (mit != net.monsters().end()) {
            if (mit->second.hp() <= 0 ||
                protocol::MonsterRaceInfo::is_openable(mit->second.race())) {
                return false;
            }
            tx = mit->second.wx();
            tz = mit->second.wz();
        } else {
            const client::RemotePlayer* remote = remote_players.find(target_id);
            if (!remote) {
                return false;
            }
            tx = remote->wx();
            tz = remote->wz();
        }
        const auto* def = skill_catalog.find(2, static_cast<int>(pending_skill_index));
        const int sk_lv =
            std::max(1, player_skills.level_of(static_cast<int>(pending_skill_index)));
        const int range = client::Catalog::skill_cast_range(2, def, sk_lv, weapon_attack_range());
        if (!in_client_range(tx, tz, range)) {
            return false;
        }
        face_world_xz(tx, tz);
        pending_skill_target = target_id;
        pending_skill_left = 0.0f;
        return true;
    };

    auto catalog_job_for_skill = [&](int skill_index) -> int {
        if (protocol::is_animal_skill(skill_index)) {
            return protocol::animal_skill_class(
                protocol::transform_tflag_race(net.transform_tflag()));
        }
        return static_cast<int>(local_player.clazz());
    };

    auto effect_job_for_skill = [&](int skill_index, int /*phase*/, int caster_job = -1) -> int {
        if (protocol::is_animal_skill(skill_index)) {
            return client::kSkillEffectJobTransform;
        }
        // `-2` selects the monster catalog. Only the `-1` sentinel means that
        // the effect belongs to the local player's class.
        return caster_job != -1 ? caster_job : static_cast<int>(local_player.clazz());
    };

    auto spawn_skill_fire_fx =
        [&](int job, std::uint8_t skill_index, std::uint32_t mob_id, bool ground_target = false) {
            client::SkillPresentEndpoints ends;
            mage_effect_tip(ends.tip_x, ends.tip_y, ends.tip_z);
            ends.caster_entity = net.local_player_id();
            // Ground Explosive Burst must stay world-fixed; following the local
            // player would drag effect_ef091 with the caster.
            if (ground_target) {
                ends.target_entity = 0;
            } else {
                ends.target_entity = mob_id != 0 ? mob_id : net.local_player_id();
            }
            ends.valid = true;
            auto mit = net.monsters().find(mob_id);
            if (mit != net.monsters().end()) {
                float impact_scale = 1.0f;
                resolve_monster_impact(
                    mob_id, mit->second, ends.hit_x, ends.hit_y, ends.hit_z, impact_scale);
                ends.impact_scale = impact_scale;
            } else if (ground_target) {
                ends.hit_x = coords::to_client(pending_skill_ground_x, profile.origin_patch());
                ends.hit_z = coords::to_client(pending_skill_ground_y, profile.origin_patch());
                ends.hit_y = floor.sample(ends.hit_x, ends.hit_z, local_player.pos().y);
            } else {
                player_impact_world(mob_id, ends.hit_x, ends.hit_y, ends.hit_z);
            }
            fill_target_origin(mob_id, ends, ground_target);
            fill_caster_origin(net.local_player_id(), ends);
            const auto* def =
                skill_catalog.find(catalog_job_for_skill(static_cast<int>(skill_index)),
                                   static_cast<int>(skill_index));
            (void) skill_presenter.present(*engine,
                                           job,
                                           static_cast<int>(skill_index),
                                           client::SkillPresentPhase::Fire,
                                           def,
                                           ends);
        };

    auto fire_skill_now = [&](PendingSkillKind kind,
                              std::uint8_t skill_index,
                              std::uint32_t target,
                              std::uint32_t item_iid) -> bool {
        if (local_player.clazz() == client::Class::Knight && skill_index == 17 &&
            net.mortal() <= 0) {
            return false;
        }
        if (kind == PendingSkillKind::SelfHeal || kind == PendingSkillKind::SelfArea) {
            if (!net.send_skill_self(skill_index)) {
                return false;
            }
        } else if (kind == PendingSkillKind::Player || kind == PendingSkillKind::DeadPlayer) {
            if (!net.send_skill_target_player(skill_index, target)) {
                return false;
            }
        } else if (kind == PendingSkillKind::Ground) {
            if (protocol::archer_special_kind(skill_index) ==
                protocol::ArcherSpecialKind::ArrowRain) {
                local_player.clear_click_target();
                local_player.set_moving(false);
                net.seed_move_baseline(
                    coords::to_server(local_player.pos().x, profile.origin_patch()),
                    coords::to_server(local_player.pos().z, profile.origin_patch()),
                    coords::to_server_z(local_player.pos().y));
            }
            if (!net.send_skill_ground(
                    skill_index, pending_skill_ground_x, pending_skill_ground_y)) {
                return false;
            }
        } else if (kind == PendingSkillKind::Animal) {
            if (target == 0) {
                if (!net.send_skill_self(skill_index)) {
                    return false;
                }
            } else if (!net.send_skill_target_monster(skill_index, target)) {
                return false;
            }
        } else if (kind == PendingSkillKind::Archer &&
                   protocol::archer_special_kind(skill_index) ==
                       protocol::ArcherSpecialKind::PoisonArrow) {
            const std::uint8_t target_kind = net.monsters().find(target) != net.monsters().end()
                                                 ? protocol::kCkMonster
                                                 : protocol::kCkPlayer;
            if (!net.send_skill_target_item(skill_index, target_kind, target, item_iid)) {
                return false;
            }
        } else if (kind == PendingSkillKind::Archer &&
                   protocol::archer_target_shape(skill_index) ==
                       protocol::ArcherTargetShape::HostileCharacter &&
                   net.monsters().find(target) == net.monsters().end()) {
            if (!net.send_skill_target_player(skill_index, target)) {
                return false;
            }
        } else if (kind == PendingSkillKind::Mage && net.monsters().count(target) == 0) {
            if (!net.castle_enemy(target) || net.castle_defeated(target) ||
                !net.send_skill_target_player(skill_index, target)) {
                return false;
            }
        } else if (kind == PendingSkillKind::Melee &&
                   (local_player.clazz() == client::Class::Thief ||
                    local_player.clazz() == client::Class::Knight) &&
                   net.monsters().find(target) == net.monsters().end()) {
            if (!net.send_skill_target_player(skill_index, target)) {
                return false;
            }
        } else if (!net.send_skill_target_monster(skill_index, target)) {
            return false;
        }
        const int clazz = catalog_job_for_skill(static_cast<int>(skill_index));
        const int fx_job = effect_job_for_skill(static_cast<int>(skill_index), 1);
        const bool support_cast = kind == PendingSkillKind::SelfHeal ||
                                  kind == PendingSkillKind::Player ||
                                  kind == PendingSkillKind::DeadPlayer;
        if (kind == PendingSkillKind::Mage || kind == PendingSkillKind::SelfHeal ||
            kind == PendingSkillKind::SelfArea || kind == PendingSkillKind::Ground) {
            stop_skill_cast_sfx();
            const auto* fire_def = skill_catalog.find(clazz, static_cast<int>(skill_index));
            if (fire_def && fire_def->motion_fire() != 0) {
                player_actor.trigger_skill_motion(fire_def->motion_fire(), support_cast);
            } else if (support_cast) {
                // No fire clip: leave the looping prep pose (Tw_7_A_04 *LOOP) as soon
                // as the buff/heal packet goes out.
                player_actor.end_skill();
            }
            spawn_skill_fire_fx(fx_job, skill_index, target, kind == PendingSkillKind::Ground);
        } else if (kind == PendingSkillKind::Animal) {
            stop_skill_cast_sfx();
            const auto* fire_def = skill_catalog.find(clazz, static_cast<int>(skill_index));
            if (fire_def && fire_def->motion_fire() != 0) {
                player_actor.trigger_skill_motion(fire_def->motion_fire(), true);
            }
            spawn_skill_fire_fx(fx_job, skill_index, target);
        } else if (kind == PendingSkillKind::Player || kind == PendingSkillKind::DeadPlayer) {
            // Speed Up / blessings / etc. previously never switched off the prep clip,
            // so GO_SKILL kept PlayAniRepeat after the buff was already on.
            stop_skill_cast_sfx();
            const auto* fire_def = skill_catalog.find(clazz, static_cast<int>(skill_index));
            if (fire_def && fire_def->motion_fire() != 0) {
                player_actor.trigger_skill_motion(fire_def->motion_fire(), true);
            } else {
                player_actor.end_skill();
            }
            spawn_skill_fire_fx(fx_job, skill_index, target);
        } else if (kind == PendingSkillKind::Archer) {
            spawn_skill_fire_fx(fx_job, skill_index, target);
            const auto* fire_def = skill_catalog.find(clazz, static_cast<int>(skill_index));
            if (fire_def && fire_def->motion_fire() != 0) {
                player_actor.trigger_skill_motion(fire_def->motion_fire(), true);
            } else {
                player_actor.end_skill();
            }
        } else if (kind == PendingSkillKind::Melee) {
            if (clazz == 0 && skill_index == 21) {
                const auto* rush_def = skill_catalog.find(clazz, skill_index);
                player_actor.trigger_skill_motion(rush_def ? rush_def->motion_fire() : 34, true);
            }
            spawn_skill_fire_fx(fx_job, skill_index, target);
        }
        const auto* cd_def = skill_catalog.find(clazz, static_cast<int>(skill_index));
        if (cd_def) {
            const int aspeed_ms = static_cast<int>(weapon_attack_speed_sec() * 1000.0f);
            const int cd_ms = client::Catalog::ui_cooldown_ms(
                *cd_def, aspeed_ms, player_skills.level_of(skill_index));
            skill_win.begin_cooldown(cd_def->index(), 0, cd_ms);
        }
        if (kind == PendingSkillKind::Archer) {
            return true;
        }
        // compatibility behavior sets runtime state = GetASpeed() (weapon delay), not the
        // icon timer (compare[3]).
        attack_cooldown = weapon_attack_speed_sec();
        return true;
    };

    auto begin_skill_cast = [&](PendingSkillKind kind,
                                std::uint8_t skill_index,
                                std::uint32_t target,
                                const client::SkillDef* def) -> bool {
        if (behead_animation_active) {
            return false;
        }
        if (local_player.clazz() == client::Class::Knight && skill_index == 17 &&
            net.mortal() <= 0) {
            return false;
        }
        int pre_ms = def ? client::Catalog::preskill_ms(*def,
                                                        player_skills.level_of(skill_index),
                                                        player_skills.level_of(14))
                         : 0;
        if (kind == PendingSkillKind::Archer && def) {
            pre_ms = archer_skill_prep_ms(def);
        }
        std::uint32_t preskill_target = target;
        if (kind == PendingSkillKind::SelfHeal || kind == PendingSkillKind::SelfArea ||
            kind == PendingSkillKind::Ground || (kind == PendingSkillKind::Animal && target == 0)) {
            preskill_target = net.local_player_id();
        }
        const bool focus_shot_charge =
            kind == PendingSkillKind::Archer &&
            protocol::archer_special_kind(skill_index) == protocol::ArcherSpecialKind::FocusShot;
        if (focus_shot_charge) {
            preskill_target = net.local_player_id();
        }
        const bool rush = kind == PendingSkillKind::Melee &&
                          local_player.clazz() == client::Class::Knight && skill_index == 21;
        if (rush) {
            if (local_player.boosting()) {
                (void) net.send_skill_boost(false);
                local_player.set_boosting(false);
            }
            rush_elapsed_sec = 0.0f;
            player_actor.cancel_attack();
            net.flush_local_move(local_player.pos().x,
                                 local_player.pos().y,
                                 local_player.pos().z,
                                 profile.origin_patch());
        }
        if (pre_ms > 0 && preskill_target != 0) {
            if (!net.send_preskill(skill_index, preskill_target)) {
                return false;
            }
            pending_skill_kind = kind;
            pending_skill_index = skill_index;
            pending_skill_target = focus_shot_charge ? 0 : target;
            pending_skill_left = static_cast<float>(pre_ms) / 1000.0f;
            pending_skill_total = pending_skill_left;
            pending_skill_start_position = local_player.pos();
            if (focus_shot_charge) {
                focus_shot_elapsed_sec = 0.0f;
                player_actor.cancel_attack();
            }
            // Archer: runtime state = now at C2S_PRESKILL; LMB after the shot uses
            // runtime state from that stamp. Other classes keep the precast lock.
            attack_cooldown =
                kind == PendingSkillKind::Archer ? weapon_attack_speed_sec() : pending_skill_left;
        } else if (!fire_skill_now(kind, skill_index, target, pending_skill_item_iid)) {
            return false;
        }
        queued_skill_index = -1;
        queued_skill_target = 0;
        queued_skill_is_player = false;
        if (kind == PendingSkillKind::Archer) {
            if (pre_ms <= 0) {
                attack_cooldown = weapon_attack_speed_sec();
            }
            if (def && def->attack_auto()) {
                const int auto_ms = std::max(0, def->delay_pre_ms()) + std::max(0, def->delay_ms());
                if (auto_ms > 0) {
                    attack_cooldown =
                        std::max(attack_cooldown, static_cast<float>(auto_ms) / 1000.0f);
                }
            }
        }
        if (pre_ms > 0) {
            play_skill_cast_sfx(def);
            // Prep FX via SkillPresenter (ET_* / effect[]) for local caster.
            client::SkillPresentEndpoints ends;
            mage_effect_tip(ends.tip_x, ends.tip_y, ends.tip_z);
            ends.caster_entity = net.local_player_id();
            ends.target_entity = target != 0 ? target : net.local_player_id();
            ends.valid = true;
            auto mit = net.monsters().find(target);
            if (mit != net.monsters().end()) {
                float impact_scale = 1.0f;
                resolve_monster_impact(
                    target, mit->second, ends.hit_x, ends.hit_y, ends.hit_z, impact_scale);
                ends.impact_scale = impact_scale;
            } else if (kind == PendingSkillKind::Ground) {
                ends.hit_x = coords::to_client(pending_skill_ground_x, profile.origin_patch());
                ends.hit_z = coords::to_client(pending_skill_ground_y, profile.origin_patch());
                ends.hit_y = floor.sample(ends.hit_x, ends.hit_z, local_player.pos().y);
            } else if (kind == PendingSkillKind::Mage && net.castle_enemy(target)) {
                player_impact_world(target, ends.hit_x, ends.hit_y, ends.hit_z);
            } else {
                player_spine_world(ends.hit_x, ends.hit_y, ends.hit_z);
            }
            fill_target_origin(target, ends, kind == PendingSkillKind::Ground);
            fill_caster_origin(net.local_player_id(), ends);
            (void) skill_presenter.present(*engine,
                                           effect_job_for_skill(static_cast<int>(skill_index), 0),
                                           static_cast<int>(skill_index),
                                           client::SkillPresentPhase::Prep,
                                           def,
                                           ends);
        }
        if (kind == PendingSkillKind::Melee) {
            if (rush) {
                player_actor.trigger_skill_motion(def ? def->motion_prep() : 29);
            } else if (skill_index == 1) {
                player_actor.trigger_behead_swing();
                behead_animation_active = player_actor.attack_locked();
                local_player.clear_click_target();
                local_player.set_moving(false);
            } else if (local_player.clazz() == client::Class::Knight &&
                       (skill_index == 13 || skill_index == 41)) {
                player_actor.trigger_attack();
            } else {
                // ModelSkill gubun 1 → ModelChange(GO_SKILL, motion[1]). attackauto is
                // motion[2] (extra gubun) and must not play the weapon auto-attack clip.
                // play_once: OnAnimationComplete without (*LOOP) → ModelChange(0) idle.
                player_actor.trigger_skill_motion(def ? def->motion_fire() : 0, true);
            }
        } else if (kind == PendingSkillKind::Archer) {
            // ModelSkill gubun 0 → ModelChange(GO_SKILL, motion[0]). Prep GB is
            // JSON effect[0] via SkillPresenter (Flamy Skill_M_6_0; Blow-up none).
            if (pre_ms > 0) {
                const int sub = def ? def->motion_prep() : 0;
                if (sub != 0) {
                    player_actor.trigger_skill_motion(sub);
                }
            }
        } else if (kind == PendingSkillKind::Animal) {
            if (pre_ms > 0) {
                const int sub = def ? def->motion_prep() : 0;
                if (sub != 0) {
                    player_actor.trigger_skill_motion(sub, true);
                }
            }
        } else if (pre_ms > 0) {
            // Mage / self-heal / player: prep uses motion[0]. Immediate fire is
            // already started in fire_skill_now (play-once for support casts).
            const int sub = def ? def->motion_prep() : 0;
            player_actor.trigger_skill_motion(sub != 0 ? sub : (def ? def->motion_fire() : 0));
        }
        if (kind == PendingSkillKind::Mage || kind == PendingSkillKind::Archer ||
            kind == PendingSkillKind::Melee) {
            // Behead is a one-shot finish; do not chase / recast after the corpse vanishes.
            const bool focus_shot =
                kind == PendingSkillKind::Archer && protocol::archer_special_kind(skill_index) ==
                                                        protocol::ArcherSpecialKind::FocusShot;
            if (!(kind == PendingSkillKind::Melee && skill_index == 1) && !focus_shot && !rush) {
                // Mage motion[2] `none` (Thunder, Ice Restriction, Explosion, …) is a
                // one-shot: do not recast and do not fall through to Lightning Magic.
                const bool mage_repeat =
                    kind == PendingSkillKind::Mage && def != nullptr && def->attack_auto();
                const bool archer_repeat =
                    kind == PendingSkillKind::Archer && def != nullptr && def->attack_auto();
                // Knight/thief motion[2] `none` (Powerful Upward Slash) is a one-shot:
                // keep chasing with basic attacks, do not recast the skill.
                const bool melee_repeat =
                    kind == PendingSkillKind::Melee && def != nullptr && def->attack_auto();
                auto_attack = kind != PendingSkillKind::Mage || mage_repeat;
                auto_skill = mage_repeat || archer_repeat || melee_repeat;
            } else {
                auto_attack = false;
                auto_skill = false;
            }
            local_player.clear_click_target();
        }
        return true;
    };

    auto try_mage_skill = [&](std::uint8_t skill_index, std::uint32_t target_id) -> bool {
        if (target_id == 0 || !player_wear.has_weapon() || local_player.dead() ||
            pending_skill_kind != PendingSkillKind::None || attack_cooldown > 0.0f) {
            return false;
        }
        if (skill_win.on_cooldown(static_cast<int>(skill_index))) {
            return false;
        }
        if (local_player.resting() && !request_stand()) {
            return false;
        }
        if (!player_skills.is_learned(static_cast<int>(skill_index))) {
            return false;
        }
        const auto monster = net.monsters().find(target_id);
        float x = 0.0f, z = 0.0f;
        if (monster != net.monsters().end()) {
            x = monster->second.wx();
            z = monster->second.wz();
        } else {
            const auto* player = remote_players.find(target_id);
            const auto shape = protocol::mage_target_shape(skill_index);
            if (!player || !net.castle_enemy(target_id) || net.castle_defeated(target_id) ||
                protocol::mage_effect_kind(skill_index) != protocol::MageEffectKind::Damage ||
                (shape != protocol::MageTargetShape::Monster &&
                 shape != protocol::MageTargetShape::HostileCharacter &&
                 shape != protocol::MageTargetShape::TargetArea)) {
                return false;
            }
            x = player->wx();
            z = player->wz();
        }
        const int sk_lv = std::max(1, player_skills.level_of(static_cast<int>(skill_index)));
        const int range = client::Catalog::magic_skill_range(static_cast<int>(skill_index), sk_lv);
        if (!in_client_range(x, z, range)) {
            return false;
        }
        face_world_xz(x, z);
        const auto* def = skill_catalog.find(1, static_cast<int>(skill_index));
        if (monster == net.monsters().end() &&
            (!def || client::Catalog::skill_spend_mp(1, def, sk_lv, skill_win.player_level()) >
                         player_mp)) {
            return false;
        }
        return begin_skill_cast(PendingSkillKind::Mage, skill_index, target_id, def);
    };

    auto clear_queued_target_skill = [&]() {
        queued_skill_index = -1;
        queued_skill_target = 0;
        queued_skill_is_player = false;
    };

    auto queue_target_skill =
        [&](std::uint8_t skill_index, std::uint32_t target_id, bool player_target) {
            queued_skill_index = static_cast<int>(skill_index);
            queued_skill_target = target_id;
            queued_skill_is_player = player_target;
        };

    auto try_archer_skill = [&](std::uint8_t skill_index, std::uint32_t mob_id) -> bool {
        const auto shape = protocol::archer_target_shape(static_cast<int>(skill_index));
        std::uint32_t target_id = mob_id;
        bool player_target = false;
        if (shape == protocol::ArcherTargetShape::HostileCharacter && selected_player_id != 0 &&
            selected_player_id != net.local_player_id() && mob_id == 0) {
            target_id = selected_player_id;
            player_target = true;
        }
        const auto item_req = protocol::archer_item_requirement(static_cast<int>(skill_index));
        if (item_req != protocol::ArcherItemRequirement::None) {
            const client::ItemDef* weapon = item_catalog.find(player_wear.weapon_index());
            if (!weapon || weapon->class_small() != "bow") {
                return false;
            }
        }
        if (target_id == 0 || local_player.dead() || pending_skill_kind != PendingSkillKind::None) {
            return false;
        }
        const bool focus_shot = protocol::archer_special_kind(static_cast<int>(skill_index)) ==
                                protocol::ArcherSpecialKind::FocusShot;
        if (skill_win.on_cooldown(static_cast<int>(skill_index))) {
            return false;
        }
        if (local_player.resting() && !request_stand()) {
            return false;
        }
        if (!player_skills.is_learned(static_cast<int>(skill_index))) {
            return false;
        }
        float tx = 0.0f;
        float tz = 0.0f;
        if (player_target) {
            const client::RemotePlayer* remote = remote_players.find(target_id);
            if (!remote) {
                return false;
            }
            tx = remote->wx();
            tz = remote->wz();
        } else {
            auto mit = net.monsters().find(target_id);
            if (mit == net.monsters().end()) {
                return false;
            }
            tx = mit->second.wx();
            tz = mit->second.wz();
        }
        const auto* def = skill_catalog.find(2, static_cast<int>(skill_index));
        const int sk_lv = std::max(1, player_skills.level_of(static_cast<int>(skill_index)));
        const int need_mp =
            client::Catalog::skill_spend_mp(2, def, sk_lv, skill_win.player_level());
        if (need_mp > 0 && player_mp < need_mp) {
            return false;
        }
        pending_skill_item_iid = 0;
        const int required_item =
            protocol::archer_required_item_index(static_cast<int>(skill_index));
        if (required_item > 0) {
            int item_count = 0;
            if (!player_bag.try_find_stack(required_item, pending_skill_item_iid, item_count) ||
                pending_skill_item_iid == 0 || item_count <= 0) {
                pending_skill_item_iid = 0;
                return false;
            }
        }
        const int range = client::Catalog::skill_cast_range(2, def, sk_lv, weapon_attack_range());
        const bool in_range = in_client_range(tx, tz, range);
        // Send_Skill queues across runtime state (runtime state)
        // instead of dropping the click. Out of range walks in and retries.
        if (!focus_shot && (attack_cooldown > 0.0f || !in_range)) {
            queue_target_skill(skill_index, target_id, player_target);
            if (!in_range) {
                local_player.set_click_target(tx, tz);
            } else {
                local_player.clear_click_target();
            }
            pending_skill_item_iid = 0;
            return true;
        }
        if (attack_cooldown > 0.0f) {
            pending_skill_item_iid = 0;
            return false;
        }
        if (!in_range) {
            pending_skill_item_iid = 0;
            return false;
        }
        clear_queued_target_skill();
        face_world_xz(tx, tz);
        if (!begin_skill_cast(PendingSkillKind::Archer, skill_index, target_id, def)) {
            pending_skill_item_iid = 0;
            return false;
        }
        return true;
    };

    auto try_melee_skill = [&](std::uint8_t skill_index, std::uint32_t mob_id) -> bool {
        if (mob_id == 0 || !player_wear.has_weapon() || local_player.dead() ||
            pending_skill_kind != PendingSkillKind::None) {
            return false;
        }
        // Behead (index 1) is CastKind::Behead only — never as a combat auto-recast.
        if (client::Catalog::cast_kind_for(static_cast<int>(local_player.clazz()),
                                           static_cast<int>(skill_index)) ==
            client::CastKind::Behead) {
            return false;
        }
        if (skill_win.on_cooldown(static_cast<int>(skill_index))) {
            return false;
        }
        if (local_player.resting() && !request_stand()) {
            return false;
        }
        if (!player_skills.is_learned(static_cast<int>(skill_index))) {
            return false;
        }
        auto mit = net.monsters().find(mob_id);
        const client::RemotePlayer* remote = remote_players.find(mob_id);
        if ((mit == net.monsters().end() || mit->second.hp() <= 0) &&
            ((local_player.clazz() != client::Class::Thief &&
              local_player.clazz() != client::Class::Knight) ||
             !remote)) {
            return false;
        }
        const int clazz = static_cast<int>(local_player.clazz());
        const auto* def = skill_catalog.find(clazz, static_cast<int>(skill_index));
        const int sk_lv = std::max(1, player_skills.level_of(static_cast<int>(skill_index)));
        const int need_mp =
            client::Catalog::skill_spend_mp(clazz, def, sk_lv, skill_win.player_level());
        if (need_mp > 0 && player_mp < need_mp) {
            return false;
        }
        const int range =
            client::Catalog::skill_cast_range(clazz, def, sk_lv, weapon_attack_range());
        const float target_x = mit != net.monsters().end() ? mit->second.wx() : remote->wx();
        const float target_z = mit != net.monsters().end() ? mit->second.wz() : remote->wz();
        const bool rush = clazz == 0 && skill_index == 21;
        if (rush &&
            std::hypot(target_x - local_player.pos().x, target_z - local_player.pos().z) < 60.0f) {
            return false;
        }
        const bool in_range = in_client_range(target_x, target_z, range);
        if (attack_cooldown > 0.0f || !in_range) {
            queue_target_skill(skill_index, mob_id, mit == net.monsters().end());
            if (!in_range) {
                local_player.set_click_target(target_x, target_z);
            } else {
                local_player.clear_click_target();
            }
            return true;
        }
        clear_queued_target_skill();
        face_world_xz(target_x, target_z);
        return begin_skill_cast(PendingSkillKind::Melee, skill_index, mob_id, def);
    };

    auto try_self_area_skill = [&](std::uint8_t skill_index) -> bool {
        if (!want_net || !net.connected() || !player_wear.has_weapon() || local_player.dead() ||
            pending_skill_kind != PendingSkillKind::None ||
            !player_skills.is_learned(skill_index) || skill_win.on_cooldown(skill_index)) {
            return false;
        }
        const int clazz = static_cast<int>(local_player.clazz());
        if (clazz == 0 && skill_index == 38) {
            const client::ItemDef* weapon = item_catalog.find(player_wear.weapon_index());
            if (!weapon || weapon->class_small() != "sword2h") {
                return false;
            }
        }
        const auto* def = skill_catalog.find(clazz, skill_index);
        if (!def || !client::Catalog::is_castable(*def) ||
            skill_win.player_level() < def->level_limit()) {
            return false;
        }
        const int skill_level = std::max(1, player_skills.level_of(skill_index));
        if (player_mp <
            client::Catalog::skill_spend_mp(clazz, def, skill_level, skill_win.player_level())) {
            return false;
        }
        if (local_player.resting() && !request_stand()) {
            return false;
        }
        if (attack_cooldown > 0.0f) {
            queue_target_skill(skill_index, 0, false);
            local_player.clear_click_target();
            return true;
        }
        // The area is centered on the caster's current server position.
        net.flush_local_move(local_player.pos().x,
                             local_player.pos().y,
                             local_player.pos().z,
                             profile.origin_patch());
        local_player.clear_click_target();
        return begin_skill_cast(PendingSkillKind::SelfArea, skill_index, 0, def);
    };

    auto try_behead_skill = [&](std::uint8_t skill_index, std::uint32_t target_id) -> bool {
        if (!want_net || !net.connected() || !player_wear.has_weapon() || local_player.dead() ||
            behead_animation_active || pending_skill_kind != PendingSkillKind::None ||
            !player_skills.is_learned(skill_index) || skill_win.on_cooldown(skill_index)) {
            return false;
        }
        const auto monster = net.monsters().find(target_id);
        if (monster == net.monsters().end() || !monster->second.kneeling() ||
            !in_client_range(monster->second.wx(), monster->second.wz(), kAutoChaseRange)) {
            local_player.clear_click_target();
            return false;
        }
        const auto* def = skill_catalog.find(static_cast<int>(local_player.clazz()), skill_index);
        if (!def) {
            return false;
        }
        auto_attack = false;
        auto_skill = false;
        selected_mob_id = target_id;
        selected_player_id = selected_npc_id = selected_item_id = 0;
        pending_pickup_id = pending_ask_npc_id = 0;
        const bool in_range =
            in_client_range(monster->second.wx(), monster->second.wz(), protocol::kBeheadRange);
        if (!in_range || attack_cooldown > 0.0f) {
            queue_target_skill(skill_index, target_id, false);
            if (!in_range) {
                local_player.set_click_target(monster->second.wx(), monster->second.wz());
            } else {
                local_player.clear_click_target();
            }
            return true;
        }
        net.flush_local_move(local_player.pos().x,
                             local_player.pos().y,
                             local_player.pos().z,
                             profile.origin_patch());
        if (!server::combat::in_attack_range(
                net.server_px(),
                net.server_py(),
                coords::to_server(monster->second.wx(), profile.origin_patch()),
                coords::to_server(monster->second.wz(), profile.origin_patch()),
                protocol::kBeheadRange)) {
            queue_target_skill(skill_index, target_id, false);
            local_player.set_click_target(monster->second.wx(), monster->second.wz());
            return true;
        }
        face_world_xz(monster->second.wx(), monster->second.wz());
        return begin_skill_cast(PendingSkillKind::Melee, skill_index, target_id, def);
    };

    auto flush_queued_target_skill = [&]() {
        if (queued_skill_index < 0 || local_player.dead() ||
            pending_skill_kind != PendingSkillKind::None) {
            return;
        }
        const auto skill_index = static_cast<std::uint8_t>(queued_skill_index);
        if (client::Catalog::cast_kind_for(static_cast<int>(local_player.clazz()), skill_index) ==
            client::CastKind::SelfArea) {
            if (!try_self_area_skill(skill_index)) {
                clear_queued_target_skill();
            }
            return;
        }
        if (client::Catalog::cast_kind_for(static_cast<int>(local_player.clazz()), skill_index) ==
            client::CastKind::Behead) {
            if (!try_behead_skill(skill_index, queued_skill_target)) {
                clear_queued_target_skill();
            }
            return;
        }
        const std::uint32_t target_id = queued_skill_target;
        const bool player_target = queued_skill_is_player;
        selected_player_id = player_target ? target_id : 0;
        selected_mob_id = player_target ? 0 : target_id;
        if (local_player.clazz() == client::Class::Knight ||
            local_player.clazz() == client::Class::Thief) {
            if (!try_melee_skill(skill_index, target_id)) {
                clear_queued_target_skill();
            }
            return;
        }
        if (player_target) {
            if (!try_archer_skill(skill_index, 0)) {
                clear_queued_target_skill();
            }
            return;
        }
        if (!try_archer_skill(skill_index, target_id)) {
            clear_queued_target_skill();
        }
    };

    auto reset_range_skill = [&]() {
        range_skill_cast = 0;
        range_skill_index = -1;
        range_skill_decal.clear(sprite_fx);
    };

    auto show_range_skill_marker = [&](float wx, float wy, float wz) {
        range_skill_decal.show(
            *engine,
            sprite_fx,
            wx,
            wy,
            wz,
            client::range_skill_preview_fx(skill_win.player_clazz(), range_skill_index));
    };

    auto ground_skill_range = [&](const client::SkillDef* def, int skill_level) -> int {
        int range = client::Catalog::skill_cast_range(
            skill_win.player_clazz(), def, skill_level, weapon_attack_range());
        if (def && def->cast_range() > 0) {
            range += 48;
        }
        return range;
    };

    auto try_fire_locked_range_skill = [&]() -> bool {
        if (range_skill_cast != 2 || range_skill_index < 0) {
            return false;
        }
        if (pending_skill_kind != PendingSkillKind::None || attack_cooldown > 0.0f) {
            return false;
        }
        const auto* def = skill_catalog.find(skill_win.player_clazz(), range_skill_index);
        if (!def || !player_skills.is_learned(range_skill_index) ||
            skill_win.on_cooldown(range_skill_index) || !player_wear.has_weapon()) {
            return false;
        }
        const int skill_level = std::max(1, player_skills.level_of(range_skill_index));
        const int spend_mp = client::Catalog::skill_spend_mp(
            skill_win.player_clazz(), def, skill_level, skill_win.player_level());
        if (player_mp < spend_mp) {
            return false;
        }
        const int range = ground_skill_range(def, skill_level);
        if (!in_client_range(range_skill_wx, range_skill_wz, range)) {
            return false;
        }
        pending_skill_ground_x = range_skill_server_x;
        pending_skill_ground_y = range_skill_server_y;
        face_world_xz(range_skill_wx, range_skill_wz);
        local_player.clear_click_target();
        if (!begin_skill_cast(
                PendingSkillKind::Ground, static_cast<std::uint8_t>(range_skill_index), 0, def)) {
            return false;
        }
        selected_mob_id = 0;
        selected_npc_id = 0;
        selected_player_id = 0;
        selected_item_id = 0;
        auto_attack = false;
        reset_range_skill();
        return true;
    };

    auto lock_range_skill_at_cursor = [&](float mx, float my, int width, int height) -> bool {
        if (range_skill_cast != 1 || range_skill_index < 0) {
            return false;
        }
        float gx = 0.0f;
        float gz = 0.0f;
        const bool got = client::WorldPick::pick_ground(cam,
                                                        local_player,
                                                        floor,
                                                        mx,
                                                        my,
                                                        static_cast<float>(width),
                                                        static_cast<float>(height),
                                                        gx,
                                                        gz);
        if (!got) {
            return false;
        }
        const int ground_x = coords::to_server(gx, profile.origin_patch());
        const int ground_y = coords::to_server(gz, profile.origin_patch());
        const int map_cells =
            map::MapStream::ensure_around(walk_grid, server_map_dir, ground_x, ground_y, 1);
        const bool valid_ground =
            map_cells > 0 &&
            (uses_indoor_visuals()
                 ? (floor.get_floor(gx, gz) > 0.0f)
                 : client::LocalPlayer::may_stand_at(
                       gx, gz, local_player.pos().y, floor, &walk_grid, profile.origin_patch()));
        if (!valid_ground) {
            return false;
        }
        range_skill_wx = gx;
        range_skill_wz = gz;
        range_skill_server_x = ground_x;
        range_skill_server_y = ground_y;
        range_skill_cast = 2;
        const float gy = floor.sample(gx, gz, local_player.pos().y) + 0.15f;
        show_range_skill_marker(gx, gy, gz);
        if (!try_fire_locked_range_skill()) {
            local_player.set_click_target(gx, gz);
        }
        return true;
    };

    auto begin_focus_shot_charge = [&](std::uint8_t skill_index) -> bool {
        if (protocol::archer_special_kind(static_cast<int>(skill_index)) !=
            protocol::ArcherSpecialKind::FocusShot) {
            return false;
        }
        if (is_focus_shot_charging()) {
            return true;
        }
        if (local_player.dead() || pending_skill_kind != PendingSkillKind::None) {
            return false;
        }
        const client::ItemDef* weapon = item_catalog.find(player_wear.weapon_index());
        if (!weapon || weapon->class_small() != "bow") {
            return false;
        }
        if (skill_win.on_cooldown(static_cast<int>(skill_index))) {
            return false;
        }
        if (local_player.resting() && !request_stand()) {
            return false;
        }
        if (!player_skills.is_learned(static_cast<int>(skill_index))) {
            return false;
        }
        if (net.local_player_id() == 0) {
            return false;
        }
        const auto* def = skill_catalog.find(2, static_cast<int>(skill_index));
        const int sk_lv = std::max(1, player_skills.level_of(static_cast<int>(skill_index)));
        const int need_mp =
            client::Catalog::skill_spend_mp(2, def, sk_lv, skill_win.player_level());
        if (need_mp > 0 && player_mp < need_mp) {
            return false;
        }
        return begin_skill_cast(PendingSkillKind::Archer, skill_index, 0, def);
    };

    auto enter_range_skill = [&](int skill_index) -> bool {
        range_skill_cast = 1;
        range_skill_index = skill_index;
        return true;
    };

    client::SocialAction social_mode = client::SocialAction::None;

    auto cast_skill_index = [&](int skill_index) -> bool {
        if (behead_animation_active) {
            return false;
        }
        if (skill_index < 0 || local_player.dead()) {
            return false;
        }
        const auto* action_def = skill_catalog.find(skill_win.player_clazz(), skill_index);
        const auto social =
            action_def ? client::social_action(*action_def) : client::SocialAction::None;
        if (social != client::SocialAction::None) {
            if (!multiplayer || !net.connected() || social == client::SocialAction::Removed) {
                return false;
            }
            if (client::social_action_uses_active_slot(social)) {
                social_mode = client::SocialAction::None;
                player_skills.set_active(skill_index);
            } else if (social == client::SocialAction::Guild) {
                social_mode = client::SocialAction::None;
                const bool was_open = guild_window.open();
                guild_window.set_open(true);
                update_window_order(client::InGameWindowId::Guild, was_open, true);
            } else {
                social_mode = social_mode == social ? client::SocialAction::None : social;
                game_hud.chat().push_info(social_mode == client::SocialAction::Whisper
                                              ? "Whisper: left-click a player."
                                              : "Player interaction cleared.",
                                          {});
            }
            auto_attack = false;
            auto_skill = false;
            local_player.clear_click_target();
            return true;
        }
        social_mode = client::SocialAction::None;
        const int emote_motion = protocol::Emote::motion_for_shortcut(skill_index);
        if (emote_motion != 0) {
            if (!player_skills.is_learned(skill_index) || player_actor.opening_box() ||
                local_player.resting() || local_player.moving() ||
                pending_skill_kind != PendingSkillKind::None || attack_cooldown > 0.0f ||
                (net.player_gstate(net.local_player_id()) &
                 (protocol::kCgsOnTransform | protocol::kCgsFish | protocol::kCgsCooking)) != 0) {
                return false;
            }
            if (want_net) {
                if (!net.connected() || !net.send_emote(static_cast<std::uint8_t>(emote_motion))) {
                    return false;
                }
            } else if (!player_actor.trigger_emote(emote_motion)) {
                return false;
            }
            auto_attack = false;
            auto_skill = false;
            local_player.clear_click_target();
            reset_range_skill();
            return true;
        }
        if (protocol::is_transform_into_skill(skill_index)) {
            const int stage = protocol::transform_skill_stage(skill_index);
            if (!protocol::transform_stage_unlocked(
                    net.transform_egg_level() > 0 ? net.transform_egg_level() : 1, stage)) {
                return false;
            }
            const int current = protocol::transform_tflag_stage(net.transform_tflag());
            if (current == stage || pending_transform_execute_stage == stage) {
                request_human_form();
                return true;
            }
            request_transform_stage(stage);
            return true;
        }
        if (is_focus_shot_charging()) {
            return skill_index == static_cast<int>(pending_skill_index);
        }
        if (pending_skill_kind != PendingSkillKind::None) {
            return false;
        }
        if (protocol::is_animal_skill(skill_index)) {
            if (!want_net || !net.connected() ||
                protocol::transform_tflag_stage(net.transform_tflag()) <
                    ((skill_index - 101) % 3) + 1) {
                return false;
            }
            if (skill_win.on_cooldown(skill_index)) {
                return false;
            }
            const int race = protocol::transform_tflag_race(net.transform_tflag());
            const auto* def = skill_catalog.find(protocol::animal_skill_class(race), skill_index);
            const bool needs_target = skill_index != 103 && skill_index != 106 &&
                                      skill_index != 109 && skill_index != 110 &&
                                      skill_index != 112;
            if (needs_target) {
                if (selected_mob_id == 0) {
                    return false;
                }
                return begin_skill_cast(PendingSkillKind::Animal,
                                        static_cast<std::uint8_t>(skill_index),
                                        selected_mob_id,
                                        def);
            }
            return begin_skill_cast(
                PendingSkillKind::Animal, static_cast<std::uint8_t>(skill_index), 0, def);
        }
        if (!player_skills.is_learned(skill_index)) {
            return false;
        }
        if (skill_win.on_cooldown(skill_index)) {
            return false;
        }
        const auto* def = skill_catalog.find(skill_win.player_clazz(), skill_index);
        if (!def || !client::Catalog::is_castable(*def)) {
            return false;
        }
        if (skill_win.player_level() < def->level_limit()) {
            return false;
        }
        if (skill_index == protocol::kBeaconSkill) {
            return want_net && net.connected() &&
                   net.send_talent_request(protocol::TalentCommand::Beacon);
        }
        if (!client::Catalog::uses_range_skill_selector(skill_win.player_clazz(), skill_index)) {
            reset_range_skill();
        }
        const auto kind = client::Catalog::cast_kind_for(skill_win.player_clazz(), skill_index);
        const int skill_level = std::max(1, player_skills.level_of(skill_index));
        const int spend_mp = client::Catalog::skill_spend_mp(
            skill_win.player_clazz(), def, skill_level, skill_win.player_level());
        if (kind != client::CastKind::GroundPosition && spend_mp > player_mp) {
            return false;
        }
        bool sent = false;
        switch (kind) {
        case client::CastKind::Boost: {
            local_player.toggle_boost();
            sent = true;
            if (want_net && net.connected()) {
                if (!net.send_skill_boost(local_player.boosting())) {
                    log::warn("failed to send skill boost state");
                }
            }
            break;
        }
        case client::CastKind::Rest: {
            const bool want = !local_player.resting();
            if (want && local_player.boosting()) {
                local_player.set_boosting(false);
                if (want_net && net.connected()) {
                    if (!net.send_skill_boost(false)) {
                        log::warn("failed to clear skill boost before rest");
                    }
                }
            }
            if (want_net && net.connected()) {
                sent = net.send_rest(want);
                if (sent) {
                    local_player.set_resting(want);
                    local_player.clear_click_target();
                }
            } else {
                local_player.set_resting(want);
                local_player.clear_click_target();
                sent = true;
            }
            break;
        }
        case client::CastKind::SelfHeal:
        case client::CastKind::Party: {
            const bool walk_on_air = local_player.clazz() == client::Class::Knight &&
                                     protocol::knight_special_kind(skill_index) ==
                                         protocol::KnightSpecialKind::WalkOnAir;
            if (walk_on_air && local_player.boosting()) {
                local_player.set_boosting(false);
                if (want_net && net.connected()) {
                    if (!net.send_skill_boost(false)) {
                        log::warn("failed to clear skill boost before Walk on the Air");
                    }
                }
            }
            if (want_net && net.connected()) {
                sent = begin_skill_cast(
                    PendingSkillKind::SelfHeal, static_cast<std::uint8_t>(skill_index), 0, def);
            } else if (walk_on_air) {
                sent = true;
            }
            if (sent && walk_on_air) {
                local_player.set_walking_on_air(!local_player.walking_on_air());
            }
            break;
        }
        case client::CastKind::SelfArea:
            sent = try_self_area_skill(static_cast<std::uint8_t>(skill_index));
            break;
        case client::CastKind::Player: {
            if (!want_net || !net.connected() || net.local_player_id() == 0) {
                break;
            }
            const bool archer_hostile_player =
                local_player.clazz() == client::Class::Archer &&
                (skill_index == 15 || skill_index == 27 || skill_index == 34);
            if (archer_hostile_player) {
                sent = try_archer_skill(static_cast<std::uint8_t>(skill_index), 0);
                break;
            }
            const std::uint32_t target =
                selected_player_id != 0 ? selected_player_id : net.local_player_id();
            sent = begin_skill_cast(
                PendingSkillKind::Player, static_cast<std::uint8_t>(skill_index), target, def);
            break;
        }
        case client::CastKind::DeadPlayer:
            if (want_net && net.connected() && selected_player_id != 0) {
                sent = begin_skill_cast(PendingSkillKind::DeadPlayer,
                                        static_cast<std::uint8_t>(skill_index),
                                        selected_player_id,
                                        def);
            }
            break;
        case client::CastKind::GroundPosition:
            if (client::Catalog::uses_range_skill_selector(skill_win.player_clazz(), skill_index)) {
                sent = enter_range_skill(skill_index);
            } else {
                player_skills.set_active(skill_index);
                sent = true;
            }
            break;
        case client::CastKind::Behead: {
            if (!player_wear.has_weapon() || pending_skill_kind != PendingSkillKind::None ||
                attack_cooldown > 0.0f) {
                break;
            }
            if (local_player.resting() && !request_stand()) {
                break;
            }
            std::uint32_t target_id = 0;
            if (selected_mob_id != 0) {
                auto mit = net.monsters().find(selected_mob_id);
                if (mit != net.monsters().end() && mit->second.kneeling()) {
                    target_id = selected_mob_id;
                }
            }
            if (target_id == 0) {
                target_id = find_nearest_kneeling_id();
            }
            if (target_id == 0) {
                break;
            }
            auto mit = net.monsters().find(target_id);
            if (mit == net.monsters().end() || !mit->second.kneeling()) {
                break;
            }
            sent = try_behead_skill(static_cast<std::uint8_t>(skill_index), target_id);
            break;
        }
        case client::CastKind::Combat:
        case client::CastKind::TargetArea:
            if (local_player.clazz() == client::Class::Magician) {
                sent = try_mage_skill(static_cast<std::uint8_t>(skill_index),
                                      selected_mob_id != 0 ? selected_mob_id : selected_player_id);
            } else if (local_player.clazz() == client::Class::Archer) {
                if (protocol::archer_special_kind(skill_index) ==
                    protocol::ArcherSpecialKind::FocusShot) {
                    sent = begin_focus_shot_charge(static_cast<std::uint8_t>(skill_index));
                } else {
                    sent =
                        try_archer_skill(static_cast<std::uint8_t>(skill_index), selected_mob_id);
                    if (!sent && selected_player_id != 0 &&
                        selected_player_id != net.local_player_id()) {
                        sent = try_archer_skill(static_cast<std::uint8_t>(skill_index), 0);
                    }
                }
            } else {
                const std::uint32_t melee_target =
                    selected_mob_id != 0 ? selected_mob_id : selected_player_id;
                sent = try_melee_skill(static_cast<std::uint8_t>(skill_index), melee_target);
            }
            break;
        default:
            break;
        }
        return sent;
    };

    auto sync_equip_net = [&](client::EquipNetOp op, std::uint32_t iid) {
        if (!want_net || !net.connected() || iid == 0) {
            return;
        }
        if (op == client::EquipNetOp::PutOn) {
            if (!net.send_puton_item(iid)) {
                log::warn("failed to send put-on for item iid=" + std::to_string(iid));
            }
        } else if (op == client::EquipNetOp::PutOff) {
            if (!net.send_putoff_item(iid)) {
                log::warn("failed to send put-off for item iid=" + std::to_string(iid));
            }
        }
    };

    auto apply_local_enchant = [&]() {
        const int from = inv_win.last_enchant_from_slot();
        const int to = inv_win.last_enchant_to_slot();
        if (!player_bag.item_at(from) || !player_bag.item_at(to)) {
            return;
        }
        auto& src = *player_bag.item_at(from);
        auto& dst = *player_bag.item_at(to);
        const client::ItemDef* tool = item_catalog.find(src.index);
        const client::ItemDef* gear = item_catalog.find(dst.index);
        if (!tool || !gear || dst.index <= 0 || dst.count <= 0) {
            return;
        }
        auto consume_src = [&]() {
            src.count -= 1;
            if (src.count <= 0) {
                src = {};
            }
        };
        auto fill_dst_end = [&]() {
            if (gear->is_ornament()) {
                dst.cur_end = 0;
                dst.max_end = 0;
                return;
            }
            if (dst.max_end <= 0 && gear->endurance() > 0) {
                dst.max_end = gear->endurance();
            }
            if (dst.cur_end <= 0 && dst.max_end > 0) {
                dst.cur_end = dst.max_end;
            }
        };
        if (server::enchantment_talisman_grade(tool->index()) != 0) {
            if (from == to || src.equipped ||
                !server::can_apply_weapon_enchantment(
                    tool->index(), gear->is_weapon(), dst.equipped, dst.count, gear->grade())) {
                return;
            }
            server::CharmState current;
            current.x_attack = dst.x_attack;
            current.x_magic = dst.x_magic;
            current.x_hit = dst.x_hit;
            current.x_defense = dst.x_defense;
            current.x_dodge = dst.x_dodge;
            current.upgrade_level = dst.upgrade_level;
            current.upgrade_rate = dst.upgrade_rate;
            current.cur_end = dst.cur_end;
            current.max_end = dst.max_end;
            current.enchant_protected = dst.enchant_protected;
            const auto rolled = server::apply_weapon_enchantment(
                current, gear->class_small() == "wand", server::roll_weapon_enchantment());
            const auto entry = [&](const client::ItemDef& def) {
                return server::EnchantmentWeapon{def.index(),
                                                 def.grade(),
                                                 def.class_small(),
                                                 def.class_limit(),
                                                 mix_catalog.is_imperial_weapon(def.index()) ||
                                                     server::is_imperial_weapon_name(def.name())};
            };
            std::vector<server::EnchantmentWeapon> weapons;
            for (const auto& pair : item_catalog.by_index()) {
                if (pair.second.is_weapon()) {
                    weapons.push_back(entry(pair.second));
                }
            }
            const int replacement =
                server::adjacent_enchantment_weapon(entry(*gear), weapons, rolled.grade_step);
            consume_src();
            dst.index = replacement;
            dst.x_attack = rolled.state.x_attack;
            dst.x_magic = rolled.state.x_magic;
            dst.x_hit = rolled.state.x_hit;
            dst.upgrade_level = rolled.state.upgrade_level;
            dst.upgrade_rate = rolled.state.upgrade_rate;
            dst.enchant_protected = rolled.state.enchant_protected;
            enchant_overlay.show_success();
            return;
        }
        if (tool->index() == server::kRevisionHammerIndex) {
            if (!item_catalog.find(server::kRevisionStoneIndex) || src.equipped ||
                !server::can_apply_revision_hammer(gear->is_weapon(),
                                                   gear->is_defense(),
                                                   gear->is_ornament(),
                                                   gear->class_small(),
                                                   dst.equipped,
                                                   dst.count,
                                                   gear->grade())) {
                return;
            }
            const int stones = server::revision_hammer_stones(
                gear->is_weapon(),
                gear->is_ornament(),
                gear->grade(),
                gear->is_ornament() ? 0 : server::combat::irand(0, gear->grade() / 10));
            auto converted = player_bag;
            auto* hammer = converted.item_at(from);
            auto* target = converted.item_at(to);
            if (--hammer->count == 0) {
                *hammer = {};
            }
            *target = {};
            if (converted.try_insert(server::kRevisionStoneIndex, stones, item_catalog)) {
                player_bag = std::move(converted);
                enchant_overlay.show_success();
            }
            return;
        }
        if (tool->index() == server::kPolishingStoneIndex) {
            if (!src.equipped && server::can_apply_polishing_stone(gear->is_weapon(),
                                                                   gear->is_defense(),
                                                                   dst.equipped,
                                                                   dst.count,
                                                                   dst.enchant_protected)) {
                dst.enchant_protected = true;
                consume_src();
            }
            return;
        }
        if (tool->has_repair()) {
            fill_dst_end();
            const server::RepairState repaired = server::apply_repair(
                tool->repair_mode(), tool->repair_amount(), dst.cur_end, dst.max_end);
            if (repaired.outcome == server::RepairOutcome::Rejected) {
                return;
            }
            consume_src();
            if (repaired.outcome == server::RepairOutcome::Destroyed) {
                if (dst.equipped) {
                    player_wear.unequip_index(dst.index);
                }
                dst = {};
                refresh_player_visuals();
                return;
            }
            dst.cur_end = repaired.cur_end;
            dst.max_end = repaired.max_end;
            return;
        }
        if (tool->has_charming()) {
            server::CharmTarget target;
            target.is_weapon = gear->is_weapon();
            target.is_wand = gear->class_small() == "wand";
            target.is_defense = gear->is_defense();
            target.is_shield = gear->class_small() == "shield";
            target.equipped = dst.equipped;
            target.grade = gear->grade();
            server::CharmState in;
            in.x_attack = dst.x_attack;
            in.x_magic = dst.x_magic;
            in.x_hit = dst.x_hit;
            in.x_defense = dst.x_defense;
            in.x_dodge = dst.x_dodge;
            in.upgrade_level = dst.upgrade_level;
            in.upgrade_rate = dst.upgrade_rate;
            fill_dst_end();
            in.cur_end = dst.cur_end;
            in.max_end = dst.max_end;
            in.enchant_protected = dst.enchant_protected;
            const auto rolled = server::apply_charm(server::parse_charm_kind(tool->charming()),
                                                    in,
                                                    target,
                                                    server::combat::irand(1, 10000000),
                                                    server::combat::irand(1, 100),
                                                    server::combat::irand(1, 100));
            if (rolled.outcome == server::CharmOutcome::Rejected) {
                return;
            }
            consume_src();
            dst.enchant_protected = rolled.state.enchant_protected;
            if (rolled.outcome == server::CharmOutcome::Success) {
                dst.x_attack = rolled.state.x_attack;
                dst.x_magic = rolled.state.x_magic;
                dst.x_hit = rolled.state.x_hit;
                dst.x_defense = rolled.state.x_defense;
                dst.x_dodge = rolled.state.x_dodge;
                dst.cur_end = rolled.state.cur_end;
                dst.max_end = rolled.state.max_end;
                enchant_overlay.show_success();
                return;
            }
            if (rolled.outcome == server::CharmOutcome::FailDestroyed) {
                if (dst.equipped) {
                    player_wear.unequip_index(dst.index);
                }
                dst = {};
                if (rolled.stones > 0) {
                    (void) player_bag.try_insert(
                        server::kRevisionStoneIndex, rolled.stones, item_catalog);
                }
                refresh_player_visuals();
                enchant_overlay.show_failed();
                return;
            }
            dst.x_attack = rolled.state.x_attack;
            dst.x_magic = rolled.state.x_magic;
            dst.x_hit = rolled.state.x_hit;
            dst.x_defense = rolled.state.x_defense;
            dst.x_dodge = rolled.state.x_dodge;
            dst.cur_end = rolled.state.cur_end;
            dst.max_end = rolled.state.max_end;
            enchant_overlay.show_failed();
            return;
        }
        if (!tool->has_changeprefix()) {
            return;
        }
        const int roll = server::combat::irand(1, 100);
        const int next = server::roll_change_prefix(dst.prefix,
                                                    tool->changeprefix_rate0(),
                                                    tool->changeprefix_prefix0(),
                                                    tool->changeprefix_rate1(),
                                                    tool->changeprefix_prefix1(),
                                                    roll);
        const bool protected_attempt = dst.enchant_protected;
        dst.enchant_protected = false;
        if (next != dst.prefix) {
            dst.prefix = next;
            enchant_overlay.show_success();
        } else {
            fill_dst_end();
            const server::PrefixFailEndurance failed =
                server::apply_prefix_fail_endurance(gear->is_ornament(),
                                                    dst.cur_end,
                                                    dst.max_end,
                                                    server::combat::irand(0, 5),
                                                    protected_attempt);
            if (failed.destroyed) {
                const int extra = server::combat::irand(0, gear->grade() / 10);
                const int stones = server::revision_stones_for_grade(gear->grade(), extra);
                if (dst.equipped) {
                    player_wear.unequip_index(dst.index);
                }
                dst = {};
                if (stones > 0) {
                    (void) player_bag.try_insert(server::kRevisionStoneIndex, stones, item_catalog);
                }
                refresh_player_visuals();
            } else {
                dst.cur_end = failed.cur_end;
            }
            enchant_overlay.show_failed();
        }
        consume_src();
    };

    auto apply_local_upgrade = [&](bool rate_up) {
        const std::uint32_t iid = blacksmith_win.weapon_iid();
        if (iid == 0) {
            return;
        }
        client::ItemStack* weapon = nullptr;
        for (auto& slot : player_bag.normal) {
            if (slot.iid == iid && slot.count > 0) {
                weapon = &slot;
                break;
            }
        }
        if (!weapon) {
            return;
        }
        const client::ItemDef* def = item_catalog.find(weapon->index);
        if (!def) {
            return;
        }
        server::CharmTarget target;
        target.is_weapon = def->is_weapon();
        target.is_wand = def->class_small() == "wand";
        target.is_defense = def->is_defense();
        target.is_shield = def->class_small() == "shield";
        target.equipped = weapon->equipped;
        target.grade = def->grade();
        server::CharmState state;
        state.x_attack = weapon->x_attack;
        state.x_magic = weapon->x_magic;
        state.x_hit = weapon->x_hit;
        state.x_defense = weapon->x_defense;
        state.x_dodge = weapon->x_dodge;
        state.upgrade_level = weapon->upgrade_level;
        state.upgrade_rate = weapon->upgrade_rate;
        state.cur_end = weapon->cur_end;
        state.max_end = weapon->max_end;
        state.enchant_protected = weapon->enchant_protected;
        if (rate_up) {
            int consumed = 0;
            int owned = 0;
            for (const auto& slot : player_bag.normal) {
                if (slot.index == server::kRevisionStoneIndex) {
                    owned += slot.count;
                }
            }
            if (server::try_upgrade_rate_up(state, target, owned, consumed) !=
                server::UpgradeRateOutcome::Applied) {
                return;
            }
            if (!player_bag.try_remove(server::kRevisionStoneIndex, consumed)) {
                return;
            }
            weapon->upgrade_rate = state.upgrade_rate;
            enchant_overlay.show_success();
            return;
        }
        int gold_spent = 0;
        const auto outcome = server::apply_upgrade_level(state,
                                                         target,
                                                         player_bag.money,
                                                         server::combat::irand(1, 100),
                                                         server::combat::irand(1, 100),
                                                         gold_spent);
        if (outcome == server::UpgradeLevelOutcome::Rejected) {
            return;
        }
        weapon->enchant_protected = state.enchant_protected;
        player_bag.money -= gold_spent;
        if (player_bag.money < 0) {
            player_bag.money = 0;
        }
        if (outcome == server::UpgradeLevelOutcome::Destroyed) {
            if (weapon->equipped) {
                player_wear.unequip_index(weapon->index);
            }
            *weapon = {};
            refresh_player_visuals();
            enchant_overlay.show_failed();
            return;
        }
        weapon->upgrade_level = state.upgrade_level;
        weapon->upgrade_rate = state.upgrade_rate;
        if (outcome == server::UpgradeLevelOutcome::Success) {
            enchant_overlay.show_success();
        } else {
            enchant_overlay.show_failed();
        }
    };

    refresh_player_visuals();

    auto apply_city_teleport = [&](int sx, int sy, int sz) {
        if (cam.fly_mode()) {
            cam.exit_fly(local_player, floor);
            engine->set_cursor_mode(engine::CursorMode::Free);
        }
        const float wx = coords::to_client(sx, profile.origin_patch());
        const float wz = coords::to_client(sy, profile.origin_patch());
        auto drop_all_outdoor = [&]() {
            std::vector<std::pair<int, int>> keys;
            keys.reserve(drawn.size());
            for (const auto& d : drawn) {
                keys.emplace_back(d.patch_x, d.patch_y);
            }
            for (const auto& k : keys) {
                drop_streamed_patch(k.first, k.second);
            }
            loaded_patches.clear();
            loaded_opl.clear();
        };
        auto sync_indoor = [&]() {
            if (!uses_indoor_visuals()) {
                indoor_view.unload(floor);
                return;
            }
            int tile_x = 0;
            int tile_y = 0;
            coords::pixel_to_tile(sx, sy, tile_x, tile_y);
            int cell_x = 0;
            int cell_y = 0;
            int local_tx = 0;
            int local_ty = 0;
            coords::world_tile_to_cell(tile_x, tile_y, cell_x, cell_y, local_tx, local_ty);
            const map::MapInfo* info = indoor_maps.find_cell(local_map, cell_x, cell_y);
            if (info == nullptr) {
                info = indoor_maps.find_index(local_map);
            }
            if (info == nullptr || info->mapname().empty()) {
                log::warn("no indoor mapname for map " + std::to_string(local_map) + " cell " +
                          std::to_string(cell_x) + "," + std::to_string(cell_y));
                return;
            }
            const std::string map_file =
                map::IndoorMap::resolve_map_file(indoor_root, info->mapname());
            if (map_file.empty()) {
                log::warn("indoor .map missing for " + info->mapname() + " under " + indoor_root);
                return;
            }
            if (indoor_view.path() == map_file) {
                return;
            }
            if (!indoor_view.load(*engine,
                                  indoor_root,
                                  map_file,
                                  profile.origin_patch(),
                                  profile.origin_patch(),
                                  cell_x,
                                  cell_y,
                                  model_cache,
                                  prop_tex_cache,
                                  floor,
                                  &world_objects,
                                  &world_prop_fx_serial)) {
                log::warn("indoor visual load failed: " + map_file);
                return;
            }
            drop_all_outdoor();
        };
        if (!uses_indoor_visuals()) {
            indoor_view.unload(floor);
            (void) ensure_world_around(wx, wz, -1, true);
        } else {
            sync_indoor();
            if (!indoor_view.loaded()) {
                (void) ensure_world_around(wx, wz, -1, true);
            }
        }
        rebuild_water_mesh(wx, wz);
        const int map_cells = map::MapStream::ensure_around(walk_grid, server_map_dir, sx, sy, 2);
        if (map_cells <= 0) {
            log::warn("map stream ensure_around loaded no walk cells");
        }
        const float saved_y = coords::to_client_y(sz);
        const float spawn_y = floor.sample(wx, wz, saved_y);
        local_player.set_pos(wx, spawn_y, wz);
        local_player.clear_click_target();
        local_player.set_resting(false);
        refresh_npc_candidates(sx, sy);
        selected_mob_id = 0;
        selected_npc_id = 0;
        selected_player_id = 0;
        selected_item_id = 0;
        pending_pickup_id = 0;
        for (auto& [id, inst] : monster_instances) {
            detach_monster_visual(id);
            inst.destroy(*engine);
        }
        monster_instances.clear();
        monster_loop_sfx.clear();
        monster_loop_armed.clear();
        for (const std::uint32_t id : freeze_fx_armed) {
            combat_fx.kill_clip_entity(id);
        }
        freeze_fx_armed.clear();
    };
    apply_city_teleport(spawn_sx, spawn_sy, spawn_sz);
    if (drawn.empty() && !indoor_view.loaded()) {
        log::warn("spawn has no drawable map; falling back to spawn");
        reset_to_profile_spawn();
        apply_city_teleport(spawn_sx, spawn_sy, spawn_sz);
        if (drawn.empty() && !indoor_view.loaded()) {
            log::error("no patches near Narootuh ? check MAPS data");
            embedded_server.stop();
            if (store) {
                store->close();
            }
            ui.destroy();
            engine->shutdown();
            return 1;
        }
    }

    auto try_use_item = [&](int item_index,
                            const char* via,
                            std::uint8_t auto_hp_percent = 0,
                            std::uint32_t requested_iid = 0) -> bool {
        if (item_index <= 0) {
            return false;
        }
        if (!want_net || !net.connected() || !net.handshook()) {
            return false;
        }
        if (item_cooldowns.remaining_fraction(item_index) > 0.0f) {
            return false;
        }
        const client::ItemDef* def = item_catalog.find(item_index);
        if (!def || !def->is_consumable()) {
            return false;
        }
        if (def->auto_hp() != (auto_hp_percent != 0)) {
            return false;
        }
        const bool is_revival = def->revival_extra_cut() >= 0;
        const bool is_buff = def->buff_type() >= 0;
        const bool is_refresh = def->refresh_hp() > 0 || def->refresh_mp() > 0;
        const bool is_reward_box = def->is_riding_cache() || def->is_essence_box();
        const bool is_teleport = def->teleport_loc() >= 0;
        if (is_revival) {
            if (!local_player.dead()) {
                return false;
            }
        } else if (is_buff || is_refresh || is_reward_box || is_teleport) {
            if (local_player.dead()) {
                return false;
            }
        } else {
            return false;
        }
        std::uint32_t iid = 0;
        int count = 0;
        if (!player_bag.try_find_stack(item_index, iid, count)) {
            return false;
        }
        if (requested_iid != 0) {
            const auto* selected = player_bag.find_instance(requested_iid);
            if (!selected || selected->index != item_index || selected->count <= 0 ||
                selected->equipped) {
                return false;
            }
            iid = requested_iid;
        }
        if (iid == 0 || !net.send_use_item(iid, auto_hp_percent)) {
            return false;
        }
        item_cooldowns.start(item_index);
        log::debug(std::string("use item request: index=") + std::to_string(item_index) +
                   " via=" + via);
        return true;
    };

    bool pending_server_fly_off = false;
    bool show_fps = false;
    constexpr std::string_view kDialogUsage = "Usage: /dialog <id>";
    constexpr std::string_view kBofUsage =
        "Usage: /bof <helmet|chest|gloves|pants|boots|all|clear|level N|status>";
    constexpr std::string_view kClientHelp =
        "Commands: /help, /fly, /debug, /fps, /reset, /teleport, /getitemid, /level, /exp, "
        "/eggexp, "
        "/rage, /job, /dialog, /dialogreload, /bof, /show, /hide";
    if (!world_debug.load_spawn_areas(monsters_dir)) {
        log::warn("monster spawn debug overlays unavailable: " + monsters_dir);
    }
    client::BofFx bof_fx;
    auto sync_bof_fx = [&]() {
        if (!engine) {
            return;
        }
        bof_fx.sync(*engine, sprite_fx, local_player.clazz());
    };
    auto sync_bof_from_wear = [&]() {
        if (!engine) {
            return;
        }
        if (player_actor.metamo_type() >= 0) {
            bof_fx.detach(sprite_fx);
            return;
        }
        if (bof_fx.debug_override()) {
            return;
        }
        std::vector<server::BofWornPiece> pieces;
        for (const auto& slot : player_bag.equipment) {
            if (!slot.equipped || slot.bof == 0 || slot.index <= 0) {
                continue;
            }
            const client::ItemDef* def = item_catalog.find(slot.index);
            if (!def || !item_catalog.bof().is_bof_armor_part(def->class_small())) {
                continue;
            }
            server::BofWornPiece piece;
            piece.set_grade(def->grade());
            pieces.push_back(piece);
        }
        const server::BofCombatBonus bonus =
            item_catalog.bof().compute_bonus(static_cast<int>(local_player.clazz()), pieces);
        bof_fx.apply_wear(
            bonus.completeness(), bonus.fx_level(), *engine, sprite_fx, local_player.clazz());
    };
    auto dialog_json_name = [&](int id) -> std::string {
        char name[32];
        std::snprintf(name, sizeof(name), "%06d.json", id);
        return std::string(name);
    };
    auto open_dialog_by_id = [&](int id) -> bool {
        if (id <= 0) {
            return false;
        }
        const client::DialogDef* dlg = dialog_catalog.find(id);
        if (!dlg) {
            return false;
        }
        if (client::DebugOverlay::enabled()) {
            game_hud.chat().push_info("Dialog open: " + dialog_json_name(id), {});
        }
        const bool was_open = npc_talk_win.open();
        npc_talk_win.show_dialog(*dlg, 0, boot.selected.name);
        update_window_order(client::InGameWindowId::NpcTalk, was_open, npc_talk_win.open());
        return true;
    };
    game_hud.chat().commands().register_command(
        "help", [&](const client::ClientChatCommandContext&) {
            game_hud.chat().blur();
            return client::ClientChatCommandResult(true, std::string(kClientHelp));
        });
    game_hud.chat().commands().register_command(
        "fly", [&](const client::ClientChatCommandContext&) {
            game_hud.chat().blur();
            if (boot.multiplayer || uses_indoor_visuals()) {
                return client::ClientChatCommandResult(true, "Fly mode is unavailable here.");
            }
            if (cam.fly_mode()) {
                cam.exit_fly(local_player, floor);
                engine->set_cursor_mode(engine::CursorMode::Free);
                if (want_net && net.connected()) {
                    pending_server_fly_off = true;
                    return client::ClientChatCommandResult(true, {});
                }
                return client::ClientChatCommandResult(true, "Fly mode off.");
            }
            cam.enter_fly(local_player);
            engine->set_cursor_mode(engine::CursorMode::Captured);
            local_player.clear_click_target();
            if (want_net && net.connected()) {
                (void) net.send_chat("/fly");
                return client::ClientChatCommandResult(true, {});
            }
            return client::ClientChatCommandResult(true, "Fly mode on.");
        });
    game_hud.chat().commands().register_command(
        "debug", [&](const client::ClientChatCommandContext&) {
            game_hud.chat().blur();
            client::DebugOverlay::set_enabled(!client::DebugOverlay::enabled());
            const bool on = client::DebugOverlay::enabled();
            log::info(on ? "debug overlay on" : "debug overlay off");
            if (want_net && net.connected()) {
                (void) net.send_chat(on ? "/debug on" : "/debug off");
            }
            return client::ClientChatCommandResult(true,
                                                   on ? "Debug overlay on." : "Debug overlay off.");
        });
    game_hud.chat().commands().register_command(
        "fps", [&](const client::ClientChatCommandContext&) {
            game_hud.chat().blur();
            show_fps = !show_fps;
            return client::ClientChatCommandResult(
                true, show_fps ? "FPS counter on." : "FPS counter off.");
        });
    game_hud.chat().commands().register_command(
        "show", [&](const client::ClientChatCommandContext& ctx) {
            game_hud.chat().blur();
            return client::ClientChatCommandResult(true, world_debug.show(ctx.args()));
        });
    game_hud.chat().commands().register_command(
        "hide", [&](const client::ClientChatCommandContext& ctx) {
            game_hud.chat().blur();
            return client::ClientChatCommandResult(true, world_debug.hide(ctx.args()));
        });
    game_hud.chat().commands().register_command(
        "reset", [&](const client::ClientChatCommandContext&) {
            game_hud.chat().blur();
            skill_win.clear_cooldowns();
            attack_cooldown = 0.0f;
            if (want_net && net.connected()) {
                (void) net.send_chat("/reset");
                return client::ClientChatCommandResult(true, {});
            }
            player_hp = player_max_hp;
            player_mp = player_max_mp;
            return client::ClientChatCommandResult(true, "Reset.");
        });
    game_hud.chat().commands().register_command(
        "teleport", [&](const client::ClientChatCommandContext& ctx) {
            game_hud.chat().blur();
            int dungeon = 0;
            int area = 0;
            std::string place_id;
            if (map::PortalCatalog::parse_dungeon_arg(ctx.args(), dungeon) ||
                map::PortalCatalog::parse_place_arg(ctx.args(), place_id) ||
                map::PortalCatalog::parse_area_arg(ctx.args(), area)) {
                (void) dungeon;
                (void) place_id;
                (void) area;
                return client::ClientChatCommandResult(false, {});
            }
            return client::ClientChatCommandResult(true, map::kTeleportUsage);
        });
    game_hud.chat().commands().register_command(
        "getitemid", [&](const client::ClientChatCommandContext& ctx) {
            game_hud.chat().blur();
            protocol::GetItemIdArgs parsed;
            if (!protocol::GetItemId::parse_args(ctx.args(), parsed)) {
                return client::ClientChatCommandResult(true, protocol::kGetItemIdUsage);
            }
            if (want_net && net.connected()) {
                return client::ClientChatCommandResult(false, {});
            }
            if (item_catalog.find(parsed.index) == nullptr) {
                return client::ClientChatCommandResult(true, "Unknown item.");
            }
            if (!player_bag.try_insert(parsed.index, parsed.count, item_catalog, parsed.prefix)) {
                return client::ClientChatCommandResult(true, "Your inventory is full.");
            }
            std::string item_name = "item #" + std::to_string(parsed.index);
            if (const client::ItemDef* def = item_catalog.find(parsed.index)) {
                if (!def->name().empty()) {
                    item_name = def->name();
                }
            }
            return client::ClientChatCommandResult(
                true, client::BattleMsgBuilder::item_acquired(item_name, parsed.count));
        });
    game_hud.chat().commands().register_command(
        "level", [&](const client::ClientChatCommandContext& ctx) {
            game_hud.chat().blur();
            protocol::LevelUpArgs parsed;
            if (!protocol::LevelUp::parse_args(ctx.args(), parsed)) {
                return client::ClientChatCommandResult(true, protocol::kLevelUsage);
            }
            if (want_net && net.connected()) {
                return client::ClientChatCommandResult(false, {});
            }
            const int current = std::max(1, inv_win.player_level());
            int next = current;
            if (parsed.op == protocol::LevelOp::Add) {
                if (current >= protocol::kMaxPlayerLevel) {
                    return client::ClientChatCommandResult(true, "Already at maximum level.");
                }
                next = std::min(current + parsed.value, protocol::kMaxPlayerLevel);
            } else if (parsed.op == protocol::LevelOp::Subtract) {
                if (current <= 1) {
                    return client::ClientChatCommandResult(true, "Already at minimum level.");
                }
                next = std::max(1, current - parsed.value);
            } else {
                next = std::clamp(parsed.value, 1, protocol::kMaxPlayerLevel);
            }
            inv_win.set_player_level(next);
            skill_win.set_player_level(next);
            if (parsed.op != protocol::LevelOp::Add) {
                const auto base =
                    openkal::server::combat::class_base_plus_bonus(inv_win.player_clazz());
                const int pu = protocol::kPuPerLevel * next;
                net.seed_char_primaries(base.str, base.hth, base.intl, base.wis, base.dex, pu);
                char_info_win.set_stats(net.char_info_stats());
            }
            if (next > current) {
                const auto pos = local_player.pos();
                combat_fx.spawn_level_up(*engine, pos.x, pos.y, pos.z);
            }
            if (parsed.op == protocol::LevelOp::Add) {
                return client::ClientChatCommandResult(
                    true, "Leveled up to " + std::to_string(next) + ".");
            }
            return client::ClientChatCommandResult(true,
                                                   "Level set to " + std::to_string(next) + ".");
        });
    game_hud.chat().commands().register_command(
        "exp", [&](const client::ClientChatCommandContext& ctx) {
            game_hud.chat().blur();
            protocol::ChatAmountArgs parsed;
            if (!protocol::ChatAmount::parse_args(ctx.args(), parsed)) {
                return client::ClientChatCommandResult(true, protocol::kExpUsage);
            }
            if (want_net && net.connected() && net.handshook()) {
                (void) net.send_chat(ctx.raw());
                return client::ClientChatCommandResult(true, {});
            }
            return client::ClientChatCommandResult(true, "Not connected to server.");
        });
    game_hud.chat().commands().register_command(
        "eggexp", [&](const client::ClientChatCommandContext& ctx) {
            game_hud.chat().blur();
            protocol::ChatAmountArgs parsed;
            if (!protocol::ChatAmount::parse_args(ctx.args(), parsed)) {
                return client::ClientChatCommandResult(true, protocol::kEggExpUsage);
            }
            if (want_net && net.connected() && net.handshook()) {
                (void) net.send_chat(ctx.raw());
                return client::ClientChatCommandResult(true, {});
            }
            return client::ClientChatCommandResult(true, "Not connected to server.");
        });
    game_hud.chat().commands().register_command(
        "rage", [&](const client::ClientChatCommandContext& ctx) {
            game_hud.chat().blur();
            protocol::ChatAmountArgs parsed;
            if (!protocol::ChatAmount::parse_args(ctx.args(), parsed)) {
                return client::ClientChatCommandResult(true, protocol::kRageUsage);
            }
            if (want_net && net.connected() && net.handshook()) {
                (void) net.send_chat(ctx.raw());
                return client::ClientChatCommandResult(true, {});
            }
            return client::ClientChatCommandResult(true, "Not connected to server.");
        });
    game_hud.chat().commands().register_command(
        "dialog", [&](const client::ClientChatCommandContext& ctx) {
            game_hud.chat().blur();
            std::string_view args = ctx.args();
            while (!args.empty() && (args.front() == ' ' || args.front() == '\t')) {
                args.remove_prefix(1);
            }
            if (args.empty()) {
                return client::ClientChatCommandResult(true, std::string(kDialogUsage));
            }
            int id = 0;
            const char* first = args.data();
            const char* last = args.data() + args.size();
            const auto [ptr, ec] = std::from_chars(first, last, id);
            if (ec != std::errc() || ptr != last || id <= 0) {
                return client::ClientChatCommandResult(true, std::string(kDialogUsage));
            }
            if (!open_dialog_by_id(id)) {
                return client::ClientChatCommandResult(
                    true, "Dialog #" + std::to_string(id) + " not found.");
            }
            return client::ClientChatCommandResult(true,
                                                   "Opened dialog #" + std::to_string(id) + ".");
        });
    game_hud.chat().commands().register_command(
        "dialogreload", [&](const client::ClientChatCommandContext& ctx) {
            game_hud.chat().blur();
            if (!dialog_catalog.load_dir(dialogs_dir)) {
                return client::ClientChatCommandResult(true,
                                                       "Dialog reload failed (empty catalog).");
            }
            std::string_view args = ctx.args();
            while (!args.empty() && (args.front() == ' ' || args.front() == '\t')) {
                args.remove_prefix(1);
            }
            if (args.empty()) {
                return client::ClientChatCommandResult(true, "Dialogs reloaded.");
            }
            int id = 0;
            const char* first = args.data();
            const char* last = args.data() + args.size();
            const auto [ptr, ec] = std::from_chars(first, last, id);
            if (ec != std::errc() || ptr != last || id <= 0) {
                return client::ClientChatCommandResult(true, "Usage: /dialogreload [id]");
            }
            if (!open_dialog_by_id(id)) {
                return client::ClientChatCommandResult(
                    true, "Dialogs reloaded, but dialog #" + std::to_string(id) + " not found.");
            }
            return client::ClientChatCommandResult(
                true, "Dialogs reloaded and opened #" + std::to_string(id) + ".");
        });
    game_hud.chat().commands().register_command(
        "bof", [&](const client::ClientChatCommandContext& ctx) {
            game_hud.chat().blur();
            std::string_view args = ctx.args();
            while (!args.empty() && (args.front() == ' ' || args.front() == '\t')) {
                args.remove_prefix(1);
            }
            if (args.empty() || args == "status") {
                return client::ClientChatCommandResult(true, bof_fx.status_line());
            }

            std::string_view token = args;
            std::string_view rest;
            const auto sp = args.find_first_of(" \t");
            if (sp != std::string_view::npos) {
                token = args.substr(0, sp);
                rest = args.substr(sp + 1);
                while (!rest.empty() && (rest.front() == ' ' || rest.front() == '\t')) {
                    rest.remove_prefix(1);
                }
            }

            auto ascii_lower = [](std::string_view in) {
                std::string out(in);
                for (char& c : out) {
                    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                }
                return out;
            };
            const std::string cmd = ascii_lower(token);

            if (cmd == "clear" || cmd == "off" || cmd == "none") {
                bof_fx.set_debug_override(false);
                bof_fx.clear();
                sync_bof_from_wear();
                return client::ClientChatCommandResult(true, bof_fx.status_line());
            }
            bof_fx.set_debug_override(true);
            if (cmd == "all") {
                const bool turn_on = bof_fx.enabled_count() < client::BofFx::kFullCount;
                bof_fx.set_all(turn_on);
                sync_bof_fx();
                return client::ClientChatCommandResult(true, bof_fx.status_line());
            }
            if (cmd == "level" || cmd == "lvl" || cmd == "lv") {
                int level = 0;
                const char* first = rest.data();
                const char* last = rest.data() + rest.size();
                const auto [ptr, ec] = std::from_chars(first, last, level);
                if (ec != std::errc() || ptr != last || !bof_fx.set_level(level)) {
                    return client::ClientChatCommandResult(true,
                                                           "Usage: /bof level <46|50|55|60|65>");
                }
                sync_bof_fx();
                return client::ClientChatCommandResult(true, bof_fx.status_line());
            }

            // Bare tier: `/bof 60`
            {
                int level = 0;
                const char* first = token.data();
                const char* last = token.data() + token.size();
                const auto [ptr, ec] = std::from_chars(first, last, level);
                if (ec == std::errc() && ptr == last) {
                    if (!bof_fx.set_level(level)) {
                        return client::ClientChatCommandResult(
                            true, "Usage: /bof level <46|50|55|60|65>");
                    }
                    sync_bof_fx();
                    return client::ClientChatCommandResult(true, bof_fx.status_line());
                }
            }

            client::BofFx::Part part = client::BofFx::Part::Chest;
            if (!client::BofFx::parse_part(cmd, part)) {
                return client::ClientChatCommandResult(true, std::string(kBofUsage));
            }
            const bool on = bof_fx.toggle_part(part);
            sync_bof_fx();
            std::string msg = bof_fx.status_line();
            msg += on ? " (on)" : " (off)";
            return client::ClientChatCommandResult(true, std::move(msg));
        });

    if (!loading_screen.present("Entering world", 6.0f / kWorldLoadingStages)) {
        embedded_server.stop();
        if (store) {
            store->close();
        }
        ui.destroy();
        engine->shutdown();
        return 0;
    }
    // The handshake creates the authoritative player. Keep it behind every
    // terrain, object, catalog, UI, and player-visual load so monsters cannot
    // target the character while the loading screen is still doing work.
    if (multiplayer) {
        choice.lobby->set_wait_callback({});
        net.adopt_session(choice.lobby->release_session());
        if (!net.handshake()) {
            m_connection_message = "Online world entry failed. Please reconnect.";
            net.close();
        }
    } else if (embedded_server_ready) {
        timespec ts{0, 50 * 1000 * 1000};
        nanosleep(&ts, nullptr);
        if (!net.connect_local_unix(scfg.unix_path) || !net.handshake()) {
            log::warn("embedded server up but client handshake failed");
        } else {
            log::info("single-player linked to embedded MainSvr via " + scfg.unix_path);
            net.update(0.0f, profile.origin_patch());
            protocol::CreatePlayerFields self{};
            if (net.take_create_player(self)) {
                log::info("enter-world self id=" + std::to_string(self.id) +
                          " class=" + std::to_string(self.class_byte & 0x7f));
            }
            char_info_win.set_stats(net.char_info_stats());
            if (client::DebugOverlay::enabled()) {
                (void) net.send_chat("/debug on");
            }
        }
    }

    if (!loading_screen.present("Ready", 1.0f)) {
        embedded_server.stop();
        if (store) {
            store->close();
        }
        ui.destroy();
        engine->shutdown();
        return 0;
    }
    entering_world = false;

    engine::FrameInput input;
    float water_anim_time = 0.0f;
    client::FrameProfiler frame_profiler;
    const bool show_frame_stats = frame_profiler.enabled();
    if (client::DebugOverlay::enabled()) {
        log::info("DEBUG=1 (monster hitboxes + chunk overlay)");
    }
    std::uint64_t monster_skin_frame = 0;
    const auto bag_restore_until = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    bool select_character_requested = false;
    std::vector<client::BuffSlotEntry> hud_buff_slots;
    std::uint64_t hud_buff_now_ms = 0;
    int hud_buff_tip = -1;
    while (engine->poll(input)) {
        const engine::FrameInput observer_look = input;
        behead_animation_active = behead_animation_active && player_actor.attack_locked() &&
                                  !player_actor.behead_swing_cancelable();
        if (behead_animation_active) {
            client::mute_gameplay_input(input);
            input.mouse_left_clicked = false;
            input.mouse_right_clicked = false;
        }
        if (read_only_session) {
            client::restrict_read_only_input(input);
        }
        if (multiplayer && !net.connected()) {
            if (m_connection_message.empty()) {
                m_connection_message = "Connection lost. Please reconnect.";
            }
            select_character_requested = true;
            break;
        }
        ++monster_skin_frame;
        const auto frame_work_started = std::chrono::steady_clock::now();
        if (input.quit || cpu_profiler.deadline_reached() || m_shutdown_signal.requested()) {
            break;
        }
        trading_post_input = input;
        trading_post.update(net.trading_post(),
                            net.trading_post_serial(),
                            input,
                            game_hud.scale(),
                            player_bag,
                            item_catalog);
        if (net.trade().locked()) {
            trading_post.set_open(false);
        }
        if (trading_post.open()) {
            if (const auto command = trading_post.handle(trading_post_input, ui, item_catalog)) {
                if (!net.send_trading_post(*command)) {
                    trading_post.send_failed();
                }
            }
            client::mute_ui_input(input);
            local_player.clear_click_target();
            local_player.set_moving(false);
            auto_attack = auto_skill = false;
            pending_pickup_id = pending_ask_npc_id = 0;
            clear_pending_skill();
            reset_range_skill();
            player_actor.cancel_attack();
        }
        trade_input = input;
        trade_window.update(net.trade(),
                            net.trade_serial(),
                            static_cast<float>(input.width),
                            static_cast<float>(input.height),
                            game_hud.scale());
        if (net.trade().locked()) {
            if (const auto command = trade_window.handle(trade_input, ui)) {
                if (!net.send_trade(*command)) {
                    trade_window.send_failed();
                }
            }
            client::mute_ui_input(input);
            local_player.clear_click_target();
            local_player.set_moving(false);
            auto_attack = auto_skill = false;
            pending_pickup_id = pending_ask_npc_id = 0;
            clear_pending_skill();
            reset_range_skill();
            player_actor.cancel_attack();
        }
        // The guild editor owns character input before any modal or game shortcut is evaluated.
        const std::array<bool, 4> guild_cursor_keys = {
            input.key_left, input.key_right, input.key_up, input.key_down};
        if (guild_window.focused() && !game_menu.open() && !settings_window.open() &&
            !powerbook.is_open() && !talent_tree.is_open() && !world_map_open &&
            !player_options.open()) {
            client::mute_gameplay_input(input);
        }
        cam.set_clip_far(indoor_view.loaded() ? client::kFarZIndoor : client::kFarZ);
        cam.tick_shake(input.dt);
        if (doggebi_ritual_active_mask == 0xffu && doggebi_ritual_phase >= 1 &&
            doggebi_ritual_phase <= 4) {
            constexpr std::array<float, 4> kDoggebiQuakeAmplitudes = {0.45f, 0.65f, 0.85f, 1.05f};
            cam.sustain_shake(kDoggebiQuakeAmplitudes[doggebi_ritual_phase - 1]);
        }
        water_anim_time += input.dt;

        // Resolve UI scale before any floating-window open/layout so K/I and HUD
        // share it.
        const float frame_w = static_cast<float>(input.width);
        const float frame_h = static_cast<float>(input.height);
        talent_tree.tick(input.dt);
        if (!net.connected() && talent_tree.awaiting()) {
            talent_tree.request_sent(false);
        }
        protocol::TalentState talent_state;
        if (net.take_talent_state(talent_state)) {
            talent_tree.receive(talent_state);
            const auto ranks = protocol::talent_ranks(talent_state.mask);
            for (int i = 0; i < 5; ++i) {
                (void) player_skills.set_learned_level(protocol::kTalentSkillFirst + i, ranks[i]);
            }
            (void) player_skills.set_learned_level(
                protocol::kBeaconSkill, protocol::talent_bonuses(talent_state.mask).beacon ? 1 : 0);
            skill_win.sync_cooldown(protocol::kBeaconSkill,
                                    static_cast<int>(talent_state.beacon_cooldown_ms),
                                    static_cast<int>(protocol::kBeaconCooldownMs));
        }
        const bool powerbook_was_open = powerbook.is_open();
        if (input.key_p_pressed && !powerbook_was_open && !world_map_open &&
            !talent_tree.is_open() && !game_hud.chat().focused()) {
            powerbook.open(configuration_root, item_catalog);
            // The opening key must not immediately close the new modal.
            input.key_p_pressed = false;
        }
        if (powerbook.is_open()) {
            powerbook.set_player(inv_win.player_clazz(), inv_win.player_level());
            auto_attack = false;
            local_player.clear_click_target();
            reset_range_skill();
            powerbook.update(input, ui);
            client::mute_ui_input(input);
        }
        if (input.key_shift && input.key_k_pressed && !world_map_open &&
            !game_hud.chat().focused()) {
            talent_tree.open();
            if (want_net && net.connected()) {
                (void) net.send_talent_request(protocol::TalentCommand::Snapshot);
            }
        }
        if (talent_tree.is_open()) {
            auto_attack = false;
            local_player.clear_click_target();
            reset_range_skill();
            const auto action = talent_tree.update(input, ui);
            if (action == client::TalentTreeWindow::Action::Apply) {
                talent_tree.request_sent(want_net && net.connected() &&
                                         net.send_talent_request(protocol::TalentCommand::Apply,
                                                                 talent_tree.committed(),
                                                                 talent_tree.draft()));
            } else if (action == client::TalentTreeWindow::Action::Beacon) {
                talent_tree.request_sent(want_net && net.connected() &&
                                         net.send_talent_request(protocol::TalentCommand::Beacon));
            }
            client::mute_ui_input(input);
        }
        if (world_map_open) {
            if (input.key_escape_pressed || input.key_m_pressed ||
                (input.mouse_left_clicked && client::WorldMapWidget::close_contains(
                                                 input.mouse_x, input.mouse_y, frame_w, frame_h))) {
                world_map_open = false;
            }
            // The previous world map captured input and hid the rest of the UI.
            client::mute_ui_input(input);
        }
        const float want_ui_scale = ui_settings.resolve_scale(frame_h);
        if (game_hud.ensure_layout(frame_w, frame_h, want_ui_scale)) {
            ui_settings.apply_character(game_hud, boot.selected.id, game_hud.scale());
            game_hud.clamp_to_screen(frame_w, frame_h);
        }
        const bool skill_layout_hot = ui_layouts.poll() > 0;
        if (skill_layout_hot) {
            powerbook.set_layout(&ui_layouts.get("powerbook"));
            quest_journal_win.set_snitch_context(
                &ui_layouts.get("quest_snitch"), configuration_root, npc_catalog.spawns());
            talent_tree.set_layout(&ui_layouts.get("talent_tree"));
            skill_bar.set_layout(&ui_layouts.get("dragon_hud"));
        }
        {
            const float s = game_hud.scale();
            if (std::fabs(skill_win.scale() - s) > 0.001f) {
                const float old = std::max(0.25f, skill_win.scale());
                const float lx = skill_win.x() / old;
                const float ly = skill_win.y() / old;
                skill_win.set_scale(s);
                skill_win.sync_from_layout();
                skill_win.set_pos(lx * s, ly * s);
            } else if (skill_layout_hot) {
                skill_win.sync_from_layout();
            } else {
                skill_win.set_scale(s);
            }
            if (std::fabs(transform_win.scale() - s) > 0.001f) {
                const float old = std::max(0.25f, transform_win.scale());
                const float lx = transform_win.x() / old;
                const float ly = transform_win.y() / old;
                transform_win.set_scale(s);
                transform_win.sync_from_layout();
                transform_win.set_pos(lx * s, ly * s);
            } else if (skill_layout_hot) {
                transform_win.sync_from_layout();
            } else {
                transform_win.set_scale(s);
            }
            if (skill_layout_hot) {
                client::UiView::reload(riding_view, &ui_layouts.get("riding"));
            }
            if (std::fabs(char_info_win.scale() - s) > 0.001f) {
                const float old = std::max(0.25f, char_info_win.scale());
                const float lx = char_info_win.x() / old;
                const float ly = char_info_win.y() / old;
                char_info_win.set_scale(s);
                char_info_win.sync_from_layout();
                char_info_win.set_pos(lx * s, ly * s);
            } else if (skill_layout_hot) {
                char_info_win.sync_from_layout();
            } else {
                char_info_win.set_scale(s);
            }
            if (std::fabs(inv_win.scale() - s) > 0.001f) {
                const float old = std::max(0.25f, inv_win.scale());
                const float lx = inv_win.x() / old;
                const float ly = inv_win.y() / old;
                inv_win.set_scale(s);
                inv_win.sync_from_layout();
                inv_win.set_pos(lx * s, ly * s);
                player_bag.set_compact_register_enabled(inv_win.register_enabled());
            } else if (skill_layout_hot) {
                inv_win.sync_from_layout();
                player_bag.set_compact_register_enabled(inv_win.register_enabled());
            } else {
                inv_win.set_scale(s);
            }
            if (std::fabs(mix_win.scale() - s) > 0.001f) {
                const float old = std::max(0.25f, mix_win.scale());
                const float lx = mix_win.x() / old;
                const float ly = mix_win.y() / old;
                mix_win.set_scale(s);
                mix_win.sync_from_layout();
                mix_win.set_pos(lx * s, ly * s);
            } else if (skill_layout_hot) {
                mix_win.sync_from_layout();
            } else {
                mix_win.set_scale(s);
            }
            skin_shop_win.set_scale(s);
            shop_win.set_scale(s);
            if (skill_layout_hot) {
                shop_win.sync_from_layout();
            }
            storage_win.set_scale(s);
            if (skill_layout_hot) {
                storage_win.sync_from_layout();
            }
            npc_talk_win.set_scale(s);
            speed_medicine_win.set_scale(s);
            consumable_crafting_win.set_scale(s);
            if (skill_layout_hot) {
                speed_medicine_win.sync_from_layout();
                consumable_crafting_win.sync_from_layout();
            }
            quest_journal_win.set_scale(s);
            mail_window.set_scale(s);
            guild_window.set_scale(s);
            blacksmith_win.set_scale(s);
            game_menu.set_scale(s, frame_w, frame_h);
            settings_window.set_scale(s, frame_w, frame_h);
            const auto& qs =
                static_cast<const client::GameHud&>(game_hud).panel(client::HudPanelId::Dragon);
            skill_bar.set_panel(
                qs.x(),
                qs.y(),
                qs.w(),
                qs.h(),
                qs.w() / std::max(1.0f, ui_layouts.get("dragon_hud").get_f("window.w", 792.0f)));
        }

        if (enchant_overlay.locking()) {
            client::mute_ui_input(input);
        }
        auto send_pending_shop_sales = [&]() {
            std::uint32_t iid = 0;
            std::uint16_t quantity = 0;
            while (shop_win.take_pending_sale(iid, quantity)) {
                if (want_net && net.connected()) {
                    (void) net.send_sell_item(shop_win.npc_id(), iid, quantity);
                }
            }
        };
        auto send_pending_storage_transfers = [&](client::StorageClickResult result) {
            if (result == client::StorageClickResult::MoneyTransfer) {
                if (const auto request = storage_win.take_pending_money();
                    request && want_net && net.connected()) {
                    (void) net.send_storage_money(*request);
                }
                return;
            }
            std::uint32_t iid = 0;
            std::uint32_t count = 0;
            while (storage_win.take_pending_transfer(iid, count)) {
                if (!want_net || !net.connected()) {
                    continue;
                }
                if (result == client::StorageClickResult::Deposit) {
                    (void) net.send_storage_deposit(storage_win.npc_id(), iid, count);
                } else if (result == client::StorageClickResult::Withdraw) {
                    (void) net.send_storage_withdraw(storage_win.npc_id(), iid, count);
                }
            }
        };
        const bool storage_quantity_open = storage_win.quantity_open();
        const bool shop_quantity_open = shop_win.quantity_open();
        if (storage_quantity_open) {
            const auto result = storage_win.handle_keyboard(input);
            if (result == client::StorageClickResult::Deposit ||
                result == client::StorageClickResult::Withdraw ||
                result == client::StorageClickResult::MoneyTransfer) {
                send_pending_storage_transfers(result);
            }
        } else if (shop_quantity_open) {
            const auto result = shop_win.handle_keyboard(input);
            if (result == client::ShopClickResult::Buy && want_net && net.connected()) {
                (void) net.send_buy_item(
                    shop_win.npc_id(), shop_win.pending_index(), shop_win.pending_quantity());
            } else if (result == client::ShopClickResult::Sell) {
                send_pending_shop_sales();
            }
        }
        if (storage_quantity_open || shop_quantity_open) {
            client::mute_gameplay_input(input);
            input.key_escape = false;
            input.key_escape_pressed = false;
            input.key_enter = false;
            input.key_enter_pressed = false;
            input.key_backspace_pressed = false;
            input.key_delete_pressed = false;
            input.text_char_count = 0;
        }
        if (fishing_active || cooking_active) {
            if (input.key_escape_pressed && want_net && net.connected()) {
                (void) net.send_quest_job_end();
                input.key_escape = false;
                input.key_escape_pressed = false;
            }
            client::mute_gameplay_input(input);
            input.mouse_left = false;
            input.mouse_left_clicked = false;
        }
        if (want_net && net.connected() && !party_preferences_sent) {
            protocol::PartyRequest preference;
            preference.command = protocol::PartyCommand::Ignore;
            preference.value = client_settings.ignore_party_requests ? 1 : 0;
            party_preferences_sent = net.send_party(preference);
        }
        party_window.update(net.party(), game_hud.scale(), frame_w, frame_h, net.local_player_id());
        if (input.key_escape_pressed) {
            if (game_hud.chat().focused() || game_hud.chat().options_open() ||
                game_hud.chat().history_open()) {
                game_hud.chat().blur();
                game_hud.chat().set_options_open(false);
            } else if (party_window.actions_open()) {
                party_window.close_actions();
            } else if (game_menu.open()) {
                game_menu.handle_escape();
            } else if (!close_latest_window()) {
                selected_mob_id = 0;
                selected_npc_id = 0;
                selected_player_id = 0;
                selected_item_id = 0;
                pending_pickup_id = 0;
                auto_attack = false;
                game_menu.show(frame_w, frame_h);
            }
        }
        if (settings_window.open()) {
            if (input.mouse_left_clicked) {
                const auto settings_action =
                    settings_window.handle_click(input.mouse_x, input.mouse_y, &ui);
                if (settings_action == client::SettingsWindowAction::Save) {
                    settings::ClientSettings saved = settings_window.settings();
                    std::string save_error;
                    if (runtime.save_client_settings(saved, save_error)) {
                        client_settings = saved;
                        game_hud.set_clock_visible(saved.clock_enabled);
                        protocol::PartyRequest preference;
                        preference.command = protocol::PartyCommand::Ignore;
                        preference.value = saved.ignore_party_requests ? 1 : 0;
                        (void) net.send_party(preference);
                        game_hud.chat().filters().set_battle_flags(
                            settings_window.chat_filters().battle_flags());
                        game_hud.chat().filters().set_system_enabled(
                            settings_window.chat_filters().system_enabled());
                        persist_hud();
                        const bool display_applied = apply_display_settings(saved.display);
                        sfx.set_effect_volume(static_cast<float>(saved.audio.effect_volume) / 5.0f);
                        bgm.configure(saved.audio.bgm_enabled, saved.audio.bgm_volume);
                        sprite_fx.set_cull_distance_scale(
                            static_cast<float>(saved.world.effect_distance) / 300.0f);
                        render_settings_applier.apply(saved.render);
                        log::info("saved client settings to " + runtime.profile_path());
                        if (display_applied) {
                            settings_window.close();
                            in_game_window_order.closed(client::InGameWindowId::Settings);
                        } else {
                            log::warn("failed to apply saved window mode");
                            settings_window.set_status(
                                "Saved, but the display mode could not be applied.");
                        }
                    } else {
                        log::warn("failed to save client settings: " + save_error);
                        settings_window.set_status("Save failed: " + save_error);
                    }
                }
            }
            client::mute_ui_input(input);
        }
        if (game_menu.open()) {
            if (input.mouse_left_clicked) {
                const auto menu_action = game_menu.handle_click(input.mouse_x, input.mouse_y, &ui);
                if (menu_action == client::GameMenuAction::Options) {
                    game_menu.close();
                    const bool was_open = settings_window.open();
                    settings_window.show(
                        client_settings, frame_w, frame_h, game_hud.chat().filters());
                    update_window_order(
                        client::InGameWindowId::Settings, was_open, settings_window.open());
                }
                if (menu_action == client::GameMenuAction::SelectCharacter) {
                    if (net.castle_side(net.local_player_id()) != 0) {
                        game_menu.close();
                        game_hud.chat().push_info(
                            "Character selection is unavailable during Castle War.", {});
                    } else {
                        select_character_requested =
                            loading_screen.present("Returning to character selection");
                        if (select_character_requested && !multiplayer) {
                            m_preferred_character_id = boot.selected.id;
                        }
                        break;
                    }
                }
                if (menu_action == client::GameMenuAction::QuitClient) {
                    break;
                }
            }
            client::mute_ui_input(input);
        }
        if (guild_view_serial != net.guild_serial()) {
            guild_view_serial = net.guild_serial();
            platform::Json view;
            while (net.take_guild_view(view)) {
                guild_window.update(view);
            }
            if (net.guild_view().at("formation").as_int() != 0 &&
                !net.guild_view().at("formation_accepted").boolean() && !guild_window.open()) {
                guild_window.set_open(true);
                in_game_window_order.opened(client::InGameWindowId::Guild);
            }
        }
        protocol::PlayerOptionsState options_state;
        while (net.take_player_options(options_state)) {
            player_options.update(options_state);
        }
        if (player_options.open() &&
            (!net.connected() || player_options.target() != selected_player_id ||
             !remote_players.find(player_options.target()) ||
             net.castle_enemy(player_options.target()))) {
            player_options.close();
            in_game_window_order.closed(client::InGameWindowId::PlayerOptions);
        }
        if (player_options.open()) {
            if (auto request = player_options.handle(input, ui, game_hud.scale())) {
                player_options.request_sent(net.send_player_options(*request));
            }
            if (const auto partner = player_options.take_whisper_target(); !partner.empty()) {
                game_hud.chat().begin_whisper(partner);
            }
            if (!player_options.open()) {
                in_game_window_order.closed(client::InGameWindowId::PlayerOptions);
            }
            client::mute_ui_input(input);
        }
        if (multiplayer && !game_menu.open() && !settings_window.open()) {
            auto guild_input = input;
            guild_input.key_left = guild_cursor_keys[0];
            guild_input.key_right = guild_cursor_keys[1];
            guild_input.key_up = guild_cursor_keys[2];
            guild_input.key_down = guild_cursor_keys[3];
            if (auto request = guild_window.handle(guild_input, ui)) {
                if (!net.send_guild(*request)) {
                    guild_window.request_failed();
                    game_hud.chat().push_info("Guild request could not be sent.", {});
                }
            }
            if (guild_window.focused()) {
                input.text_char_count = 0;
                input.key_enter_pressed = false;
                input.key_backspace_pressed = false;
                input.key_delete_pressed = false;
            }
            if (guild_window.contains(input.mouse_x, input.mouse_y) && input.mouse_left_clicked) {
                input.mouse_left_clicked = false;
            }
        }
        const auto& chat_panel =
            static_cast<const client::GameHud&>(game_hud).panel(client::HudPanelId::Chat);
        const bool chat_opts_before = game_hud.chat().options_open();
        const auto chat_in =
            game_hud.chat().handle_input(input,
                                         chat_panel.x(),
                                         chat_panel.y(),
                                         chat_panel.w(),
                                         chat_panel.h(),
                                         game_hud.scale(),
                                         &ui_layouts.get(game_hud.chat().layout_name()),
                                         [&](std::string_view text) {
                                             if (want_net && net.connected()) {
                                                 (void) net.send_chat(text);
                                             }
                                         });
        const bool chat_focused = game_hud.chat().focused() || guild_window.focused();
        if (chat_focused) {
            // Match compatibility behavior: focused editor consumes keys
            // before game hotkeys (WASD, Space target/attack, F skill, C/I/K/X, digits).
            client::mute_gameplay_input(input);
        }
        if (!chat_focused && input.key_g_pressed && !game_menu.open() && !settings_window.open() &&
            !world_map_open) {
            if (multiplayer && net.guild_view().is_object()) {
                const bool was_open = guild_window.open();
                guild_window.set_open(!was_open);
                update_window_order(client::InGameWindowId::Guild, was_open, guild_window.open());
            } else {
                game_hud.chat().push_info("Guilds are unavailable on this server.", {});
            }
            client::mute_ui_input(input);
        }
        if (!chat_focused && input.key_o_pressed && !game_menu.open() && !settings_window.open() &&
            !world_map_open) {
            if (net.castle_enemy(selected_player_id)) {
                game_hud.chat().push_info("Player options are unavailable for Castle War enemies.",
                                          {});
            } else if (multiplayer && selected_player_id != 0 &&
                       selected_player_id != net.local_player_id() &&
                       remote_players.find(selected_player_id)) {
                player_options.show(selected_player_id);
                in_game_window_order.opened(client::InGameWindowId::PlayerOptions);
                client::mute_ui_input(input);
                if (auto request = player_options.handle(input, ui, game_hud.scale())) {
                    player_options.request_sent(net.send_player_options(*request));
                }
            } else {
                game_hud.chat().push_info("Select another online player first.", {});
            }
            client::mute_ui_input(input);
        }
        if (!chat_focused && input.key_m_pressed) {
            world_map_open = true;
            client::mute_ui_input(input);
        }
        if (chat_opts_before && !game_hud.chat().options_open()) {
            persist_hud();
        }
        const bool chat_options_hit = game_hud.chat().options_contains_pointer(input.mouse_x,
                                                                               input.mouse_y,
                                                                               chat_panel.x(),
                                                                               chat_panel.y(),
                                                                               chat_panel.w(),
                                                                               chat_panel.h(),
                                                                               game_hud.scale());
        (void) chat_in;
        if (!chat_focused && input.key_c_pressed) {
            const bool was_open = char_info_win.open();
            char_info_win.toggle();
            update_window_order(
                client::InGameWindowId::CharacterInfo, was_open, char_info_win.open());
            if (char_info_win.open()) {
                char_info_win.center_on_screen(static_cast<float>(input.width),
                                               static_cast<float>(input.height));
            }
        }
        if (!chat_focused && input.key_i_pressed) {
            const bool was_open = inv_win.open();
            inv_win.toggle();
            update_window_order(client::InGameWindowId::Inventory, was_open, inv_win.open());
            if (inv_win.open()) {
                inv_win.center_on_screen(static_cast<float>(input.width),
                                         static_cast<float>(input.height));
            }
        }
        if (!chat_focused && input.key_k_pressed) {
            const bool was_open = skill_win.open();
            skill_win.toggle();
            update_window_order(client::InGameWindowId::Skills, was_open, skill_win.open());
            if (skill_win.open()) {
                skill_win.center_on_screen(static_cast<float>(input.width),
                                           static_cast<float>(input.height));
            }
        }
        if (!chat_focused && input.key_h_pressed) {
            const bool was_open = transform_win.open();
            transform_win.toggle();
            update_window_order(client::InGameWindowId::Transform, was_open, transform_win.open());
            if (transform_win.open()) {
                transform_win.center_on_screen(static_cast<float>(input.width),
                                               static_cast<float>(input.height));
            }
        }
        if (!chat_focused && input.key_e_pressed) {
            toggle_riding_window(frame_w, frame_h, game_hud.scale());
        }
        if (!chat_focused && input.key_r_pressed && player_wear.ride_index() > 0 &&
            !local_player.dead() && want_net && net.connected()) {
            if (!net.send_riding_toggle()) {
                log::warn("failed to send common riding toggle");
            }
        }
        if (!chat_focused && input.key_q_pressed) {
            const bool was_open = quest_journal_win.open();
            quest_journal_win.toggle();
            update_window_order(
                client::InGameWindowId::QuestJournal, was_open, quest_journal_win.open());
            if (quest_journal_win.open()) {
                quest_journal_win.sync_quests(net.quest_flags(), task_catalog, item_catalog);
                quest_journal_win.center_on_screen(static_cast<float>(input.width),
                                                   static_cast<float>(input.height));
            }
        }
        if (!chat_focused && input.key_l_pressed) {
            const bool was_open = mail_window.open();
            mail_window.toggle();
            update_window_order(client::InGameWindowId::Mail, was_open, mail_window.open());
            if (mail_window.open()) {
                mail_window.set_entries(net.mail());
                mail_window.center_on_screen(static_cast<float>(input.width),
                                             static_cast<float>(input.height));
                if (want_net && net.connected() && mail_window.refresh_ready()) {
                    mail_window.request_sent(client::MailClickResult::Refresh,
                                             net.send_mail_list());
                }
            }
        }
        // Z = haste/boost (skill 0 / compatibility behavior), X = rest — KalOnline hotkeys
        // (layout-aware). Toggle locally first (matches runtime state), then notify
        // MainSvr when linked.
        if (!chat_focused && input.key_z_pressed && !local_player.resting() &&
            !local_player.dead()) {
            local_player.toggle_boost();
            if (want_net && net.connected()) {
                if (!net.send_skill_boost(local_player.boosting())) {
                    log::warn("failed to send haste/boost toggle");
                }
            }
            log::info(local_player.boosting() ? "haste on (Z)" : "haste off (Z)");
        }
        if (!chat_focused && input.key_x_pressed && !local_player.dead()) {
            // Sit / rest toggle (skill 11 presentation + C2S_REST).
            const bool want = !local_player.resting();
            if (want && local_player.boosting()) {
                local_player.set_boosting(false);
                if (want_net && net.connected()) {
                    if (!net.send_skill_boost(false)) {
                        log::warn("failed to clear boost before rest toggle");
                    }
                }
            }
            if (want_net && net.connected()) {
                if (net.send_rest(want)) {
                    local_player.set_resting(want);
                    local_player.clear_click_target();
                    if (want) {
                        auto_attack = false;
                    }
                }
            } else {
                local_player.set_resting(want);
                local_player.clear_click_target();
                if (want) {
                    auto_attack = false;
                }
            }
        }
        if (input.key_enter_pressed && !chat_focused && local_player.dead() && want_net) {
            if (net.send_revival()) {
                log::info("sent revival request");
            }
        }
        // Digits: 1-0 = skill hotbar (sets active + casts) or item shortcuts.
        // Shift selects the upper ten slots. GLFW digit keys are physical keys.
        for (int digit = 0; digit <= 9; ++digit) {
            const bool hotkey_active = input.key_digit_pressed[digit] || input.key_digit[digit];
            if (chat_focused || !hotkey_active || local_player.dead()) {
                continue;
            }
            {
                const int slot = ((digit == 0) ? 9 : (digit - 1)) + (input.key_shift ? 10 : 0);
                const int binding = player_skills.hotbar_slot(slot);
                if (protocol::shortcut_is_item(binding)) {
                    const int item_index = protocol::shortcut_item_index(binding);
                    const client::ItemDef* def = item_catalog.find(item_index);
                    if (def) {
                        (void) try_use_item(item_index, "hotbar");
                    }
                } else if (protocol::shortcut_is_skill(binding)) {
                    const auto kind =
                        client::Catalog::cast_kind_for(skill_win.player_clazz(), binding);
                    const bool click_cast = client::Catalog::skill_bar_casts_on_click(
                        skill_win.player_clazz(), binding);
                    const auto* definition = skill_catalog.find(skill_win.player_clazz(), binding);
                    if (click_cast || (definition && client::social_action(*definition) !=
                                                         client::SocialAction::None)) {
                        if (input.key_digit_pressed[digit] && !cast_skill_index(binding)) {
                            log::debug("hotkey skill cast rejected (index=" +
                                       std::to_string(binding) + ")");
                        }
                    } else if (kind == client::CastKind::Combat ||
                               kind == client::CastKind::TargetArea ||
                               kind == client::CastKind::GroundPosition ||
                               kind == client::CastKind::Behead) {
                        social_mode = client::SocialAction::None;
                        player_skills.activate_hotbar_slot(slot);
                    }
                    if (!click_cast &&
                        (kind == client::CastKind::Boost || kind == client::CastKind::Rest ||
                         kind == client::CastKind::SelfHeal || kind == client::CastKind::Party ||
                         kind == client::CastKind::SelfArea || kind == client::CastKind::Player ||
                         kind == client::CastKind::DeadPlayer)) {
                        if (!cast_skill_index(binding)) {
                            log::debug("hotkey skill cast rejected (index=" +
                                       std::to_string(binding) + ")");
                        }
                    }
                }
            }
        }
        std::array<std::uint64_t, client::kMaxRegisters> inventory_unlock_expire_ms{};
        const auto& network_unlock_expire_ms = net.inventory_register_expire_ms();
        for (std::size_t i = 0; i < network_unlock_expire_ms.size(); ++i) {
            inventory_unlock_expire_ms[i + 2] = network_unlock_expire_ms[i];
        }
        inv_win.set_register_unlock_expire_ms(inventory_unlock_expire_ms);
        player_bag.set_compact_register_enabled(inv_win.register_enabled());
        inv_win.layout_size(static_cast<float>(input.width), static_cast<float>(input.height));
        const bool wheel_moved =
            input.mouse_scroll != 0 || (read_only_session && observer_look.mouse_scroll != 0);
        const bool storage_owns_wheel =
            wheel_moved && storage_win.contains(input.mouse_x, input.mouse_y);
        const bool inventory_owns_wheel =
            wheel_moved && inv_win.contains(input.mouse_x, input.mouse_y);
        if (storage_owns_wheel) {
            auto inventory_input = input;
            inventory_input.mouse_scroll = 0;
            inv_win.update_pointer(inventory_input, player_bag);
        } else {
            inv_win.update_pointer(input, player_bag);
        }
        npc_talk_win.update_pointer(input);
        blacksmith_win.update_pointer(input);
        speed_medicine_win.update_pointer(input);
        consumable_crafting_win.update_pointer(input);
        quest_journal_win.update_pointer(input);
        mail_window.update_pointer(input);
        mix_win.update_pointer(input);
        shop_win.update_pointer(input, player_bag, item_catalog);
        skin_shop_win.update_pointer(input);
        storage_win.update_pointer(input, player_bag, item_catalog);
        if (inv_win.open() && inv_win.capturing_pointer() && !input.mouse_left) {
            const client::UiHit riding_drop =
                riding_hit_at(input.mouse_x, input.mouse_y, game_hud.scale());
            if (inv_win.drag_slot() >= 0 && riding_drop.kind() != client::UiHitKind::None) {
                const client::UiWidget* target = riding_view.find_widget(riding_drop.widget_id());
                if (!game_menu.open() && !settings_window.open() &&
                    inv_win.drag_source_tab() == client::Tab::Normal && target &&
                    inv_win.drag_slot() < static_cast<int>(player_bag.normal.size())) {
                    const auto& stack =
                        player_bag.normal[static_cast<std::size_t>(inv_win.drag_slot())];
                    const client::ItemDef* item = item_catalog.find(stack.index);
                    if (target->bind() == "stablemaster.duplicate_mount") {
                        if (stablemaster_npc_id != 0 && stack.iid != 0 && stack.count > 0 &&
                            !stack.equipped && stack.iid != registered_ride_iid && item &&
                            item->class_big() == "ride") {
                            stablemaster_duplicate_iid = stack.iid;
                            riding_host.set_duplicate_item_index(stack.index);
                            riding_feedback.clear();
                        } else {
                            riding_feedback = "Select an unregistered Riding Pet.";
                        }
                    } else if (stack.iid != 0 && stack.count > 0 && !stack.equipped &&
                               client::riding_equipment_accepts(
                                   *target, riding_host.state(), stack.index)) {
                        send_riding_equipment(protocol::BattleRidingOperation::Equip, stack.iid);
                    } else {
                        riding_feedback = "Use matching equipment of the pet's grade.";
                    }
                }
                // Every drop over this window stays in the bag until the server confirms it.
                inv_win.set_drag_slot(-1);
            }
            if (mix_win.open() && mix_win.contains(input.mouse_x, input.mouse_y) &&
                inv_win.drag_source_tab() == client::Tab::Normal && inv_win.drag_slot() >= 0 &&
                inv_win.drag_slot() < static_cast<int>(player_bag.normal.size())) {
                const auto& stack =
                    player_bag.normal[static_cast<std::size_t>(inv_win.drag_slot())];
                if (mix_win.try_drop_item(stack, item_catalog, input.mouse_x, input.mouse_y)) {
                    inv_win.set_drag_slot(-1);
                }
            }
            if (blacksmith_win.open() && blacksmith_win.contains(input.mouse_x, input.mouse_y) &&
                inv_win.drag_source_tab() == client::Tab::Normal && inv_win.drag_slot() >= 0 &&
                inv_win.drag_slot() < static_cast<int>(player_bag.normal.size())) {
                const auto& stack =
                    player_bag.normal[static_cast<std::size_t>(inv_win.drag_slot())];
                if (blacksmith_win.try_drop_weapon(
                        stack, item_catalog, input.mouse_x, input.mouse_y)) {
                    inv_win.set_drag_slot(-1);
                }
            }
            if (inv_win.drag_slot() >= 0) {
                const auto& slots = (inv_win.drag_source_tab() == client::Tab::Quest)
                                        ? player_bag.quest
                                        : player_bag.normal;
                if (inv_win.drag_slot() < static_cast<int>(slots.size())) {
                    const auto& stack = slots[static_cast<std::size_t>(inv_win.drag_slot())];
                    const client::ItemDef* def = item_catalog.find(stack.index);
                    if (def && !def->is_coin() && stack.count > 0) {
                        const int packed = protocol::shortcut_pack_item(stack.index);
                        if (packed >= 0 &&
                            skill_bar.try_drop(input.mouse_x, input.mouse_y, packed)) {
                            inv_win.set_drag_slot(-1);
                        }
                    }
                }
            }
            const auto release =
                inv_win.handle_release(input.mouse_x, input.mouse_y, player_bag, player_wear);
            if (release == client::ClickResult::EquipChanged) {
                refresh_player_visuals();
                sync_equip_net(inv_win.last_equip_op(), inv_win.last_equip_iid());
            } else if (release == client::ClickResult::Enchanted) {
                enchant_overlay.begin_charge(inv_win.last_enchant_from_id(),
                                             inv_win.last_enchant_to_id());
            } else if (release == client::ClickResult::Destroyed) {
                if (want_net && net.connected() && inv_win.last_destroyed_iid() != 0) {
                    (void) net.send_trash_item(inv_win.last_destroyed_iid());
                } else {
                    inv_win.apply_last_destroy(player_bag, &player_wear);
                }
            } else if (release == client::ClickResult::DroppedOnWorld) {
                const bool over_ui = skill_win.contains(input.mouse_x, input.mouse_y) ||
                                     transform_win.contains(input.mouse_x, input.mouse_y) ||
                                     char_info_win.contains(input.mouse_x, input.mouse_y) ||
                                     npc_talk_win.contains(input.mouse_x, input.mouse_y) ||
                                     blacksmith_win.contains(input.mouse_x, input.mouse_y) ||
                                     mix_win.contains(input.mouse_x, input.mouse_y) ||
                                     (shop_win.contains(input.mouse_x, input.mouse_y) ||
                                      skin_shop_win.contains(input.mouse_x, input.mouse_y)) ||
                                     quest_journal_win.contains(input.mouse_x, input.mouse_y) ||
                                     skill_bar.contains(input.mouse_x, input.mouse_y) ||
                                     game_hud.contains(input.mouse_x, input.mouse_y) ||
                                     game_hud.chat().contains_pointer(input.mouse_x,
                                                                      input.mouse_y,
                                                                      chat_panel.x(),
                                                                      chat_panel.y(),
                                                                      chat_panel.w(),
                                                                      chat_panel.h(),
                                                                      game_hud.scale()) ||
                                     chat_options_hit;
                if (!over_ui && want_net && net.connected()) {
                    (void) net.send_drop_item(
                        inv_win.last_drop_iid(),
                        static_cast<std::uint32_t>(std::max(0, inv_win.last_drop_count())));
                }
            } else if (release == client::ClickResult::Ignored) {
                log::debug("inventory pointer release ignored");
            }
        }
        bool ui_ate_scroll =
            storage_owns_wheel || inventory_owns_wheel ||
            (input.mouse_scroll != 0 &&
             riding_hit_at(input.mouse_x, input.mouse_y, game_hud.scale()).kind() !=
                 client::UiHitKind::None);
        if ((npc_talk_win.contains(input.mouse_x, input.mouse_y) ||
             skin_shop_win.contains(input.mouse_x, input.mouse_y)) &&
            std::fabs(input.mouse_scroll) > 0.0f) {
            ui_ate_scroll = true;
        }
        if (!ui_ate_scroll && skill_win.open() && std::fabs(input.mouse_scroll) > 0.0f &&
            skill_win.contains(input.mouse_x, input.mouse_y)) {
            ui_ate_scroll = true;
        }
        if (!ui_ate_scroll && transform_win.open() && std::fabs(input.mouse_scroll) > 0.0f &&
            transform_win.contains(input.mouse_x, input.mouse_y)) {
            ui_ate_scroll = true;
        }
        if (!ui_ate_scroll && char_info_win.open() && std::fabs(input.mouse_scroll) > 0.0f &&
            char_info_win.contains(input.mouse_x, input.mouse_y)) {
            ui_ate_scroll = true;
        }
        if (!ui_ate_scroll && std::fabs(input.mouse_scroll) > 0.0f &&
            game_hud.chat().contains_pointer(input.mouse_x,
                                             input.mouse_y,
                                             chat_panel.x(),
                                             chat_panel.y(),
                                             chat_panel.w(),
                                             chat_panel.h(),
                                             game_hud.scale())) {
            ui_ate_scroll = true;
        }

        skill_win.update_pointer(input);
        transform_win.update_pointer(input);
        if (skill_win.dragging()) {
            // Keep consuming while dragging the skill window.
        }
        char_info_win.update_pointer(input);
        if (!riding_window_open || !input.mouse_left) {
            riding_window_dragging = false;
        } else if (riding_window_dragging) {
            const float s = game_hud.scale();
            const client::WindowFrameStyle style = client::UiView::frame_style(riding_view);
            const float riding_w = riding_view.window_w() * s;
            const float riding_h = riding_view.window_h() * s;
            const float min_x = style.pad_left() * s;
            const float min_y = style.pad_top() * s;
            const float max_x = std::max(min_x, frame_w - riding_w - style.pad_right() * s);
            const float max_y = std::max(min_y, frame_h - riding_h - style.pad_bottom() * s);
            riding_window_x = std::clamp(input.mouse_x - riding_window_drag_dx, min_x, max_x);
            riding_window_y = std::clamp(input.mouse_y - riding_window_drag_dy, min_y, max_y);
        }

        if (riding_pressed_slot >= 0) {
            const auto slot = static_cast<std::size_t>(riding_pressed_slot);
            if (!riding_window_open || game_menu.open() || settings_window.open() ||
                registered_battle_equipment_iids[slot] != riding_pressed_iid) {
                riding_pressed_slot = -1;
                riding_equipment_dragging = false;
            } else if (!input.mouse_left) {
                if (riding_equipment_dragging && inv_win.open() &&
                    inv_win.tab() == client::Tab::Normal &&
                    inv_win.contains(input.mouse_x, input.mouse_y) &&
                    riding_hit_at(input.mouse_x, input.mouse_y, game_hud.scale()).kind() ==
                        client::UiHitKind::None) {
                    send_riding_equipment(protocol::BattleRidingOperation::Unequip,
                                          static_cast<std::uint32_t>(slot));
                }
                riding_pressed_slot = -1;
                riding_equipment_dragging = false;
            } else {
                const float dx = input.mouse_x - riding_press_x;
                const float dy = input.mouse_y - riding_press_y;
                riding_equipment_dragging = riding_equipment_dragging || dx * dx + dy * dy >= 36.0f;
            }
        }

        const auto menu_pointer_blocked = [&]() {
            return game_menu.open() || settings_window.open() || player_options.open() ||
                   party_window.contains(input.mouse_x, input.mouse_y, ui) ||
                   shop_win.quantity_open() || storage_win.quantity_open() || chat_options_hit ||
                   riding_hit_at(input.mouse_x, input.mouse_y, game_hud.scale()).kind() !=
                       client::UiHitKind::None ||
                   inv_win.contains(input.mouse_x, input.mouse_y) ||
                   skill_win.contains(input.mouse_x, input.mouse_y) ||
                   transform_win.contains(input.mouse_x, input.mouse_y) ||
                   char_info_win.contains(input.mouse_x, input.mouse_y) ||
                   npc_talk_win.contains(input.mouse_x, input.mouse_y) ||
                   blacksmith_win.contains(input.mouse_x, input.mouse_y) ||
                   mix_win.contains(input.mouse_x, input.mouse_y) ||
                   (shop_win.contains(input.mouse_x, input.mouse_y) ||
                    skin_shop_win.contains(input.mouse_x, input.mouse_y)) ||
                   storage_win.contains(input.mouse_x, input.mouse_y) ||
                   speed_medicine_win.contains(input.mouse_x, input.mouse_y) ||
                   consumable_crafting_win.contains(input.mouse_x, input.mouse_y) ||
                   quest_journal_win.contains(input.mouse_x, input.mouse_y) ||
                   guild_window.contains(input.mouse_x, input.mouse_y) ||
                   mail_window.contains(input.mouse_x, input.mouse_y) ||
                   inv_win.capturing_pointer() || skill_bar.interacting() ||
                   skill_win.drag_skill_index() >= 0 || transform_win.drag_skill_index() >= 0 ||
                   riding_equipment_dragging || riding_window_dragging || skill_win.dragging() ||
                   transform_win.dragging() || char_info_win.dragging() || mix_win.dragging() ||
                   (shop_win.dragging() || skin_shop_win.dragging()) || storage_win.dragging() ||
                   (speed_medicine_win.dragging() || consumable_crafting_win.dragging());
        };

        // HUD hit-test before world click — movable panels + menu icons.
        bool ui_ate_click =
            riding_equipment_dragging || riding_window_dragging || skill_win.dragging() ||
            transform_win.dragging() || char_info_win.dragging() || mix_win.dragging() ||
            (shop_win.dragging() || skin_shop_win.dragging()) || storage_win.dragging() ||
            (speed_medicine_win.dragging() || consumable_crafting_win.dragging()) ||
            (input.mouse_left_clicked && chat_options_hit);
        if (!game_menu.open() && !settings_window.open()) {
            const bool party_was_dragging = party_window.dragging();
            const bool party_hit = party_window.contains(input.mouse_x, input.mouse_y, ui);
            if (const auto request = party_window.handle(input, ui)) {
                if (!net.send_party(*request)) {
                    game_hud.chat().push_info("The party request could not be sent.", {});
                }
            }
            if (party_hit) {
                ui_ate_click = true;
            }
            if (party_window.dragging()) {
                hud_layout_dirty = true;
            }
            if (hud_layout_dirty && party_was_dragging && !party_window.dragging()) {
                persist_hud();
            }
        }
        if (input.mouse_left_clicked && !ui_ate_click && riding_window_open) {
            const client::UiHit hit = riding_hit_at(input.mouse_x, input.mouse_y, game_hud.scale());
            if (hit.kind() == client::UiHitKind::Close ||
                (hit.kind() == client::UiHitKind::Button && hit.action() == "window.close")) {
                close_riding_window();
                ui_ate_click = true;
            } else if (hit.kind() == client::UiHitKind::TitleDrag) {
                riding_window_dragging = true;
                riding_window_drag_dx = input.mouse_x - riding_window_x;
                riding_window_drag_dy = input.mouse_y - riding_window_y;
                ui_ate_click = true;
            } else if (hit.kind() == client::UiHitKind::IconSlot) {
                const int slot = client::riding_equipment_slot(hit.bind());
                if (slot >= 0 &&
                    registered_battle_equipment_iids[static_cast<std::size_t>(slot)] != 0) {
                    riding_pressed_slot = slot;
                    riding_pressed_iid =
                        registered_battle_equipment_iids[static_cast<std::size_t>(slot)];
                    riding_press_x = input.mouse_x;
                    riding_press_y = input.mouse_y;
                    riding_equipment_dragging = false;
                }
                ui_ate_click = true;
            } else if (hit.kind() == client::UiHitKind::Button && want_net && net.connected()) {
                protocol::BattleRidingRequest request;
                request.operation_id = next_battle_riding_operation_id++;
                if (next_battle_riding_operation_id == 0) {
                    next_battle_riding_operation_id = 1;
                }
                request.mount_iid = registered_ride_iid;
                if (hit.action() == "stablemaster.exchange" && stablemaster_npc_id != 0) {
                    request.operation = protocol::BattleRidingOperation::StableExchange;
                    request.mount_iid = 0;
                    request.secondary_iid = stablemaster_npc_id;
                } else if (hit.action() == "stablemaster.promote" && stablemaster_npc_id != 0) {
                    request.operation = protocol::BattleRidingOperation::StablePromote;
                    request.secondary_iid = stablemaster_npc_id;
                } else if (hit.action() == "stablemaster.grade" && stablemaster_npc_id != 0) {
                    request.operation = protocol::BattleRidingOperation::StableGrade;
                    request.secondary_iid = stablemaster_npc_id;
                } else if (hit.action() == "stablemaster.writ" && stablemaster_npc_id != 0) {
                    std::uint32_t writ_iid = 0;
                    int count = 0;
                    (void) player_bag.try_find_stack(41323, writ_iid, count);
                    request.operation = protocol::BattleRidingOperation::StableTimeWrit;
                    request.primary_iid = writ_iid;
                    request.secondary_iid = stablemaster_npc_id;
                } else if (hit.action() == "stablemaster.duplicate" && stablemaster_npc_id != 0) {
                    request.operation = protocol::BattleRidingOperation::StableConvertDuplicate;
                    request.mount_iid = 0;
                    request.primary_iid = stablemaster_duplicate_iid;
                    request.secondary_iid = stablemaster_npc_id;
                    if (stablemaster_duplicate_iid == 0) {
                        request.operation_id = 0;
                        riding_feedback = "Drag an unregistered Riding Pet into the slot first.";
                    }
                } else if (registered_ride_iid != 0 && hit.action() == "riding.feed") {
                    std::uint32_t feed_iid = 0;
                    int count = 0;
                    (void) player_bag.try_find_stack(30320, feed_iid, count);
                    request.operation = protocol::BattleRidingOperation::Feed;
                    request.primary_iid = feed_iid;
                } else if (registered_ride_iid != 0 &&
                           (hit.action() == "riding.ability" || hit.action() == "riding.reroll")) {
                    std::uint32_t egg_iid = 0;
                    int count = 0;
                    (void) player_bag.try_find_stack(30330, egg_iid, count);
                    request.operation = hit.action() == "riding.ability"
                                            ? protocol::BattleRidingOperation::UnlockAbility
                                            : protocol::BattleRidingOperation::RerollAbilities;
                    request.primary_iid = egg_iid;
                } else if (registered_ride_iid != 0 &&
                           (hit.action() == "riding.enchant" || hit.action() == "riding.upgrade")) {
                    const bool upgrade = hit.action() == "riding.upgrade";
                    for (std::size_t i = 0; i < registered_battle_equipment_iids.size(); ++i) {
                        const int equipment_index = registered_battle_equipment_indices[i];
                        const int enchant = registered_battle_equipment_enchants[i];
                        if (registered_battle_equipment_iids[i] == 0 || equipment_index < 30200 ||
                            equipment_index > 30223 || (upgrade ? enchant != 10 : enchant >= 10)) {
                            continue;
                        }
                        const int grade = (equipment_index - 30200) / 6;
                        std::uint32_t material_iid = 0;
                        int count = 0;
                        (void) player_bag.try_find_stack(
                            (upgrade ? 30310 : 30300) + grade, material_iid, count);
                        request.operation = upgrade
                                                ? protocol::BattleRidingOperation::UpgradeEquipment
                                                : protocol::BattleRidingOperation::EnchantEquipment;
                        request.primary_iid = registered_battle_equipment_iids[i];
                        request.secondary_iid = material_iid;
                        break;
                    }
                } else {
                    request.operation_id = 0;
                }
                if (request.operation_id != 0) {
                    if (net.send_battle_riding_request(request)) {
                        riding_feedback.clear();
                    } else {
                        riding_feedback = "The Riding request could not be sent.";
                    }
                } else if (hit.action().substr(0, 13) == "stablemaster." &&
                           hit.action() != "stablemaster.duplicate") {
                    riding_feedback = "Register an eligible Riding Pet first.";
                }
                ui_ate_click = true;
            } else if (hit.kind() != client::UiHitKind::None) {
                ui_ate_click = true;
            }
        }
        if (input.mouse_left_clicked && !ui_ate_click) {
            const auto now = std::chrono::steady_clock::now().time_since_epoch();
            const std::uint64_t now_ms = static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(now).count());
            const int buff_click = client::BuffSlotBar::hit_test(0.0f,
                                                                 90.0f * game_hud.scale(),
                                                                 hud_buff_slots,
                                                                 now_ms,
                                                                 game_hud.scale(),
                                                                 input.mouse_x,
                                                                 input.mouse_y);
            if (buff_click >= 0 && buff_click < static_cast<int>(hud_buff_slots.size()) &&
                hud_buff_slots[static_cast<std::size_t>(buff_click)].is_mail_notification) {
                ui_ate_click = true;
                const bool was_open = mail_window.open();
                mail_window.set_open(true);
                update_window_order(client::InGameWindowId::Mail, was_open, mail_window.open());
                mail_window.set_entries(net.mail());
                mail_window.center_on_screen(static_cast<float>(input.width),
                                             static_cast<float>(input.height));
                if (want_net && net.connected() && mail_window.refresh_ready()) {
                    mail_window.request_sent(client::MailClickResult::Refresh,
                                             net.send_mail_list());
                }
            }
        }
        if (input.mouse_right_clicked && !ui_ate_click && riding_window_open) {
            const auto hit = riding_hit_at(input.mouse_x, input.mouse_y, game_hud.scale());
            if (hit.kind() != client::UiHitKind::None) {
                ui_ate_click = true;
                const int slot = client::riding_equipment_slot(hit.bind());
                if (slot >= 0 &&
                    registered_battle_equipment_iids[static_cast<std::size_t>(slot)] != 0) {
                    send_riding_equipment(protocol::BattleRidingOperation::Unequip,
                                          static_cast<std::uint32_t>(slot));
                }
            }
        }
        bool rmb_skill_consumed = false;
        {
            // Finish skill drag-drop on mouse release (after panel rects are known).
            if (skill_win.drag_skill_index() >= 0 && !input.mouse_left) {
                if (!skill_bar.try_drop(
                        input.mouse_x, input.mouse_y, skill_win.drag_skill_index())) {
                    const int clicked =
                        skill_win.click_cast_on_release(input.mouse_x, input.mouse_y);
                    if (clicked >= 0) {
                        (void) cast_skill_index(clicked);
                    }
                }
                skill_win.set_drag_skill_index(-1);
            }
            if (transform_win.drag_skill_index() >= 0 && !input.mouse_left) {
                if (!skill_bar.try_drop(
                        input.mouse_x, input.mouse_y, transform_win.drag_skill_index())) {
                    log::debug("transform skill drop missed hotbar");
                }
                transform_win.set_drag_skill_index(-1);
            }
            if (skill_bar.interacting() && !input.mouse_left) {
                const auto bar_click = skill_bar.handle_release(
                    input.mouse_x,
                    input.mouse_y,
                    inv_win.open() && inv_win.contains(input.mouse_x, input.mouse_y));
                ui_ate_click = true;
                if (bar_click == client::BarResult::Cast && skill_bar.last_cast_index() >= 0) {
                    if (!cast_skill_index(skill_bar.last_cast_index())) {
                        log::debug("skill-bar cast rejected (index=" +
                                   std::to_string(skill_bar.last_cast_index()) + ")");
                    }
                } else if (bar_click == client::BarResult::SetActive) {
                    social_mode = client::SocialAction::None;
                } else if (bar_click == client::BarResult::UseItem &&
                           skill_bar.last_item_index() > 0) {
                    const int item_index = skill_bar.last_item_index();
                    const client::ItemDef* def = item_catalog.find(item_index);
                    if (def) {
                        (void) try_use_item(item_index, "hotbar-click");
                    }
                } else if (bar_click == client::BarResult::DropItem) {
                    if (want_net && net.connected() && skill_bar.last_drop_iid() != 0) {
                        (void) net.send_drop_item(
                            skill_bar.last_drop_iid(),
                            static_cast<std::uint32_t>(std::max(0, skill_bar.last_drop_count())));
                    } else if (skill_bar.last_drop_index() > 0 && skill_bar.last_drop_count() > 0) {
                        (void) player_bag.try_remove(skill_bar.last_drop_index(),
                                                     skill_bar.last_drop_count());
                    }
                }
            }
            const bool dragging_before = game_hud.is_dragging();
            if (input.mouse_left_clicked && !ui_ate_click && skill_win.drag_skill_index() < 0 &&
                transform_win.drag_skill_index() < 0) {
                if (skill_bar.begin_press(input.mouse_x, input.mouse_y)) {
                    ui_ate_click = true;
                }
            }
            skill_bar.update_pointer(input);
            if (skill_bar.interacting()) {
                ui_ate_click = true;
            }
            const bool hud_blocked = menu_pointer_blocked() && !game_hud.is_dragging();
            const auto hud_ptr = ui_ate_click  ? client::HudPointerResult::Handled
                                 : hud_blocked ? client::HudPointerResult::Ignored
                                               : game_hud.handle_pointer(input, frame_w, frame_h);
            if (hud_ptr == client::HudPointerResult::ClockClosed) {
                client_settings.clock_enabled = false;
                std::string save_error;
                if (!runtime.save_client_settings(client_settings, save_error)) {
                    log::warn("Failed to save clock preference: " + save_error);
                }
            }
            if (hud_ptr != client::HudPointerResult::Ignored) {
                ui_ate_click = true;
                if (hud_ptr == client::HudPointerResult::MenuAction) {
                    if (game_hud.last_menu_action() == client::HudMenuAction::Inventory) {
                        const bool was_open = inv_win.open();
                        inv_win.toggle();
                        update_window_order(
                            client::InGameWindowId::Inventory, was_open, inv_win.open());
                        if (inv_win.open()) {
                            inv_win.center_on_screen(static_cast<float>(input.width),
                                                     static_cast<float>(input.height));
                        }
                    } else if (game_hud.last_menu_action() == client::HudMenuAction::Sinsu) {
                        const bool was_open = transform_win.open();
                        transform_win.toggle();
                        update_window_order(
                            client::InGameWindowId::Transform, was_open, transform_win.open());
                        if (transform_win.open()) {
                            transform_win.center_on_screen(frame_w, frame_h);
                        }
                    } else if (game_hud.last_menu_action() == client::HudMenuAction::Riding) {
                        toggle_riding_window(frame_w, frame_h, game_hud.scale());
                    } else if (game_hud.last_menu_action() == client::HudMenuAction::Character) {
                        const bool was_open = char_info_win.open();
                        char_info_win.toggle();
                        update_window_order(
                            client::InGameWindowId::CharacterInfo, was_open, char_info_win.open());
                        if (char_info_win.open()) {
                            char_info_win.center_on_screen(static_cast<float>(input.width),
                                                           static_cast<float>(input.height));
                        }
                    } else if (game_hud.last_menu_action() == client::HudMenuAction::Skill) {
                        const bool was_open = skill_win.open();
                        skill_win.toggle();
                        update_window_order(
                            client::InGameWindowId::Skills, was_open, skill_win.open());
                        if (skill_win.open()) {
                            skill_win.center_on_screen(static_cast<float>(input.width),
                                                       static_cast<float>(input.height));
                        }
                    } else if (game_hud.last_menu_action() == client::HudMenuAction::Quest) {
                        const bool was_open = quest_journal_win.open();
                        quest_journal_win.toggle();
                        update_window_order(client::InGameWindowId::QuestJournal,
                                            was_open,
                                            quest_journal_win.open());
                        if (quest_journal_win.open()) {
                            quest_journal_win.sync_quests(
                                net.quest_flags(), task_catalog, item_catalog);
                            quest_journal_win.center_on_screen(static_cast<float>(input.width),
                                                               static_cast<float>(input.height));
                        }
                    } else if (game_hud.last_menu_action() == client::HudMenuAction::Party) {
                        if (party_window.open()) {
                            const auto* target = net.castle_enemy(selected_player_id)
                                                     ? nullptr
                                                     : remote_players.find(selected_player_id);
                            party_window.toggle_actions(target ? selected_player_id : 0,
                                                        target ? target->name() : "");
                        } else if (net.castle_enemy(selected_player_id)) {
                            game_hud.chat().push_info(
                                "You cannot invite a Castle War enemy to your party.", {});
                        } else if (selected_player_id != 0 &&
                                   selected_player_id != net.local_player_id() &&
                                   remote_players.find(selected_player_id)) {
                            protocol::PartyRequest invitation;
                            invitation.command = protocol::PartyCommand::Invite;
                            invitation.target = selected_player_id;
                            if (!net.send_party(invitation)) {
                                game_hud.chat().push_info("The party invitation could not be sent.",
                                                          {});
                            }
                        } else {
                            game_hud.chat().push_info(
                                "Select a player, then click Party to invite.", {});
                        }
                    } else if (game_hud.last_menu_action() == client::HudMenuAction::Community) {
                        if (multiplayer && net.guild_view().is_object()) {
                            const bool was_open = guild_window.open();
                            guild_window.set_open(!was_open);
                            update_window_order(
                                client::InGameWindowId::Guild, was_open, guild_window.open());
                        } else {
                            game_hud.chat().push_info("Guilds are unavailable on this server.", {});
                        }
                    } else if (game_hud.last_menu_action() == client::HudMenuAction::TradingPost) {
                        trading_post.set_open(!trading_post.open());
                    } else if (game_hud.last_menu_action() == client::HudMenuAction::Messages) {
                        const bool was_open = mail_window.open();
                        mail_window.toggle();
                        update_window_order(
                            client::InGameWindowId::Mail, was_open, mail_window.open());
                        if (mail_window.open()) {
                            mail_window.set_entries(net.mail());
                            mail_window.center_on_screen(frame_w, frame_h);
                            if (want_net && net.connected() && mail_window.refresh_ready()) {
                                mail_window.request_sent(client::MailClickResult::Refresh,
                                                         net.send_mail_list());
                            }
                        }
                    } else if (game_hud.last_menu_action() == client::HudMenuAction::Map) {
                        world_map_open = !world_map_open;
                    } else if (game_hud.last_menu_action() == client::HudMenuAction::System) {
                        game_menu.show(frame_w, frame_h);
                    }
                } else if (game_hud.is_dragging()) {
                    hud_layout_dirty = true;
                }
            }
            if (hud_layout_dirty && dragging_before && !game_hud.is_dragging()) {
                persist_hud();
            }
            if (!input.mouse_left) {
                const auto mix_rel =
                    mix_win.handle_release(input.mouse_x, input.mouse_y, item_catalog);
                if (mix_rel != client::MixClickResult::Ignored) {
                    ui_ate_click = true;
                    if (mix_rel == client::MixClickResult::MakeMix) {
                        enchant_overlay.begin_mix_charge(4.0f,
                                                         mix_win.mode(),
                                                         mix_win.weapon_iid(),
                                                         mix_win.extra_weapon_iid(),
                                                         mix_win.armor_iid(),
                                                         mix_win.stone1_iid(),
                                                         mix_win.stone2_iid());
                    }
                }
            }
            if (input.mouse_left_clicked && !ui_ate_click) {
                const auto talk_click = npc_talk_win.handle_click(input.mouse_x, input.mouse_y);
                if (talk_click != client::NpcTalkClickResult::Ignored) {
                    ui_ate_click = true;
                    if (talk_click == client::NpcTalkClickResult::OpenDialog) {
                        if (const client::DialogDef* dlg =
                                dialog_catalog.find(npc_talk_win.pending_dialog_id())) {
                            const bool was_open = npc_talk_win.open();
                            npc_talk_win.show_dialog(
                                *dlg, npc_talk_win.npc_id(), boot.selected.name);
                            update_window_order(
                                client::InGameWindowId::NpcTalk, was_open, npc_talk_win.open());
                        }
                    } else if (talk_click == client::NpcTalkClickResult::Guild ||
                               talk_click == client::NpcTalkClickResult::GuildCreate) {
                        if (multiplayer && net.guild_view().is_object()) {
                            const bool was_open = guild_window.open();
                            if (talk_click == client::NpcTalkClickResult::GuildCreate) {
                                guild_window.show_creation();
                            } else {
                                guild_window.set_open(true);
                            }
                            update_window_order(
                                client::InGameWindowId::Guild, was_open, guild_window.open());
                        } else {
                            game_hud.chat().push_info("Guilds are unavailable on this server.", {});
                        }
                    } else if (talk_click == client::NpcTalkClickResult::QuestLink && want_net &&
                               net.connected()) {
                        (void) net.send_quest(npc_talk_win.pending_quest_index(),
                                              npc_talk_win.pending_quest_flag());
                    } else if (talk_click == client::NpcTalkClickResult::SaveRevival && want_net &&
                               net.connected()) {
                        (void) net.send_save_revival_point(npc_talk_win.pending_revival_point(),
                                                           npc_talk_win.npc_id());
                    } else if (talk_click == client::NpcTalkClickResult::VillageRevival &&
                               want_net && net.connected()) {
                        (void) net.send_revival();
                    } else if (talk_click == client::NpcTalkClickResult::NpcTeleport && want_net &&
                               net.connected()) {
                        (void) net.send_ask_npc(npc_talk_win.npc_id());
                    } else if (talk_click == client::NpcTalkClickResult::Blacksmith) {
                        const bool was_open = blacksmith_win.open();
                        blacksmith_win.set_open(true);
                        update_window_order(
                            client::InGameWindowId::Blacksmith, was_open, blacksmith_win.open());
                        blacksmith_win.center_on_screen(static_cast<float>(input.width),
                                                        static_cast<float>(input.height));
                    } else if (talk_click == client::NpcTalkClickResult::Mixing ||
                               talk_click == client::NpcTalkClickResult::MixingList) {
                        const bool was_open = mix_win.open();
                        if (talk_click == client::NpcTalkClickResult::MixingList) {
                            mix_win.show_materials(npc_talk_win.pending_mixing_mode());
                        } else {
                            mix_win.set_open(true, npc_talk_win.pending_mixing_mode());
                        }
                        update_window_order(
                            client::InGameWindowId::Mixing, was_open, mix_win.open());
                        mix_win.center_on_screen(static_cast<float>(input.width),
                                                 static_cast<float>(input.height));
                    } else if (talk_click == client::NpcTalkClickResult::CraftMedicine ||
                               talk_click == client::NpcTalkClickResult::CraftMagicPotion ||
                               talk_click == client::NpcTalkClickResult::CraftAutoMedicine) {
                        const bool was_open = consumable_crafting_win.open();
                        const auto family =
                            talk_click == client::NpcTalkClickResult::CraftMagicPotion
                                ? server::ConsumableFamily::MagicPotion
                            : talk_click == client::NpcTalkClickResult::CraftAutoMedicine
                                ? server::ConsumableFamily::AutoMedicine
                                : server::ConsumableFamily::Medicine;
                        consumable_crafting_win.set_family(family);
                        consumable_crafting_win.set_open(true);
                        update_window_order(
                            client::InGameWindowId::ConsumableCrafting, was_open, true);
                        consumable_crafting_win.center_on_screen(static_cast<float>(input.width),
                                                                 static_cast<float>(input.height));
                    } else if (talk_click == client::NpcTalkClickResult::SpeedMedicine) {
                        const bool was_open = speed_medicine_win.open();
                        speed_medicine_win.set_open(true);
                        update_window_order(client::InGameWindowId::SpeedMedicine,
                                            was_open,
                                            speed_medicine_win.open());
                        speed_medicine_win.center_on_screen(static_cast<float>(input.width),
                                                            static_cast<float>(input.height));
                    } else if (talk_click == client::NpcTalkClickResult::Stablemaster) {
                        riding_window_open = true;
                        in_game_window_order.opened(client::InGameWindowId::Riding);
                        stablemaster_npc_id = npc_talk_win.npc_id();
                        if (!riding_window_positioned) {
                            const float s = game_hud.scale();
                            const float riding_w = riding_view.window_w() * s;
                            const float riding_h = riding_view.window_h() * s;
                            riding_window_x =
                                std::max(0.0f, (static_cast<float>(input.width) - riding_w) * 0.5f);
                            riding_window_y = std::max(
                                0.0f, (static_cast<float>(input.height) - riding_h) * 0.35f);
                            riding_window_positioned = true;
                        }
                    } else if ((talk_click == client::NpcTalkClickResult::SkinWeapons ||
                                talk_click == client::NpcTalkClickResult::SkinFull ||
                                talk_click == client::NpcTalkClickResult::SkinParts) &&
                               want_net && net.connected()) {
                        pending_skin_category =
                            talk_click == client::NpcTalkClickResult::SkinFull
                                ? client::SkinShopCategory::FullBody
                            : talk_click == client::NpcTalkClickResult::SkinParts
                                ? client::SkinShopCategory::Parts
                                : client::SkinShopCategory::Weapons;
                        (void) net.send_shop_item_list(npc_talk_win.npc_id());
                    } else if (talk_click == client::NpcTalkClickResult::Shop && want_net &&
                               net.connected()) {
                        (void) net.send_shop_item_list(npc_talk_win.npc_id());
                    } else if (talk_click == client::NpcTalkClickResult::TradingPostCollection &&
                               want_net && net.connected()) {
                        trading_post.show_collection(npc_talk_win.npc_id());
                    } else if (talk_click == client::NpcTalkClickResult::Storage && want_net &&
                               net.connected()) {
                        (void) net.send_storage_info(npc_talk_win.npc_id());
                    }
                }
                const auto craft_click = ui_ate_click
                                             ? client::ConsumableCraftingClickResult::Ignored
                                             : consumable_crafting_win.handle_click(
                                                   input.mouse_x, input.mouse_y, player_bag);
                if (craft_click != client::ConsumableCraftingClickResult::Ignored) {
                    ui_ate_click = true;
                    if (craft_click == client::ConsumableCraftingClickResult::Close) {
                        in_game_window_order.closed(client::InGameWindowId::ConsumableCrafting);
                    } else if (craft_click == client::ConsumableCraftingClickResult::Make &&
                               want_net && net.connected()) {
                        (void) net.send_quest(server::kConsumableCraftingQuestIndex,
                                              consumable_crafting_win.take_recipe());
                    }
                }
                const auto medicine_click = speed_medicine_win.handle_click(
                    input.mouse_x, input.mouse_y, player_bag, input.key_ctrl);
                if (medicine_click != client::SpeedMedicineClickResult::Ignored) {
                    ui_ate_click = true;
                    if (medicine_click == client::SpeedMedicineClickResult::Close) {
                        in_game_window_order.closed(client::InGameWindowId::SpeedMedicine);
                    } else if (medicine_click == client::SpeedMedicineClickResult::Make) {
                        in_game_window_order.closed(client::InGameWindowId::SpeedMedicine);
                        if (want_net && net.connected()) {
                            const protocol::CookingOrder order = speed_medicine_win.take_order();
                            if (net.send_cooking(order)) {
                                pending_cooking_order = order;
                            }
                        }
                    }
                }
                const auto skin_click =
                    ui_ate_click ? client::SkinShopClickResult::Ignored
                                 : skin_shop_win.handle_click(
                                       input.mouse_x, input.mouse_y, player_bag, item_catalog);
                if (skin_click != client::SkinShopClickResult::Ignored) {
                    ui_ate_click = true;
                    if (skin_click == client::SkinShopClickResult::Close) {
                        in_game_window_order.closed(client::InGameWindowId::Shop);
                    } else if (skin_click == client::SkinShopClickResult::Redeem && want_net &&
                               net.connected()) {
                        (void) net.send_buy_item(
                            skin_shop_win.npc_id(), skin_shop_win.selected_index(), 1);
                    }
                }
                const auto shop_click = shop_win.handle_click(
                    input.mouse_x, input.mouse_y, ui, player_bag, item_catalog, input.key_ctrl);
                if (shop_click != client::ShopClickResult::Ignored) {
                    ui_ate_click = true;
                    if (shop_click == client::ShopClickResult::Buy && want_net && net.connected()) {
                        (void) net.send_buy_item(shop_win.npc_id(),
                                                 shop_win.pending_index(),
                                                 shop_win.pending_quantity());
                    } else if (shop_click == client::ShopClickResult::Sell) {
                        send_pending_shop_sales();
                    }
                }
                const auto storage_click = storage_win.handle_click(
                    input.mouse_x, input.mouse_y, ui, player_bag, item_catalog, input.key_ctrl);
                if (storage_click != client::StorageClickResult::Ignored) {
                    ui_ate_click = true;
                    if (storage_click == client::StorageClickResult::Deposit ||
                        storage_click == client::StorageClickResult::Withdraw ||
                        storage_click == client::StorageClickResult::MoneyTransfer) {
                        send_pending_storage_transfers(storage_click);
                    } else if (storage_click == client::StorageClickResult::Close) {
                        in_game_window_order.closed(client::InGameWindowId::Storage);
                    }
                }
                const auto smith_click = blacksmith_win.handle_click(
                    input.mouse_x, input.mouse_y, player_bag, item_catalog);
                if (smith_click != client::BlacksmithClickResult::Ignored) {
                    ui_ate_click = true;
                    if (smith_click == client::BlacksmithClickResult::Revise ||
                        smith_click == client::BlacksmithClickResult::ExplosiveBlow) {
                        enchant_overlay.begin_upgrade_charge(
                            blacksmith_win.weapon_iid(),
                            smith_click == client::BlacksmithClickResult::Revise);
                    }
                }
                const auto mix_click =
                    mix_win.handle_click(input.mouse_x, input.mouse_y, item_catalog);
                if (mix_click != client::MixClickResult::Ignored) {
                    ui_ate_click = true;
                    if (mix_click == client::MixClickResult::Close) {
                        mix_win.set_open(false, mix_win.mode());
                        in_game_window_order.closed(client::InGameWindowId::Mixing);
                    }
                }
                if (quest_journal_win.handle_click(input.mouse_x, input.mouse_y)) {
                    ui_ate_click = true;
                }
                const auto mail_click = mail_window.handle_click(input.mouse_x, input.mouse_y);
                if (mail_click != client::MailClickResult::Ignored) {
                    ui_ate_click = true;
                    if (mail_click == client::MailClickResult::Close) {
                        in_game_window_order.closed(client::InGameWindowId::Mail);
                    } else if (mail_click == client::MailClickResult::Claim ||
                               mail_click == client::MailClickResult::Read ||
                               mail_click == client::MailClickResult::Delete ||
                               mail_click == client::MailClickResult::Refresh) {
                        bool sent = false;
                        if (want_net && net.connected()) {
                            if (mail_click == client::MailClickResult::Claim) {
                                sent = net.send_mail_claim(mail_window.selected_id());
                            } else if (mail_click == client::MailClickResult::Refresh) {
                                sent = net.send_mail_list();
                            } else {
                                sent =
                                    net.send_mail_action(mail_click == client::MailClickResult::Read
                                                             ? protocol::MailCommand::Read
                                                             : protocol::MailCommand::Delete,
                                                         mail_window.selected_id());
                            }
                        }
                        mail_window.request_sent(mail_click, sent);
                    }
                }
                const auto char_click =
                    char_info_win.handle_click(input.mouse_x, input.mouse_y, &ui);
                if (char_click != client::CharacterInfoClickResult::Ignored) {
                    ui_ate_click = true;
                }
                const auto skill_click = skill_win.handle_click(input.mouse_x, input.mouse_y);
                if (skill_click != client::SkillClickResult::Ignored) {
                    ui_ate_click = true;
                    if (skill_click == client::SkillClickResult::OpenTalents) {
                        auto_attack = false;
                        local_player.clear_click_target();
                        talent_tree.open();
                        if (want_net && net.connected()) {
                            (void) net.send_talent_request(protocol::TalentCommand::Snapshot);
                        }
                    } else if (skill_click == client::SkillClickResult::Learn &&
                               skill_win.last_cast_index() >= 0) {
                        const int idx = skill_win.last_cast_index();
                        const std::uint8_t skill =
                            static_cast<std::uint8_t>(std::clamp(idx, 0, 255));
                        if (want_net && net.connected()) {
                            const bool ok = player_skills.level_of(idx) <= 0
                                                ? net.send_learn_skill(skill)
                                                : net.send_skill_up(skill);
                            if (!ok) {
                                log::debug("learn-window SU spend not sent");
                            }
                        }
                    } else if (skill_click == client::SkillClickResult::Cast &&
                               skill_win.last_cast_index() >= 0) {
                        if (!cast_skill_index(skill_win.last_cast_index())) {
                            log::debug("learn-window cast rejected");
                        }
                    }
                }
                const auto transform_click =
                    transform_win.handle_click(input.mouse_x, input.mouse_y);
                if (transform_click != client::TransformClickResult::Ignored) {
                    ui_ate_click = true;
                    if (transform_click == client::TransformClickResult::CastTransform &&
                        transform_win.last_skill_index() >= 0) {
                        if (!cast_skill_index(transform_win.last_skill_index())) {
                            log::debug("transform-window cast rejected");
                        }
                    } else if (transform_click == client::TransformClickResult::CastAnimal &&
                               transform_win.last_skill_index() >= 0) {
                        if (!cast_skill_index(transform_win.last_skill_index())) {
                            log::debug("transform-window animal cast rejected");
                        }
                    }
                }
                const auto inv_click =
                    inv_win.handle_click(input.mouse_x, input.mouse_y, player_bag, player_wear);
                if (inv_click != client::ClickResult::Ignored) {
                    ui_ate_click = true;
                    if (inv_click == client::ClickResult::EquipChanged) {
                        refresh_player_visuals();
                        sync_equip_net(inv_win.last_equip_op(), inv_win.last_equip_iid());
                    } else if (inv_click == client::ClickResult::SortRequested) {
                        const bool quest = inv_win.tab() == client::Tab::Quest;
                        if (want_net && net.connected()) {
                            (void) net.send_inventory_sort(quest);
                        } else if (!want_net) {
                            inv_win.complete_sort(player_bag, quest);
                        }
                    } else if (inv_click == client::ClickResult::Consumed) {
                        (void) try_use_item(inv_win.last_consumed_index(), "inventory expansion");
                    } else if (inv_click == client::ClickResult::Enchanted) {
                        enchant_overlay.begin_charge(inv_win.last_enchant_from_id(),
                                                     inv_win.last_enchant_to_id());
                    } else if (inv_click == client::ClickResult::OpenMail) {
                        const bool was_open = mail_window.open();
                        mail_window.set_open(true);
                        update_window_order(
                            client::InGameWindowId::Mail, was_open, mail_window.open());
                        mail_window.set_entries(net.mail());
                        mail_window.center_on_screen(static_cast<float>(input.width),
                                                     static_cast<float>(input.height));
                        if (want_net && net.connected() && mail_window.refresh_ready()) {
                            mail_window.request_sent(client::MailClickResult::Refresh,
                                                     net.send_mail_list());
                        }
                    }
                }
            }
            if (input.mouse_right_clicked && !ui_ate_click) {
                if (inv_win.contains(input.mouse_x, input.mouse_y)) {
                    const auto inv_rclick =
                        inv_win.handle_right_click(input.mouse_x, input.mouse_y, player_bag);
                    if (inv_rclick != client::ClickResult::Ignored) {
                        ui_ate_click = true;
                        if (inv_rclick == client::ClickResult::Consumed) {
                            (void) try_use_item(inv_win.last_consumed_index(),
                                                "inventory",
                                                0,
                                                inv_win.last_consumed_iid());
                        }
                    }
                }
            }
            if (input.mouse_right_clicked && !ui_ate_click && !cam.fly_mode() &&
                range_skill_cast == 1) {
                reset_range_skill();
                local_player.clear_click_target();
                ui_ate_click = true;
                rmb_skill_consumed = true;
            }
            if (input.mouse_right_clicked && !ui_ate_click && !local_player.dead() && want_net &&
                net.connected() && !cam.fly_mode() && !behead_animation_active) {
                float mob_t = 0.0f;
                float player_t = 0.0f;
                const std::uint32_t hit_mob = pick_world_mob(input.mouse_x,
                                                             input.mouse_y,
                                                             static_cast<float>(input.width),
                                                             static_cast<float>(input.height),
                                                             mob_t);
                const std::uint32_t hit_player =
                    remote_players.pick(cam,
                                        local_player,
                                        input.mouse_x,
                                        input.mouse_y,
                                        static_cast<float>(input.width),
                                        static_cast<float>(input.height),
                                        player_t);
                const bool prefer_player = hit_player != 0 && hit_player != net.local_player_id() &&
                                           (hit_mob == 0 || player_t <= mob_t);
                const std::uint32_t hostile_id = prefer_player ? hit_player : hit_mob;
                if (hostile_id != 0) {
                    if (queued_skill_index == 1) {
                        clear_queued_target_skill();
                        local_player.clear_click_target();
                    }
                    if (player_actor.attack_locked()) {
                        player_actor.cancel_attack();
                    }
                    if (prefer_player) {
                        selected_player_id = hit_player;
                        selected_mob_id = 0;
                    } else {
                        selected_mob_id = hit_mob;
                        selected_player_id = 0;
                    }
                    selected_npc_id = 0;
                    selected_item_id = 0;
                    pending_pickup_id = 0;
                    pending_ask_npc_id = 0;
                    auto_attack = !prefer_player;
                    auto_skill = false;
                    const auto* active_definition =
                        skill_catalog.find(skill_win.player_clazz(), player_skills.active_index());
                    const auto active_social = active_definition
                                                   ? client::social_action(*active_definition)
                                                   : client::SocialAction::None;
                    if (prefer_player && net.castle_enemy(hit_player) &&
                        active_social != client::SocialAction::None) {
                        auto_attack = false;
                    } else if (prefer_player && active_social == client::SocialAction::Party) {
                        protocol::PartyRequest invite;
                        invite.command = protocol::PartyCommand::Invite;
                        invite.target = hit_player;
                        (void) net.send_party(invite);
                    } else if (prefer_player && active_social == client::SocialAction::Trade) {
                        protocol::TradeRequest request;
                        request.command = protocol::TradeCommand::Invite;
                        request.target = hit_player;
                        (void) net.send_trade(request);
                    } else if (social_mode != client::SocialAction::None ||
                               client::social_action_uses_active_slot(active_social)) {
                        auto_attack = false;
                    } else if (is_focus_shot_charging()) {
                        auto_attack = false;
                    } else if (player_skills.active_index() >= 0) {
                        const int sk = player_skills.active_index();
                        const auto kind =
                            client::Catalog::cast_kind_for(skill_win.player_clazz(), sk);
                        if (kind == client::CastKind::Combat ||
                            kind == client::CastKind::TargetArea) {
                            const auto* sk_def = skill_catalog.find(skill_win.player_clazz(), sk);
                            if (local_player.clazz() == client::Class::Magician) {
                                auto_skill = sk_def != nullptr && sk_def->attack_auto();
                                auto_attack = auto_skill;
                            } else if (!prefer_player) {
                                auto_attack = true;
                                auto_skill = local_player.clazz() == client::Class::Knight ||
                                             local_player.clazz() == client::Class::Thief ||
                                             (local_player.clazz() == client::Class::Archer &&
                                              sk_def != nullptr && sk_def->attack_auto());
                            }
                        }
                        if (!cast_skill_index(sk)) {
                            log::debug("rmb skill cast rejected");
                        }
                    }
                    rmb_skill_consumed = true;
                    ui_ate_click = true;
                }
            }
        }

        auto begin_pickup = [&](std::uint32_t item_id) -> bool {
            if (!net.connected() || local_player.dead() ||
                (local_player.resting() && !request_stand())) {
                return false;
            }
            auto iit = net.ground_items().find(item_id);
            if (iit == net.ground_items().end()) {
                return false;
            }
            selected_item_id = item_id;
            pending_pickup_id = item_id;
            selected_mob_id = 0;
            selected_npc_id = 0;
            selected_player_id = 0;
            pending_ask_npc_id = 0;
            auto_attack = false;
            auto_skill = false;
            player_actor.cancel_attack();
            pending_arrow_target = 0;
            if (in_client_range(iit->second.wx, iit->second.wz, 16)) {
                const int sx = coords::to_server(iit->second.wx, profile.origin_patch());
                const int sy = coords::to_server(iit->second.wz, profile.origin_patch());
                int tx = 0;
                int ty = 0;
                coords::pixel_to_tile(sx, sy, tx, ty);
                if (!net.send_pickup_item(item_id, tx, ty)) {
                    return false;
                }
                local_player.clear_click_target();
                selected_item_id = 0;
                pending_pickup_id = 0;
                return true;
            }
            local_player.set_click_target(iit->second.wx, iit->second.wz);
            return true;
        };

        struct WorldPointerTargets {
            std::uint32_t mob = 0;
            float mob_distance = 0.0f;
            std::uint32_t item = 0;
            float item_distance = 0.0f;
            std::uint32_t npc = 0;
            float npc_distance = 0.0f;
            std::uint32_t player = 0;
            float player_distance = 0.0f;
        };
        const auto pick_world_targets = [&]() -> WorldPointerTargets {
            std::uint32_t hit_mob = 0;
            float best = 0.0f;
            std::uint32_t hit_item = 0;
            float best_item = 0.0f;
            std::uint32_t hit_npc = 0;
            float best_npc = 0.0f;
            std::uint32_t hit_player = 0;
            float best_player = 0.0f;
            if (want_net) {
                for (const auto& [id, nm] : net.monsters()) {
                    if (!is_clickable_mob(nm)) {
                        continue;
                    }
                    float half = client::WorldPick::kMonsterHalfExtent;
                    float height = client::WorldPick::kMonsterBaseHeight;
                    monster_pick_size(nm, half, height);
                    const float base_y = floor.get_floor(nm.wx(), nm.wz());
                    // World-space AABB: forgiving for combat, matches the DEBUG hitbox
                    // volume.
                    float hit_t = 0.0f;
                    if (!client::WorldPick::pick_monster_box(cam,
                                                             local_player,
                                                             input.mouse_x,
                                                             input.mouse_y,
                                                             static_cast<float>(input.width),
                                                             static_cast<float>(input.height),
                                                             nm.wx(),
                                                             base_y,
                                                             nm.wz(),
                                                             half,
                                                             height,
                                                             hit_t)) {
                        continue;
                    }
                    if (hit_mob == 0 || hit_t < best) {
                        best = hit_t;
                        hit_mob = id;
                    }
                }
                for (const auto& [id, nn] : net.npcs()) {
                    const float base_y = nn.wy();
                    float half = client::WorldPick::kNpcHalfExtent;
                    float height = client::WorldPick::kNpcBaseHeight;
                    npc_pick_size(nn, half, height);
                    float hit_t = 0.0f;
                    if (!client::WorldPick::pick_monster_box(cam,
                                                             local_player,
                                                             input.mouse_x,
                                                             input.mouse_y,
                                                             static_cast<float>(input.width),
                                                             static_cast<float>(input.height),
                                                             nn.wx(),
                                                             base_y,
                                                             nn.wz(),
                                                             half,
                                                             height,
                                                             hit_t)) {
                        continue;
                    }
                    if (hit_npc == 0 || hit_t < best_npc) {
                        best_npc = hit_t;
                        hit_npc = id;
                    }
                }
                hit_player = remote_players.pick(cam,
                                                 local_player,
                                                 input.mouse_x,
                                                 input.mouse_y,
                                                 static_cast<float>(input.width),
                                                 static_cast<float>(input.height),
                                                 best_player);
                float best_item_screen = 0.0f;
                const auto eye = cam.eye(local_player);
                for (const auto& [id, gi] : net.ground_items()) {
                    const auto position = ground_item_position(gi);
                    auto center = position;
                    center.y += 4.0f;
                    const int mesh_no =
                        client::DropItemVisual::mesh_number(gi.index, gi.count, &item_catalog);
                    const auto model = model_cache.find(
                        client::DropItemVisual::gb_path(profile.data_root(), mesh_no));
                    const bool has_bounds = model != model_cache.end() && model->second.has_bounds;
                    if (has_bounds) {
                        const auto& bounds = model->second;
                        center = {position.x + (bounds.min_x + bounds.max_x) * 0.5f,
                                  position.y + (bounds.min_y + bounds.max_y) * 0.5f,
                                  position.z + (bounds.min_z + bounds.max_z) * 0.5f};
                    }
                    float sx = 0.0f, sy = 0.0f;
                    const auto project = [&](const engine::Vec3& point, float& x, float& y) {
                        return client::WorldPick::project_world_to_screen(
                            cam,
                            local_player,
                            point.x,
                            point.y,
                            point.z,
                            static_cast<float>(input.width),
                            static_cast<float>(input.height),
                            x,
                            y);
                    };
                    if (!project(center, sx, sy)) {
                        continue;
                    }
                    const float scale = std::max(1.0f, game_hud.scale());
                    float radius = 42.0f * scale;
                    if (has_bounds) {
                        const auto& bounds = model->second;
                        for (int corner = 0; corner < 8; ++corner) {
                            const engine::Vec3 point{
                                position.x + ((corner & 1) ? bounds.max_x : bounds.min_x),
                                position.y + ((corner & 2) ? bounds.max_y : bounds.min_y),
                                position.z + ((corner & 4) ? bounds.max_z : bounds.min_z)};
                            float x = 0.0f, y = 0.0f;
                            if (project(point, x, y)) {
                                radius =
                                    std::max(radius, std::hypot(x - sx, y - sy) + 8.0f * scale);
                            }
                        }
                    }
                    const float dx = input.mouse_x - sx;
                    const float dy = input.mouse_y - sy;
                    const float d2 = dx * dx + dy * dy;
                    if (d2 <= radius * radius && (hit_item == 0 || d2 < best_item_screen)) {
                        best_item_screen = d2;
                        // Compare eye distances with actor ray hits, never squared pixels.
                        best_item = std::sqrt((center.x - eye.x) * (center.x - eye.x) +
                                              (center.y - eye.y) * (center.y - eye.y) +
                                              (center.z - eye.z) * (center.z - eye.z));
                        hit_item = id;
                    }
                }
            }
            return {hit_mob, best, hit_item, best_item, hit_npc, best_npc, hit_player, best_player};
        };

        if (behead_animation_active) {
            client::mute_gameplay_input(input);
            input.mouse_left_clicked = false;
            input.mouse_right_clicked = false;
        }
        if (input.mouse_left_clicked && !ui_ate_click && queued_skill_index == 1) {
            clear_queued_target_skill();
            local_player.clear_click_target();
        }
        if (input.mouse_left_clicked && !ui_ate_click && !input.mouse_right && !cam.fly_mode() &&
            range_skill_cast == 1 && range_skill_index >= 0) {
            (void) lock_range_skill_at_cursor(
                input.mouse_x, input.mouse_y, input.width, input.height);
            ui_ate_click = true;
        } else if (input.mouse_left_clicked && !ui_ate_click && !input.mouse_right &&
                   !cam.fly_mode()) {
            // Select nearest monster under cursor (XZ distance to ground pick).
            float gx = 0, gz = 0;
            const bool got = client::WorldPick::pick_ground(cam,
                                                            local_player,
                                                            floor,
                                                            input.mouse_x,
                                                            input.mouse_y,
                                                            static_cast<float>(input.width),
                                                            static_cast<float>(input.height),
                                                            gx,
                                                            gz);
            const auto [hit_mob,
                        best,
                        hit_item,
                        best_item,
                        hit_npc,
                        best_npc,
                        hit_player,
                        best_player] = pick_world_targets();
            const bool prefer_player = hit_player != 0 && (hit_mob == 0 || best_player <= best) &&
                                       (hit_npc == 0 || best_player <= best_npc) &&
                                       (hit_item == 0 || best_player <= best_item);
            const bool prefer_npc = !prefer_player && hit_npc != 0 &&
                                    (hit_mob == 0 || best_npc <= best) &&
                                    (hit_item == 0 || best_npc <= best_item);
            if (is_focus_shot_charging()) {
                if (prefer_player) {
                    selected_player_id = hit_player;
                    selected_mob_id = 0;
                    selected_npc_id = 0;
                    selected_item_id = 0;
                    pending_pickup_id = 0;
                    pending_ask_npc_id = 0;
                    (void) try_release_focus_shot(hit_player);
                } else if (hit_mob != 0 && !prefer_npc) {
                    selected_mob_id = hit_mob;
                    selected_player_id = 0;
                    selected_npc_id = 0;
                    selected_item_id = 0;
                    pending_pickup_id = 0;
                    pending_ask_npc_id = 0;
                    auto mit = net.monsters().find(hit_mob);
                    if (mit != net.monsters().end() &&
                        protocol::MonsterRaceInfo::is_openable(mit->second.race())) {
                        clear_pending_skill();
                    } else {
                        (void) try_release_focus_shot(hit_mob);
                    }
                } else {
                    clear_pending_skill();
                }
                ui_ate_click = true;
            } else if (prefer_player) {
                if (player_actor.attack_locked()) {
                    player_actor.cancel_attack();
                }
                const bool enemy = net.castle_enemy(hit_player);
                if (!enemy && social_mode == client::SocialAction::Whisper) {
                    if (const auto* target = remote_players.find(hit_player)) {
                        game_hud.chat().begin_whisper(target->name());
                    }
                }
                selected_player_id = hit_player;
                selected_mob_id = 0;
                selected_npc_id = 0;
                selected_item_id = 0;
                pending_pickup_id = 0;
                pending_ask_npc_id = 0;
                auto_attack = enemy && !net.castle_defeated(hit_player) &&
                              player_wear.has_weapon() && !local_player.dead() &&
                              social_mode == client::SocialAction::None;
                auto_skill = false;
                local_player.clear_click_target();
                if (auto_attack) {
                    abort_pending_skill_for_basic_attack();
                    player_actor.end_skill();
                }
            } else if (prefer_npc) {
                if (player_actor.attack_locked()) {
                    player_actor.cancel_attack();
                }
                selected_mob_id = 0;
                selected_npc_id = hit_npc;
                selected_player_id = 0;
                selected_item_id = 0;
                pending_pickup_id = 0;
                auto_attack = false;
                auto nit = net.npcs().find(hit_npc);
                if (nit != net.npcs().end()) {
                    if (in_client_range(nit->second.wx(), nit->second.wz(), kNpcTalkEngageRange) &&
                        net.connected()) {
                        pending_ask_npc_id = 0;
                        local_player.clear_click_target();
                        (void) net.send_ask_npc(hit_npc);
                    } else {
                        pending_ask_npc_id = hit_npc;
                        local_player.set_click_target(nit->second.wx(), nit->second.wz());
                    }
                }
            } else if (hit_item != 0 && (hit_mob == 0 || best_item <= best)) {
                (void) begin_pickup(hit_item);
            } else if (hit_mob != 0) {
                // LMB on a mob: select, walk in, and start auto-attack (same loop as
                // Space). Treasure boxes use GETITEMMONSTER instead of attack.
                if (player_actor.attack_locked()) {
                    player_actor.cancel_attack();
                }
                selected_mob_id = hit_mob;
                selected_npc_id = 0;
                selected_player_id = 0;
                selected_item_id = 0;
                pending_pickup_id = 0;
                pending_ask_npc_id = 0;
                auto mit = net.monsters().find(hit_mob);
                if (mit != net.monsters().end() &&
                    protocol::MonsterRaceInfo::is_openable(mit->second.race())) {
                    auto_attack = true;  // reuse chase loop; try_basic_attack opens the chest
                    auto_skill = false;
                    abort_pending_skill_for_basic_attack();
                    player_actor.end_skill();
                    local_player.set_click_target(mit->second.wx(), mit->second.wz());
                } else {
                    auto_attack = mit != net.monsters().end() && mit->second.hp() > 0;
                    auto_skill = false;
                    abort_pending_skill_for_basic_attack();
                    player_actor.end_skill();
                    if (mit != net.monsters().end()) {
                        // SendAttack TRUE in range → no SetMove. Walking here during a
                        // skill delay made the archer run into the mob instead of shooting.
                        approach_for_basic_attack(mit->second.wx(), mit->second.wz());
                    }
                }
            } else if (got) {
                const int active_skill = player_skills.active_index();
                const auto active_kind =
                    active_skill >= 0
                        ? client::Catalog::cast_kind_for(skill_win.player_clazz(), active_skill)
                        : client::CastKind::None;
                const auto* ground_def =
                    active_skill >= 0 ? skill_catalog.find(skill_win.player_clazz(), active_skill)
                                      : nullptr;
                const int ground_level =
                    active_skill >= 0 ? std::max(1, player_skills.level_of(active_skill)) : 1;
                const int ground_mp = client::Catalog::skill_spend_mp(
                    skill_win.player_clazz(), ground_def, ground_level, skill_win.player_level());
                if (active_kind == client::CastKind::GroundPosition && ground_def &&
                    !client::Catalog::uses_range_skill_selector(skill_win.player_clazz(),
                                                                active_skill) &&
                    player_skills.is_learned(active_skill) && player_wear.has_weapon() &&
                    pending_skill_kind == PendingSkillKind::None && attack_cooldown <= 0.0f &&
                    !skill_win.on_cooldown(active_skill) && player_mp >= ground_mp) {
                    bool bow_ok = true;
                    if (skill_win.player_clazz() == 2 &&
                        protocol::archer_item_requirement(active_skill) !=
                            protocol::ArcherItemRequirement::None) {
                        const client::ItemDef* weapon =
                            item_catalog.find(player_wear.weapon_index());
                        bow_ok = weapon != nullptr && weapon->class_small() == "bow";
                    }
                    const int ground_x = coords::to_server(gx, profile.origin_patch());
                    const int ground_y = coords::to_server(gz, profile.origin_patch());
                    const int map_cells = map::MapStream::ensure_around(
                        walk_grid, server_map_dir, ground_x, ground_y, 1);
                    const bool valid_ground =
                        map_cells > 0 &&
                        (uses_indoor_visuals()
                             ? (floor.get_floor(gx, gz) > 0.0f)
                             : client::LocalPlayer::may_stand_at(gx,
                                                                 gz,
                                                                 local_player.pos().y,
                                                                 floor,
                                                                 &walk_grid,
                                                                 profile.origin_patch()));
                    const int ground_range = ground_skill_range(ground_def, ground_level);
                    if (bow_ok && valid_ground && in_client_range(gx, gz, ground_range)) {
                        pending_skill_ground_x = ground_x;
                        pending_skill_ground_y = ground_y;
                        if (begin_skill_cast(PendingSkillKind::Ground,
                                             static_cast<std::uint8_t>(active_skill),
                                             0,
                                             ground_def)) {
                            selected_mob_id = 0;
                            selected_npc_id = 0;
                            selected_player_id = 0;
                            selected_item_id = 0;
                            auto_attack = false;
                            local_player.clear_click_target();
                            click_marker.show(
                                gx, floor.sample(gx, gz, local_player.pos().y) + 0.15f, gz);
                        }
                    }
                } else {
                    if (player_actor.attack_locked()) {
                        player_actor.cancel_attack();
                    }
                    selected_mob_id = 0;
                    selected_npc_id = 0;
                    selected_player_id = 0;
                    selected_item_id = 0;
                    pending_pickup_id = 0;
                    pending_ask_npc_id = 0;
                    auto_attack = false;
                    if (opening_chest_id != 0 || pending_chest_id != 0 ||
                        player_actor.opening_box()) {
                        cancel_open_chest("ground_click");
                    }
                    if (pending_skill_kind == PendingSkillKind::Melee &&
                        local_player.clazz() == client::Class::Knight &&
                        pending_skill_index == 21) {
                        clear_pending_skill();
                    }
                    const int click_sx = coords::to_server(gx, profile.origin_patch());
                    const int click_sy = coords::to_server(gz, profile.origin_patch());
                    const int map_cells = map::MapStream::ensure_around(
                        walk_grid, server_map_dir, click_sx, click_sy, 1);
                    if (map_cells <= 0) {
                        log::warn("map stream ensure_around loaded no walk cells");
                    }
                    const float floor_y = uses_indoor_visuals()
                                              ? floor.get_floor(gx, gz)
                                              : floor.sample(gx, gz, local_player.pos().y);
                    const bool stand_ok =
                        uses_indoor_visuals()
                            ? (floor_y > 0.0f)
                            : client::LocalPlayer::may_stand_at(gx,
                                                                gz,
                                                                local_player.pos().y,
                                                                floor,
                                                                &walk_grid,
                                                                profile.origin_patch());
                    if (stand_ok) {
                        local_player.set_click_target(gx, gz);
                        click_marker.show(gx, floor_y + 0.15f, gz);
                    }
                }
            }
        }

        // Splashy Ice must be interrupted before this frame can finish its wind-up.
        const bool walking_input =
            !chat_focused && (input.key_w || input.key_a || input.key_s || input.key_d);
        const bool moved_during_splashy = pending_skill_kind == PendingSkillKind::SelfArea &&
                                          pending_skill_index == 24 &&
                                          (local_player.pos().x != pending_skill_start_position.x ||
                                           local_player.pos().z != pending_skill_start_position.z);
        if (local_player.clazz() == client::Class::Magician &&
            (walking_input || local_player.has_click_target() || moved_during_splashy)) {
            if (pending_skill_kind == PendingSkillKind::SelfArea && pending_skill_index == 24) {
                abort_pending_skill_for_basic_attack();
            }
            if (queued_skill_index == 24) {
                clear_queued_target_skill();
            }
        }

        // WASD: stand first (ModelChange away from action 5 sends Rest
        // off), then cancel auto-attack (don't chase back while walking away).
        if (walking_input && !local_player.dead() && !behead_animation_active) {
            if (queued_skill_index == 1) {
                clear_queued_target_skill();
                local_player.clear_click_target();
            }
            (void) request_stand();
            if (want_net && !local_player.resting()) {
                if (opening_chest_id != 0 || pending_chest_id != 0 || player_actor.opening_box()) {
                    cancel_open_chest("wasd");
                }
                if (player_actor.attack_locked()) {
                    player_actor.cancel_attack();
                }
                if (pending_skill_kind == PendingSkillKind::Melee &&
                    local_player.clazz() == client::Class::Knight && pending_skill_index == 21) {
                    clear_pending_skill();
                    local_player.clear_click_target();
                }
                // Keeps runtime state across movement and
                // continues GO_SKILL until fire. Dropping the queue or aborting
                // Pain / Sight Interruption / Buff Remover PRESKILL on WASD made
                // Imperial Commander one-shots look unused while the icon was ready.
                const bool keep_archer_preskill =
                    pending_skill_kind == PendingSkillKind::Archer && pending_skill_left > 0.0f &&
                    protocol::archer_special_kind(static_cast<int>(pending_skill_index)) !=
                        protocol::ArcherSpecialKind::FocusShot;
                if (auto_attack) {
                    auto_attack = false;
                    pending_arrow_target = 0;
                    if (!keep_archer_preskill) {
                        const int q_index = queued_skill_index;
                        const std::uint32_t q_target = queued_skill_target;
                        const bool q_player = queued_skill_is_player;
                        clear_pending_skill();
                        queued_skill_index = q_index;
                        queued_skill_target = q_target;
                        queued_skill_is_player = q_player;
                    }
                    local_player.clear_click_target();
                }
                if (!net.castle_enemy(selected_player_id)) {
                    const std::uint32_t nearest = find_nearest_mob_id();
                    if (nearest != 0) {
                        selected_mob_id = nearest;
                        selected_player_id = 0;
                        selected_npc_id = 0;
                    }
                }
            }
        }

        // Space: hold to keep looting pouches; tap with no pouches attacks (no
        // hold-to-attack-next). Chat focus already muted Space via mute_gameplay_keys.
        if (want_net && net.connected() && !chat_focused && !cam.fly_mode() &&
            !local_player.dead() && (input.key_space || input.key_space_pressed)) {
            if (local_player.resting() && !request_stand()) {
                // Stay seated if the rest-off packet could not be sent.
            } else if (!local_player.resting()) {
                auto find_nearest_item_id = [&]() -> std::uint32_t {
                    std::uint32_t nearest_item = 0;
                    float best = 0.0f;
                    const float max_r = static_cast<float>(auto_engage_range());
                    const float max_r2 = max_r * max_r;
                    for (const auto& [id, gi] : net.ground_items()) {
                        const float dx = gi.wx - local_player.pos().x;
                        const float dz = gi.wz - local_player.pos().z;
                        const float d2 = dx * dx + dz * dz;
                        if (d2 <= max_r2 && (nearest_item == 0 || d2 < best)) {
                            best = d2;
                            nearest_item = id;
                        }
                    }
                    return nearest_item;
                };

                const std::uint32_t nearest_item = find_nearest_item_id();
                const bool have_loot =
                    pending_pickup_id != 0 || selected_item_id != 0 || nearest_item != 0;

                if (have_loot && input.key_space) {
                    // Hold Space: chain-pick every pouch; never fall through to
                    // attacking.
                    if (pending_pickup_id == 0) {
                        const std::uint32_t target =
                            (selected_item_id != 0) ? selected_item_id : nearest_item;
                        if (target != 0) {
                            begin_pickup(target);
                        }
                    }
                } else if (input.key_space_pressed && net.castle_enemy(selected_player_id)) {
                    if (player_wear.has_weapon() && !net.castle_defeated(selected_player_id)) {
                        auto_attack = true;
                        auto_skill = false;
                        abort_pending_skill_for_basic_attack();
                        player_actor.end_skill();
                    }
                } else if (input.key_space_pressed &&
                           (player_wear.has_weapon() || (selected_mob_id != 0 && [&]() {
                                auto mit = net.monsters().find(selected_mob_id);
                                return mit != net.monsters().end() &&
                                       protocol::MonsterRaceInfo::is_openable(mit->second.race());
                            }()))) {
                    // Tap Space with no pouches in range: attack current/nearest mob
                    // (starts auto-attack). Chests may be opened without a weapon
                    // (GETITEMMONSTER).
                    if (selected_mob_id == 0) {
                        selected_mob_id = find_nearest_mob_id();
                    }
                    if (selected_mob_id != 0) {
                        selected_player_id = 0;
                        auto_attack = true;
                        auto_skill = false;
                        abort_pending_skill_for_basic_attack();
                        player_actor.end_skill();
                        if (attack_cooldown <= 0.0f &&
                            pending_skill_kind == PendingSkillKind::None) {
                            auto mit = net.monsters().find(selected_mob_id);
                            const bool chest =
                                mit != net.monsters().end() &&
                                protocol::MonsterRaceInfo::is_openable(mit->second.race());
                            if (chest) {
                                if (!try_open_chest(selected_mob_id)) {
                                    log::debug("space chest open not ready; will retry while "
                                               "approaching");
                                }
                            } else if (!try_basic_attack(selected_mob_id)) {
                                log::debug("space basic attack not ready; will retry while "
                                           "auto-attacking");
                            }
                        }
                    }
                }
            }
        }
        if (want_net && net.connected() && !chat_focused && !local_player.dead() &&
            input.key_f_pressed) {
            if (local_player.resting() && !request_stand()) {
                // Stay seated if the rest-off packet could not be sent.
            } else if (!local_player.resting() && player_skills.active_index() >= 0) {
                const bool behead_slot =
                    client::Catalog::cast_kind_for(skill_win.player_clazz(),
                                                   player_skills.active_index()) ==
                    client::CastKind::Behead;
                if (behead_slot) {
                    if (!cast_skill_index(player_skills.active_index())) {
                        log::debug("f skill cast rejected");
                    }
                } else {
                    const bool archer_hostile =
                        local_player.clazz() == client::Class::Archer &&
                        protocol::archer_target_shape(player_skills.active_index()) ==
                            protocol::ArcherTargetShape::HostileCharacter;
                    if (net.castle_enemy(selected_player_id) ||
                        (archer_hostile && selected_player_id != 0 &&
                         selected_player_id != net.local_player_id())) {
                        if (!cast_skill_index(player_skills.active_index())) {
                            log::debug("f skill cast rejected");
                        }
                    } else {
                        if (selected_mob_id == 0) {
                            selected_mob_id = find_nearest_mob_id();
                        }
                        if (selected_mob_id != 0) {
                            selected_player_id = 0;
                            if (!cast_skill_index(player_skills.active_index())) {
                                log::debug("f skill cast rejected");
                            }
                        }
                    }
                }
            }
        }
        if (pending_skill_kind == PendingSkillKind::Melee &&
            local_player.clazz() == client::Class::Knight && pending_skill_index == 21) {
            rush_elapsed_sec += input.dt;
            float tx = 0.0f;
            float tz = 0.0f;
            bool valid = false;
            const auto monster = net.monsters().find(pending_skill_target);
            if (monster != net.monsters().end() && monster->second.hp() > 0) {
                tx = monster->second.wx();
                tz = monster->second.wz();
                valid = true;
            } else if (const auto* remote = remote_players.find(pending_skill_target)) {
                tx = remote->wx();
                tz = remote->wz();
                valid = true;
            }
            if (!valid || local_player.dead() || rush_elapsed_sec > 1.5f) {
                clear_pending_skill();
                auto_attack = false;
                auto_skill = false;
                local_player.clear_click_target();
            } else if (in_client_range(tx, tz, weapon_attack_range())) {
                local_player.clear_click_target();
                local_player.set_moving(false);
                net.flush_local_move(local_player.pos().x,
                                     local_player.pos().y,
                                     local_player.pos().z,
                                     profile.origin_patch());
                pending_skill_left = 0.0f;
            } else {
                local_player.set_click_target(tx, tz);
                // Rush's 1 ms compare value starts movement, not a timed ranged stun.
                pending_skill_left = input.dt + 1.0f;
            }
        }
        if (pending_skill_kind != PendingSkillKind::None) {
            if (is_focus_shot_charging() && pending_skill_left > 0.0f) {
                focus_shot_elapsed_sec += input.dt;
            }
            if (pending_skill_kind == PendingSkillKind::None) {
                pending_skill_left = 0.0f;
            } else if (!(is_focus_shot_charging() && pending_skill_left > 0.0f)) {
                if (!is_focus_shot_charging()) {
                    pending_skill_left = std::max(0.0f, pending_skill_left - input.dt);
                }
                if (pending_skill_left <= 0.0f) {
                    const PendingSkillKind kind = pending_skill_kind;
                    const std::uint8_t skill_index = pending_skill_index;
                    const std::uint32_t target = pending_skill_target;
                    const std::uint32_t item_iid = pending_skill_item_iid;
                    clear_pending_skill(false);
                    bool target_ok =
                        kind == PendingSkillKind::SelfHeal || kind == PendingSkillKind::SelfArea ||
                        kind == PendingSkillKind::Ground ||
                        (kind == PendingSkillKind::Animal && target == 0) ||
                        (kind == PendingSkillKind::Player && target == net.local_player_id());
                    if (!target_ok) {
                        auto mit = net.monsters().find(target);
                        if (mit == net.monsters().end()) {
                            target_ok = remote_players.find(target) != nullptr;
                        } else if (skill_index == 1) {
                            target_ok = mit->second.kneeling();
                        } else {
                            target_ok = mit->second.hp() > 0;
                        }
                    }
                    if (target_ok) {
                        // Re-check MP at fire time: PRESKILL may have started with enough MP,
                        // but the bar can hit 0 before C2S_SKILL. Drop the cast instead of
                        // letting a no-MP skill resolve and keep damaging.
                        bool mp_ok = true;
                        if (skill_index != 1) {
                            const int clazz = static_cast<int>(local_player.clazz());
                            const auto* def =
                                skill_catalog.find(clazz, static_cast<int>(skill_index));
                            const int sk_lv =
                                std::max(1, player_skills.level_of(static_cast<int>(skill_index)));
                            const int need_mp = client::Catalog::skill_spend_mp(
                                clazz, def, sk_lv, skill_win.player_level());
                            mp_ok = need_mp <= 0 || player_mp >= need_mp;
                        }
                        if (mp_ok && kind == PendingSkillKind::Archer && target != 0) {
                            float tx = 0.0f;
                            float tz = 0.0f;
                            bool player_target = false;
                            auto mit = net.monsters().find(target);
                            if (mit != net.monsters().end()) {
                                tx = mit->second.wx();
                                tz = mit->second.wz();
                            } else if (const client::RemotePlayer* remote =
                                           remote_players.find(target)) {
                                tx = remote->wx();
                                tz = remote->wz();
                                player_target = true;
                            } else {
                                mp_ok = false;
                            }
                            if (mp_ok) {
                                const auto* range_def =
                                    skill_catalog.find(2, static_cast<int>(skill_index));
                                const int sk_lv = std::max(
                                    1, player_skills.level_of(static_cast<int>(skill_index)));
                                const int range = client::Catalog::skill_cast_range(
                                    2, range_def, sk_lv, weapon_attack_range());
                                if (!in_client_range(tx, tz, range)) {
                                    queue_target_skill(skill_index, target, player_target);
                                    local_player.set_click_target(tx, tz);
                                    player_actor.end_skill();
                                    mp_ok = false;
                                }
                            } else {
                                player_actor.end_skill();
                            }
                        }
                        if (mp_ok) {
                            if (!fire_skill_now(kind, skill_index, target, item_iid) &&
                                kind == PendingSkillKind::Archer) {
                                player_actor.end_skill();
                            }
                        } else if (kind == PendingSkillKind::Archer) {
                            player_actor.end_skill();
                        } else {
                            player_actor.end_skill();
                            player_actor.cancel_attack();
                        }
                    } else if (kind == PendingSkillKind::Archer) {
                        player_actor.end_skill();
                    }
                }
            }
        }
        if (attack_cooldown > 0.0f) {
            attack_cooldown = std::max(0.0f, attack_cooldown - input.dt);
        }
        if (queued_skill_index >= 0) {
            flush_queued_target_skill();
        }
        item_cooldowns.advance(input.dt);
        if (chest_open_retry > 0.0f) {
            chest_open_retry = std::max(0.0f, chest_open_retry - input.dt);
        }
        if (!local_player.dead() && skill_bar.auto_hp_needed(player_hp, player_max_hp)) {
            (void) try_use_item(skill_bar.auto_hp_item_index(),
                                "auto-hp",
                                static_cast<std::uint8_t>(skill_bar.auto_hp_percent()));
        }

        if (auto_attack && selected_player_id != 0 && want_net && net.connected() &&
            !local_player.dead() && net.castle_side(net.local_player_id()) != 0) {
            const auto* target = remote_players.find(selected_player_id);
            if (!target || !net.castle_enemy(selected_player_id) ||
                net.castle_defeated(selected_player_id) || !player_wear.has_weapon() ||
                !in_client_range(target->wx(), target->wz(), kAutoChaseRange)) {
                auto_attack = false;
                auto_skill = false;
                player_actor.cancel_attack();
                local_player.clear_click_target();
            } else if ((!local_player.resting() || request_stand()) && queued_skill_index < 0 &&
                       pending_skill_kind == PendingSkillKind::None) {
                if (auto_skill && local_player.clazz() == client::Class::Magician) {
                    const int skill = player_skills.active_index();
                    const auto* def = skill_catalog.find(1, skill);
                    if (!def || !def->attack_auto() || player_mp <= 0) {
                        auto_attack = false;
                        auto_skill = false;
                    } else {
                        const int level = std::max(1, player_skills.level_of(skill));
                        if (in_client_range(target->wx(),
                                            target->wz(),
                                            client::Catalog::magic_skill_range(skill, level))) {
                            local_player.clear_click_target();
                            (void) try_mage_skill(static_cast<std::uint8_t>(skill),
                                                  selected_player_id);
                        } else {
                            local_player.set_click_target(target->wx(), target->wz());
                        }
                    }
                } else if (!auto_skill) {
                    approach_for_basic_attack(target->wx(), target->wz());
                    if (in_weapon_engage_range(target->wx(), target->wz())) {
                        (void) try_basic_attack(selected_player_id, true);
                    }
                }
            }
        }

        // Continuous auto-attack until the mob dies, we cancel, or (mage) MP is
        // empty. Treasure boxes: walk in and GETITEMMONSTER (no weapon required).
        if (auto_attack && selected_mob_id != 0 && want_net && net.connected() &&
            !local_player.dead()) {
            if (local_player.resting()) {
                (void) request_stand();
            }
            if (local_player.resting()) {
                // Rest-off did not apply this frame.
            } else {
                auto mit = net.monsters().find(selected_mob_id);
                if (mit == net.monsters().end()) {
                    auto_attack = false;
                    selected_mob_id = 0;
                    pending_arrow_target = 0;
                    clear_pending_skill();
                } else if (mit->second.hp() <= 0) {
                    auto_attack = false;
                    pending_arrow_target = 0;
                    clear_pending_skill();
                    if (!mit->second.kneeling()) {
                        selected_mob_id = 0;
                    }
                } else if (protocol::MonsterRaceInfo::is_openable(mit->second.race())) {
                    // true = opening / sent / waiting — stand still (MoveOn cancels
                    // compatibility behavior). false = still need to walk in.
                    if (try_open_chest(selected_mob_id)) {
                        local_player.clear_click_target();
                    } else if (!in_client_range(
                                   mit->second.wx(), mit->second.wz(), kAutoChaseRange)) {
                        auto_attack = false;
                        local_player.clear_click_target();
                    } else {
                        local_player.set_click_target(mit->second.wx(), mit->second.wz());
                    }
                } else if (player_wear.has_weapon() && attack_cooldown <= 0.0f &&
                           pending_skill_kind == PendingSkillKind::None) {
                    if (queued_skill_index >= 0) {
                        // Runtime state: do not let C2S_ATTACK eat the
                        // queued one-shot (Life Absorption) while walking into skill range.
                        if (!in_client_range(mit->second.wx(), mit->second.wz(), kAutoChaseRange)) {
                            auto_attack = false;
                            clear_queued_target_skill();
                            player_actor.cancel_attack();
                            local_player.clear_click_target();
                        }
                    } else if (!in_client_range(
                                   mit->second.wx(), mit->second.wz(), kAutoChaseRange)) {
                        // Drop the chase if the target flees beyond a long leash (LMB can
                        // start from farther than the short WASD/Space engage radius).
                        auto_attack = false;
                        player_actor.cancel_attack();
                        local_player.clear_click_target();
                    } else if (auto_skill && local_player.clazz() == client::Class::Magician) {
                        if (player_mp <= 0) {
                            auto_attack = false;
                            auto_skill = false;
                        } else {
                            int sk = player_skills.active_index() >= 0
                                         ? player_skills.active_index()
                                         : 4;
                            const auto* sk_def = skill_catalog.find(1, sk);
                            if (sk_def && !sk_def->attack_auto() &&
                                client::Catalog::is_mage_combat_skill(sk)) {
                                // Hermit one-shots (Thunder, Ice Restriction, …): never
                                // recast and never substitute Lightning Magic.
                                auto_skill = false;
                                auto_attack = false;
                            } else if (!client::Catalog::is_mage_combat_skill(sk)) {
                                sk = 4;
                            }
                            if (auto_skill) {
                                if (skill_win.on_cooldown(sk)) {
                                    local_player.clear_click_target();
                                } else if (!try_mage_skill(static_cast<std::uint8_t>(sk),
                                                           selected_mob_id)) {
                                    player_actor.cancel_attack();
                                    local_player.set_click_target(mit->second.wx(),
                                                                  mit->second.wz());
                                } else {
                                    local_player.clear_click_target();
                                }
                            }
                        }
                    } else if (auto_skill && local_player.clazz() == client::Class::Archer) {
                        const int sk = player_skills.active_index();
                        const auto* adef = (sk >= 0) ? skill_catalog.find(2, sk) : nullptr;
                        const bool want_skill = sk >= 0 &&
                                                client::Catalog::is_archer_combat_skill(sk) &&
                                                player_skills.is_learned(sk);
                        const int sk_lv = std::max(1, player_skills.level_of(sk));
                        const int spend_mp = client::Catalog::skill_spend_mp(
                            2, adef, sk_lv, skill_win.player_level());
                        const bool need_mp = spend_mp > 0;
                        const bool have_mp = !need_mp || player_mp >= spend_mp;
                        if (!want_skill || (need_mp && player_mp <= 0)) {
                            auto_skill = false;
                            if (!try_basic_attack(selected_mob_id)) {
                                player_actor.cancel_attack();
                                approach_for_basic_attack(mit->second.wx(), mit->second.wz());
                            } else {
                                local_player.clear_click_target();
                            }
                        } else if (skill_win.on_cooldown(sk)) {
                            local_player.clear_click_target();
                        } else if (!have_mp) {
                            if (!try_basic_attack(selected_mob_id)) {
                                player_actor.cancel_attack();
                                approach_for_basic_attack(mit->second.wx(), mit->second.wz());
                            } else {
                                local_player.clear_click_target();
                            }
                            auto_skill = true;
                        } else {
                            const bool sent =
                                try_archer_skill(static_cast<std::uint8_t>(sk), selected_mob_id);
                            if (!sent) {
                                player_actor.cancel_attack();
                                local_player.set_click_target(mit->second.wx(), mit->second.wz());
                            } else {
                                local_player.clear_click_target();
                            }
                        }
                    } else if (auto_skill && (local_player.clazz() == client::Class::Knight ||
                                              local_player.clazz() == client::Class::Thief)) {
                        const int sk = player_skills.active_index();
                        const int clazz = static_cast<int>(local_player.clazz());
                        const auto* mdef = (sk >= 0) ? skill_catalog.find(clazz, sk) : nullptr;
                        const bool taunt = client::Catalog::is_taunt_skill(clazz, sk);
                        const bool combat_skill = client::Catalog::is_melee_combat_skill(clazz, sk);
                        // Behead / rest / boost must not ride the combat auto-recast loop.
                        const bool want_skill =
                            sk >= 0 && player_skills.is_learned(sk) && (combat_skill || taunt);
                        const int sk_lv = std::max(1, player_skills.level_of(sk));
                        const int spend_mp = client::Catalog::skill_spend_mp(
                            clazz, mdef, sk_lv, skill_win.player_level());
                        const bool need_mp = spend_mp > 0;
                        const bool have_mp = !need_mp || player_mp >= spend_mp;
                        if (need_mp && player_mp <= 0) {
                            // Match mage: skill auto-combat ends when MP is empty. Abort any
                            // in-flight PRESKILL so a late C2S_SKILL cannot keep dealing
                            // damage after the attack anims have stopped.
                            auto_attack = false;
                            auto_skill = false;
                            clear_pending_skill(true);
                            player_actor.cancel_attack();
                            local_player.clear_click_target();
                        } else if (!want_skill) {
                            // Active skill is not a combat/taunt skill (e.g. Behead selected).
                            // Drop skill auto-recast; keep chasing with basic attacks only.
                            auto_skill = false;
                            if (!try_basic_attack(selected_mob_id)) {
                                player_actor.cancel_attack();
                                local_player.set_click_target(mit->second.wx(), mit->second.wz());
                            } else {
                                local_player.clear_click_target();
                            }
                        } else if (skill_win.on_cooldown(sk)) {
                            local_player.clear_click_target();
                        } else if (!have_mp) {
                            // Not enough MP for this cast yet — wait (no basic fallback).
                            player_actor.cancel_attack();
                            local_player.clear_click_target();
                        } else {
                            const bool sent =
                                try_melee_skill(static_cast<std::uint8_t>(sk), selected_mob_id);
                            if (!sent) {
                                player_actor.cancel_attack();
                                local_player.set_click_target(mit->second.wx(), mit->second.wz());
                            } else {
                                local_player.clear_click_target();
                            }
                        }
                    } else if (!try_basic_attack(selected_mob_id)) {
                        player_actor.cancel_attack();
                        approach_for_basic_attack(mit->second.wx(), mit->second.wz());
                    } else {
                        local_player.clear_click_target();
                    }
                } else if (player_wear.has_weapon()) {
                    // Weapon delay / PRESKILL: SendAttack still returns TRUE
                    // in range, so the archer must not walk in while waiting to shoot.
                    if (in_weapon_engage_range(mit->second.wx(), mit->second.wz())) {
                        local_player.clear_click_target();
                    } else if (!in_client_range(
                                   mit->second.wx(), mit->second.wz(), kAutoChaseRange)) {
                        auto_attack = false;
                        player_actor.cancel_attack();
                        local_player.clear_click_target();
                    }
                }
            }
        }

        skill_win.tick(input.dt);
        enchant_overlay.tick(input.dt);
        riding_cache_overlay.tick(input.dt);
        if (enchant_overlay.take_charge_complete()) {
            if (enchant_overlay.mix_charge()) {
                if (want_net && net.connected()) {
                    if (!net.send_mixing(enchant_overlay.mix_mode(),
                                         enchant_overlay.mix_weapon_iid(),
                                         enchant_overlay.mix_extra_weapon_iid(),
                                         enchant_overlay.mix_armor_iid(),
                                         enchant_overlay.mix_stone1_iid(),
                                         enchant_overlay.mix_stone2_iid())) {
                        log::warn("failed to send mixing");
                    }
                }
                mix_win.set_open(false, mix_win.mode());
                in_game_window_order.closed(client::InGameWindowId::Mixing);
            } else if (enchant_overlay.upgrade_charge()) {
                const auto type = enchant_overlay.upgrade_rate_up()
                                      ? protocol::UpgradeItemType::RateUp
                                      : protocol::UpgradeItemType::Level;
                if (want_net && net.connected()) {
                    if (!net.send_upgrade_item(type, enchant_overlay.charge_to_id())) {
                        log::warn("failed to send upgrade item");
                    }
                } else {
                    apply_local_upgrade(enchant_overlay.upgrade_rate_up());
                }
            } else if (want_net && net.connected()) {
                if (!net.send_enchant_item(enchant_overlay.charge_from_id(),
                                           enchant_overlay.charge_to_id())) {
                    log::warn("failed to send enchant item");
                }
            } else {
                apply_local_enchant();
            }
        }

        {
            engine::FrameInput look = read_only_session ? observer_look : input;
            if (ui_ate_scroll) {
                look.mouse_scroll = 0;
            }
            if (inv_win.open() && inv_win.contains(input.mouse_x, input.mouse_y)) {
                look.mouse_right = false;
                look.mouse_right_clicked = false;
            }
            if (char_info_win.open() && char_info_win.contains(input.mouse_x, input.mouse_y)) {
                look.mouse_right = false;
                look.mouse_right_clicked = false;
            }
            if (rmb_skill_consumed ||
                riding_hit_at(input.mouse_x, input.mouse_y, game_hud.scale()).kind() !=
                    client::UiHitKind::None) {
                look.mouse_right = false;
                look.mouse_right_clicked = false;
            }
            if (!cam.fly_mode()) {
                cam.update_look(look);
            }
        }
        engine::FrameInput move_input = input;
        if (chat_focused) {
            move_input.key_w = false;
            move_input.key_a = false;
            move_input.key_s = false;
            move_input.key_d = false;
            move_input.key_space = false;
            move_input.key_space_pressed = false;
            move_input.key_ctrl = false;
            move_input.key_shift = false;
            move_input.mouse_dx = 0.0f;
            move_input.mouse_dy = 0.0f;
        }
        if (fishing_active || cooking_active) {
            move_input.key_w = false;
            move_input.key_a = false;
            move_input.key_s = false;
            move_input.key_d = false;
            local_player.clear_click_target();
            local_player.set_moving(false);
        }
        if (local_player.resting() &&
            ((move_input.key_w || move_input.key_a || move_input.key_s || move_input.key_d) ||
             local_player.has_click_target())) {
            (void) request_stand();
        }
        bool haste_active = false;
        int item_haste_percent = 0;
        bool escape_active = false;
        bool sharpness_active = false;
        bool hiding_active = false;
        const std::uint64_t movement_now_ms =
            static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                           std::chrono::steady_clock::now().time_since_epoch())
                                           .count());
        for (const auto& buff : net.buff_slots()) {
            if (buff.expire_ms != 0 && buff.expire_ms <= movement_now_ms) {
                continue;
            }
            if ((buff.bstate & protocol::kCbsSkillHaste) != 0) {
                if (buff.source_item_index > 0) {
                    const client::ItemDef* haste_item = item_catalog.find(buff.source_item_index);
                    if (haste_item != nullptr &&
                        haste_item->buff_type() == static_cast<int>(protocol::BuffType::Haste)) {
                        item_haste_percent = haste_item->buff_amount();
                    }
                } else {
                    haste_active = true;
                }
            }
            if ((buff.bstate & protocol::kCbsSkillEscape) != 0) {
                escape_active = true;
            }
            if ((buff.bstate & protocol::kCbsSkillArcherSharpness) != 0) {
                sharpness_active = true;
            }
            if ((buff.bstate & protocol::kCbsSkillHiding) != 0) {
                hiding_active = true;
            }
        }
        const int haste_level = haste_active ? player_skills.level_of(26) : 0;
        int move_pct = item_haste_percent > 0 ? item_haste_percent : 15 * haste_level;
        if (item_haste_percent > 0 && !item_haste_fx_active) {
            combat_fx.spawn_clip_model(*engine,
                                       "haste",
                                       local_player.pos().x,
                                       local_player.pos().y,
                                       local_player.pos().z,
                                       local_player.yaw(),
                                       0.0f,
                                       0.0f,
                                       net.local_player_id());
        }
        item_haste_fx_active = item_haste_percent > 0;
        if (item_haste_percent > 0 && local_player.boosting()) {
            local_player.set_boosting(false);
        }
        if (escape_active) {
            move_pct += protocol::kArcherEscapeMoveSpeedPercent;
        }
        if (sharpness_active) {
            move_pct += protocol::kArcherSharpnessMoveSpeedPercent;
        }
        if (hiding_active) {
            move_pct += protocol::kThiefHidingMoveSpeedPercent;
        }
        move_pct += static_cast<int>(net.local_mspeed());
        if (pending_skill_kind == PendingSkillKind::Melee &&
            local_player.clazz() == client::Class::Knight && pending_skill_index == 21) {
            move_pct += 300;
        }
        const float move_scale =
            protocol::move_speed_factor(static_cast<std::int16_t>(std::clamp(move_pct, -99, 300)));
        local_player.set_walk_speed(client::kWalkSpeed * move_scale);
        local_player.set_run_speed(client::kRunSpeed * move_scale);
        if (read_only_session) {
            local_player.clear_click_target();
            local_player.set_moving(net.observed_moving());
        } else if (cam.fly_mode()) {
            cam.update_fly(move_input);
            const engine::Vec3 e = cam.eye(local_player);
            local_player.set_pos(e.x, e.y - cam.look_height(), e.z);
            local_player.set_yaw(cam.yaw_rad());
            local_player.set_moving(cam.fly_moved());
            local_player.clear_click_target();
        } else {
            local_player.update(move_input,
                                cam.yaw_rad(),
                                floor,
                                &walk_grid,
                                profile.origin_patch(),
                                behead_animation_active);
        }
        if (range_skill_cast != 0) {
            click_marker.clear();
        } else if (local_player.has_click_target() && selected_mob_id == 0 &&
                   selected_item_id == 0 && selected_npc_id == 0 && selected_player_id == 0) {
            click_marker.show(local_player.click_tx(),
                              floor.sample(local_player.click_tx(),
                                           local_player.click_tz(),
                                           local_player.pos().y) +
                                  0.15f,
                              local_player.click_tz());
        } else {
            click_marker.clear();
        }
        click_marker.tick(input.dt);
        expire_npc_dialog();
        if (local_player.dead() && range_skill_cast != 0) {
            reset_range_skill();
        } else if (range_skill_cast == 1) {
            float gx = 0.0f;
            float gz = 0.0f;
            if (client::WorldPick::pick_ground(cam,
                                               local_player,
                                               floor,
                                               input.mouse_x,
                                               input.mouse_y,
                                               static_cast<float>(input.width),
                                               static_cast<float>(input.height),
                                               gx,
                                               gz)) {
                const float gy = floor.sample(gx, gz, local_player.pos().y) + 0.15f;
                show_range_skill_marker(gx, gy, gz);
            }
        } else if (range_skill_cast == 2) {
            const float gy =
                floor.sample(range_skill_wx, range_skill_wz, local_player.pos().y) + 0.15f;
            show_range_skill_marker(range_skill_wx, gy, range_skill_wz);
            (void) try_fire_locked_range_skill();
        }
        range_skill_decal.tick(input.dt);
        quest_marker.tick(input.dt);
        select_marker.tick(input.dt);
        if (selected_mob_id != 0) {
            auto mit = net.monsters().find(selected_mob_id);
            if (mit != net.monsters().end() && is_clickable_mob(mit->second)) {
                const float my = floor.get_floor(mit->second.wx(), mit->second.wz());
                select_marker.show(mit->second.wx(), my + 0.12f, mit->second.wz());
            } else {
                select_marker.clear();
            }
        } else if (selected_npc_id != 0) {
            auto nit = net.npcs().find(selected_npc_id);
            if (nit != net.npcs().end()) {
                const float ny = nit->second.wy();
                select_marker.show(nit->second.wx(), ny + 0.12f, nit->second.wz());
            } else {
                selected_npc_id = 0;
                select_marker.clear();
            }
        } else if (selected_player_id != 0) {
            const client::RemotePlayer* remote = remote_players.find(selected_player_id);
            if (remote != nullptr) {
                select_marker.show(remote->wx(), remote->wy() + 0.12f, remote->wz());
            } else {
                selected_player_id = 0;
                select_marker.clear();
            }
        } else if (selected_item_id != 0) {
            auto iit = net.ground_items().find(selected_item_id);
            if (iit != net.ground_items().end()) {
                const float iy = floor.get_floor(iit->second.wx, iit->second.wz);
                select_marker.show(iit->second.wx, iy + 0.08f, iit->second.wz);
            } else {
                select_marker.clear();
            }
        } else {
            select_marker.clear();
        }
        {
            // Stream visual KCM/OPL patches and walk masks as the player (or fly
            // camera) crosses outdoor patch / area cell boundaries.
            const float wx = local_player.pos().x;
            const float wz = local_player.pos().z;
            if (!uses_indoor_visuals()) {
                (void) ensure_world_around(wx, wz, stream_request_batch);
            }
            const int sx = coords::to_server(wx, profile.origin_patch());
            const int sy = coords::to_server(wz, profile.origin_patch());
            const int map_cells =
                map::MapStream::ensure_around(walk_grid, server_map_dir, sx, sy, 1);
            if (map_cells <= 0) {
                log::warn("map stream ensure_around loaded no walk cells");
            }
        }
        if (pending_face) {
            const float dx = pending_face_x - local_player.pos().x;
            const float dz = pending_face_z - local_player.pos().z;
            if (dx * dx + dz * dz > 1e-4f) {
                local_player.set_yaw(std::atan2(-dx, -dz));
            }
            pending_face = false;
        }
        player_actor.update(*engine,
                            local_player.moving() ? input.dt * move_scale : input.dt,
                            local_player.moving(),
                            local_player.running(),
                            local_player.resting(),
                            net.entity_consuming(net.local_player_id()),
                            &sfx,
                            local_player.pos().x,
                            local_player.pos().y,
                            local_player.pos().z,
                            fishing_active,
                            local_riding_mounted);
        const client::ShinsuInfo* shinsu_prt = nullptr;
        if (player_actor.metamo_type() >= 0 && player_actor.metamo_level() >= 1) {
            shinsu_prt = shinsu_db.find(player_actor.metamo_type(), player_actor.metamo_level());
        }
        const auto shinsu_model = player_world_matrix();
        for (int i = 0; i < kShinsuPrtMax; ++i) {
            const std::uint32_t id = kShinsuPrtBase + static_cast<std::uint32_t>(i);
            if (!sprite_fx.has(id)) {
                continue;
            }
            float x = local_player.pos().x;
            float y = local_player.pos().y;
            float z = local_player.pos().z;
            if (shinsu_prt != nullptr && i < static_cast<int>(shinsu_prt->particles().size())) {
                const auto& prt = shinsu_prt->particles()[static_cast<std::size_t>(i)];
                const int skel = shinsu_db.skeleton_bone(shinsu_prt->bone(), prt.bone());
                if (!player_actor.bone_world(shinsu_model, skel, x, y, z)) {
                    x = local_player.pos().x;
                    y = local_player.pos().y;
                    z = local_player.pos().z;
                }
            }
            sprite_fx.follow_all(id, x, y, z);
        }
        for (client::RemotePlayer* remote : remote_players.all()) {
            if (remote == nullptr) {
                continue;
            }
            if (net.castle_defeated(remote->id())) {
                if (defeated_castle_players.insert(remote->id()).second) {
                    remote->actor().trigger_die();
                }
            } else if (defeated_castle_players.erase(remote->id()) != 0) {
                remote->actor().clear_die();
            }
            remote->set_resting(net.entity_resting(remote->id()));
            remote->set_consuming(net.entity_consuming(remote->id()));
            const auto gathering =
                (net.player_gstate(remote->id()) & protocol::kCgsHarvestPlant) != 0;
            if (gathering && gathering_players.insert(remote->id()).second) {
                remote->actor().trigger_harvest();
            }
            if (!gathering && gathering_players.erase(remote->id()) > 0 && remote->moving()) {
                remote->actor().clear_open_box();
            }
        }
        for (auto it = gathering_players.begin(); it != gathering_players.end();) {
            if (!remote_players.find(*it)) {
                it = gathering_players.erase(it);
            } else {
                ++it;
            }
        }
        std::unordered_set<std::uint32_t> own_party_ids;
        if (net.party().id != 0) {
            for (const auto& member : net.party().members) {
                own_party_ids.insert(member.id);
            }
        }
        std::vector<client::PlayerRenderCandidate> player_candidates;
        for (const auto* remote : remote_players.all()) {
            const float dx = remote->wx() - local_player.pos().x;
            const float dz = remote->wz() - local_player.pos().z;
            player_candidates.push_back({remote->id(),
                                         dx * dx + dz * dz,
                                         remote->id() == selected_player_id,
                                         own_party_ids.count(remote->id()) != 0});
        }
        remote_players.set_render_selection(client::select_player_ids(
            std::move(player_candidates), client_settings.render.max_rendered_players));
        remote_players.update(*engine, input.dt, &sfx);
        for (auto it = remote_skill_loops.begin(); it != remote_skill_loops.end();) {
            const auto* remote = remote_players.find(it->first);
            if (!remote || !remote->actor().skill_playing() || remote->moving()) {
                sfx.stop(it->second);
                it = remote_skill_loops.erase(it);
            } else {
                ++it;
            }
        }
        sync_weapon_mix_fx();
        sync_bof_from_wear();
        if (weapon_mix_fx_magic > 0) {
            engine::Mat4 mix_mat;
            const auto clazz = local_player.clazz();
            const bool use_weapon_socket =
                clazz == client::Class::Magician || clazz == client::Class::Archer;
            bool followed = use_weapon_socket &&
                            player_actor.weapon_effect_socket_world(player_world_matrix(), mix_mat);
            if (!followed) {
                followed = player_actor.weapon_world_matrix(player_world_matrix(), mix_mat);
            }
            if (followed) {
                sprite_fx.follow_oriented(kWeaponMixFxEntity, -1, mix_mat);
            }
        }
        {
            const auto model = player_world_matrix();
            bof_fx.follow(sprite_fx,
                          player_actor,
                          model,
                          local_player.clazz(),
                          local_player.pos().x,
                          local_player.pos().y,
                          local_player.pos().z);
        }
        {
            bool perfect_defense = false;
            for (const auto& buff : net.buff_slots()) {
                if ((buff.bstate & protocol::kCbsSkillKnightPerfectDefense) != 0) {
                    perfect_defense = true;
                    break;
                }
            }
            const float px = local_player.pos().x;
            const float py = local_player.pos().y;
            const float pz = local_player.pos().z;
            const float pyaw = local_player.yaw();
            knight_fx_dispatcher.set_perfect_defense(
                *engine, net.local_player_id(), perfect_defense, px, py, pz, pyaw);
            knight_fx_dispatcher.follow_perfect_defense(net.local_player_id(), px, py, pz, pyaw);
        }
        if (pending_skill_kind != PendingSkillKind::Mage && !player_actor.skill_playing() &&
            !player_actor.attack_playing()) {
            stop_skill_cast_sfx();
        }
        if (player_actor.take_arrow_release() && pending_arrow_target != 0) {
            spawn_archer_shot(pending_arrow_target);
            pending_arrow_target = 0;
        }
        auto present_actor_clip_effects = [&](client::AnimatedActor& actor,
                                              const engine::Mat4& model,
                                              const engine::Vec3& position,
                                              float yaw,
                                              std::uint32_t entity,
                                              std::uint32_t tid) {
            const auto cues = actor.take_clip_effects();
            if (!cues.empty()) {
                float impact_x = position.x;
                float impact_y = position.y + 16.0f;
                float impact_z = position.z;
                bool placed = false;
                if (tid != 0) {
                    auto mit = net.monsters().find(tid);
                    if (mit != net.monsters().end()) {
                        float impact_scale = 1.0f;
                        resolve_monster_impact(
                            tid, mit->second, impact_x, impact_y, impact_z, impact_scale);
                        placed = true;
                    }
                }
                float caster_x = position.x;
                float caster_y = position.y + 16.0f;
                float caster_z = position.z;
                (void) actor.effect_tip_world(model, caster_x, caster_y, caster_z);
                if (!placed) {
                    impact_x = caster_x;
                    impact_y = caster_y;
                    impact_z = caster_z;
                }
                for (const auto& cue : cues) {
                    if (cue.kind == client::AnimatedActor::ClipEffectCue::Kind::Model) {
                        combat_fx.spawn_clip_model(
                            *engine, cue.name, impact_x, impact_y, impact_z, yaw);
                    } else if (sprite_fx.has_fx(cue.name)) {
                        // Clip (*EFFECT_FX) belongs to the caster swing, not the
                        // selected mob (AddFxToTarget(caster)).
                        const std::uint32_t follow = entity;
                        const auto sounds = sprite_fx.spawn_oneshot_fx(
                            *engine, cue.name, caster_x, caster_y, caster_z, follow);
                        for (const auto& sound : sounds) {
                            combat_fx.play_sfx(sound, caster_x, caster_y, caster_z);
                        }
                        for (const auto& stage : sprite_fx.fx_model_spawns(cue.name)) {
                            if (stage.stem().empty()) {
                                continue;
                            }
                            const float duration = stage.restart() != 0 ? stage.duration() : 0.0f;
                            combat_fx.spawn_clip_model(*engine,
                                                       stage.stem(),
                                                       caster_x + stage.ox(),
                                                       caster_y + stage.oy(),
                                                       caster_z + stage.oz(),
                                                       yaw,
                                                       stage.delay(),
                                                       duration,
                                                       follow,
                                                       false,
                                                       false,
                                                       stage.effect_bone(),
                                                       stage.ox(),
                                                       stage.oy(),
                                                       stage.oz(),
                                                       stage.use_effect_bone_rotation());
                        }
                    } else {
                        combat_fx.spawn_clip_model(
                            *engine, cue.name, impact_x, impact_y, impact_z, yaw);
                    }
                }
            }
        };
        present_actor_clip_effects(player_actor,
                                   player_world_matrix(),
                                   local_player.pos(),
                                   local_player.yaw(),
                                   net.local_player_id(),
                                   pending_skill_target != 0 ? pending_skill_target
                                                             : selected_mob_id);
        for (auto* remote : remote_players.all()) {
            const auto model = remote->model_matrix();
            if (remote->actor().take_arrow_release() && remote->actor().attack_playing() &&
                remote->clazz() == client::Class::Archer) {
                float tx = 0, ty = 0, tz = 0;
                const auto target = net.monsters().find(remote->combat_target());
                if (target != net.monsters().end()) {
                    float scale = 1.0f;
                    resolve_monster_impact(
                        remote->combat_target(), target->second, tx, ty, tz, scale);
                } else {
                    player_impact_world(remote->combat_target(), tx, ty, tz);
                }
                float x = remote->wx(), y = remote->wy() + 16.0f, z = remote->wz();
                (void) remote->actor().effect_tip_world(model, x, y, z);
                combat_fx.spawn_arrow(client::SkillEt::None,
                                      x,
                                      y,
                                      z,
                                      tx,
                                      ty,
                                      tz,
                                      0.0f,
                                      remote->combat_target(),
                                      1,
                                      1.0f,
                                      remote->id());
            }
            const float yaw = std::atan2(model.m[8], model.m[10]);
            present_actor_clip_effects(remote->actor(),
                                       model,
                                       {remote->wx(), remote->wy(), remote->wz()},
                                       yaw,
                                       remote->id(),
                                       remote->combat_target());
        }
        combat_fx.follow_attached(local_player.pos().x, local_player.pos().y, local_player.pos().z);
        if (pending_skill_kind == PendingSkillKind::Archer) {
            float tip_x = 0.0f;
            float tip_y = 0.0f;
            float tip_z = 0.0f;
            mage_effect_tip(tip_x, tip_y, tip_z);
            combat_fx.follow_cast_fx(
                tip_x, tip_y, tip_z, local_player.yaw(), net.local_player_id());
        }
        if (indoor_view.loaded()) {
            cam.limit_indoor_zoom(local_player,
                                  floor,
                                  input.height > 0 ? static_cast<float>(input.width) / input.height
                                                   : 1.777f);
        } else {
            cam.clear_indoor_collision();
        }
        const engine::Vec3 cam_eye_early = cam.eye(local_player);
        const float cam_yaw = cam.yaw_rad();
        const engine::Vec3 listener_position =
            client_settings.audio.listener_follows_camera ? cam_eye_early : local_player.pos();
        sfx.set_listener(listener_position.x,
                         listener_position.y,
                         listener_position.z,
                         std::sin(cam_yaw),
                         std::cos(cam_yaw));
        sfx.update();
        {
            float hx = 0.0f;
            float hy = 0.0f;
            float hz = 0.0f;
            player_spine_world(hx, hy, hz);
            combat_fx.set_homing_target(hx, hy, hz);
        }
        for (auto& [id, monster] : monster_instances) {
            monster.advance_hit_outline(input.dt);
        }
        combat_fx.update(
            *engine, input.dt, cam_yaw, cam_eye_early.x, cam_eye_early.y, cam_eye_early.z);
        present_arrow_feedback();
        {
            client::CombatFx::ProjectileCrash crash;
            while (combat_fx.take_projectile_crash(crash)) {
                for (auto it = latched_bolt_floats.begin(); it != latched_bolt_floats.end();) {
                    if (it->anchor_id == 0 || it->anchor_id == crash.target_entity()) {
                        combat_floats.spawn(it->kind, it->text, it->anchor_id);
                        it = latched_bolt_floats.erase(it);
                    } else {
                        ++it;
                    }
                }
            }
            for (auto it = latched_bolt_floats.begin(); it != latched_bolt_floats.end();) {
                it->age += input.dt;
                if (it->age >= 2.0f) {
                    combat_floats.spawn(it->kind, it->text, it->anchor_id);
                    it = latched_bolt_floats.erase(it);
                } else {
                    ++it;
                }
            }
        }
        combat_floats.update(input.dt);
        overhead_chat.update(input.dt);
        archer_fx_dispatcher.update(*engine, input.dt);
        {
            float sx = 0.0f;
            float sy = 0.0f;
            float sz = 0.0f;
            player_spine_world(sx, sy, sz);
            follow_player_fx_bones(
                net.local_player_id(), player_actor, player_world_matrix(), local_player.clazz());
            archer_fx_dispatcher.follow_target(net.local_player_id(), sx, sy, sz);
            combat_fx.follow_heal_donut(net.local_player_id(),
                                        local_player.pos().x,
                                        local_player.pos().y,
                                        local_player.pos().z);
            combat_fx.follow_ice_nova(net.local_player_id(),
                                      local_player.pos().x,
                                      local_player.pos().y,
                                      local_player.pos().z);
            combat_fx.follow_stone_shield_prep(net.local_player_id(), sx, sy, sz);
            combat_fx.follow_meditation(net.local_player_id(), sx, sy, sz);
        }
        for (const client::RemotePlayer* remote : remote_players.all()) {
            if (remote == nullptr) {
                continue;
            }
            float sx = 0.0f;
            float sy = 0.0f;
            float sz = 0.0f;
            remote->spine_world(sx, sy, sz);
            const auto model = remote->model_matrix();
            follow_player_fx_bones(remote->id(), remote->actor(), model, remote->clazz());
            float tip_x = remote->wx(), tip_y = remote->wy() + 16.0f, tip_z = remote->wz();
            (void) remote->actor().effect_tip_world(model, tip_x, tip_y, tip_z);
            combat_fx.follow_cast_fx(
                tip_x, tip_y, tip_z, std::atan2(model.m[8], model.m[10]), remote->id());
            if (!remote->actor().skill_playing() || remote->moving()) {
                combat_fx.clear_cast_fx(remote->id());
            }
            archer_fx_dispatcher.follow_target(remote->id(), remote->wx(), sy, remote->wz());
            combat_fx.follow_heal_donut(remote->id(), remote->wx(), remote->wy(), remote->wz());
            combat_fx.follow_ice_nova(remote->id(), remote->wx(), remote->wy(), remote->wz());
            combat_fx.follow_stone_shield_prep(remote->id(), sx, sy, sz);
            combat_fx.follow_meditation(remote->id(), sx, sy, sz);
        }
        sync_doggebi_quake_fx(cam_eye_early);
        sprite_fx.update(input.dt, cam_eye_early.x, cam_eye_early.y, cam_eye_early.z);
        std::size_t monster_skin_updates = 0;
        if (want_net && net.connected()) {
            bool cooking_finished_this_update = false;
            bool cooking_completed_this_update = false;
            const bool chest_hold_still =
                opening_chest_id != 0 || pending_chest_id != 0 || player_actor.opening_box();
            const bool hold_still = chest_hold_still || fishing_active || cooking_active;
            if (hold_still) {
                local_player.clear_click_target();
            }
            net.sync_local_move(local_player.pos().x,
                                local_player.pos().y,
                                local_player.pos().z,
                                local_player.moving() && !hold_still,
                                input.dt,
                                profile.origin_patch(),
                                chest_hold_still);
            if (pending_server_fly_off) {
                const int sx = coords::to_server(local_player.pos().x, profile.origin_patch());
                const int sy = coords::to_server(local_player.pos().z, profile.origin_patch());
                if (net.server_px() == sx && net.server_py() == sy) {
                    (void) net.send_chat("/fly");
                    pending_server_fly_off = false;
                }
            }
            net.update(input.dt, profile.origin_patch());
            const bool server_fishing_active =
                (net.player_gstate(net.local_player_id()) & protocol::kCgsFish) != 0;
            if (!server_fishing_active) {
                fishing_active = false;
            }
            const auto fishing_now_ms =
                static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                               std::chrono::steady_clock::now().time_since_epoch())
                                               .count());
            const std::uint64_t next_fishing_cycle_duration_ms =
                std::any_of(net.buff_slots().begin(),
                            net.buff_slots().end(),
                            [fishing_now_ms](const client::BuffSlotEntry& buff) {
                                return (buff.bstate & protocol::kCbsStoneFishery) != 0 &&
                                       buff.expire_ms > fishing_now_ms;
                            })
                    ? fishing_accelerated_cycle_duration_ms
                    : fishing_base_cycle_duration_ms;
            while (net.take_fishing_progress_start()) {
                const bool already_fishing = fishing_active;
                fishing_active = true;
                fishing_cycle_started = std::chrono::steady_clock::now();
                fishing_cycle_duration_ms = next_fishing_cycle_duration_ms;
                local_player.clear_click_target();
                local_player.set_moving(false);
                if (already_fishing) {
                    player_actor.restart_fishing_cycle();
                }
            }
            if (server_fishing_active && !fishing_active) {
                // A reconnect or a future server snapshot may establish compatibility behavior
                // without a cycle-start signal; begin a conservative local cycle.
                fishing_active = true;
                fishing_cycle_started = std::chrono::steady_clock::now();
                fishing_cycle_duration_ms = next_fishing_cycle_duration_ms;
            }
            const bool server_cooking_active =
                (net.player_gstate(net.local_player_id()) & protocol::kCgsCooking) != 0;
            if (!server_cooking_active) {
                cooking_finished_this_update = cooking_active;
                cooking_active = false;
            } else if (!cooking_active) {
                cooking_active = true;
                cooking_started = std::chrono::steady_clock::now();
                local_player.clear_click_target();
                local_player.set_moving(false);
            }
            if (pending_transform_execute_stage > 0) {
                pending_transform_execute_left -= input.dt;
                if (pending_transform_execute_left <= 0.0f) {
                    (void) net.send_transform(
                        protocol::kTransMExecute,
                        static_cast<std::uint8_t>(pending_transform_execute_stage));
                    pending_transform_execute_stage = 0;
                    pending_transform_execute_left = 0.0f;
                }
            }
            {
                protocol::CreatePlayerFields other{};
                while (net.take_create_other_player(other)) {
                    const float wx = coords::to_client(other.x, profile.origin_patch());
                    const float wz = coords::to_client(other.y, profile.origin_patch());
                    const float wy = coords::to_client_y(other.z);
                    if (!remote_players.upsert(*engine,
                                               profile.data_root(),
                                               prop_tex_cache,
                                               other,
                                               item_catalog,
                                               wx,
                                               wy,
                                               wz)) {
                        log::warn("remote player load failed id=" + std::to_string(other.id));
                    }
                }
                std::uint32_t removed_id = 0;
                while (net.take_remove_other_player(removed_id)) {
                    defeated_castle_players.erase(removed_id);
                    if (const auto* remote = remote_players.find(removed_id)) {
                        overhead_chat.remove(remote->name());
                    }
                    if (selected_player_id == removed_id) {
                        selected_player_id = 0;
                    }
                    const auto loop = remote_skill_loops.find(removed_id);
                    if (loop != remote_skill_loops.end()) {
                        sfx.stop(loop->second);
                        remote_skill_loops.erase(loop);
                    }
                    combat_fx.kill_clip_entity(removed_id);
                    remote_players.remove(*engine, removed_id);
                }
                for (const auto& [id, position] : net.remote_player_positions()) {
                    if (auto* remote = remote_players.find(id)) {
                        const float wx = coords::to_client(position.x, profile.origin_patch());
                        const float wz = coords::to_client(position.y, profile.origin_patch());
                        remote->apply_network_pose(
                            wx, coords::to_client_y(position.z), wz, position.moving);
                    }
                }
            }
            if (pending_ask_npc_id != 0 && net.connected()) {
                auto nit = net.npcs().find(pending_ask_npc_id);
                if (nit == net.npcs().end()) {
                    if (selected_npc_id == pending_ask_npc_id) {
                        selected_npc_id = 0;
                    }
                    pending_ask_npc_id = 0;
                } else if (in_client_range(
                               nit->second.wx(), nit->second.wz(), kNpcTalkEngageRange)) {
                    (void) net.send_ask_npc(pending_ask_npc_id);
                    pending_ask_npc_id = 0;
                    local_player.clear_click_target();
                } else if (!local_player.has_click_target()) {
                    local_player.set_click_target(nit->second.wx(), nit->second.wz());
                }
            }
            if (selected_mob_id != 0) {
                auto dead_it = net.monsters().find(selected_mob_id);
                if (dead_it == net.monsters().end() || !is_clickable_mob(dead_it->second)) {
                    if (!player_actor.attack_locked()) {
                        player_actor.cancel_attack();
                    }
                    pending_arrow_target = 0;
                    auto_attack = false;
                    selected_mob_id = 0;
                } else if (dead_it->second.hp() <= 0) {
                    if (!player_actor.attack_locked()) {
                        player_actor.cancel_attack();
                    }
                    pending_arrow_target = 0;
                    auto_attack = false;
                }
            }
            {
                std::uint32_t trash_iid = 0;
                while (net.take_trash_item(trash_iid)) {
                    if (!player_bag.try_remove_iid(trash_iid)) {
                        log::debug("trash item iid=" + std::to_string(trash_iid) +
                                   " not in local bag");
                    }
                }
                client::PendingInsertItem ins{};
                bool wear_from_insert = false;
                while (net.take_insert_item(ins)) {
                    if (server::enchantment_talisman_grade(ins.index) != 0 &&
                        ins.iid == enchant_overlay.charge_from_id() &&
                        (ins.info & protocol::kInsertReplace) != 0) {
                        enchantment_result_iid = enchant_overlay.charge_to_id();
                    }
                    bool prefix_changed = false;
                    bool updated_existing_instance = false;
                    bool applied = false;
                    if (ins.index == 31) {
                        if ((ins.info & protocol::kInsertReplace) != 0) {
                            player_bag.money = ins.count;
                        } else {
                            player_bag.money += ins.count;
                            if (ins.count > 0) {
                                game_hud.chat().push_info(
                                    client::BattleMsgBuilder::money_acquired(ins.count),
                                    protocol::BattleMsgKind::MoneyGet);
                            }
                        }
                        continue;
                    }
                    const int cur_end = (ins.info & protocol::kInsertHasEndurance) != 0
                                            ? protocol::insert_cur_end(ins.info)
                                            : -1;
                    const int max_end = (ins.info & protocol::kInsertHasEndurance) != 0
                                            ? protocol::insert_max_end(ins.info)
                                            : -1;
                    const auto instance_is_equipped = [&]() {
                        if (ins.iid == 0) {
                            return false;
                        }
                        for (const auto* slots : {&player_bag.normal,
                                                  &player_bag.quest,
                                                  &player_bag.equipment,
                                                  &player_bag.collection}) {
                            for (const auto& slot : *slots) {
                                if (slot.iid == ins.iid) {
                                    return slot.equipped;
                                }
                            }
                        }
                        return false;
                    };
                    const bool removing_equipped_instance =
                        ins.count <= 0 && instance_is_equipped();
                    if (ins.count <= 0 && (ins.info & 1u) == 0) {
                        const int remove_n = ins.iid > 0 ? static_cast<int>(ins.iid) : 1;
                        applied = player_bag.try_remove(ins.index, remove_n);
                    } else if (ins.iid != 0 && (ins.info & 1u) != 0 && ins.count <= 0) {
                        applied = player_bag.try_remove_iid(ins.iid);
                    } else if (ins.iid != 0 && player_bag.try_update_instance(ins.iid,
                                                                              ins.index,
                                                                              ins.prefix,
                                                                              ins.count,
                                                                              (ins.info & 1u) != 0,
                                                                              prefix_changed,
                                                                              cur_end,
                                                                              max_end)) {
                        applied = true;
                        updated_existing_instance = true;
                    } else {
                        applied = player_bag.try_insert(ins.index,
                                                        ins.count,
                                                        item_catalog,
                                                        ins.prefix,
                                                        ins.iid,
                                                        cur_end > 0 ? cur_end : 0,
                                                        max_end > 0 ? max_end : 0,
                                                        protocol::insert_put_on(ins.info));
                    }
                    if (!applied) {
                        log::warn("bag insert failed index=" + std::to_string(ins.index));
                    } else {
                        if (ins.iid != 0 && (ins.count <= 0 || ins.has_expire_at)) {
                            inv_win.set_item_expire_at(ins.iid, ins.count > 0 ? ins.expire_at : 0);
                        }
                        if (pending_cooking_order && (ins.info & protocol::kInsertReplace) != 0 &&
                            ins.count > 0) {
                            const server::SpeedMedicineCost cost =
                                server::speed_medicine_cost(*pending_cooking_order);
                            cooking_completed_this_update =
                                cooking_completed_this_update ||
                                (ins.index == server::kBloodFishItemIndex &&
                                 cost.output_blood_fish > 0) ||
                                (ins.index == server::kSpeedMedicineItemIndex &&
                                 cost.output_medicine > 0);
                        }
                        bool charm_grew = false;
                        if (ins.iid != 0) {
                            auto apply_charm = [&](std::vector<client::ItemStack>& slots) -> bool {
                                for (auto& slot : slots) {
                                    if (slot.iid != ins.iid) {
                                        continue;
                                    }
                                    charm_grew =
                                        ins.charm.x_attack > slot.x_attack ||
                                        ins.charm.x_magic > slot.x_magic ||
                                        ins.charm.x_hit > slot.x_hit ||
                                        ins.charm.x_defense > slot.x_defense ||
                                        ins.charm.x_dodge > slot.x_dodge ||
                                        ins.charm.upgrade_level > slot.upgrade_level ||
                                        static_cast<int>(ins.charm.mix_type) != slot.mix_type ||
                                        static_cast<int>(ins.charm.bof) != slot.bof ||
                                        static_cast<int>(ins.charm.qigong_perforated) !=
                                            slot.qigong_perforated ||
                                        static_cast<int>(ins.charm.qigong_effect) !=
                                            slot.qigong_effect ||
                                        static_cast<int>(ins.charm.dragon_effect) !=
                                            slot.dragon_effect ||
                                        static_cast<int>(ins.charm.random_stat_type) !=
                                            slot.random_stat_type ||
                                        static_cast<int>(ins.charm.random_stat_value) !=
                                            slot.random_stat_value;
                                    slot.x_attack = ins.charm.x_attack;
                                    slot.x_magic = ins.charm.x_magic;
                                    slot.x_hit = ins.charm.x_hit;
                                    slot.x_defense = ins.charm.x_defense;
                                    slot.x_dodge = ins.charm.x_dodge;
                                    slot.upgrade_level = ins.charm.upgrade_level;
                                    slot.upgrade_rate = ins.charm.upgrade_rate;
                                    slot.mix_type = static_cast<int>(ins.charm.mix_type);
                                    slot.bof = static_cast<int>(ins.charm.bof);
                                    slot.qigong_perforated =
                                        static_cast<int>(ins.charm.qigong_perforated);
                                    slot.qigong_effect = static_cast<int>(ins.charm.qigong_effect);
                                    slot.dragon_effect = static_cast<int>(ins.charm.dragon_effect);
                                    slot.random_stat_type =
                                        static_cast<int>(ins.charm.random_stat_type);
                                    slot.random_stat_value =
                                        static_cast<int>(ins.charm.random_stat_value);
                                    slot.account_bound = protocol::insert_account_bound(ins.info);
                                    slot.character_bound =
                                        protocol::insert_character_bound(ins.info);
                                    slot.enchant_protected =
                                        (ins.info & protocol::kInsertEnchantProtected) != 0;
                                    return true;
                                }
                                return false;
                            };
                            if (!apply_charm(player_bag.normal) && !apply_charm(player_bag.quest) &&
                                !apply_charm(player_bag.equipment)) {
                                (void) apply_charm(player_bag.collection);
                            }
                            if (ins.count > 0 && (ins.info & protocol::kInsertReplace) != 0) {
                                const bool collection = server::collection_def(ins.index).kind !=
                                                        server::CollectionKind::None;
                                const auto* def = item_catalog.find(ins.index);
                                if (collection) {
                                    (void) player_bag.set_collection_equipped(
                                        ins.iid, protocol::insert_put_on(ins.info));
                                } else if (def && def->can_equip()) {
                                    (void) player_bag.set_equipment_equipped(
                                        ins.iid, protocol::insert_put_on(ins.info), item_catalog);
                                }
                            }
                        }
                        if (protocol::insert_put_on(ins.info) && ins.index > 0) {
                            if (const client::ItemDef* def = item_catalog.find(ins.index)) {
                                if (def->can_equip()) {
                                    player_wear.apply_item(*def);
                                    wear_from_insert = true;
                                }
                            }
                        }
                        if (updated_existing_instance &&
                            (ins.info & protocol::kInsertReplace) != 0 &&
                            (prefix_changed || charm_grew || ins.iid == enchantment_result_iid)) {
                            if (ins.iid == enchantment_result_iid) {
                                enchantment_result_iid = 0;
                            }
                            enchant_overlay.show_success();
                        } else if (removing_equipped_instance && ins.index > 0) {
                            player_wear.unequip_index(ins.index);
                            refresh_player_visuals();
                        } else if (ins.count > 0 && (ins.info & 1u) == 0 &&
                                   std::chrono::steady_clock::now() >= bag_restore_until) {
                            std::string item_name = "item #" + std::to_string(ins.index);
                            if (const client::ItemDef* def = item_catalog.find(ins.index)) {
                                if (!def->name().empty()) {
                                    item_name = def->name();
                                }
                            }
                            game_hud.chat().push_info(
                                client::BattleMsgBuilder::item_acquired(item_name, ins.count),
                                protocol::BattleMsgKind::ItemGet);
                        }
                    }
                }
                bool sorted_quest = false;
                if (!inv_win.capturing_pointer() && net.take_inventory_sort(sorted_quest)) {
                    inv_win.complete_sort(player_bag, sorted_quest);
                }
                client::PendingItemWear item_wear{};
                while (net.take_item_wear(item_wear)) {
                    if (item_wear.player_id != net.local_player_id()) {
                        continue;
                    }
                    if (server::collection_def(item_wear.item_index).kind !=
                        server::CollectionKind::None) {
                        if (!player_bag.set_collection_equipped(item_wear.iid,
                                                                item_wear.equipped)) {
                            log::warn("collection wear update failed iid=" +
                                      std::to_string(item_wear.iid));
                        }
                        if (item_wear.equipped && item_wear.expire_at > 0) {
                            inv_win.set_item_expire_at(item_wear.iid, item_wear.expire_at);
                        }
                        continue;
                    }
                    // Absolute item updates above own storage; wear events update presentation.
                    if (item_wear.equipped && item_wear.expire_at > 0) {
                        inv_win.set_item_expire_at(item_wear.iid, item_wear.expire_at);
                    }
                    if (item_wear.equipped) {
                        if (const client::ItemDef* def = item_catalog.find(item_wear.item_index)) {
                            if (def->can_equip()) {
                                player_wear.apply_item(*def);
                                if (def->is_pet()) {
                                    equipped_pet_expire_at = item_wear.expire_at;
                                    inv_win.set_item_expire_at(item_wear.iid, item_wear.expire_at);
                                }
                            }
                        }
                    } else {
                        const bool removed_pet = player_wear.pet_index() == item_wear.item_index;
                        player_wear.unequip_index(item_wear.item_index);
                        if (removed_pet) {
                            equipped_pet_expire_at = 0;
                        }
                    }
                    refresh_player_visuals();
                }
                protocol::RidingState riding_state{};
                while (net.take_riding_state(riding_state)) {
                    if (riding_state.player_id != net.local_player_id()) {
                        continue;
                    }
                    local_riding_mounted = riding_state.mounted;
                    registered_ride_expire_at = riding_state.expire_at;
                    registered_ride_iid = riding_state.iid;
                    inv_win.set_item_expire_at(riding_state.iid, riding_state.expire_at);
                    registered_riding_kind = riding_state.kind;
                    riding_host.set_state(riding_state);
                    registered_battle_grade = riding_state.grade;
                    registered_battle_rarity = riding_state.rarity;
                    registered_battle_satiety = riding_state.satiety;
                    registered_battle_max_satiety = riding_state.max_satiety;
                    registered_battle_equipment_iids = riding_state.equipment_iids;
                    registered_battle_equipment_indices = riding_state.equipment_item_indices;
                    registered_battle_equipment_enchants = riding_state.equipment_enchants;
                    if (riding_state.item_index == 0) {
                        player_wear.set_ride_index(0);
                        registered_ride_expire_at = 0;
                    } else {
                        player_wear.set_ride_index(riding_state.item_index);
                    }
                    if (!local_riding_mounted) {
                        local_riding_offset_y = 0.0f;
                    }
                }
                protocol::RidingCacheResult cache_result{};
                while (net.take_riding_cache_result(cache_result)) {
                    riding_cache_overlay.show(cache_result);
                }
                protocol::UpdateItemInfoFields egg_info{};
                while (net.take_update_item_info(egg_info)) {
                    if (!player_bag.try_set_transform_progress(egg_info.iid,
                                                               static_cast<int>(egg_info.level),
                                                               static_cast<int>(egg_info.exp))) {
                        log::debug("transform item info iid=" + std::to_string(egg_info.iid) +
                                   " not in local bag");
                    }
                }
                if (wear_from_insert) {
                    refresh_player_visuals();
                }
                if (cooking_completed_this_update && pending_cooking_order) {
                    const server::SpeedMedicineCost cost =
                        server::speed_medicine_cost(*pending_cooking_order);
                    const auto item_name = [&](int index) -> std::string {
                        if (const client::ItemDef* def = item_catalog.find(index);
                            def && !def->name().empty()) {
                            return def->name();
                        }
                        return std::string("item #") + std::to_string(index);
                    };
                    const auto show_removed = [&](int index, int count) {
                        if (count <= 0) {
                            return;
                        }
                        game_hud.chat().push_info(
                            client::BattleMsgBuilder::item_removed(item_name(index), count),
                            protocol::BattleMsgKind::ItemUse);
                    };
                    const auto show_acquired = [&](int index, int count) {
                        if (count <= 0) {
                            return;
                        }
                        game_hud.chat().push_info(
                            client::BattleMsgBuilder::item_acquired(item_name(index), count),
                            protocol::BattleMsgKind::ItemGet);
                    };
                    show_removed(server::kDemonBloodItemIndex, cost.demon_blood);
                    show_removed(server::kSeaBreamItemIndex, cost.sea_bream);
                    show_removed(server::kBambooKegItemIndex, cost.bamboo_keg);
                    show_removed(server::kBloodFishItemIndex, cost.blood_fish);
                    show_acquired(server::kBloodFishItemIndex, cost.output_blood_fish);
                    show_acquired(server::kSpeedMedicineItemIndex, cost.output_medicine);
                    pending_cooking_order.reset();
                } else if (cooking_finished_this_update) {
                    pending_cooking_order.reset();
                }
                notify_ready_quest_items();
                std::int32_t html_id = 0;
                std::uint32_t dialog_npc_id = 0;
                while (net.take_send_html(html_id, &dialog_npc_id)) {
                    if (dialog_npc_id != 0 && !npc_dialog_available(dialog_npc_id)) {
                        continue;
                    }
                    if (const client::DialogDef* dlg = dialog_catalog.find(html_id)) {
                        if (client::DebugOverlay::enabled()) {
                            game_hud.chat().push_info("NPC html -> " + dialog_json_name(html_id),
                                                      {});
                        }
                        const bool was_open = npc_talk_win.open();
                        npc_talk_win.show_dialog(*dlg, dialog_npc_id, boot.selected.name);
                        update_window_order(
                            client::InGameWindowId::NpcTalk, was_open, npc_talk_win.open());
                        npc_talk_win.center_on_screen(static_cast<float>(input.width),
                                                      static_cast<float>(input.height));
                    } else {
                        log::warn("missing dialog html id=" + std::to_string(html_id));
                    }
                }
                protocol::ShopItemList shop_list;
                while (net.take_shop_item_list(shop_list)) {
                    if (shop_list.currency_index != 0) {
                        shop_win.close();
                        const bool was_open = skin_shop_win.open();
                        skin_shop_win.show(
                            shop_list, pending_skin_category, local_player.clazz(), item_catalog);
                        update_window_order(client::InGameWindowId::Shop, was_open, true);
                        skin_shop_win.center_on_screen(static_cast<float>(input.width),
                                                       static_cast<float>(input.height));
                        continue;
                    }
                    skin_shop_win.close();
                    const bool was_open = shop_win.open();
                    shop_win.show(shop_list);
                    update_window_order(client::InGameWindowId::Shop, was_open, shop_win.open());
                    shop_win.center_on_screen(static_cast<float>(input.width),
                                              static_cast<float>(input.height));
                }
                protocol::StorageInfo storage_info;
                while (net.take_storage_info(storage_info)) {
                    const bool was_open = storage_win.open();
                    storage_win.show(storage_info);
                    update_window_order(
                        client::InGameWindowId::Storage, was_open, storage_win.open());
                    if (!was_open) {
                        storage_win.center_on_screen(static_cast<float>(input.width),
                                                     static_cast<float>(input.height));
                    }
                }
                protocol::StorageBalance storage_balance;
                while (net.take_storage_balance(storage_balance)) {
                    storage_win.set_balance(storage_balance);
                }
                const bool quest_state_changed = net.take_quest_state_changed();
                const bool quest_accepted = net.take_quest_accepted();
                protocol::MailClaimResult mail_claim;
                while (net.take_mail_claim_result(mail_claim)) {
                    mail_window.claim_result(mail_claim);
                }
                if (net.take_mail_dirty()) {
                    mail_window.set_entries(net.mail());
                }
                if (net.take_quest_flags_dirty()) {
                    quest_journal_win.sync_quests(net.quest_flags(), task_catalog, item_catalog);
                    notify_ready_quest_items();
                }
                if (quest_accepted) {
                    (void) sfx.play_ui("quest_accept");
                } else if (quest_state_changed) {
                    (void) sfx.play_ui("quest_update");
                }
                client::PendingChat chat{};
                while (net.take_chat(chat)) {
                    game_hud.chat().push_chat(chat.speaker, chat.text, boot.selected.name);
                    overhead_chat.show(chat.speaker, chat.text);
                    if (chat.speaker == "Server" &&
                        (chat.text.find("Doggebi") != std::string::npos ||
                         chat.text.find("doggebi") != std::string::npos)) {
                        doggebi_ritual_overlay = chat.text;
                        doggebi_ritual_overlay_until =
                            std::chrono::steady_clock::now() + std::chrono::seconds(5);
                    }
                }
                client::PendingMapState map_state{};
                while (net.take_map_state(map_state)) {
                    const std::uint8_t added_mask = static_cast<std::uint8_t>(
                        map_state.active_mask & ~doggebi_ritual_active_mask);
                    if (doggebi_ritual_state_received && added_mask != 0) {
                        int active_count = 0;
                        for (std::uint8_t bits = map_state.active_mask; bits != 0; bits >>= 1u) {
                            active_count += static_cast<int>(bits & 1u);
                        }
                        doggebi_ritual_overlay = "A Doggebi mirror has been activated. (" +
                                                 std::to_string(active_count) + "/8)";
                        doggebi_ritual_overlay_until =
                            std::chrono::steady_clock::now() + std::chrono::seconds(5);
                    }
                    doggebi_ritual_state_received = true;
                    doggebi_ritual_active_mask = map_state.active_mask;
                    doggebi_countdown_timer = map_state.timer;
                    doggebi_countdown_seconds = map_state.timer_seconds;
                    doggebi_countdown_received = std::chrono::steady_clock::now();
                    sync_doggebi_ritual_fx();
                    if (map_state.phase != doggebi_ritual_phase) {
                        doggebi_ritual_phase = map_state.phase;
                        switch (map_state.phase) {
                        case 1:
                            cam.start_shake(2.0f, 0.30f);
                            break;
                        case 2:
                            cam.start_shake(2.5f, 0.45f);
                            break;
                        case 3:
                            cam.start_shake(3.0f, 0.60f);
                            break;
                        case 4:
                            cam.start_shake(3.5f, 0.75f);
                            break;
                        default:
                            break;
                        }
                    }
                }
                protocol::PartyItemNotice party_item;
                while (net.take_party_item(party_item)) {
                    std::string item_name = "item #" + std::to_string(party_item.item_index);
                    if (const auto* definition = item_catalog.find(party_item.item_index)) {
                        if (!definition->name().empty()) {
                            item_name = definition->name();
                        }
                    }
                    game_hud.chat().push_info(
                        client::BattleMsgBuilder::party_item_acquired(
                            party_item.recipient_name, item_name, party_item.count),
                        protocol::BattleMsgKind::ItemGet,
                        client::kPartyItemColor);
                }
                client::PendingInfoMessage info{};
                while (net.take_info_message(info)) {
                    game_hud.chat().push_info(info.text, info.kind, info.color);
                }
                if (net.take_enchant_failed()) {
                    enchant_overlay.show_failed();
                }
                client::PendingPlayerAttack attack{};
                while (net.take_player_attack(attack)) {
                    if (read_only_session && attack.attacker_id == net.local_player_id()) {
                        const auto target = net.monsters().find(attack.target_id);
                        if (target != net.monsters().end()) {
                            face_world_xz(target->second.wx(), target->second.wz());
                        }
                        player_actor.end_skill();
                        player_actor.trigger_attack();
                    } else if (auto* remote = remote_players.find(attack.attacker_id)) {
                        remote->set_combat_target(attack.target_id);
                        float x = 0, y = 0, z = 0;
                        const auto target = net.monsters().find(attack.target_id);
                        if (target != net.monsters().end()) {
                            x = target->second.wx();
                            z = target->second.wz();
                        } else {
                            player_impact_world(attack.target_id, x, y, z);
                        }
                        remote->face_target(x, z);
                        combat_fx.clear_cast_fx(remote->id());
                        remote->actor().trigger_attack();
                    }
                }
                client::PendingHitFx hit_fx{};
                while (net.take_hit_fx(hit_fx)) {
                    auto hit_it = net.monsters().find(hit_fx.target_id);
                    const bool castle_player =
                        hit_it == net.monsters().end() && net.castle_side(hit_fx.target_id) != 0;
                    if (hit_it == net.monsters().end() && !castle_player) {
                        continue;
                    }
                    float hx = 0.0f;
                    float hy = 0.0f;
                    float hz = 0.0f;
                    float impact_scale = 1.0f;
                    if (castle_player) {
                        player_impact_world(hit_fx.target_id, hx, hy, hz);
                    } else {
                        resolve_monster_impact(
                            hit_fx.target_id, hit_it->second, hx, hy, hz, impact_scale);
                    }
                    const auto* remote_attacker = remote_players.find(hit_fx.attacker_id);
                    const bool local_hit = hit_fx.attacker_id == net.local_player_id();
                    const int attacker_job = remote_attacker
                                                 ? static_cast<int>(remote_attacker->clazz())
                                             : local_hit ? static_cast<int>(local_player.clazz())
                                                         : -2;
                    const bool archer_hit = attacker_job == static_cast<int>(client::Class::Archer);
                    if (archer_hit && hit_fx.skill_index < 0) {
                        combat_fx.note_arrow_result(
                            hit_fx.attacker_id, hit_fx.target_id, hit_fx.flags);
                        continue;
                    }
                    if ((hit_fx.flags & protocol::kAtkHit) == 0) {
                        continue;
                    }
                    if (archer_hit) {
                        pull_archer_impact_front(hx, hz, impact_scale);
                    }
                    if (!castle_player) {
                        present_monster_hit(hit_fx.target_id, hit_fx.flags, {hx, hy, hz});
                    }
                    combat_fx.spawn_hit(
                        *engine, hx, hy, hz, hit_fx.flags, impact_scale, archer_hit);
                    // Knight/melee: arm hit SFX for the (*EFFECT_MODEL) keyframe (after whoosh).
                    if (!archer_hit && hit_fx.attacker_id != 0 &&
                        hit_fx.attacker_id == net.local_player_id()) {
                        player_actor.note_melee_hit();
                    }
                    if (!archer_hit && hit_fx.skill_index < 0) {
                        if (auto* remote = remote_players.find(hit_fx.attacker_id)) {
                            remote->actor().note_melee_hit();
                        }
                    }
                    if (hit_fx.skill_index >= 0) {
                        if (local_hit) {
                            stop_skill_cast_sfx();
                        }
                        const auto* hit_def = skill_catalog.find(attacker_job, hit_fx.skill_index);
                        if (hit_def && !hit_def->sound(2).empty()) {
                            (void) sfx.play(hit_def->sound(2), hx, hy, hz);
                        }
                    }
                }
                present_arrow_feedback();
                if (net.take_local_evade()) {
                    player_actor.trigger_evade();
                    const std::string evade_sys = item_catalog.sys(140);
                    game_hud.chat().push_info(client::BattleMsgBuilder::evaded_attack(evade_sys),
                                              protocol::BattleMsgKind::Damage);
                }
                client::PendingCombatFloat combat_line{};
                while (net.take_combat_float(combat_line)) {
                    if (combat_line.wait_for_bolt) {
                        LatchedBoltFloat latched;
                        latched.kind = combat_line.kind;
                        latched.text = std::move(combat_line.text);
                        latched.anchor_id = combat_line.anchor_id;
                        latched_bolt_floats.push_back(std::move(latched));
                    } else {
                        combat_floats.spawn(
                            combat_line.kind, combat_line.text, combat_line.anchor_id);
                    }
                }
                client::PendingDelayDamage delay_line{};
                while (net.take_delay_damage(delay_line)) {
                    if (delay_line.target_id != 0 && delay_line.target_id == selected_mob_id &&
                        delay_line.damage > 0) {
                        delay_damage_hud.active = true;
                        delay_damage_hud.target_id = delay_line.target_id;
                        delay_damage_hud.saved_damage = delay_line.damage;
                        delay_damage_hud.start = std::chrono::steady_clock::now();
                    }
                }
            }
            if (selected_item_id != 0) {
                auto iit = net.ground_items().find(selected_item_id);
                if (iit == net.ground_items().end()) {
                    selected_item_id = 0;
                }
            }
            if (pending_pickup_id != 0) {
                auto iit = net.ground_items().find(pending_pickup_id);
                if (iit == net.ground_items().end()) {
                    pending_pickup_id = 0;
                } else if (in_client_range(iit->second.wx, iit->second.wz, 16)) {
                    const int sx = coords::to_server(iit->second.wx, profile.origin_patch());
                    const int sy = coords::to_server(iit->second.wz, profile.origin_patch());
                    int tx = 0;
                    int ty = 0;
                    coords::pixel_to_tile(sx, sy, tx, ty);
                    if (net.send_pickup_item(pending_pickup_id, tx, ty)) {
                        local_player.clear_click_target();
                        if (selected_item_id == pending_pickup_id) {
                            selected_item_id = 0;
                        }
                        pending_pickup_id = 0;
                    }
                } else if (!local_player.has_click_target()) {
                    local_player.set_click_target(iit->second.wx, iit->second.wz);
                }
            }
            {
                std::int32_t v = 0;
                const std::int32_t prev_hp = player_hp;
                if (net.take_cur_hp(v)) {
                    if (v < prev_hp) {
                        player_overhead_hp_until =
                            std::chrono::steady_clock::now() +
                            std::chrono::milliseconds(
                                static_cast<int>(client::Nameplate::kCombatHpTimeoutSec * 1000.0f));
                    }
                    player_hp = v;
                    if (player_hp <= 0 && !local_player.dead()) {
                        village_revival_deadline =
                            std::chrono::steady_clock::now() + std::chrono::minutes(10);
                        local_player.set_dead(true);
                        local_player.set_boosting(false);
                        local_player.clear_click_target();
                        auto_attack = false;
                        player_actor.trigger_die();
                        reset_range_skill();
                        client::DialogDef revival_dialog;
                        revival_dialog.caption = "Revive in 10:00";
                        revival_dialog.width = 440.0f;
                        revival_dialog.height = 120.0f;
                        revival_dialog.lines = {
                            "You have been defeated. You can wait for a kind magican to be revived "
                            "or using a Scroll of Rebirth. The only alternative is starting from "
                            "Village but be warned, it is going to cost some experience (up to "
                            "60%)."};
                        revival_dialog.links = {{"npc.village_revival", "Start from Village"}};
                        const bool was_open = npc_talk_win.open();
                        npc_talk_win.show_dialog(revival_dialog, 0, boot.selected.name);
                        update_window_order(
                            client::InGameWindowId::NpcTalk, was_open, npc_talk_win.open());
                        npc_talk_win.set_closable(false);
                        npc_talk_win.center_on_screen(static_cast<float>(input.width),
                                                      static_cast<float>(input.height));
                        log::info("player died");
                    } else if (player_hp > 0 && local_player.dead()) {
                        village_revival_deadline = {};
                        npc_talk_win.set_closable(true);
                        npc_talk_win.close();
                        in_game_window_order.closed(client::InGameWindowId::NpcTalk);
                        local_player.set_dead(false);
                        player_actor.clear_die();
                    }
                }
                if (local_player.dead() && npc_talk_win.open() &&
                    village_revival_deadline != std::chrono::steady_clock::time_point{}) {
                    const auto remaining = std::max<std::int64_t>(
                        0,
                        std::chrono::duration_cast<std::chrono::seconds>(
                            village_revival_deadline - std::chrono::steady_clock::now())
                            .count());
                    const std::int64_t minutes = remaining / 60;
                    const std::int64_t seconds = remaining % 60;
                    npc_talk_win.set_caption("Revive in " + std::to_string(minutes) + ":" +
                                             (seconds < 10 ? "0" : "") + std::to_string(seconds));
                }
                if (net.take_max_hp(v)) {
                    player_max_hp = std::max(1, v);
                    if (player_hp > player_max_hp) {
                        player_hp = player_max_hp;
                    }
                }
                if (net.take_cur_mp(v)) {
                    player_mp = v;
                }
                if (net.take_max_mp(v)) {
                    player_max_mp = std::max(1, v);
                    if (player_mp > player_max_mp) {
                        player_mp = player_max_mp;
                    }
                }
                {
                    std::int64_t cur = 0;
                    std::int64_t tot = 0;
                    if (net.take_exp(cur, tot)) {
                        player_exp_current = cur;
                        player_exp_total = std::max<std::int64_t>(1, tot);
                    }
                }
                {
                    std::int32_t lv = 0;
                    if (net.take_level(lv)) {
                        inv_win.set_player_level(std::max(1, static_cast<int>(lv)));
                        skill_win.set_player_level(inv_win.player_level());
                    }
                    skill_win.set_player_specialty(static_cast<int>(net.specialty()));
                }
                {
                    std::int32_t pu = 0;
                    std::int32_t su = 0;
                    if (net.take_points(pu, su)) {
                        (void) pu;
                        (void) su;
                    }
                }
                {
                    std::unordered_map<int, int> learned;
                    if (net.take_skill_info(learned)) {
                        player_skills.apply_server_info(
                            learned, skill_catalog, skill_win.player_clazz());
                    }
                    int sk_index = 0;
                    int sk_level = 0;
                    while (net.take_skill_up(sk_index, sk_level)) {
                        const int prev = player_skills.set_learned_level(sk_index, sk_level);
                        if (sk_level > prev) {
                            const client::SkillDef* def =
                                skill_catalog.find(skill_win.player_clazz(), sk_index);
                            const std::string name = def && !def->name().empty()
                                                         ? def->name()
                                                         : ("skill " + std::to_string(sk_index));
                            const std::string line =
                                prev == 0
                                    ? client::BattleMsgBuilder::skill_learned(name)
                                    : client::BattleMsgBuilder::skill_level_up(name, sk_level);
                            game_hud.chat().push_info(line, protocol::BattleMsgKind::Skill);
                        }
                        player_skills.ensure_default_hotbar(skill_catalog,
                                                            skill_win.player_clazz());
                    }
                    {
                        bool shortcut_present = false;
                        protocol::ShortcutBar shortcut_bar;
                        if (net.take_shortcut(shortcut_present, shortcut_bar) && shortcut_present) {
                            player_skills.apply_shortcut(shortcut_bar.active_index,
                                                         shortcut_bar.slots);
                        }
                    }
                    if (player_skills.hotbar_dirty()) {
                        protocol::ShortcutBar save_bar;
                        save_bar.active_index = player_skills.active_index();
                        for (std::size_t i = 0; i < protocol::kShortcutSlots; ++i) {
                            save_bar.slots[i] = player_skills.hotbar_slot(static_cast<int>(i));
                        }
                        if (net.send_shortcut(save_bar)) {
                            player_skills.clear_hotbar_dirty();
                        }
                    }
                }
                {
                    std::uint32_t fx_id = 0;
                    while (net.take_level_up_effect(fx_id)) {
                        if (fx_id == net.local_player_id() && fx_id != 0) {
                            const auto& pos = local_player.pos();
                            combat_fx.spawn_level_up(*engine, pos.x, pos.y, pos.z);
                        } else if (const auto* remote = remote_players.find(fx_id)) {
                            combat_fx.spawn_level_up(
                                *engine, remote->wx(), remote->wy(), remote->wz(), false, fx_id);
                        }
                    }
                }
                {
                    std::uint32_t charm_id = 0;
                    std::uint8_t charm_effect = 0;
                    while (net.take_charm_effect(charm_id, charm_effect)) {
                        const std::string_view fx_name =
                            protocol::oneshot_effect_fx_name(charm_effect);
                        if (fx_name.empty() || !engine || !sprite_fx.ready()) {
                            continue;
                        }
                        float fx_x = local_player.pos().x;
                        float fx_y = local_player.pos().y;
                        float fx_z = local_player.pos().z;
                        if (charm_id != 0 && charm_id != net.local_player_id()) {
                            if (const client::RemotePlayer* remote =
                                    remote_players.find(charm_id)) {
                                fx_x = remote->wx();
                                fx_y = remote->wy();
                                fx_z = remote->wz();
                            }
                        }
                        const std::uint32_t follow_id =
                            charm_id != 0 ? charm_id : net.local_player_id();
                        const auto sounds = sprite_fx.spawn_oneshot_fx(
                            *engine, fx_name, fx_x, fx_y, fx_z, follow_id);
                        for (const auto& snd : sounds) {
                            combat_fx.play_sfx(snd, fx_x, fx_y, fx_z);
                        }
                        log::debug(std::string("oneshot effect fx=") + std::string(fx_name));
                    }
                }
                {
                    std::uint32_t damaged_id = 0;
                    std::uint16_t damage = 0;
                    while (net.take_damaged_effect(damaged_id, damage)) {
                        (void) damage;
                        if (!engine || !sprite_fx.ready()) {
                            continue;
                        }
                        float fx_x = local_player.pos().x;
                        float fx_y = local_player.pos().y;
                        float fx_z = local_player.pos().z;
                        if (damaged_id != 0 && damaged_id != net.local_player_id()) {
                            if (const client::RemotePlayer* remote =
                                    remote_players.find(damaged_id)) {
                                fx_x = remote->wx();
                                fx_y = remote->wy();
                                fx_z = remote->wz();
                            }
                        }
                        sprite_fx.attach_particle(
                            *engine, damaged_id, "BanGoRi34", 1, 0.0f, 0.0f, 0.0f);
                        sprite_fx.follow_all(damaged_id, fx_x, fx_y, fx_z);
                    }
                }
                bool rest = false;
                if (net.take_rest_state(rest)) {
                    local_player.set_resting(rest);
                    if (rest) {
                        local_player.set_boosting(false);
                        local_player.clear_click_target();
                    }
                }
                {
                    std::uint8_t open_sec = 0;
                    if (net.take_chest_open_start(open_sec)) {
                        (void) open_sec;
                        if (pending_chest_id != 0) {
                            opening_chest_id = pending_chest_id;
                        } else if (selected_mob_id != 0) {
                            opening_chest_id = selected_mob_id;
                        }
                        pending_chest_id = 0;
                        chest_open_retry = 0.0f;
                        const auto target = net.monsters().find(opening_chest_id);
                        if (target != net.monsters().end() &&
                            scfg.plants.find_type(target->second.index()) != nullptr) {
                            player_actor.trigger_harvest();
                        } else {
                            player_actor.trigger_open_box();
                        }
                        auto_attack = false;
                        local_player.clear_click_target();
                        if (client::DebugOverlay::enabled()) {
                            log::info("chest open_start id=" + std::to_string(opening_chest_id));
                        }
                    }
                    if (net.take_chest_open_stop()) {
                        if (client::DebugOverlay::enabled()) {
                            log::info("chest open_stop fb=" +
                                      std::to_string(net.last_chest_feedback_msg()) +
                                      " id=" + std::to_string(opening_chest_id));
                        }
                        opening_chest_id = 0;
                        pending_chest_id = 0;
                        chest_open_retry = 0.0f;
                        if (net.last_chest_feedback_msg() !=
                                static_cast<std::uint8_t>(protocol::ItemMonsterMsg::Opened) ||
                            !player_actor.harvesting()) {
                            player_actor.clear_open_box();
                        }
                    }
                }
                // Intentional cancel is WASD / ground-click only. Residual walk-in
                // movement must not abort open (server also cancels on MoveOn — sync is
                // frozen while opening).
                if (net.take_local_skill_cast()) {
                    // Local presentation is already triggered when the C2S cast is sent.
                    log::debug("consumed S2C local skill-cast ack");
                }
                protocol::PlayerEmote emote;
                while (net.take_emote(emote)) {
                    if (emote.player_id == net.local_player_id()) {
                        if (!local_player.dead() && !local_player.resting() &&
                            !local_player.moving() && player_actor.trigger_emote(emote.motion)) {
                            sync_weapon_mix_fx();
                        }
                    } else if (auto* remote = remote_players.find(emote.player_id)) {
                        if (!remote->moving()) {
                            (void) remote->actor().trigger_emote(emote.motion);
                        }
                    }
                }
                client::PendingSkillAction skill_act;
                while (net.take_skill_action(skill_act)) {
                    // Local prep/fire FX already started in begin_skill_cast / fire_skill_now.
                    if (!read_only_session && skill_act.caster_id == net.local_player_id() &&
                        net.local_player_id() != 0) {
                        continue;
                    }
                    const auto monster_caster = net.monsters().find(skill_act.caster_id);
                    const client::SkillDef* monster_skill =
                        monster_caster != net.monsters().end()
                            ? skill_catalog.find(-2, static_cast<int>(skill_act.skill))
                            : nullptr;
                    if (monster_caster != net.monsters().end() &&
                        monster_caster->second.index() == 213) {
                        const char* doggebi_fx = nullptr;
                        switch (skill_act.skill) {
                        case 0:
                            doggebi_fx = skill_act.fire ? "Doggaebi_11" : "Doggaebi_05";
                            break;
                        case 1:
                            doggebi_fx = skill_act.fire ? "Doggaebi_46" : "Doggaebi_45";
                            break;
                        case 2:
                            doggebi_fx = skill_act.fire ? "Doggaebi_24" : "Doggaebi_23";
                            break;
                        case 3:
                            doggebi_fx = skill_act.fire ? "Doggaebi_01" : "Doggaebi_19";
                            break;
                        case 4:
                            doggebi_fx = skill_act.fire ? "Doggaebi_12" : "Doggaebi_16";
                            break;
                        case 5:
                            doggebi_fx = skill_act.fire ? "Doggaebi_17" : "Doggaebi_13";
                            break;
                        case 6:
                            doggebi_fx = skill_act.fire ? "Doggaebi_07" : "Doggaebi_10";
                            break;
                        case 7:
                            doggebi_fx = skill_act.fire ? "Doggaebi_21" : "Doggaebi_50";
                            break;
                        case 8:
                            doggebi_fx = skill_act.fire ? "Doggaebi_47" : "Doggaebi_48";
                            break;
                        case 13:
                            doggebi_fx = skill_act.fire ? "Doggaebi_57" : nullptr;
                            break;
                        default:
                            break;
                        }
                        if (doggebi_fx != nullptr) {
                            float fx_x = monster_caster->second.wx();
                            float fx_y = mob_world_y(fx_x, monster_caster->second.wz());
                            float fx_z = monster_caster->second.wz();
                            std::uint32_t follow_entity = skill_act.caster_id;
                            if (skill_act.skill == 2 && skill_act.fire) {
                                const auto summon_target = net.monsters().find(skill_act.target_id);
                                if (summon_target != net.monsters().end()) {
                                    fx_x = summon_target->second.wx();
                                    fx_z = summon_target->second.wz();
                                    follow_entity = skill_act.target_id;
                                }
                            } else if (skill_act.skill == 13 && skill_act.fire) {
                                player_impact_world(skill_act.target_id, fx_x, fx_y, fx_z);
                                follow_entity = skill_act.target_id;
                            }
                            const auto sounds = sprite_fx.spawn_oneshot_fx(
                                *engine, doggebi_fx, fx_x, fx_y, fx_z, follow_entity);
                            for (const std::string& sound : sounds) {
                                combat_fx.play_sfx(sound, fx_x, fx_y, fx_z);
                            }
                        }
                        continue;
                    }
                    if (monster_caster != net.monsters().end()) {
                        const auto visual = monster_instances.find(skill_act.caster_id);
                        const auto rig = monster_rig_cache.find(monster_caster->second.index());
                        if (visual != monster_instances.end() && rig != monster_rig_cache.end()) {
                            const int motion =
                                skill_act.fire ? (monster_skill ? monster_skill->motion_fire() : 0)
                                               : (monster_skill ? monster_skill->motion_prep() : 0);
                            const auto* macro =
                                monster_catalog.find_rig(monster_caster->second.index());
                            if (macro != nullptr &&
                                rig->second.ensure_skill(profile.data_root(), *macro, motion)) {
                                visual->second.trigger_skill(motion);
                            } else {
                                // Preserve a visible attack when an older asset
                                // pack has no Action 7 clip for this monster.
                                if (macro != nullptr) {
                                    (void) rig->second.ensure_attack(profile.data_root(), *macro);
                                }
                                visual->second.trigger_attack();
                            }
                        }
                    }
                    int caster_job = static_cast<int>(local_player.clazz());
                    if (monster_caster != net.monsters().end()) {
                        // Monster skill indices overlap player skills. Resolve their
                        // own catalog row so Thunder Strike is not presented as a
                        // player Mage/Archer skill with the same number.
                        caster_job = -2;
                    }
                    auto* remote_caster = remote_players.find(skill_act.caster_id);
                    if (remote_caster) {
                        caster_job = static_cast<int>(remote_caster->clazz());
                    } else if (monster_caster == net.monsters().end() &&
                               !(read_only_session &&
                                 skill_act.caster_id == net.local_player_id())) {
                        // Never borrow the local player's class or position for an unknown caster.
                        continue;
                    }
                    client::SkillPresentEndpoints ends;
                    ends.valid = true;
                    ends.caster_entity = skill_act.caster_id;
                    ends.target_entity = skill_act.target_id;
                    auto mit = net.monsters().find(skill_act.target_id);
                    if (mit != net.monsters().end()) {
                        float impact_scale = 1.0f;
                        resolve_monster_impact(skill_act.target_id,
                                               mit->second,
                                               ends.hit_x,
                                               ends.hit_y,
                                               ends.hit_z,
                                               impact_scale);
                        ends.impact_scale = impact_scale;
                    } else {
                        player_impact_world(
                            skill_act.target_id, ends.hit_x, ends.hit_y, ends.hit_z);
                    }
                    if (remote_caster && skill_act.target_id == 0) {
                        // Untargeted player skills attach to their caster.
                        remote_caster->spine_world(ends.hit_x, ends.hit_y, ends.hit_z);
                        ends.target_entity = skill_act.caster_id;
                    }
                    fill_target_origin(ends.target_entity, ends, false);
                    fill_caster_origin(skill_act.caster_id, ends);
                    if (monster_caster != net.monsters().end()) {
                        ends.caster_origin_x = monster_caster->second.wx();
                        ends.caster_origin_y =
                            mob_world_y(monster_caster->second.wx(), monster_caster->second.wz());
                        ends.caster_origin_z = monster_caster->second.wz();
                        if (!resolve_monster_effect_tip(skill_act.caster_id,
                                                        monster_caster->second,
                                                        ends.tip_x,
                                                        ends.tip_y,
                                                        ends.tip_z)) {
                            ends.tip_x = ends.caster_origin_x;
                            ends.tip_y = ends.caster_origin_y;
                            ends.tip_z = ends.caster_origin_z;
                        }
                    }
                    const int phase_i = skill_act.fire ? 1 : 0;
                    const int job = effect_job_for_skill(
                        static_cast<int>(skill_act.skill), phase_i, caster_job);
                    const auto* def =
                        protocol::is_animal_skill(static_cast<int>(skill_act.skill))
                            ? skill_catalog.find_any(static_cast<int>(skill_act.skill))
                        : monster_skill != nullptr
                            ? monster_skill
                            : skill_catalog.find(job, static_cast<int>(skill_act.skill));
                    if (read_only_session && skill_act.caster_id == net.local_player_id()) {
                        if (skill_act.target_id != 0 &&
                            skill_act.target_id != skill_act.caster_id) {
                            face_world_xz(ends.hit_x, ends.hit_z);
                        }
                        if (skill_act.fire) {
                            stop_skill_cast_sfx();
                        } else {
                            play_skill_cast_sfx(def);
                        }
                        if (def) {
                            const int motion =
                                skill_act.fire ? def->motion_fire() : def->motion_prep();
                            if (motion != 0) {
                                player_actor.trigger_skill_motion(motion, true);
                            } else if (skill_act.fire) {
                                player_actor.end_skill();
                            }
                        }
                    } else if (remote_caster) {
                        remote_caster->set_combat_target(skill_act.target_id);
                        if (skill_act.target_id != 0 &&
                            skill_act.target_id != skill_act.caster_id) {
                            remote_caster->face_target(ends.hit_x, ends.hit_z);
                        }
                        auto& actor = remote_caster->actor();
                        remote_caster->present_skill(*engine, def, skill_act.skill, skill_act.fire);
                        ends.tip_x = remote_caster->wx();
                        ends.tip_y = remote_caster->wy() + 16.0f;
                        ends.tip_z = remote_caster->wz();
                        (void) actor.effect_tip_world(
                            remote_caster->model_matrix(), ends.tip_x, ends.tip_y, ends.tip_z);
                        auto loop = remote_skill_loops.find(skill_act.caster_id);
                        if (loop != remote_skill_loops.end()) {
                            sfx.stop(loop->second);
                            remote_skill_loops.erase(loop);
                        }
                        if (!skill_act.fire && def &&
                            client::Audio::parse_name(def->sound(0)).looping()) {
                            remote_skill_loops[skill_act.caster_id] = sfx.play(def->sound(0),
                                                                               remote_caster->wx(),
                                                                               remote_caster->wy(),
                                                                               remote_caster->wz());
                        }
                    }
                    const auto phase = skill_act.fire ? client::SkillPresentPhase::Fire
                                                      : client::SkillPresentPhase::Prep;
                    (void) skill_presenter.present(
                        *engine, job, static_cast<int>(skill_act.skill), phase, def, ends);
                }
                client::PendingTransformAction tf{};
                while (net.take_transform_action(tf)) {
                    const bool local =
                        tf.entity_id == net.local_player_id() || net.local_player_id() == 0;
                    if (tf.pre) {
                        const int stage = static_cast<int>(tf.arg);
                        const auto* def =
                            skill_catalog.find_any(protocol::transform_into_skill(stage));
                        const int sub = (def && def->motion_fire() != 0) ? def->motion_fire() : 64;
                        if (local) {
                            pending_transform_execute_stage = stage;
                            pending_transform_execute_left = 1.0f;
                            player_actor.trigger_skill_motion(sub, true);
                        } else if (client::RemotePlayer* remote =
                                       remote_players.find(tf.entity_id)) {
                            remote->actor().trigger_skill_motion(sub, true);
                        }
                        continue;
                    }
                    const int race = protocol::transform_tflag_race(static_cast<int>(tf.arg));
                    const int stage = protocol::transform_tflag_stage(static_cast<int>(tf.arg));
                    if (local) {
                        pending_transform_execute_stage = 0;
                        pending_transform_execute_left = 0.0f;
                        apply_local_metamo(stage > 0 ? race : -1, stage);
                    } else {
                        client::RemotePlayer* remote = remote_players.find(tf.entity_id);
                        if (remote) {
                            const client::ShinsuInfo* info =
                                stage > 0 ? shinsu_db.find(race, stage) : nullptr;
                            const float scale = info ? info->scale() : 1.0f;
                            (void) remote->actor().apply_metamo(*engine,
                                                                prop_tex_cache,
                                                                stage > 0 ? race : -1,
                                                                stage,
                                                                scale,
                                                                &remote->wear(),
                                                                &item_catalog);
                            sprite_fx.spawn_oneshot_particle(*engine,
                                                             "metamo_fog",
                                                             remote->wx(),
                                                             remote->wy(),
                                                             remote->wz(),
                                                             tf.entity_id);
                        }
                    }
                }
                std::uint32_t hit_from = 0;
                if (net.take_hit_from(hit_from)) {
                    // Incoming monster hits must not turn the local player or cancel
                    // click-walk. Facing the target is only for the player's own swing
                    // (try_basic_attack / skills) and for the attacking monster.
                    (void) hit_from;
                }
            }
            int snap_sx = 0, snap_sy = 0, snap_sz = 0;
            if (net.take_move_before(snap_sx, snap_sy, snap_sz)) {
                const float snap_x = coords::to_client(snap_sx, profile.origin_patch());
                const float snap_z = coords::to_client(snap_sy, profile.origin_patch());
                local_player.set_pos(snap_x, coords::to_client_y(snap_sz), snap_z);
                local_player.clear_click_target();
            }
            int tele_map = 0, tele_sx = 0, tele_sy = 0, tele_sz = 0;
            if (net.take_teleport(tele_map, tele_sx, tele_sy, tele_sz)) {
                local_map = tele_map;
                if (local_map != 3 && doggebi_ritual_active_mask != 0) {
                    doggebi_ritual_active_mask = 0;
                    sync_doggebi_ritual_fx();
                }
                if (local_map != 3) {
                    doggebi_countdown_timer = protocol::MapStateTimer::None;
                    doggebi_countdown_seconds = 0;
                }
                apply_city_teleport(tele_sx, tele_sy, tele_sz);
                if (!net.send_teleport_ack(tele_sx, tele_sy, tele_sz)) {
                    log::warn("failed to send teleport ack (" + std::to_string(tele_sx) + "," +
                              std::to_string(tele_sy) + ")");
                }
                log::info("teleport applied (" + std::to_string(tele_sx) + "," +
                          std::to_string(tele_sy) + ")");
            }
            if (read_only_session && net.observed_pose_ready()) {
                const auto& motion = net.observed_motion();
                local_player.apply_observed_pose(
                    motion.position(), motion.yaw(), motion.moving(), floor);
            }
            expire_npc_dialog();
            // Advance the equipped companion as an independent follower. Pet asset
            // paths are selected from the JSON catalog and visual definitions,
            // never from the old text configuration.
            const int equipped_pet = player_wear.pet_index();
            const auto pet_def = pet_presentations.find(equipped_pet);
            if (pet_def == pet_presentations.end() ||
                (pet_def->second.visual_key <= 0 && pet_def->second.monster_type <= 0)) {
                if (pet_rendered_item_index != 0) {
                    pet_instance.destroy(*engine);
                    pet_rendered_item_index = 0;
                }
                pet_controller.clear();
                pet_controller_item_index = 0;
            } else {
                const client::PetOwnerState pet_owner{local_player.pos().x,
                                                      local_player.pos().z,
                                                      local_player.yaw(),
                                                      local_player.walk_speed(),
                                                      local_player.run_speed()};
                if (pet_controller_item_index != equipped_pet) {
                    if (pet_rendered_item_index != 0 && pet_rendered_item_index != equipped_pet) {
                        pet_instance.destroy(*engine);
                        pet_rendered_item_index = 0;
                    }
                    pet_controller.reset(pet_owner);
                    pet_controller_item_index = equipped_pet;
                }
                pet_pickup_targets.clear();
                if (pet_def->second.auto_pickup) {
                    pet_pickup_targets.reserve(net.ground_items().size());
                    for (const auto& [id, item] : net.ground_items()) {
                        pet_pickup_targets.push_back({id, item.wx, item.wz});
                    }
                }
                const client::PetUpdateResult pet_update = pet_controller.update(
                    input.dt, pet_owner, pet_def->second.auto_pickup, pet_pickup_targets);
                if (pet_update.pickup_item_id != 0 && net.connected()) {
                    const auto item = net.ground_items().find(pet_update.pickup_item_id);
                    if (item != net.ground_items().end()) {
                        const int sx = coords::to_server(item->second.wx, profile.origin_patch());
                        const int sy = coords::to_server(item->second.wz, profile.origin_patch());
                        int tx = 0;
                        int ty = 0;
                        coords::pixel_to_tile(sx, sy, tx, ty);
                        (void) net.send_pickup_item(pet_update.pickup_item_id, tx, ty);
                    }
                }

                const int visual_key = pet_def->second.visual_key;
                const client::MonsterRigGpu* active_rig = nullptr;
                if (pet_def->second.monster_type > 0) {
                    const int monster_type = pet_def->second.monster_type;
                    auto rig = monster_rig_cache.find(monster_type);
                    if (rig == monster_rig_cache.end()) {
                        const client::MonsterRig* macro = monster_catalog.find_rig(monster_type);
                        if (macro != nullptr) {
                            client::MonsterRigGpu loaded;
                            const client::MonsterBoneInfo* bones =
                                monster_catalog.find_bone(macro->bone_no());
                            if (client::MonsterRigGpu::load(*engine,
                                                            profile.data_root(),
                                                            monster_type,
                                                            *macro,
                                                            loaded,
                                                            prop_tex_cache,
                                                            bones)) {
                                rig = monster_rig_cache.emplace(monster_type, std::move(loaded))
                                          .first;
                            }
                        }
                    }
                    if (rig != monster_rig_cache.end()) {
                        active_rig = &rig->second;
                    }
                } else {
                    auto rig = pet_rig_cache.find(visual_key);
                    if (rig == pet_rig_cache.end()) {
                        // The visual key (or its variant base) selects Pxxx clothes,
                        // while Petinfo.bone independently selects the Tpxxx rig.
                        const int mesh_key =
                            pet_def->second.mesh_key > 0 ? pet_def->second.mesh_key : visual_key;
                        char clothes_stem_name[16];
                        std::snprintf(
                            clothes_stem_name, sizeof(clothes_stem_name), "P%03d", mesh_key);
                        char motion_stem_name[16];
                        std::snprintf(motion_stem_name,
                                      sizeof(motion_stem_name),
                                      "Tp%03d",
                                      pet_def->second.bone);
                        const std::string clothes_dir =
                            platform::join_path(profile.data_root(), "Pet/Clothes");
                        const std::string motion_dir =
                            platform::join_path(profile.data_root(), "Pet/Motion");
                        std::vector<client::PetMeshAsset> clothes;
                        if (!pet_def->second.mesh_assets.empty()) {
                            for (const PetPresentation::MeshAsset& asset :
                                 pet_def->second.mesh_assets) {
                                const std::string path =
                                    platform::resolve_path_case(clothes_dir, asset.mesh_name);
                                if (platform::file_exists(path)) {
                                    clothes.push_back({path, asset.texture_name});
                                }
                            }
                        } else {
                            // Petinfo.part stores the number of sequential clothes
                            // meshes. For example, part 3 uses b1, b2, and b3.
                            const int part_count = std::max(1, pet_def->second.part_count);
                            for (int part = 1; part <= part_count; ++part) {
                                const std::string path = platform::resolve_path_case(
                                    clothes_dir,
                                    std::string(clothes_stem_name) + "_b" + std::to_string(part) +
                                        ".gb");
                                if (platform::file_exists(path)) {
                                    clothes.push_back({path, {}});
                                }
                            }
                        }
                        const std::string skeleton_file = platform::resolve_path_case(
                            motion_dir, std::string(motion_stem_name) + "_Bone.gb");
                        const std::string idle_file = platform::resolve_path_case(
                            motion_dir, std::string(motion_stem_name) + "_0_A_01.gb");
                        const std::string move_file = platform::resolve_path_case(
                            motion_dir, std::string(motion_stem_name) + "_2_A_01.gb");
                        client::MonsterRigGpu loaded;
                        if (client::MonsterRigGpu::load_pet(*engine,
                                                            visual_key,
                                                            pet_def->second.scale,
                                                            skeleton_file,
                                                            idle_file,
                                                            move_file,
                                                            move_file,
                                                            clothes,
                                                            loaded,
                                                            prop_tex_cache)) {
                            rig = pet_rig_cache.emplace(visual_key, std::move(loaded)).first;
                        }
                    }
                    if (rig != pet_rig_cache.end()) {
                        active_rig = &rig->second;
                    }
                }
                if (active_rig != nullptr && pet_rendered_item_index != equipped_pet) {
                    if (pet_rendered_item_index != 0) {
                        pet_instance.destroy(*engine);
                    }
                    if (pet_instance.create_from_rig(*engine, *active_rig, false)) {
                        pet_rendered_item_index = equipped_pet;
                    }
                }
                if (active_rig != nullptr && pet_rendered_item_index == equipped_pet) {
                    const bool pet_running =
                        pet_update.moving &&
                        (pet_def->second.monster_type > 0 || pet_update.running);
                    pet_instance.set_yaw(pet_controller.yaw());
                    (void) pet_instance.update(
                        *engine,
                        *active_rig,
                        input.dt,
                        pet_update.moving,
                        pet_running,
                        &sfx,
                        pet_controller.x(),
                        floor.sample(pet_controller.x(), pet_controller.z(), local_player.pos().y),
                        pet_controller.z(),
                        true);
                }
            }

            // Common riding animals use their own Ride asset family and keep the
            // player origin on the configured saddle bone while mounted.
            const int riding_item = player_wear.ride_index();
            const auto riding_def = riding_presentations.find(riding_item);
            if (!local_riding_mounted || riding_def == riding_presentations.end()) {
                if (riding_rendered_item_index != 0) {
                    riding_instance.destroy(*engine);
                    riding_rendered_item_index = 0;
                }
                local_riding_offset_y = 0.0f;
            } else {
                auto rig = riding_rig_cache.find(riding_def->second.visual_key);
                if (rig == riding_rig_cache.end()) {
                    char clothes_stem[16];
                    std::snprintf(
                        clothes_stem, sizeof(clothes_stem), "A%03d", riding_def->second.visual_key);
                    char motion_stem[16];
                    std::snprintf(
                        motion_stem, sizeof(motion_stem), "Ta%03d", riding_def->second.bone);
                    const std::string clothes_dir =
                        platform::join_path(profile.data_root(), "Ride/Clothes");
                    const std::string motion_dir =
                        platform::join_path(profile.data_root(), "Ride/Motion");
                    std::vector<client::PetMeshAsset> clothes;
                    for (int part = 1; part <= riding_def->second.part_count; ++part) {
                        const std::string path = platform::resolve_path_case(
                            clothes_dir,
                            std::string(clothes_stem) + "_b" + std::to_string(part) + ".gb");
                        if (platform::file_exists(path)) {
                            clothes.push_back({path, {}});
                        }
                    }
                    const std::string skeleton_file = platform::resolve_path_case(
                        motion_dir, std::string(motion_stem) + "_Bone.gb");
                    const std::string idle_file = platform::resolve_path_case(
                        motion_dir, std::string(motion_stem) + "_0_A_01.gb");
                    // Riding animals author only idle (0) and run (2) motions.
                    const std::string walk_file;
                    const std::string run_file = platform::resolve_path_case(
                        motion_dir, std::string(motion_stem) + "_2_A_01.gb");
                    client::MonsterRigGpu loaded;
                    if (client::MonsterRigGpu::load_pet(*engine,
                                                        riding_item,
                                                        riding_def->second.scale,
                                                        skeleton_file,
                                                        idle_file,
                                                        walk_file,
                                                        run_file,
                                                        clothes,
                                                        loaded,
                                                        prop_tex_cache)) {
                        rig = riding_rig_cache
                                  .emplace(riding_def->second.visual_key, std::move(loaded))
                                  .first;
                    }
                }
                if (rig != riding_rig_cache.end()) {
                    if (riding_rendered_item_index != riding_item) {
                        if (riding_rendered_item_index != 0) {
                            riding_instance.destroy(*engine);
                        }
                        if (riding_instance.create_from_rig(*engine, rig->second, false)) {
                            riding_rendered_item_index = riding_item;
                        }
                    }
                    if (riding_rendered_item_index == riding_item) {
                        riding_instance.set_yaw(local_player.yaw());
                        (void) riding_instance.update(*engine,
                                                      rig->second,
                                                      input.dt,
                                                      local_player.moving(),
                                                      local_player.running(),
                                                      &sfx,
                                                      local_player.pos().x,
                                                      local_player.pos().y,
                                                      local_player.pos().z,
                                                      true);
                        const float half_yaw = local_player.yaw() * 0.5f;
                        const auto rotation = engine::Mat4::from_quaternion(
                            0.0f, std::sin(half_yaw), 0.0f, std::cos(half_yaw));
                        const auto scale = engine::Mat4::scaling(
                            rig->second.scale(), rig->second.scale(), rig->second.scale());
                        const auto translation = engine::Mat4::translation(
                            local_player.pos().x, local_player.pos().y, local_player.pos().z);
                        const auto model = engine::Mat4::multiply(
                            translation, engine::Mat4::multiply(rotation, scale));
                        float seat_x = 0.0f;
                        float seat_y = 0.0f;
                        float seat_z = 0.0f;
                        if (riding_def->second.seat_bone >= 0 &&
                            riding_instance.bone_world(rig->second,
                                                       model,
                                                       riding_def->second.seat_bone,
                                                       seat_x,
                                                       seat_y,
                                                       seat_z)) {
                            local_riding_offset_y = seat_y - local_player.pos().y;
                        } else {
                            local_riding_offset_y = riding_def->second.rider_offset_y;
                        }
                    }
                }
            }

            // Create / refresh visual instances for networked monsters (nearby only).
            constexpr float kMonsterCullDist = 4000.0f;
            constexpr float kMonsterCullDist2 = kMonsterCullDist * kMonsterCullDist;
            constexpr float kMonsterFullRateSkinDist = 800.0f;
            constexpr float kMonsterFullRateSkinDist2 =
                kMonsterFullRateSkinDist * kMonsterFullRateSkinDist;
            constexpr float kMonsterHalfRateSkinDist = 1600.0f;
            constexpr float kMonsterHalfRateSkinDist2 =
                kMonsterHalfRateSkinDist * kMonsterHalfRateSkinDist;
            for (const auto& [id, nm] : net.monsters()) {
                const float mdx = nm.wx() - local_player.pos().x;
                const float mdz = nm.wz() - local_player.pos().z;
                const float mdist2 = mdx * mdx + mdz * mdz;
                if (mdist2 > kMonsterCullDist2) {
                    auto far = monster_instances.find(id);
                    if (far != monster_instances.end() && !far->second.dying()) {
                        detach_monster_visual(id);
                        far->second.destroy(*engine);
                        monster_instances.erase(far);
                    }
                    continue;
                }
                auto it = monster_instances.find(id);
                if (it == monster_instances.end() || it->second.index() != nm.index()) {
                    if (it != monster_instances.end()) {
                        detach_monster_visual(id);
                        it->second.destroy(*engine);
                    }
                    auto rit = monster_rig_cache.find(nm.index());
                    if (rit == monster_rig_cache.end()) {
                        const auto* found = monster_catalog.find_rig(nm.index());
                        if (found == nullptr) {
                            continue;
                        }
                        client::MonsterRigGpu loaded;
                        const client::MonsterBoneInfo* bone_info =
                            monster_catalog.find_bone(found->bone_no());
                        if (client::MonsterRigGpu::load(*engine,
                                                        profile.data_root(),
                                                        nm.index(),
                                                        *found,
                                                        loaded,
                                                        prop_tex_cache,
                                                        bone_info,
                                                        false,
                                                        repo_root)) {
                            rit = monster_rig_cache.emplace(nm.index(), std::move(loaded)).first;
                        }
                    }
                    if (rit != monster_rig_cache.end()) {
                        client::MonsterInstance inst;
                        if (inst.create_from_rig(*engine, rit->second, !nm.kneeling())) {
                            monster_instances[id] = std::move(inst);
                            attach_loop_fx(
                                id, rit->second, nm.wx(), mob_world_y(nm.wx(), nm.wz()), nm.wz());
                        }
                    }
                }
                auto vit = monster_instances.find(id);
                if (vit == monster_instances.end()) {
                    continue;
                }
                auto rgit = monster_rig_cache.find(nm.index());
                if (rgit == monster_rig_cache.end()) {
                    continue;
                }
                int basic_attack_visual_motion = 0;
                if (const auto* macro = monster_catalog.find_rig(nm.index())) {
                    net.set_monster_mspeed(id, macro->walk_tick(), macro->run_tick());
                    basic_attack_visual_motion = macro->basic_attack_visual_motion();
                }
                vit->second.set_yaw(nm.yaw());
                if (net.take_monster_attack(id)) {
                    const auto* macro = monster_catalog.find_rig(nm.index());
                    if (basic_attack_visual_motion > 0 && macro != nullptr &&
                        rgit->second.ensure_skill(
                            profile.data_root(), *macro, basic_attack_visual_motion)) {
                        vit->second.trigger_basic_attack_visual(basic_attack_visual_motion);
                    } else {
                        if (macro != nullptr) {
                            (void) rgit->second.ensure_attack(profile.data_root(), *macro);
                        }
                        vit->second.trigger_attack();
                    }
                    client::PendingMonsterActionDamage pad;
                    if (net.take_monster_action_damage(id, pad)) {
                        client::MonsterActionDamage ad;
                        ad.set_damage(pad.damage);
                        ad.set_flags(pad.flags);
                        ad.set_target_id(pad.target_id);
                        vit->second.latch_action_damage(ad);
                    }
                }
                if (net.take_monster_knee(id)) {
                    vit->second.trigger_knee();
                    if (selected_mob_id == id) {
                        auto_attack = false;
                    }
                }
                if (net.take_monster_die(id)) {
                    vit->second.trigger_die();
                }
                if (net.take_monster_behead(id)) {
                    if (selected_mob_id == id) {
                        // Keep the initial movement lock when the corpse disappears.
                        selected_mob_id = 0;
                        auto_attack = false;
                        pending_arrow_target = 0;
                    }
                    client::MonsterRig macro{};
                    if (const auto* found = monster_catalog.find_rig(nm.index())) {
                        macro = *found;
                    }
                    auto brit = monster_behead_rig_cache.find(nm.index());
                    if (brit == monster_behead_rig_cache.end()) {
                        client::MonsterRigGpu loaded;
                        const client::MonsterBoneInfo* bone_info =
                            monster_catalog.find_bone(macro.bone_no());
                        if (client::MonsterRigGpu::load(*engine,
                                                        profile.data_root(),
                                                        nm.index(),
                                                        macro,
                                                        loaded,
                                                        prop_tex_cache,
                                                        bone_info,
                                                        true)) {
                            brit = monster_behead_rig_cache.emplace(nm.index(), std::move(loaded))
                                       .first;
                        }
                    }
                    if (brit != monster_behead_rig_cache.end()) {
                        vit->second.destroy(*engine);
                        if (vit->second.create_from_rig(*engine, brit->second, false)) {
                            rgit = monster_rig_cache.find(nm.index());
                            (void) rgit;
                        }
                        const client::MonsterRigGpu* behead_rig = &brit->second;
                        vit->second.trigger_behead();
                        float hx = nm.wx();
                        float hy = mob_world_y(nm.wx(), nm.wz()) + 12.0f;
                        float hz = nm.wz();
                        const float hyaw = nm.yaw() * 0.5f;
                        const auto R = engine::Mat4::from_quaternion(
                            0.0f, std::sin(hyaw), 0.0f, std::cos(hyaw));
                        const auto S = engine::Mat4::scaling(
                            behead_rig->scale(), behead_rig->scale(), behead_rig->scale());
                        const auto T = engine::Mat4::translation(
                            nm.wx(), mob_world_y(nm.wx(), nm.wz()), nm.wz());
                        const auto model = engine::Mat4::multiply(T, engine::Mat4::multiply(R, S));
                        (void) vit->second.bone_world(
                            *behead_rig, model, behead_rig->head_bone(), hx, hy, hz);
                        combat_fx.spawn_behead(*engine, hx, hy, hz);
                    } else {
                        vit->second.trigger_behead();
                    }
                }
                const bool dying = vit->second.dying();
                const client::MonsterRigGpu* anim_rig = &rgit->second;
                auto brit = monster_behead_rig_cache.find(nm.index());
                if (vit->second.beheading() && brit != monster_behead_rig_cache.end()) {
                    anim_rig = &brit->second;
                }
                vit->second.set_ice_slowed(nm.ice_slowed());
                std::uint64_t skin_divisor = 1;
                if (id != selected_mob_id && mdist2 > kMonsterHalfRateSkinDist2) {
                    skin_divisor = 4;
                } else if (id != selected_mob_id && mdist2 > kMonsterFullRateSkinDist2) {
                    skin_divisor = 2;
                }
                const bool skin_this_frame =
                    skin_divisor == 1 || (monster_skin_frame + id) % skin_divisor == 0;
                if (vit->second.update(*engine,
                                       *anim_rig,
                                       input.dt * nm.anim_speed_scale(),
                                       !dying && nm.moving(),
                                       !dying && nm.running(),
                                       &sfx,
                                       nm.wx(),
                                       mob_world_y(nm.wx(), nm.wz()) + vit->second.sink_y(),
                                       nm.wz(),
                                       skin_this_frame)) {
                    ++monster_skin_updates;
                }
                {
                    const float wy = mob_world_y(nm.wx(), nm.wz()) + vit->second.sink_y();
                    const float hyaw = vit->second.yaw() * 0.5f;
                    const auto R =
                        engine::Mat4::from_quaternion(0.0f, std::sin(hyaw), 0.0f, std::cos(hyaw));
                    const auto S = engine::Mat4::scaling(
                        anim_rig->scale(), anim_rig->scale(), anim_rig->scale());
                    const auto T = engine::Mat4::translation(nm.wx(), wy, nm.wz());
                    const auto model = engine::Mat4::multiply(T, engine::Mat4::multiply(R, S));
                    attach_loop_fx(id, *anim_rig, nm.wx(), wy, nm.wz());
                    follow_loop_fx(id, vit->second, *anim_rig, model);
                    combat_fx.follow_clip_entity(id, nm.wx(), wy, nm.wz());
                    if (nm.move_stopped()) {
                        if (freeze_fx_armed.insert(id).second) {
                            const auto freeze_sounds = sprite_fx.spawn_oneshot_fx(
                                *engine, "effect_ef083a", nm.wx(), wy, nm.wz());
                            for (const auto& sound : freeze_sounds) {
                                combat_fx.play_sfx(sound, nm.wx(), wy, nm.wz());
                            }
                            combat_fx.spawn_clip_model(*engine,
                                                       "effect_ef083a1",
                                                       nm.wx(),
                                                       wy,
                                                       nm.wz(),
                                                       vit->second.yaw(),
                                                       0.0f,
                                                       0.0f,
                                                       id,
                                                       true);
                        }
                        combat_fx.follow_clip_entity(id, nm.wx(), wy, nm.wz());
                    } else if (freeze_fx_armed.erase(id)) {
                        combat_fx.kill_clip_entity(id);
                        const auto shatter_sounds = sprite_fx.spawn_oneshot_fx(
                            *engine, "effect_ef083a2", nm.wx(), wy, nm.wz());
                        for (const auto& sound : shatter_sounds) {
                            combat_fx.play_sfx(sound, nm.wx(), wy, nm.wz());
                        }
                        combat_fx.spawn_clip_model(
                            *engine, "effect_ef083a2", nm.wx(), wy, nm.wz(), vit->second.yaw());
                    }
                    spawn_monster_attack_fx(vit->second, *anim_rig, model, nm.wx(), wy, nm.wz());
                    client::MonsterActionDamage action_hit;
                    if (vit->second.take_action_damage(action_hit)) {
                        present_monster_action_damage(action_hit);
                    }
                    std::string idle_fx;
                    while (vit->second.take_idle_fx(idle_fx)) {
                        if (!sprite_fx.has_fx(idle_fx)) {
                            continue;
                        }
                        const auto sounds =
                            sprite_fx.spawn_oneshot_fx(*engine, idle_fx, nm.wx(), wy, nm.wz(), id);
                        sprite_fx.set_target_scale(sprite_fx.last_oneshot_key(), anim_rig->scale());
                        follow_loop_fx(id, vit->second, *anim_rig, model);
                        for (const auto& sound : sounds) {
                            const auto cue = client::Audio::parse_name(sound);
                            if (cue.looping()) {
                                continue;
                            }
                            (void) sfx.play(sound, nm.wx(), wy, nm.wz());
                        }
                    }
                }
            }
            std::vector<std::uint32_t> finished_corpses;
            for (const auto& [id, nm] : net.monsters()) {
                auto vit = monster_instances.find(id);
                if (vit != monster_instances.end() && vit->second.ready_to_despawn()) {
                    finished_corpses.push_back(id);
                }
            }
            for (const std::uint32_t id : finished_corpses) {
                auto vit = monster_instances.find(id);
                if (vit != monster_instances.end()) {
                    detach_monster_visual(id);
                    vit->second.destroy(*engine);
                    monster_instances.erase(vit);
                }
                net.forget_monster(id);
            }
            // Drop visuals for removed monsters.
            for (auto it = monster_instances.begin(); it != monster_instances.end();) {
                if (net.monsters().find(it->first) == net.monsters().end()) {
                    detach_monster_visual(it->first);
                    it->second.destroy(*engine);
                    it = monster_instances.erase(it);
                } else {
                    ++it;
                }
            }
            constexpr std::uint32_t kNpcFxBit = 0x80000000u;
            std::unordered_set<std::uint32_t> seen_npc_fx;
            const int npc_fx_sx = coords::to_server(local_player.pos().x, profile.origin_patch());
            const int npc_fx_sy = coords::to_server(local_player.pos().z, profile.origin_patch());
            for (const auto& s : npc_spawns) {
                if (!coords::in_sight(npc_fx_sx, npc_fx_sy, s.server_x(), s.server_y())) {
                    continue;
                }
                const client::NpcRig* rig = npc_catalog.find_rig(s.shape());
                if (!rig || (rig->particles().empty() && rig->fx().empty())) {
                    continue;
                }
                const std::uint32_t fid = kNpcFxBit | static_cast<std::uint32_t>(s.index());
                const float wx = coords::to_client(s.server_x(), profile.origin_patch());
                const float wz = coords::to_client(s.server_y(), profile.origin_patch());
                const float wy = coords::to_client_y(s.server_z());
                float head_y = wy + 17.0f;
                if (const auto* bone = npc_catalog.find_bone(rig->bone_no())) {
                    head_y = wy + bone->target_height();
                }
                if (!sprite_fx.has(fid)) {
                    for (const auto& p : rig->particles()) {
                        sprite_fx.attach_particle(
                            *engine, fid, p.name(), p.effect_bone(), p.ox(), p.oy(), p.oz());
                    }
                    for (const auto& fx : rig->fx()) {
                        sprite_fx.attach_fx(*engine, fid, fx.name(), fx.effect_bone());
                    }
                }
                sprite_fx.follow_bone(fid, -1, wx, wy, wz);
                sprite_fx.follow_bone(fid, 0, wx, wy, wz);
                sprite_fx.follow_bone(fid, 1, wx, wy + (head_y - wy) * 0.55f, wz);
                sprite_fx.follow_bone(fid, 2, wx, head_y, wz);
                seen_npc_fx.insert(fid);
            }
            for (const std::uint32_t id : npc_fx_live) {
                if (seen_npc_fx.find(id) == seen_npc_fx.end()) {
                    sprite_fx.detach(id);
                }
            }
            npc_fx_live.swap(seen_npc_fx);
        }
        if (indoor_view.loaded()) {
            cam.limit_indoor_zoom(local_player,
                                  floor,
                                  input.height > 0 ? static_cast<float>(input.width) / input.height
                                                   : 1.777f);
        } else {
            cam.clear_indoor_collision();
        }
        const engine::Vec3 cam_eye = cam.eye(local_player);
        const float aspect =
            input.height > 0 ? static_cast<float>(input.width) / static_cast<float>(input.height)
                             : 1.777f;
        const auto proj =
            engine::Mat4::perspective(60.0f * kPi / 180.0f, aspect, client::kNearZ, cam.clip_far());
        // Prepare the noticeboard before beginning the world framebuffer pass.
        // The authored board is about 270 units tall; humanoid NPCs are about 18 world units.
        constexpr float kEventBoardWorldScale = 0.1f;
        constexpr float kEventBoardSideOffset = 180.0f * kEventBoardWorldScale;
        const auto event_board_scale = engine::Mat4::scaling(
            kEventBoardWorldScale, kEventBoardWorldScale, kEventBoardWorldScale);
        std::vector<engine::Mat4> event_board_models;
        if (const auto& event = net.exp_event_state(); event && event->enabled) {
            for (const auto& [id, npc] : net.npcs()) {
                (void) id;
                if (npc.index() != event->manager_npc_index) {
                    continue;
                }
                float yaw = 0.0f;
                for (const auto& spawn : npc_spawns) {
                    if (spawn.index() == npc.index() && spawn.map() == local_map) {
                        yaw = client::NpcCatalog::yaw_from_dir(profile.origin_patch(),
                                                               spawn.server_x(),
                                                               spawn.server_y(),
                                                               spawn.dir_x(),
                                                               spawn.dir_y());
                        break;
                    }
                }
                const float wx = npc.wx() + kEventBoardSideOffset * std::cos(yaw);
                const float wz = npc.wz() - kEventBoardSideOffset * std::sin(yaw);
                const float dx = wx - local_player.pos().x;
                const float dz = wz - local_player.pos().z;
                if (dx * dx + dz * dz > 2200.0f * 2200.0f) {
                    continue;
                }
                const auto rotation = engine::Mat4::from_quaternion(
                    0.0f, std::sin(yaw * 0.5f), 0.0f, std::cos(yaw * 0.5f));
                event_board_models.push_back(engine::Mat4::multiply(
                    engine::Mat4::translation(wx, floor.get_floor(wx, wz), wz),
                    engine::Mat4::multiply(rotation, event_board_scale)));
            }
            if (!event_board_models.empty()) {
                const auto* item = item_catalog.find(event->donation_item_index);
                const std::string label = item ? item->name() : "event contributions";
                if (!exp_event_board.update(*event, label)) {
                    event_board_models.clear();
                }
            }
        }
        const auto world_draw_started = std::chrono::steady_clock::now();
        if (indoor_view.loaded()) {
            engine->begin_frame(0.06f, 0.06f, 0.07f);
        } else {
            engine->begin_frame(0.45f, 0.62f, 0.85f);
        }
        if (!indoor_view.loaded()) {
            // compatibility behavior: clear view translation so the cube is locked to the camera.
            engine::Mat4 sky_view = cam.view(local_player);
            sky_view.m[12] = 0.0f;
            sky_view.m[13] = 0.0f;
            sky_view.m[14] = 0.0f;
            engine->set_camera(sky_view, proj);
            sky_box.draw(*engine);
        }
        engine->set_camera(cam.view(local_player), proj);

        const auto view_proj = engine::Mat4::multiply(proj, cam.view(local_player));
        client::FrustumPlanes prop_frustum;
        client::WorldRenderCache::extract_frustum_planes(view_proj, prop_frustum);
        if (indoor_view.loaded()) {
            indoor_view.update_visibility(cam_eye.x,
                                          cam_eye.y,
                                          cam_eye.z,
                                          local_player.pos().x,
                                          local_player.pos().y + cam.look_height(),
                                          local_player.pos().z,
                                          prop_frustum);
        }
        const float kPatchCullRadius = kPatchWorld * 0.70710678f + 2000.0f;
        auto patch_in_frustum = [&](const client::WorldRenderCache::DrawnPatch& d) {
            float cx = 0.0f;
            float cz = 0.0f;
            patch_center_xz(d.patch_x, d.patch_y, cx, cz);
            return client::WorldRenderCache::frustum_contains_sphere(
                prop_frustum, cx, 0.0f, cz, kPatchCullRadius);
        };

        if (!indoor_view.loaded()) {
            for (const auto& d : drawn) {
                if (d.layers.empty() || !patch_in_frustum(d)) {
                    continue;
                }
                engine::TerrainLayerDraw layer;
                layer.color = d.layers[0].color;
                layer.detail = d.layers[0].detail;
                layer.repeat = d.layers[0].repeat;
                layer.alpha_blend = false;
                engine->draw_terrain_layer(d.mesh, layer);
            }
            for (const auto& d : drawn) {
                if (!patch_in_frustum(d)) {
                    continue;
                }
                for (std::size_t i = 1; i < d.layers.size(); ++i) {
                    engine::TerrainLayerDraw layer;
                    layer.color = d.layers[i].color;
                    layer.detail = d.layers[i].detail;
                    layer.repeat = d.layers[i].repeat;
                    layer.alpha_blend = true;
                    engine->draw_terrain_layer(d.mesh, layer);
                }
            }

            if (client_settings.render.grass_enabled && !decoration_models.empty()) {
                constexpr float kDecorationDistance = 3000.0f;
                constexpr float kDecorationDistance2 = kDecorationDistance * kDecorationDistance;
                std::unordered_map<const client::WorldRenderCache::GpuSubmesh*,
                                   std::vector<engine::Mat4>>
                    decoration_batches;
                for (const auto& d : drawn) {
                    const std::size_t cell_count =
                        static_cast<std::size_t>(map::kMapSize) * map::kMapSize;
                    const std::size_t height_count =
                        static_cast<std::size_t>(map::kHeightSamples) * map::kHeightSamples;
                    if (!patch_in_frustum(d) || d.deco_masks.size() != cell_count ||
                        d.heights.size() != height_count) {
                        continue;
                    }
                    const float patch_x = static_cast<float>((d.patch_x - map::kPatchGrid / 2) *
                                                             map::kMapSize * map::kTerrainCellSize);
                    const float patch_z = static_cast<float>((d.patch_y - map::kPatchGrid / 2) *
                                                             map::kMapSize * map::kTerrainCellSize);
                    const int min_x =
                        std::clamp(static_cast<int>((cam_eye.x - kDecorationDistance - patch_x) /
                                                    map::kTerrainCellSize),
                                   0,
                                   map::kMapSize - 1);
                    const int max_x =
                        std::clamp(static_cast<int>((cam_eye.x + kDecorationDistance - patch_x) /
                                                    map::kTerrainCellSize) +
                                       1,
                                   0,
                                   map::kMapSize - 1);
                    const int min_z =
                        std::clamp(static_cast<int>((cam_eye.z - kDecorationDistance - patch_z) /
                                                    map::kTerrainCellSize),
                                   0,
                                   map::kMapSize - 1);
                    const int max_z =
                        std::clamp(static_cast<int>((cam_eye.z + kDecorationDistance - patch_z) /
                                                    map::kTerrainCellSize) +
                                       1,
                                   0,
                                   map::kMapSize - 1);
                    for (int z = min_z; z <= max_z; ++z) {
                        for (int x = min_x; x <= max_x; ++x) {
                            const std::size_t cell =
                                static_cast<std::size_t>(z) * map::kMapSize + x;
                            const std::uint8_t mask = d.deco_masks[cell];
                            if (mask == 0) {
                                continue;
                            }
                            const float wx =
                                patch_x + static_cast<float>(x * map::kTerrainCellSize);
                            const float wz =
                                patch_z + static_cast<float>(z * map::kTerrainCellSize);
                            const float dx = wx - cam_eye.x;
                            const float dz = wz - cam_eye.z;
                            if (dx * dx + dz * dz > kDecorationDistance2 ||
                                !client::WorldRenderCache::frustum_contains_sphere(
                                    prop_frustum, wx, cam_eye.y, wz, 180.0f)) {
                                continue;
                            }
                            const int sx0 = std::max(0, x - 1);
                            const int sx1 = std::min(map::kMapSize, x + 1);
                            const int sz0 = std::max(0, z - 1);
                            const int sz1 = std::min(map::kMapSize, z + 1);
                            const auto height_at = [&](int hx, int hz) {
                                return static_cast<float>(
                                    d.heights[static_cast<std::size_t>(hz) * map::kHeightSamples +
                                              hx]);
                            };
                            const float wy = height_at(x, z);
                            float nx = height_at(sx0, z) - height_at(sx1, z);
                            float ny = static_cast<float>((sx1 - sx0) * map::kTerrainCellSize);
                            float nz = height_at(x, sz0) - height_at(x, sz1);
                            const float normal_length = std::sqrt(nx * nx + ny * ny + nz * nz);
                            if (normal_length > 1e-5f) {
                                nx /= normal_length;
                                ny /= normal_length;
                                nz /= normal_length;
                            } else {
                                nx = nz = 0.0f;
                                ny = 1.0f;
                            }
                            float qx = nz;
                            float qy = 0.0f;
                            float qz = -nx;
                            float qw = 1.0f + ny;
                            const float quaternion_length =
                                std::sqrt(qx * qx + qy * qy + qz * qz + qw * qw);
                            if (quaternion_length > 1e-5f) {
                                qx /= quaternion_length;
                                qy /= quaternion_length;
                                qz /= quaternion_length;
                                qw /= quaternion_length;
                            }
                            const engine::Mat4 model = engine::Mat4::multiply(
                                engine::Mat4::translation(wx, wy, wz),
                                engine::Mat4::from_quaternion(qx, qy, qz, qw));
                            for (int bit = 0; bit < 8; ++bit) {
                                if ((mask & (1u << bit)) == 0) {
                                    continue;
                                }
                                const auto model_it = decoration_models.find(
                                    d.deco_palette[static_cast<std::size_t>(bit)]);
                                if (model_it == decoration_models.end() ||
                                    model_it->second == nullptr) {
                                    continue;
                                }
                                for (const auto& part : model_it->second->parts) {
                                    decoration_batches[&part].push_back(model);
                                }
                            }
                        }
                    }
                }
                for (const auto& [part, models] : decoration_batches) {
                    engine->draw_mesh_textured_instances(
                        part->mesh, part->texture, models, true, true);
                }
            }
        }
        world_debug.update(*engine,
                           floor,
                           walk_grid,
                           profile.origin_patch(),
                           coords::to_server(local_player.pos().x, profile.origin_patch()),
                           coords::to_server(local_player.pos().z, profile.origin_patch()),
                           local_map,
                           uses_indoor_visuals(),
                           local_player.pos().y);
        if (!indoor_view.loaded()) {
            world_debug.draw(*engine);
        }

        constexpr float kPropCullDist = 5000.0f;  // was 12000; ~monster draw range + margin
        constexpr float kPropCullDist2 = kPropCullDist * kPropCullDist;
        constexpr std::size_t kNoIndoorProp = std::numeric_limits<std::size_t>::max();
        struct VisibleProp {
            const client::WorldRenderCache::DrawnProp* prop = nullptr;
            std::size_t indoor_prop_index = kNoIndoorProp;
        };
        std::vector<VisibleProp> visible_props;
        visible_props.reserve(props.size() / 4);
        for (const auto& p : props) {
            if ((!p.gpu || p.gpu->parts.empty()) && p.fx_entity == 0) {
                continue;
            }
            if (p.cull_hx > 0.0f) {
                const float min_x = p.wx - p.cull_hx;
                const float min_y = p.wy - p.cull_hy;
                const float min_z = p.wz - p.cull_hz;
                const float max_x = p.wx + p.cull_hx;
                const float max_y = p.wy + p.cull_hy;
                const float max_z = p.wz + p.cull_hz;
                const float qx = std::clamp(cam_eye.x, min_x, max_x);
                const float qy = std::clamp(cam_eye.y, min_y, max_y);
                const float qz = std::clamp(cam_eye.z, min_z, max_z);
                const float dx = qx - cam_eye.x;
                const float dy = qy - cam_eye.y;
                const float dz = qz - cam_eye.z;
                if (dx * dx + dy * dy + dz * dz > kPropCullDist2) {
                    continue;
                }
                if (!client::WorldRenderCache::frustum_contains_aabb(
                        prop_frustum, min_x, min_y, min_z, max_x, max_y, max_z)) {
                    continue;
                }
            } else {
                const float dx = p.wx - cam_eye.x;
                const float dy = p.wy - cam_eye.y;
                const float dz = p.wz - cam_eye.z;
                if (dx * dx + dy * dy + dz * dz > kPropCullDist2) {
                    continue;
                }
                if (!client::WorldRenderCache::frustum_contains_sphere(
                        prop_frustum, p.wx, p.wy, p.wz, p.cull_radius)) {
                    continue;
                }
            }
            visible_props.push_back({&p, kNoIndoorProp});
        }
        constexpr float kIndoorCullDist = 20000.0f;
        constexpr float kIndoorCullDist2 = kIndoorCullDist * kIndoorCullDist;
        constexpr float kIndoorObjectCull2 = client::kFarZIndoorObject * client::kFarZIndoorObject;
        for (std::size_t indoor_prop_index = 0; indoor_prop_index < indoor_view.props().size();
             ++indoor_prop_index) {
            const auto& p = indoor_view.props()[indoor_prop_index];
            if (!p.draw || !p.gpu || p.gpu->parts.empty()) {
                continue;
            }
            const float dx = p.wx - cam_eye.x;
            const float dy = p.wy - cam_eye.y;
            const float dz = p.wz - cam_eye.z;
            float dist2 = dx * dx + dy * dy + dz * dz;
            if (p.cull_hx > 0.0f) {
                const float nearest_x = std::clamp(cam_eye.x, p.wx - p.cull_hx, p.wx + p.cull_hx);
                const float nearest_y = std::clamp(cam_eye.y, p.wy - p.cull_hy, p.wy + p.cull_hy);
                const float nearest_z = std::clamp(cam_eye.z, p.wz - p.cull_hz, p.wz + p.cull_hz);
                const float nearest_dx = nearest_x - cam_eye.x;
                const float nearest_dy = nearest_y - cam_eye.y;
                const float nearest_dz = nearest_z - cam_eye.z;
                dist2 = nearest_dx * nearest_dx + nearest_dy * nearest_dy + nearest_dz * nearest_dz;
            }
            if (p.polygon_offset) {
                if (dist2 > kIndoorObjectCull2) {
                    continue;
                }
            } else if (dist2 > kIndoorCullDist2) {
                continue;
            }
            if (p.cull_hx > 0.0f) {
                if (!client::WorldRenderCache::frustum_contains_aabb(prop_frustum,
                                                                     p.wx - p.cull_hx,
                                                                     p.wy - p.cull_hy,
                                                                     p.wz - p.cull_hz,
                                                                     p.wx + p.cull_hx,
                                                                     p.wy + p.cull_hy,
                                                                     p.wz + p.cull_hz)) {
                    continue;
                }
            } else if (!client::WorldRenderCache::frustum_contains_sphere(
                           prop_frustum, p.wx, p.wy, p.wz, p.cull_radius)) {
                continue;
            }
            visible_props.push_back({&p, indoor_prop_index});
        }

        std::unordered_set<std::uint32_t> seen_world_fx;
        auto sync_world_prop_fx = [&](const client::WorldRenderCache::DrawnProp& p) {
            if (p.fx_entity == 0 || p.catalog_key.empty()) {
                return;
            }
            const auto* parts = world_objects.find(p.catalog_key);
            if (parts == nullptr || parts->empty()) {
                return;
            }
            if (!sprite_fx.has(p.fx_entity)) {
                for (const auto& attach : *parts) {
                    sprite_fx.attach_particle(*engine,
                                              p.fx_entity,
                                              attach.name(),
                                              attach.effect_bone(),
                                              attach.ox(),
                                              attach.oy(),
                                              attach.oz());
                }
                sprite_fx.persist_attached(p.fx_entity);
            }
            sprite_fx.follow_oriented(p.fx_entity, -1, p.model);
            for (const auto& attach : *parts) {
                if (attach.effect_bone() != -1) {
                    sprite_fx.follow_oriented(p.fx_entity, attach.effect_bone(), p.model);
                }
            }
            seen_world_fx.insert(p.fx_entity);
        };
        for (const VisibleProp& visible : visible_props) {
            sync_world_prop_fx(*visible.prop);
        }
        for (const std::uint32_t id : world_prop_fx_live) {
            if (seen_world_fx.find(id) == seen_world_fx.end()) {
                sprite_fx.detach(id);
            }
        }
        world_prop_fx_live.swap(seen_world_fx);

        struct OpaqueInstanceBatch {
            const client::WorldRenderCache::GpuSubmesh* part = nullptr;
            std::vector<engine::Mat4> models;
        };
        std::vector<OpaqueInstanceBatch> opaque_instance_batches;
        std::unordered_map<const client::WorldRenderCache::GpuSubmesh*, std::size_t>
            opaque_instance_batch_indices;
        std::vector<OpaqueInstanceBatch> opaque_outdoor_instance_batches;
        std::unordered_map<const client::WorldRenderCache::GpuSubmesh*, std::size_t>
            opaque_outdoor_instance_batch_indices;
        opaque_instance_batches.reserve(32);
        opaque_instance_batch_indices.reserve(32);
        opaque_outdoor_instance_batches.reserve(32);
        opaque_outdoor_instance_batch_indices.reserve(32);
        for (const VisibleProp& visible : visible_props) {
            const auto& prop = *visible.prop;
            if (!prop.gpu) {
                continue;
            }
            const bool indoor_offset =
                visible.indoor_prop_index != kNoIndoorProp && prop.polygon_offset;
            const bool outdoor = visible.indoor_prop_index == kNoIndoorProp && !prop.polygon_offset;
            if (!indoor_offset && !outdoor) {
                continue;
            }
            for (std::size_t part_index = 0; part_index < prop.gpu->parts.size(); ++part_index) {
                if (!indoor_view.region_part_visible(visible.indoor_prop_index, part_index)) {
                    continue;
                }
                const auto& part = prop.gpu->parts[part_index];
                if (part.alpha_blend || part.lightmap || part.procedural_water_scroll ||
                    part.procedural_waterfall_scroll) {
                    continue;
                }
                float uv_u = 0.0f;
                float uv_v = 0.0f;
                float opacity = 1.0f;
                client::WorldRenderCache::sample_prop_uv(
                    part, water_anim_time, uv_u, uv_v, opacity);
                if (std::fabs(uv_u) > 1e-6f || std::fabs(uv_v) > 1e-6f || opacity < 0.999f) {
                    continue;
                }
                auto& indices = indoor_offset ? opaque_instance_batch_indices
                                              : opaque_outdoor_instance_batch_indices;
                auto& batches =
                    indoor_offset ? opaque_instance_batches : opaque_outdoor_instance_batches;
                auto [it, inserted] = indices.emplace(&part, batches.size());
                if (inserted) {
                    batches.push_back({&part, {}});
                }
                batches[it->second].models.push_back(prop.model);
            }
        }
        std::size_t instanced_parts = 0;
        std::size_t instance_draws = 0;
        for (const auto& batch : opaque_instance_batches) {
            if (batch.models.size() >= 2) {
                instanced_parts += batch.models.size();
                ++instance_draws;
            }
        }
        for (const auto& batch : opaque_outdoor_instance_batches) {
            if (batch.models.size() >= 2) {
                instanced_parts += batch.models.size();
                ++instance_draws;
            }
        }
        frame_profiler.record_world(
            instanced_parts, instance_draws, monster_instances.size(), monster_skin_updates);

        const int monster_render_limit = client_settings.render.max_rendered_monsters;
        std::unordered_set<std::uint32_t> rendered_monster_ids;
        if (monster_render_limit > 0) {
            std::vector<std::pair<float, std::uint32_t>> nearest_monsters;
            nearest_monsters.reserve(monster_instances.size());
            for (const auto& [id, monster] : net.monsters()) {
                if (scfg.plants.find_type(monster.index()) != nullptr ||
                    monster_instances.find(id) == monster_instances.end()) {
                    continue;
                }
                const float dx = monster.wx() - local_player.pos().x;
                const float dz = monster.wz() - local_player.pos().z;
                nearest_monsters.emplace_back(dx * dx + dz * dz, id);
            }
            const std::size_t rendered_count =
                std::min(nearest_monsters.size(), static_cast<std::size_t>(monster_render_limit));
            std::partial_sort(nearest_monsters.begin(),
                              nearest_monsters.begin() + rendered_count,
                              nearest_monsters.end());
            rendered_monster_ids.reserve(rendered_count);
            for (std::size_t index = 0; index < rendered_count; ++index) {
                rendered_monster_ids.insert(nearest_monsters[index].second);
            }
        }
        const auto is_monster_rendered = [&](std::uint32_t id) {
            const auto entity = net.monsters().find(id);
            return (entity != net.monsters().end() &&
                    scfg.plants.find_type(entity->second.index()) != nullptr) ||
                   monster_render_limit == 0 ||
                   rendered_monster_ids.find(id) != rendered_monster_ids.end();
        };

        const auto player_model = player_world_matrix();
        const auto shadow_receiver_at = [&](float world_x, float world_z, float ground_y) {
            engine::ShadowReceiver receiver;
            const int patch_x = coords::patch_index_for_client(world_x);
            const int patch_y = coords::patch_index_for_client(world_z);
            const auto patch = std::find_if(drawn.begin(), drawn.end(), [&](const auto& candidate) {
                return candidate.patch_x == patch_x && candidate.patch_y == patch_y;
            });
            if (patch == drawn.end() || !patch->shadow_height_map) {
                return receiver;
            }

            // Floors supplied by GB collision (bridges, buildings) are not represented
            // by the outdoor KCM. Keep their shadow on the collision plane instead of
            // projecting it through the structure onto the terrain below.
            constexpr float kTerrainReceiverTolerance = 8.0f;
            const float terrain_y = floor.sample_terrain(world_x, world_z, ground_y);
            if (std::fabs(terrain_y - ground_y) > kTerrainReceiverTolerance) {
                return receiver;
            }

            receiver.height_map = patch->shadow_height_map;
            receiver.origin_x = static_cast<float>((patch_x - map::kPatchGrid / 2) * map::kMapSize *
                                                   map::kTerrainCellSize);
            receiver.origin_z = static_cast<float>((patch_y - map::kPatchGrid / 2) * map::kMapSize *
                                                   map::kTerrainCellSize);
            receiver.world_size = kPatchWorld;
            return receiver;
        };
        if (client_settings.render.shadows != settings::ShadowQuality::Off) {
            engine->begin_shadow_pass();
            constexpr float kActorShadowOpacity = 0.34f;
            if (!cam.fly_mode() && riding_rendered_item_index != 0 && local_riding_mounted) {
                const auto presentation = riding_presentations.find(riding_rendered_item_index);
                if (presentation != riding_presentations.end()) {
                    const auto rig = riding_rig_cache.find(presentation->second.visual_key);
                    if (rig != riding_rig_cache.end()) {
                        const float half_yaw = local_player.yaw() * 0.5f;
                        const auto rotation = engine::Mat4::from_quaternion(
                            0.0f, std::sin(half_yaw), 0.0f, std::cos(half_yaw));
                        const auto scale = engine::Mat4::scaling(
                            rig->second.scale(), rig->second.scale(), rig->second.scale());
                        const auto translation = engine::Mat4::translation(
                            local_player.pos().x, local_player.pos().y, local_player.pos().z);
                        const auto riding_model = engine::Mat4::multiply(
                            translation, engine::Mat4::multiply(rotation, scale));
                        riding_instance.draw_shadow(*engine,
                                                    riding_model,
                                                    shadow_receiver_at(local_player.pos().x,
                                                                       local_player.pos().z,
                                                                       local_player.pos().y),
                                                    local_player.pos().y,
                                                    kActorShadowOpacity);
                    }
                }
            }
            if (!cam.fly_mode() && !player_actor.empty()) {
                // The rider's model includes the saddle offset; the shadow still lands
                // on the ground beneath the mount, including terrain and collision floors.
                const float ground_y = local_player.pos().y;
                player_actor.draw_shadow(
                    *engine,
                    player_model,
                    shadow_receiver_at(player_model.m[12], player_model.m[14], ground_y),
                    ground_y,
                    kActorShadowOpacity);
            }
            for (const client::RemotePlayer* remote : remote_players.all()) {
                if (remote_players.is_rendered(remote->id()) && !remote->actor().empty()) {
                    const engine::Mat4 remote_model = remote->model_matrix();
                    remote->actor().draw_shadow(*engine,
                                                remote_model,
                                                shadow_receiver_at(remote_model.m[12],
                                                                   remote_model.m[14],
                                                                   remote_model.m[13]),
                                                remote_model.m[13],
                                                kActorShadowOpacity);
                }
            }
            const int shadow_player_sx =
                coords::to_server(local_player.pos().x, profile.origin_patch());
            const int shadow_player_sy =
                coords::to_server(local_player.pos().z, profile.origin_patch());
            for (const auto& spawn : npc_spawns) {
                if (!coords::in_sight(
                        shadow_player_sx, shadow_player_sy, spawn.server_x(), spawn.server_y())) {
                    continue;
                }
                const auto* gpu =
                    client::WorldRenderCache::get_or_load_npc_shape(*engine,
                                                                    profile.data_root(),
                                                                    spawn.shape(),
                                                                    model_cache,
                                                                    prop_tex_cache,
                                                                    npc_catalog);
                if (gpu == nullptr) {
                    continue;
                }
                const float wx = coords::to_client(spawn.server_x(), profile.origin_patch());
                const float wz = coords::to_client(spawn.server_y(), profile.origin_patch());
                const float wy = coords::to_client_y(spawn.server_z());
                const float yaw = client::NpcCatalog::yaw_from_dir(profile.origin_patch(),
                                                                   spawn.server_x(),
                                                                   spawn.server_y(),
                                                                   spawn.dir_x(),
                                                                   spawn.dir_y());
                const engine::Mat4 model = engine::Mat4::multiply(
                    engine::Mat4::translation(wx, wy, wz),
                    engine::Mat4::from_quaternion(
                        0.0f, std::sin(yaw * 0.5f), 0.0f, std::cos(yaw * 0.5f)));
                for (const auto& part : gpu->parts) {
                    engine->draw_mesh_shadow(part.mesh,
                                             part.texture,
                                             model,
                                             shadow_receiver_at(wx, wz, wy),
                                             wy,
                                             part.alpha_test || part.alpha_blend,
                                             kActorShadowOpacity);
                }
            }
            constexpr float kShadowDrawDistance2 = 4000.0f * 4000.0f;
            for (const auto& [id, monster] : net.monsters()) {
                if (!is_monster_rendered(id)) {
                    continue;
                }
                const auto instance = monster_instances.find(id);
                if (instance == monster_instances.end()) {
                    continue;
                }
                const float dx = monster.wx() - cam_eye.x;
                const float dz = monster.wz() - cam_eye.z;
                if (dx * dx + dz * dz > kShadowDrawDistance2) {
                    continue;
                }
                const auto rig = monster_rig_cache.find(monster.index());
                const float scale = rig != monster_rig_cache.end() ? rig->second.scale() : 1.0f;
                const float ground_y = floor.get_floor(monster.wx(), monster.wz());
                const float model_y = ground_y + instance->second.sink_y();
                const float half_yaw = instance->second.yaw() * 0.5f;
                const engine::Mat4 model = engine::Mat4::multiply(
                    engine::Mat4::translation(monster.wx(), model_y, monster.wz()),
                    engine::Mat4::multiply(engine::Mat4::from_quaternion(
                                               0.0f, std::sin(half_yaw), 0.0f, std::cos(half_yaw)),
                                           engine::Mat4::scaling(scale, scale, scale)));
                instance->second.draw_shadow(
                    *engine,
                    model,
                    shadow_receiver_at(monster.wx(), monster.wz(), ground_y),
                    ground_y,
                    kActorShadowOpacity);
            }
            if (client_settings.render.shadows == settings::ShadowQuality::All) {
                constexpr float kTreeShadowDistance2 = 3200.0f * 3200.0f;
                for (const VisibleProp& visible : visible_props) {
                    const auto& prop = *visible.prop;
                    if (prop.gpu == nullptr ||
                        !client::WorldRenderCache::is_tree_asset_path(prop.asset_path)) {
                        continue;
                    }
                    const float dx = prop.wx - cam_eye.x;
                    const float dz = prop.wz - cam_eye.z;
                    if (dx * dx + dz * dz > kTreeShadowDistance2) {
                        continue;
                    }
                    const float ground_y = prop.model.m[13];
                    for (const auto& part : prop.gpu->parts) {
                        engine->draw_mesh_shadow(
                            part.mesh,
                            part.texture,
                            prop.model,
                            shadow_receiver_at(prop.model.m[12], prop.model.m[14], ground_y),
                            ground_y,
                            part.alpha_test || part.alpha_blend,
                            0.26f);
                    }
                }
            }
            engine->end_shadow_pass();
        }
        // Opaque props then alpha (leaves / glass), including local player.
        // Indoor objects get a polygon offset so coplanar region floors do not flicker.
        for (int pass = 0; pass < 2; ++pass) {
            const bool want_alpha = pass == 1;
            for (int bias = 0; bias < 2; ++bias) {
                if (bias == 1) {
                    engine->set_polygon_offset(-4.0f, -64.0f);
                }
                if (!want_alpha) {
                    const auto& batches =
                        bias == 1 ? opaque_instance_batches : opaque_outdoor_instance_batches;
                    for (const auto& batch : batches) {
                        if (batch.models.size() < 2) {
                            continue;
                        }
                        engine->draw_mesh_textured_instances(batch.part->mesh,
                                                             batch.part->texture,
                                                             batch.models,
                                                             batch.part->two_sided,
                                                             batch.part->alpha_test);
                    }
                }
                for (const VisibleProp& visible : visible_props) {
                    const client::WorldRenderCache::DrawnProp& p = *visible.prop;
                    if (!p.gpu) {
                        continue;
                    }
                    if (p.polygon_offset != (bias == 1)) {
                        continue;
                    }
                    for (std::size_t part_index = 0; part_index < p.gpu->parts.size();
                         ++part_index) {
                        if (visible.indoor_prop_index != kNoIndoorProp &&
                            !indoor_view.region_part_visible(visible.indoor_prop_index,
                                                             part_index)) {
                            continue;
                        }
                        const auto& part = p.gpu->parts[part_index];
                        if (part.alpha_blend != want_alpha) {
                            continue;
                        }
                        if (!want_alpha && bias == 1) {
                            const auto batch = opaque_instance_batch_indices.find(&part);
                            if (batch != opaque_instance_batch_indices.end() &&
                                opaque_instance_batches[batch->second].models.size() >= 2) {
                                continue;
                            }
                        }
                        if (!want_alpha && bias == 0 &&
                            visible.indoor_prop_index == kNoIndoorProp && !p.polygon_offset) {
                            const auto batch = opaque_outdoor_instance_batch_indices.find(&part);
                            if (batch != opaque_outdoor_instance_batch_indices.end() &&
                                opaque_outdoor_instance_batches[batch->second].models.size() >= 2) {
                                continue;
                            }
                        }
                        float uv_u = 0.0f, uv_v = 0.0f, opacity = 1.0f;
                        client::WorldRenderCache::sample_prop_uv(
                            part, water_anim_time, uv_u, uv_v, opacity);
                        const bool blend = part.alpha_blend || opacity < 0.999f;
                        client::WorldRenderCache::draw_prop_part(*engine,
                                                                 part,
                                                                 p.model,
                                                                 blend,
                                                                 part.alpha_test && !blend,
                                                                 uv_u,
                                                                 uv_v,
                                                                 opacity);
                    }
                }
                if (bias == 1) {
                    engine->set_polygon_offset(0.0f, 0.0f);
                }
            }
            if (!want_alpha) {
                for (const auto& model : event_board_models) {
                    exp_event_board.draw(model);
                }
            }
            // NPCs: only while inside MainSvr 25-tile sight of the local player.
            const int player_sx = coords::to_server(local_player.pos().x, profile.origin_patch());
            const int player_sy = coords::to_server(local_player.pos().z, profile.origin_patch());
            for (const auto& s : npc_spawns) {
                if (!coords::in_sight(player_sx, player_sy, s.server_x(), s.server_y())) {
                    continue;
                }
                const client::WorldRenderCache::GpuModel* gpu =
                    client::WorldRenderCache::get_or_load_npc_shape(*engine,
                                                                    profile.data_root(),
                                                                    s.shape(),
                                                                    model_cache,
                                                                    prop_tex_cache,
                                                                    npc_catalog);
                if (!gpu) {
                    continue;
                }
                const float wx = coords::to_client(s.server_x(), profile.origin_patch());
                const float wz = coords::to_client(s.server_y(), profile.origin_patch());
                const float wy = coords::to_client_y(s.server_z());
                const float yaw = client::NpcCatalog::yaw_from_dir(
                    profile.origin_patch(), s.server_x(), s.server_y(), s.dir_x(), s.dir_y());
                const float hy = yaw * 0.5f;
                const auto R =
                    engine::Mat4::from_quaternion(0.0f, std::sin(hy), 0.0f, std::cos(hy));
                const auto T = engine::Mat4::translation(wx, wy, wz);
                const auto model = engine::Mat4::multiply(T, R);
                for (const auto& part : gpu->parts) {
                    if (part.alpha_blend != want_alpha) {
                        continue;
                    }
                    engine->draw_mesh_textured(part.mesh,
                                               part.texture,
                                               model,
                                               part.alpha_blend,
                                               part.two_sided,
                                               part.alpha_test);
                }
            }
            if (!cam.fly_mode() && riding_rendered_item_index != 0 && local_riding_mounted) {
                const auto presentation = riding_presentations.find(riding_rendered_item_index);
                if (presentation != riding_presentations.end()) {
                    const auto rig = riding_rig_cache.find(presentation->second.visual_key);
                    if (rig != riding_rig_cache.end()) {
                        const float half_yaw = local_player.yaw() * 0.5f;
                        const auto rotation = engine::Mat4::from_quaternion(
                            0.0f, std::sin(half_yaw), 0.0f, std::cos(half_yaw));
                        const auto scale = engine::Mat4::scaling(
                            rig->second.scale(), rig->second.scale(), rig->second.scale());
                        const auto translation = engine::Mat4::translation(
                            local_player.pos().x, local_player.pos().y, local_player.pos().z);
                        const auto riding_model = engine::Mat4::multiply(
                            translation, engine::Mat4::multiply(rotation, scale));
                        riding_instance.draw(*engine, riding_model, want_alpha);
                    }
                }
            }
            if (!cam.fly_mode() && !player_actor.empty()) {
                float hide_opacity = 1.0f;
                for (const auto& buff : net.buff_slots()) {
                    if (buff.expire_ms != 0 && buff.expire_ms <= movement_now_ms) {
                        continue;
                    }
                    if ((buff.bstate & protocol::kCbsSkillHiding) != 0) {
                        hide_opacity = 0.333f;
                        break;
                    }
                }
                player_actor.draw(
                    *engine, player_model, cam.view(local_player), want_alpha, hide_opacity);
            }
            remote_players.draw(*engine, cam.view(local_player), want_alpha);
        }

        // Indoor floor meshes are opaque and render after terrain. Draw the transparent
        // debug surface afterwards so the dungeon geometry cannot cover it completely.
        if (indoor_view.loaded()) {
            world_debug.draw(*engine);
        }

        // Networked monsters (GB clothes + idle/walk, textures from
        // Monster/Clothes).
        constexpr float kMonsterDrawDist = 4000.0f;
        constexpr float kMonsterDrawDist2 = kMonsterDrawDist * kMonsterDrawDist;
        std::vector<std::pair<const client::MonsterInstance*, engine::Mat4>> hit_outlines;
        for (int pass = 0; pass < 2; ++pass) {
            const bool want_alpha = pass == 1;
            for (const auto& [id, nm] : net.monsters()) {
                if (!is_monster_rendered(id)) {
                    continue;
                }
                auto vit = monster_instances.find(id);
                if (vit == monster_instances.end()) {
                    continue;
                }
                const float mdx = nm.wx() - cam_eye.x;
                const float mdz = nm.wz() - cam_eye.z;
                const float mdist2 = mdx * mdx + mdz * mdz;
                if (mdist2 > kMonsterDrawDist2) {
                    continue;
                }
                auto rgit = monster_rig_cache.find(nm.index());
                const float scale = (rgit != monster_rig_cache.end()) ? rgit->second.scale() : 1.0f;
                float my = floor.get_floor(nm.wx(), nm.wz());
                my += vit->second.sink_y();
                const float hy = vit->second.yaw() * 0.5f;
                const auto R =
                    engine::Mat4::from_quaternion(0.0f, std::sin(hy), 0.0f, std::cos(hy));
                const auto S = engine::Mat4::scaling(scale, scale, scale);
                const auto T = engine::Mat4::translation(nm.wx(), my, nm.wz());
                const auto model = engine::Mat4::multiply(T, engine::Mat4::multiply(R, S));
                vit->second.draw(*engine, model, want_alpha);
                if (want_alpha && vit->second.has_hit_outline()) {
                    hit_outlines.emplace_back(&vit->second, model);
                }
            }
        }

        // The pet is an independent skinned actor. Updating its meshes above is
        // not enough: submit it in the same opaque/alpha passes as monsters.
        if (pet_rendered_item_index != 0) {
            const auto pet = pet_presentations.find(pet_rendered_item_index);
            const client::MonsterRigGpu* pet_rig = nullptr;
            if (pet != pet_presentations.end()) {
                if (pet->second.monster_type > 0) {
                    const auto rig = monster_rig_cache.find(pet->second.monster_type);
                    if (rig != monster_rig_cache.end()) {
                        pet_rig = &rig->second;
                    }
                } else {
                    const auto rig = pet_rig_cache.find(pet->second.visual_key);
                    if (rig != pet_rig_cache.end()) {
                        pet_rig = &rig->second;
                    }
                }
            }
            if (pet_rig != nullptr) {
                const float pet_y = floor.get_floor(pet_controller.x(), pet_controller.z());
                const float half_yaw = pet_controller.yaw() * 0.5f;
                const auto rotation = engine::Mat4::from_quaternion(
                    0.0f, std::sin(half_yaw), 0.0f, std::cos(half_yaw));
                const float pet_scale = pet_rig->scale() * pet->second.scale;
                const auto scale = engine::Mat4::scaling(pet_scale, pet_scale, pet_scale);
                const auto translation =
                    engine::Mat4::translation(pet_controller.x(), pet_y, pet_controller.z());
                const auto model =
                    engine::Mat4::multiply(translation, engine::Mat4::multiply(rotation, scale));
                pet_instance.draw(*engine, model, false);
                pet_instance.draw(*engine, model, true);
            }
        }

        for (const auto& [monster, model] : hit_outlines) {
            monster->draw_hit_outline(*engine, model);
        }
        range_skill_decal.draw(*engine);
        combat_fx.draw(*engine);
        sprite_fx.draw(*engine);
        select_marker.draw(*engine);
        click_marker.draw(*engine);
        {
            const int player_sx = coords::to_server(local_player.pos().x, profile.origin_patch());
            const int player_sy = coords::to_server(local_player.pos().z, profile.origin_patch());
            const int player_level = inv_win.player_level();
            const int player_clazz = inv_win.player_clazz();
            constexpr float kQuestMarkLift = 6.0f;
            for (const auto& s : npc_spawns) {
                if (!s.has_quest() ||
                    !coords::in_sight(player_sx, player_sy, s.server_x(), s.server_y())) {
                    continue;
                }
                const auto kind = client::QuestMarker::kind_for_npc(
                    protocol::Quest::make_key(s.quest_index(), s.quest_flag()),
                    player_level,
                    player_clazz,
                    net.quest_flags(),
                    quest_db,
                    dialog_catalog,
                    &task_catalog,
                    [](int item_index, const void* ctx) {
                        return static_cast<const client::Bag*>(ctx)->count_item(item_index);
                    },
                    &player_bag);
                if (kind == client::QuestMarkerKind::None) {
                    continue;
                }
                const float wx = coords::to_client(s.server_x(), profile.origin_patch());
                const float wz = coords::to_client(s.server_y(), profile.origin_patch());
                const float wy = coords::to_client_y(s.server_z());
                const client::NpcRig* rig = npc_catalog.find_rig(s.shape());
                const float name_off = s.name_offset_y(rig ? rig->name_offset_y() : 5.0f);
                float name_y = wy + 17.0f + name_off;
                const client::WorldRenderCache::GpuModel* gpu =
                    client::WorldRenderCache::get_or_load_npc_shape(*engine,
                                                                    profile.data_root(),
                                                                    s.shape(),
                                                                    model_cache,
                                                                    prop_tex_cache,
                                                                    npc_catalog);
                if (gpu && gpu->has_name_anchor) {
                    name_y = wy + gpu->name_local_y + name_off;
                } else if (rig) {
                    if (const auto* bone = npc_catalog.find_bone(rig->bone_no())) {
                        name_y = wy + bone->target_height() + name_off;
                    }
                }
                quest_marker.draw(*engine, kind, wx, name_y + kQuestMarkLift, wz, cam_eye);
            }
            for (const auto& [id, nm] : net.monsters()) {
                if (!coords::in_sight(player_sx, player_sy, nm.x(), nm.y())) {
                    continue;
                }
                const client::MonsterRig* macro = monster_catalog.find_rig(nm.index());
                if (!macro || macro->quest_slots().empty()) {
                    continue;
                }
                const auto kind = client::QuestMarker::kind_for_monster(
                    macro->quest_slots(),
                    net.quest_flags(),
                    &task_catalog,
                    [](int item_index, const void* ctx) {
                        return static_cast<const client::Bag*>(ctx)->count_item(item_index);
                    },
                    &player_bag);
                if (kind != client::QuestMarkerKind::Available) {
                    continue;
                }
                auto vit = monster_instances.find(id);
                if (vit == monster_instances.end() || vit->second.dying()) {
                    continue;
                }
                auto rgit = monster_rig_cache.find(nm.index());
                const float scale = (rgit != monster_rig_cache.end()) ? rgit->second.scale() : 1.0f;
                const float my = floor.get_floor(nm.wx(), nm.wz());
                float hx = nm.wx();
                float hy = my + 17.0f;
                float hz = nm.wz();
                float name_off = macro->name_offset_y();
                if (rgit != monster_rig_cache.end()) {
                    name_off = rgit->second.name_offset_y();
                    const float hyaw = vit->second.yaw() * 0.5f;
                    const auto R =
                        engine::Mat4::from_quaternion(0.0f, std::sin(hyaw), 0.0f, std::cos(hyaw));
                    const auto S = engine::Mat4::scaling(scale, scale, scale);
                    const auto T = engine::Mat4::translation(nm.wx(), my, nm.wz());
                    const auto model = engine::Mat4::multiply(T, engine::Mat4::multiply(R, S));
                    if (rgit->second.head_bone() >= 0 &&
                        vit->second.bone_world(
                            rgit->second, model, rgit->second.head_bone(), hx, hy, hz)) {
                        hy += name_off;
                    } else {
                        hx = nm.wx();
                        hy = my + rgit->second.target_height() + name_off;
                        hz = nm.wz();
                    }
                } else {
                    hy = my + 17.0f + name_off;
                }
                if (indoor_view.loaded() &&
                    floor.segment_blocked(local_player.pos().x,
                                          local_player.pos().y + client::kBodyHeight,
                                          local_player.pos().z,
                                          hx,
                                          hy,
                                          hz)) {
                    continue;
                }
                quest_marker.draw(*engine,
                                  client::QuestMarkerKind::Available,
                                  hx,
                                  hy + kQuestMarkLift,
                                  hz,
                                  cam_eye);
            }
        }
        const bool show_debug = client::DebugOverlay::enabled();
        if (show_debug && want_net) {
            for (const auto& [id, nm] : net.monsters()) {
                (void) id;
                float half = client::WorldPick::kMonsterHalfExtent;
                float height = client::WorldPick::kMonsterBaseHeight;
                monster_pick_size(nm, half, height);
                const float base_y = floor.get_floor(nm.wx(), nm.wz());
                client::DebugOverlay::draw_monster_hitbox(
                    *engine, nm.wx(), base_y, nm.wz(), half, height);
            }
            for (const auto& [id, nn] : net.npcs()) {
                (void) id;
                float half = client::WorldPick::kNpcHalfExtent;
                float height = client::WorldPick::kNpcBaseHeight;
                npc_pick_size(nn, half, height);
                client::DebugOverlay::draw_monster_hitbox(
                    *engine, nn.wx(), nn.wy(), nn.wz(), half, height);
            }
            for (const client::RemotePlayer* remote : remote_players.all()) {
                if (!remote_players.is_rendered(remote->id())) {
                    continue;
                }
                client::DebugOverlay::draw_monster_hitbox(*engine,
                                                          remote->wx(),
                                                          remote->wy(),
                                                          remote->wz(),
                                                          client::WorldPick::kNpcHalfExtent,
                                                          client::WorldPick::kNpcBaseHeight);
            }
        }

        // Ground loot: DropItem pouch / coin-pile meshes
        // (Data/Item/DropItem/dropitemNN.gb).
        if (want_net) {
            for (const auto& [id, gi] : net.ground_items()) {
                (void) id;
                const int mesh_no =
                    client::DropItemVisual::mesh_number(gi.index, gi.count, &item_catalog);
                const std::string gb =
                    client::DropItemVisual::gb_path(profile.data_root(), mesh_no);
                if (gb.empty()) {
                    continue;
                }
                const client::WorldRenderCache::GpuModel* gpu =
                    client::WorldRenderCache::get_or_load_gb(
                        *engine, gb, model_cache, prop_tex_cache);
                if (!gpu) {
                    continue;
                }
                const auto position = ground_item_position(gi);
                const auto model = engine::Mat4::translation(position.x, position.y, position.z);
                float glow_r = 0.95f;
                float glow_g = 0.95f;
                float glow_b = 0.95f;
                if (const client::ItemDef* def = item_catalog.find(gi.index)) {
                    def->glow_color(glow_r, glow_g, glow_b);
                } else if (gi.index == client::kDropMoneyItemIndex) {
                    glow_r = 1.00f;
                    glow_g = 0.82f;
                    glow_b = 0.18f;
                }
                for (const auto& part : gpu->parts) {
                    engine->draw_mesh_textured(part.mesh,
                                               part.texture,
                                               model,
                                               part.alpha_blend,
                                               part.two_sided,
                                               part.alpha_test);
                    client::DropItemVisual::draw_mesh_glow(*engine,
                                                           part.mesh,
                                                           part.texture,
                                                           model,
                                                           part.two_sided,
                                                           glow_r,
                                                           glow_g,
                                                           glow_b,
                                                           gi.age_sec);
                }
                drop_item_visual.draw_glow(*engine,
                                           position.x,
                                           position.y,
                                           position.z,
                                           cam_eye,
                                           gi.age_sec,
                                           glow_r,
                                           glow_g,
                                           glow_b);
            }
        }

        if (water_mesh && water_tex && cam_eye.y >= map::kWaterPosY) {
            engine->draw_water_plane(
                water_mesh, water_tex, engine::Mat4::identity(), cam_eye, water_anim_time);
        }

        engine->begin_ui_pass();

        // Place labels above the head bone plus configured vertical scale.
        {
            const float sw = static_cast<float>(input.width);
            const float sh = static_cast<float>(input.height);
            const int player_level = inv_win.player_level();
            const int player_sx = coords::to_server(local_player.pos().x, profile.origin_patch());
            const int player_sy = coords::to_server(local_player.pos().z, profile.origin_patch());
            const auto npc_col = client::Nameplate::npc_color();
            constexpr float kMonsterNameDist2 = 4000.0f * 4000.0f;

            const auto set_name_depth = [&](float x, float y, float z) {
                const engine::Vec3 anchor{x, y, z};
                engine->set_ui_world_anchor(&anchor);
            };

            float selected_hp_ratio = 0.0f;
            if (selected_mob_id != 0) {
                auto mit = net.monsters().find(selected_mob_id);
                if (mit == net.monsters().end()) {
                    selected_mob_id = 0;
                    delay_damage_hud.active = false;
                    auto_attack = false;
                    pending_arrow_target = 0;
                    player_actor.cancel_attack();
                } else {
                    selected_hp_ratio = mit->second.max_hp() > 0
                                            ? static_cast<float>(mit->second.hp()) /
                                                  static_cast<float>(mit->second.max_hp())
                                            : 0.0f;
                    if (delay_damage_hud.active) {
                        if (delay_damage_hud.target_id != selected_mob_id) {
                            delay_damage_hud.active = false;
                        } else {
                            const float elapsed_s =
                                std::chrono::duration<float>(std::chrono::steady_clock::now() -
                                                             delay_damage_hud.start)
                                    .count();
                            selected_hp_ratio =
                                client::fire_delay_damage_hp_ratio(mit->second.hp(),
                                                                   mit->second.max_hp(),
                                                                   delay_damage_hud.saved_damage,
                                                                   elapsed_s);
                            if (elapsed_s >= 1.0f) {
                                delay_damage_hud.active = false;
                            }
                        }
                    }
                }
            } else if (selected_player_id != 0) {
                if (remote_players.find(selected_player_id) == nullptr) {
                    selected_player_id = 0;
                }
            }

            const auto resolve_monster_name_anchor = [&](std::uint32_t id,
                                                         const client::NetMonster& nm,
                                                         float& hx,
                                                         float& hy,
                                                         float& hz,
                                                         int& monster_level) -> bool {
                auto vit = monster_instances.find(id);
                if (vit == monster_instances.end()) {
                    return false;
                }
                auto rgit = monster_rig_cache.find(nm.index());
                const float scale = (rgit != monster_rig_cache.end()) ? rgit->second.scale() : 1.0f;
                const float my = floor.get_floor(nm.wx(), nm.wz());
                hx = nm.wx();
                hy = my + 17.0f;
                hz = nm.wz();
                monster_level = 1;
                float name_off = 5.0f;
                if (rgit != monster_rig_cache.end()) {
                    monster_level = rgit->second.level();
                    name_off = rgit->second.name_offset_y();
                    const float hyaw = vit->second.yaw() * 0.5f;
                    const auto R =
                        engine::Mat4::from_quaternion(0.0f, std::sin(hyaw), 0.0f, std::cos(hyaw));
                    const auto S = engine::Mat4::scaling(scale, scale, scale);
                    const auto T = engine::Mat4::translation(nm.wx(), my, nm.wz());
                    const auto model = engine::Mat4::multiply(T, engine::Mat4::multiply(R, S));
                    if (rgit->second.head_bone() >= 0 &&
                        vit->second.bone_world(
                            rgit->second, model, rgit->second.head_bone(), hx, hy, hz)) {
                        hy += name_off;
                    } else {
                        hx = nm.wx();
                        hy = my + rgit->second.target_height() + name_off;
                        hz = nm.wz();
                    }
                } else {
                    hy = my + 17.0f + name_off;
                }
                return true;
            };

            for (const auto& s : npc_spawns) {
                if (!coords::in_sight(player_sx, player_sy, s.server_x(), s.server_y())) {
                    continue;
                }
                const client::NpcRig* rig = npc_catalog.find_rig(s.shape());
                const bool name_on =
                    !s.name().empty() &&
                    client::Nameplate::show_npc(s.shape(), rig ? rig->show_name() : true);
                if (!name_on && s.overhead_icon().empty()) {
                    continue;
                }
                const float wx = coords::to_client(s.server_x(), profile.origin_patch());
                const float wz = coords::to_client(s.server_y(), profile.origin_patch());
                const float wy = coords::to_client_y(s.server_z());
                const float name_off = s.name_offset_y(rig ? rig->name_offset_y() : 5.0f);
                float name_y = wy + 17.0f + name_off;
                const client::WorldRenderCache::GpuModel* gpu =
                    client::WorldRenderCache::get_or_load_npc_shape(*engine,
                                                                    profile.data_root(),
                                                                    s.shape(),
                                                                    model_cache,
                                                                    prop_tex_cache,
                                                                    npc_catalog);
                if (gpu && gpu->has_name_anchor) {
                    name_y = wy + gpu->name_local_y + name_off;
                } else if (rig) {
                    if (const auto* bone = npc_catalog.find_bone(rig->bone_no())) {
                        name_y = wy + bone->target_height() + name_off;
                    }
                }
                float sx = 0.0f;
                float sy = 0.0f;
                if (!client::WorldPick::project_world_to_screen(
                        cam, local_player, wx, name_y, wz, sw, sh, sx, sy)) {
                    continue;
                }
                if (sx < -40.0f || sy < -20.0f || sx > sw + 40.0f || sy > sh + 20.0f) {
                    continue;
                }
                const std::string label =
                    show_debug ? client::Nameplate::debug_label(s.name(), s.index()) : s.name();
                sy -= s.name_offset_pixels() * game_hud.scale();
                set_name_depth(wx, name_y, wz);
                if (name_on) {
                    client::Nameplate::draw(*engine, name_font, ui, sx, sy, label, npc_col);
                }
                const float name_height =
                    name_on ? name_font.line_height() * client::Nameplate::kTextScale : 0.f;
                client::Nameplate::draw_npc_icon(
                    ui, sx, sy, s.overhead_icon(), name_height, game_hud.scale());
                engine->set_ui_world_anchor(nullptr);
            }

            bool selected_hp_drawn = false;
            for (const auto& [id, nm] : net.monsters()) {
                if (nm.name().empty() || !client::Nameplate::show_monster(nm.index(), nm.race())) {
                    continue;
                }
                const float mdx = nm.wx() - cam_eye.x;
                const float mdz = nm.wz() - cam_eye.z;
                const float mdist2 = mdx * mdx + mdz * mdz;
                if (mdist2 > kMonsterNameDist2) {
                    continue;
                }
                float hx = 0.0f;
                float hy = 0.0f;
                float hz = 0.0f;
                int monster_level = 1;
                if (!resolve_monster_name_anchor(id, nm, hx, hy, hz, monster_level)) {
                    continue;
                }
                float sx = 0.0f;
                float sy = 0.0f;
                if (!client::WorldPick::project_world_to_screen(
                        cam, local_player, hx, hy, hz, sw, sh, sx, sy)) {
                    continue;
                }
                if (sx < -40.0f || sy < -20.0f || sx > sw + 40.0f || sy > sh + 20.0f) {
                    continue;
                }
                const bool show_selected_hp =
                    id == selected_mob_id && client::Nameplate::show_monster_hp(nm.race());
                const float name_sy =
                    show_selected_hp ? sy - client::Nameplate::kSelectedHpNameLiftPx : sy;
                const std::string label =
                    client::Nameplate::monster_label(nm.name(), monster_level, nm.race());
                set_name_depth(hx, hy, hz);
                client::Nameplate::draw(
                    *engine,
                    name_font,
                    ui,
                    sx,
                    name_sy,
                    show_debug ? client::Nameplate::debug_label(label, nm.index()) : label,
                    client::Nameplate::monster_color(player_level, monster_level, nm.race()));
                if (show_selected_hp) {
                    client::Nameplate::draw_hp(
                        ui, sx, name_sy + client::Nameplate::kHpBarNameGapPx, selected_hp_ratio);
                    selected_hp_drawn = true;
                }
                engine->set_ui_world_anchor(nullptr);
            }

            if (selected_mob_id != 0 && !selected_hp_drawn) {
                auto mit = net.monsters().find(selected_mob_id);
                if (mit != net.monsters().end() &&
                    client::Nameplate::show_monster_hp(mit->second.race())) {
                    float hx = 0.0f;
                    float hy = 0.0f;
                    float hz = 0.0f;
                    int monster_level = 1;
                    if (resolve_monster_name_anchor(
                            selected_mob_id, mit->second, hx, hy, hz, monster_level)) {
                        float sx = 0.0f;
                        float sy = 0.0f;
                        if (client::WorldPick::project_world_to_screen(
                                cam, local_player, hx, hy, hz, sw, sh, sx, sy) &&
                            sx >= -40.0f && sy >= -20.0f && sx <= sw + 40.0f && sy <= sh + 20.0f) {
                            set_name_depth(hx, hy, hz);
                            client::Nameplate::draw_hp(ui, sx, sy, selected_hp_ratio);
                            engine->set_ui_world_anchor(nullptr);
                        }
                    }
                }
            }

            const auto player_col = client::Nameplate::player_color();
            struct ProjectedPlayerName {
                const client::RemotePlayer* remote;
                float sx, sy, hx, hy, hz;
            };
            std::vector<ProjectedPlayerName> projected_names;
            std::vector<client::PlayerRenderCandidate> name_candidates;
            for (const client::RemotePlayer* remote : remote_players.all()) {
                if (remote->name().empty() || !remote_players.is_rendered(remote->id()) ||
                    net.castle_enemy(remote->id())) {
                    continue;
                }
                const float hx = remote->wx();
                const float hy =
                    remote->wy() + client::kBodyHeight + client::Nameplate::kMonsterNameLift;
                const float hz = remote->wz();
                float sx = 0.0f;
                float sy = 0.0f;
                if (!client::WorldPick::project_world_to_screen(
                        cam, local_player, hx, hy, hz, sw, sh, sx, sy)) {
                    continue;
                }
                if (sx < -40.0f || sy < -20.0f || sx > sw + 40.0f || sy > sh + 20.0f) {
                    continue;
                }
                const float dx = remote->wx() - local_player.pos().x;
                const float dz = remote->wz() - local_player.pos().z;
                name_candidates.push_back({remote->id(),
                                           dx * dx + dz * dz,
                                           remote->id() == selected_player_id,
                                           own_party_ids.count(remote->id()) != 0});
                projected_names.push_back({remote, sx, sy, hx, hy, hz});
            }
            const auto named_players = client::select_player_ids(
                std::move(name_candidates), client_settings.render.max_player_names);
            for (const auto& projected : projected_names) {
                const auto* remote = projected.remote;
                const auto sx = projected.sx, sy = projected.sy;
                set_name_depth(projected.hx, projected.hy, projected.hz);
                if (named_players.count(remote->id()) != 0) {
                    const auto player_label = client::Nameplate::player_label(remote->name());
                    const std::string label =
                        show_debug ? client::Nameplate::debug_label(player_label, remote->id())
                                   : player_label;
                    const auto guild_label = client::Nameplate::guild_label(
                        net.guild_title(remote->id()), net.guild_name(remote->id()));
                    client::Nameplate::draw_player(
                        *engine, name_font, ui, sx, sy, label, guild_label, player_col);
                }
                overhead_chat.draw(*engine, name_font, remote->name(), sx, sy);
                engine->set_ui_world_anchor(nullptr);
            }

            if (name_font.ready() && (!overhead_chat.message(boot.selected.name).empty() ||
                                      !net.guild_name(net.local_player_id()).empty())) {
                float sx = 0.0f;
                float sy = 0.0f;
                if (client::WorldPick::project_world_to_screen(
                        cam,
                        local_player,
                        local_player.pos().x,
                        local_player.pos().y + client::kBodyHeight +
                            client::Nameplate::kMonsterNameLift,
                        local_player.pos().z,
                        sw,
                        sh,
                        sx,
                        sy)) {
                    set_name_depth(local_player.pos().x,
                                   local_player.pos().y + client::kBodyHeight +
                                       client::Nameplate::kMonsterNameLift,
                                   local_player.pos().z);
                    const auto guild_label =
                        client::Nameplate::guild_label(net.guild_title(net.local_player_id()),
                                                       net.guild_name(net.local_player_id()));
                    client::Nameplate::draw_player(
                        *engine,
                        name_font,
                        ui,
                        sx,
                        sy,
                        client::Nameplate::player_label(boot.selected.name),
                        guild_label,
                        player_col);
                    overhead_chat.draw(*engine, name_font, boot.selected.name, sx, sy);
                    engine->set_ui_world_anchor(nullptr);
                }
            }

            if (std::chrono::steady_clock::now() < player_overhead_hp_until) {
                const float hx = local_player.pos().x;
                const float hy = local_player.pos().y + client::kBodyHeight;
                const float hz = local_player.pos().z;
                float sx = 0.0f;
                float sy = 0.0f;
                if (client::WorldPick::project_world_to_screen(
                        cam, local_player, hx, hy, hz, sw, sh, sx, sy) &&
                    sx >= -40.0f && sy >= -20.0f && sx <= sw + 40.0f && sy <= sh + 20.0f) {
                    const float ratio =
                        player_max_hp > 0
                            ? static_cast<float>(std::max(0, static_cast<int>(player_hp))) /
                                  static_cast<float>(player_max_hp)
                            : 0.0f;
                    set_name_depth(hx, hy, hz);
                    client::Nameplate::draw_hp(ui, sx, sy, ratio);
                    engine->set_ui_world_anchor(nullptr);
                }
            }

            const client::FreetypeFont* combat_draw_font =
                combat_font.ready() ? &combat_font : (name_font.ready() ? &name_font : nullptr);
            if (combat_draw_font) {
                const float local_hx = local_player.pos().x;
                const float local_hy = local_player.pos().y + client::kBodyHeight;
                const float local_hz = local_player.pos().z;
                combat_floats.draw(
                    *engine,
                    *combat_draw_font,
                    cam,
                    local_player,
                    sw,
                    sh,
                    local_hx,
                    local_hy,
                    local_hz,
                    [&](std::uint32_t id, float& wx, float& wy, float& wz) {
                        auto mit = net.monsters().find(id);
                        if (mit == net.monsters().end()) {
                            return false;
                        }
                        const auto& nm = mit->second;
                        wx = nm.wx();
                        wz = nm.wz();
                        const float my = floor.get_floor(wx, wz);
                        wy = my + 17.0f;
                        auto vit = monster_instances.find(id);
                        auto rgit = monster_rig_cache.find(nm.index());
                        if (vit == monster_instances.end() || rgit == monster_rig_cache.end()) {
                            return true;
                        }
                        const float scale = rgit->second.scale();
                        const float hyaw = vit->second.yaw() * 0.5f;
                        const auto R = engine::Mat4::from_quaternion(
                            0.0f, std::sin(hyaw), 0.0f, std::cos(hyaw));
                        const auto S = engine::Mat4::scaling(scale, scale, scale);
                        const auto T = engine::Mat4::translation(wx, my, wz);
                        const auto model = engine::Mat4::multiply(T, engine::Mat4::multiply(R, S));
                        if (rgit->second.head_bone() >= 0 &&
                            vit->second.bone_world(
                                rgit->second, model, rgit->second.head_bone(), wx, wy, wz)) {
                            return true;
                        }
                        wx = nm.wx();
                        wy = my + rgit->second.target_height();
                        wz = nm.wz();
                        return true;
                    });
            }
        }

        const auto ui_draw_started = std::chrono::steady_clock::now();
        // ----- HUD (renewal layout; movable panels; bitmaps from data/UI) -----
        {
            client::StatusWidgetData status;
            status.set_name(boot.selected.name);
            status.set_level(std::max(1, inv_win.player_level()));
            status.set_hp(player_hp, player_max_hp);
            status.set_mp(player_mp, player_max_mp);
            status.set_class_icon("main-character-icon(musa).bmp");
            if (local_player.clazz() == client::Class::Magician) {
                status.set_class_icon("main-character-icon(soolsa).bmp");
            } else if (local_player.clazz() == client::Class::Archer) {
                status.set_class_icon("main-character-icon(koongsu).bmp");
            } else if (local_player.clazz() == client::Class::Thief) {
                status.set_class_icon("main-character-icon(dojuk).bmp");
            }

            client::DragonHudData exp;
            {
                const int egg_index = player_wear.transform_index();
                const client::ItemDef* egg = egg_index > 0 ? item_catalog.find(egg_index) : nullptr;
                const int race = egg ? egg->transform_race() : 0;
                const int level = std::max(1, net.transform_egg_level());
                const int expn = std::max(0, net.transform_egg_exp());
                const int need = protocol::need_transform_exp_for_level(level);
                const int stage = protocol::transform_tflag_stage(net.transform_tflag());
                transform_win.set_egg(egg_index > 0, egg_index, race, level, expn, need, stage);
                inv_win.set_transform_egg(egg_index, level, expn);
            }
            exp.set_exp(player_exp_current, player_exp_total);
            exp.set_rage(net.rage(), protocol::max_rage());
            exp.set_auto_hp(skill_bar.auto_hp_available() ? skill_bar.auto_hp_percent() : 0);
            world_map_data = client::MinimapWidgetData{};
            world_map_data.set_world_pos(local_player.pos().x, local_player.pos().z);
            world_map_data.set_origin_patch(profile.origin_patch());
            world_map_data.set_camera_yaw_rad(cam.yaw_rad());
            world_map_data.set_player_yaw_rad(local_player.yaw());
            world_map_data.set_area_name(std::string(
                uses_indoor_visuals() ? client::MinimapWidget::dungeon_area_name(local_map)
                                      : client::MinimapWidget::area_name(local_player.pos().x,
                                                                         local_player.pos().z,
                                                                         profile.origin_patch(),
                                                                         walk_grid)));
            if (indoor_view.loaded() && !indoor_view.stem().empty()) {
                map::MiniMapSize size = map::MiniMapSize::dungeon_defaults(indoor_view.stem());
                if (const map::MiniMapSize* found = dungeon_minimaps.find(indoor_view.stem())) {
                    size = *found;
                }
                world_map_data.set_dungeon(
                    size, indoor_view.cell_x(), indoor_view.cell_y(), profile.origin_patch());
            }
            game_hud.sync_fonts(*engine);
            game_hud.set_menu_open(client::HudMenuAction::Sinsu, transform_win.open());
            game_hud.set_menu_open(client::HudMenuAction::Riding, riding_window_open);
            game_hud.set_menu_open(client::HudMenuAction::Character, char_info_win.open());
            game_hud.set_menu_open(client::HudMenuAction::Inventory, inv_win.open());
            game_hud.set_menu_open(client::HudMenuAction::Skill, skill_win.open());
            game_hud.set_menu_open(client::HudMenuAction::Quest, quest_journal_win.open());
            game_hud.set_menu_open(client::HudMenuAction::Map, world_map_open);
            game_hud.set_menu_open(client::HudMenuAction::Community, guild_window.open());
            game_hud.set_menu_open(client::HudMenuAction::TradingPost, trading_post.open());
            game_hud.set_menu_open(client::HudMenuAction::Messages, mail_window.open());
            game_hud.set_menu_open(client::HudMenuAction::Party, party_window.open());
            game_hud.set_unread_messages(mail_window.unread_count());
            game_hud.set_trading_post_collection_pending(net.trading_post_collection_pending());
            game_hud.set_menu_open(client::HudMenuAction::System,
                                   game_menu.open() || settings_window.open());
            game_hud.update_menu_pointer(input, menu_pointer_blocked());
            game_hud.draw(ui, *engine, status, world_map_data, exp);
            {
                const auto now = std::chrono::steady_clock::now().time_since_epoch();
                const std::uint64_t now_ms = static_cast<std::uint64_t>(
                    std::chrono::duration_cast<std::chrono::milliseconds>(now).count());
                const float buff_x = 0.0f;
                const float buff_y = 90.0f * game_hud.scale();
                std::vector<client::BuffSlotEntry> buff_slots;
                std::vector<client::BuffSlotEntry> apply_buffs;
                for (auto slot : net.buff_slots()) {
                    const bool item_haste =
                        slot.source_item_index > 0 && (slot.bstate & protocol::kCbsSkillHaste) != 0;
                    if (slot.source_item_index > 0 && !item_haste) {
                        apply_buffs.push_back(std::move(slot));
                        continue;
                    }
                    if (slot.sys_name_id > 0 && slot.effect_name.empty()) {
                        slot.effect_name = item_catalog.sys(slot.sys_name_id);
                    }
                    buff_slots.push_back(std::move(slot));
                }
                const bool has_unclaimed_mail =
                    std::any_of(net.mail().begin(),
                                net.mail().end(),
                                [](const protocol::MailEntry& entry) { return !entry.claimed; });
                if (has_unclaimed_mail) {
                    client::BuffSlotEntry mail_notification;
                    mail_notification.icon_file = "buff058.BMP";
                    mail_notification.effect_name = "Unclaimed mail attachments";
                    mail_notification.is_mail_notification = true;
                    buff_slots.push_back(std::move(mail_notification));
                }
                const auto active_pet = pet_presentations.find(player_wear.pet_index());
                if (active_pet != pet_presentations.end()) {
                    client::BuffSlotEntry pet_overlay;
                    pet_overlay.icon_file = item_catalog.icon_filename(player_wear.pet_index());
                    if (const client::ItemDef* item = item_catalog.find(player_wear.pet_index())) {
                        pet_overlay.effect_name = item->name();
                        std::string stats;
                        const auto append_stat = [&stats](const char* label, int value) {
                            if (value == 0) {
                                return;
                            }
                            if (stats.empty()) {
                                stats = "\nStats: ";
                            } else {
                                stats += ", ";
                            }
                            stats += label;
                            stats += value > 0 ? "+" : "";
                            stats += std::to_string(value);
                        };
                        append_stat("ATK ", item->min_attack());
                        append_stat("MAG ", item->min_magic());
                        append_stat("DEF ", item->defense());
                        append_stat("HP ", item->hp());
                        append_stat("MP ", item->mp());
                        pet_overlay.effect_name += stats;
                    }
                    if (pet_overlay.effect_name.empty()) {
                        pet_overlay.effect_name = "Pet";
                    }
                    if (active_pet->second.heal_per_two_seconds > 0) {
                        pet_overlay.effect_name +=
                            "\nHeal: " + std::to_string(active_pet->second.heal_per_two_seconds) +
                            " HP / 2 sec";
                    }
                    if (active_pet->second.auto_pickup) {
                        pet_overlay.effect_name += "\nAuto Pickup";
                    }
                    if (equipped_pet_expire_at > 0) {
                        const std::int64_t remaining =
                            static_cast<std::int64_t>(equipped_pet_expire_at) -
                            static_cast<std::int64_t>(std::time(nullptr));
                        if (remaining > 0) {
                            pet_overlay.expire_ms =
                                now_ms + static_cast<std::uint64_t>(remaining) * 1000ull;
                        }
                    }
                    buff_slots.push_back(std::move(pet_overlay));
                }
                char_info_win.set_apply_buffs(std::move(apply_buffs));
                client::BuffSlotBar::draw(ui, buff_x, buff_y, buff_slots, now_ms, game_hud.scale());
                hud_buff_slots = std::move(buff_slots);
                hud_buff_now_ms = now_ms;
                hud_buff_tip = client::BuffSlotBar::hit_test(buff_x,
                                                             buff_y,
                                                             hud_buff_slots,
                                                             now_ms,
                                                             game_hud.scale(),
                                                             input.mouse_x,
                                                             input.mouse_y);
            }
            {
                client::SkillAbilityVars ability_vars;
                ability_vars.has_stats = true;
                const auto stats = net.char_info_stats();
                ability_vars.char_level = stats.level;
                ability_vars.char_str = stats.str;
                ability_vars.char_hth = stats.hth;
                ability_vars.char_int = stats.intl;
                ability_vars.char_wis = stats.wis;
                ability_vars.char_dex = stats.dex;
                ability_vars.lightning_mastery = player_skills.level_of(12);
                ability_vars.char_org_dodge = stats.dodge;
                skill_bar.set_ability_vars(ability_vars);
                skill_win.set_ability_vars(ability_vars);
            }
            skill_bar.set_mortal_points(net.mortal());
            skill_bar.draw(ui, tip_font.ready() ? &tip_font : &name_font);
            skill_bar.draw_drag_ghost(
                ui, input.mouse_x, input.mouse_y, tip_font.ready() ? &tip_font : &name_font);
            if (is_focus_shot_charging()) {
                const auto* focus_def =
                    skill_catalog.find(2, static_cast<int>(pending_skill_index));
                const std::string_view focus_name = focus_def ? focus_def->name() : "Focus Shot";
                client::PileSkillBar::draw(ui,
                                           *engine,
                                           tip_font.ready() ? tip_font : name_font,
                                           static_cast<float>(input.width),
                                           static_cast<float>(input.height),
                                           focus_shot_elapsed_sec * 1000.0f,
                                           static_cast<float>(protocol::kArcherFocusShotBarMs),
                                           focus_name);
            }
            if (fishing_active) {
                const float elapsed_ms =
                    static_cast<float>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                           std::chrono::steady_clock::now() - fishing_cycle_started)
                                           .count());
                client::FishingBar::draw(ui,
                                         *engine,
                                         tip_font.ready() ? tip_font : name_font,
                                         static_cast<float>(input.width),
                                         static_cast<float>(input.height),
                                         game_hud.scale(),
                                         elapsed_ms,
                                         static_cast<float>(fishing_cycle_duration_ms));
            }
            if (cooking_active) {
                const float elapsed_ms =
                    static_cast<float>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                           std::chrono::steady_clock::now() - cooking_started)
                                           .count());
                client::FishingBar::draw(ui,
                                         *engine,
                                         tip_font.ready() ? tip_font : name_font,
                                         static_cast<float>(input.width),
                                         static_cast<float>(input.height),
                                         game_hud.scale(),
                                         elapsed_ms,
                                         static_cast<float>(server::kSpeedMedicineDurationMs),
                                         "Making Speed-up Medicine");
            }
        }

        inv_win.sync_fonts(*engine);
        inv_win.draw(ui, *engine, player_bag, &item_cooldowns);
        skill_win.sync_fonts(*engine);
        skill_win.draw(*engine, ui);
        skill_win.draw_drag_ghost(ui, input.mouse_x, input.mouse_y);
        transform_win.sync_fonts(*engine);
        transform_win.draw(*engine, ui);
        transform_win.draw_drag_ghost(ui, input.mouse_x, input.mouse_y);
        {
            auto stats = net.char_info_stats();
            if (stats.hp <= 0 && player_hp > 0) {
                stats.hp = player_hp;
                stats.max_hp = player_max_hp;
                stats.mp = player_mp;
                stats.max_mp = player_max_mp;
            }
            stats.level = std::max(stats.level, inv_win.player_level());
            char_info_win.set_stats(stats);
            update_character_info_equipment_bonuses();
        }
        char_info_win.sync_fonts(*engine);
        char_info_win.draw(*engine, ui, name_font.ready() ? name_font : tip_font);
        const auto char_info_buff_now = std::chrono::steady_clock::now().time_since_epoch();
        const std::uint64_t char_info_buff_now_ms = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(char_info_buff_now).count());
        const std::string char_info_buff_tip = char_info_win.apply_buff_tooltip(
            input.mouse_x, input.mouse_y, char_info_buff_now_ms, &ui);
        npc_talk_win.sync_fonts(*engine);
        npc_talk_win.draw(*engine, ui, tip_font.ready() ? tip_font : name_font);
        speed_medicine_win.draw(*engine, ui, tip_font.ready() ? tip_font : name_font, player_bag);
        consumable_crafting_win.draw(
            *engine, ui, tip_font.ready() ? tip_font : name_font, player_bag);
        quest_journal_win.sync_fonts(*engine);
        quest_journal_win.draw(*engine, ui, tip_font.ready() ? tip_font : name_font, player_bag);
        if (const auto* item = item_catalog.find(mail_window.selected_item_index())) {
            mail_window.set_attachment_info(item->name(),
                                            item_catalog.icon_filename(item->index()));
        } else {
            mail_window.set_attachment_info({}, {});
        }
        mail_window.draw(*engine, ui, tip_font.ready() ? tip_font : name_font);
        guild_window.draw(*engine, ui, tip_font.ready() ? tip_font : name_font);
        if (mail_window.take_read_dirty()) {
            ui_settings.set_read_mail_ids(boot.selected.id, mail_window.read_ids());
            persist_hud();
        }
        blacksmith_win.sync_fonts(*engine);
        blacksmith_win.draw(
            *engine, ui, tip_font.ready() ? tip_font : name_font, player_bag, item_catalog);
        mix_win.draw(
            *engine, ui, tip_font.ready() ? tip_font : name_font, player_bag, item_catalog);
        shop_win.sync_fonts(*engine);
        shop_win.draw(
            *engine, ui, tip_font.ready() ? tip_font : name_font, player_bag, item_catalog);
        skin_shop_win.draw(*engine,
                           ui,
                           tip_font.ready() ? tip_font : name_font,
                           player_bag,
                           item_catalog,
                           player_wear,
                           profile.data_root(),
                           boot.selected.face,
                           boot.selected.hair,
                           prop_tex_cache,
                           cam.view(local_player),
                           proj);
        storage_win.sync_fonts(*engine);
        storage_win.draw(
            *engine, ui, tip_font.ready() ? tip_font : name_font, player_bag, item_catalog);
        {
            const client::FreetypeFont& banner =
                banner_font.ready() ? banner_font : (tip_font.ready() ? tip_font : name_font);
            enchant_overlay.draw(ui,
                                 *engine,
                                 banner,
                                 static_cast<float>(input.width),
                                 static_cast<float>(input.height));
            riding_cache_overlay.draw(ui,
                                      *engine,
                                      banner,
                                      item_catalog,
                                      static_cast<float>(input.width),
                                      static_cast<float>(input.height));
        }
        if (riding_window_open) {
            const int body_size = std::clamp(riding_view.font_pixel_size(), 8, 48);
            const client::UiWidget* title_widget = riding_view.find_widget("title");
            const int title_size = title_widget && title_widget->pixel_size() > 0
                                       ? title_widget->pixel_size()
                                       : body_size;
            if (!riding_body_font.ready() || riding_body_font.pixel_size() != body_size) {
                riding_body_font.destroy(*engine);
                (void) riding_body_font.load(
                    *engine, client::FreetypeFont::find_system_sans(), body_size);
            }
            if (!riding_title_font.ready() || riding_title_font.pixel_size() != title_size) {
                riding_title_font.destroy(*engine);
                (void) riding_title_font.load(
                    *engine, client::FreetypeFont::find_system_sans_bold(), title_size);
            }
            client::UiVarMap vars;
            vars["ui.riding_title"] = "Riding animals";
            vars["riding.status_label"] = "Status";
            vars["riding.duration_label"] = "Duration";
            vars["riding.grade_label"] = "Grade";
            vars["riding.satiety_label"] = "Satiety";
            vars["riding.mount_label"] = "Riding pet";
            vars["riding.helmet_label"] = "Helmet";
            vars["riding.armor_label"] = "Armor";
            vars["riding.saddle_label"] = "Saddle";
            vars["riding.cloth_label"] = "Saddle cloth";
            vars["riding.magnet_label"] = "Magnet";
            vars["riding.feed_label"] = "Feed";
            vars["riding.ability_label"] = "Ability +";
            vars["riding.reroll_label"] = "Reroll";
            vars["riding.enchant_label"] = "Enchant first gear";
            vars["riding.upgrade_label"] = "Upgrade +10 gear";
            vars["riding.registered"] = player_wear.ride_index() > 0 ? "true" : "false";
            vars["riding.empty"] = player_wear.ride_index() > 0 ? "false" : "true";
            const bool battle_riding = registered_riding_kind == protocol::RidingKind::Battle &&
                                       player_wear.ride_index() > 0;
            vars["riding.common"] = !battle_riding ? "true" : "false";
            vars["riding.battle"] = battle_riding ? "true" : "false";
            vars["riding.status"] = player_wear.ride_index() == 0 ? "Not registered"
                                    : local_riding_mounted        ? "Mounted"
                                                                  : "Registered";
            vars["riding.equipment_hint"] = !riding_feedback.empty() ? riding_feedback
                                            : battle_riding
                                                ? "Drag gear in/out. Right-click to remove."
                                                : "Equipment requires a Battle Riding pet.";
            vars["riding.hint"] = player_wear.ride_index() > 0
                                      ? "Press R to mount or dismount."
                                      : "Right-click a riding item to register it.";
            std::string riding_name = "No riding animal registered";
            if (const client::ItemDef* item = item_catalog.find(player_wear.ride_index());
                item && !item->name().empty()) {
                riding_name = item->name();
            }
            vars["riding.name"] = riding_name;
            std::string remaining = "Permanent";
            if (registered_ride_expire_at > 0) {
                const std::int64_t seconds = static_cast<std::int64_t>(registered_ride_expire_at) -
                                             static_cast<std::int64_t>(std::time(nullptr));
                if (seconds <= 0) {
                    remaining = "Expired";
                } else {
                    const std::int64_t days = seconds / 86400;
                    const std::int64_t hours = (seconds % 86400) / 3600;
                    remaining = std::to_string(days) + "d " + std::to_string(hours) + "h left";
                }
            }
            vars["riding.remaining"] = player_wear.ride_index() > 0 ? remaining : "--";
            constexpr std::array<std::string_view, 4> kGradeNames{
                "Normal", "Uncommon", "Rare", "Epic"};
            constexpr std::array<std::string_view, 6> kRarityNames{
                "White", "Blue", "Green", "Orange", "Red", "Purple"};
            const std::size_t grade_index = std::min<std::size_t>(
                static_cast<std::size_t>(registered_battle_grade), kGradeNames.size() - 1);
            const std::size_t rarity_index =
                std::min<std::size_t>(registered_battle_rarity, kRarityNames.size() - 1);
            vars["riding.grade"] = std::string(kGradeNames[grade_index]);
            vars["riding.rarity"] = std::string(kRarityNames[rarity_index]);
            vars["riding.grade_line"] =
                battle_riding ? vars["riding.grade"] + " (" + vars["riding.rarity"] + ")" : "--";
            vars["riding.satiety"] = battle_riding
                                         ? std::to_string(registered_battle_satiety) + " / " +
                                               std::to_string(registered_battle_max_satiety)
                                         : "--";
            bool duplicate_present = false;
            if (stablemaster_duplicate_iid != 0) {
                duplicate_present =
                    std::any_of(player_bag.normal.begin(),
                                player_bag.normal.end(),
                                [&](const client::ItemStack& stack) {
                                    return stack.iid == stablemaster_duplicate_iid &&
                                           stack.count > 0 && !stack.equipped &&
                                           stack.iid != registered_ride_iid;
                                });
                if (!duplicate_present) {
                    stablemaster_duplicate_iid = 0;
                    riding_host.set_duplicate_item_index(0);
                }
            }
            vars["stablemaster.open"] = stablemaster_npc_id != 0 ? "true" : "false";
            vars["stablemaster.title"] = "Stablemaster services";
            vars["stablemaster.seals"] =
                "Riding Seals: " + std::to_string(player_bag.count_item(41322));
            vars["stablemaster.exchange_label"] = "Horse (2 Seals)";
            vars["stablemaster.promote_label"] = "Promote (6 Seals)";
            constexpr std::array<int, 3> kStableGradeCosts{5, 10, 20};
            vars["stablemaster.grade_label"] =
                registered_battle_grade == protocol::BattleRidingGrade::Epic
                    ? "Grade: Epic"
                    : "Grade (" +
                          std::to_string(kStableGradeCosts[std::min<std::size_t>(
                              static_cast<std::size_t>(registered_battle_grade),
                              kStableGradeCosts.size() - 1)]) +
                          " Seals)";
            vars["stablemaster.writ_label"] = "Time Writ (+7d)";
            vars["stablemaster.duplicate_label"] = "Convert (+2 Seals)";
            vars["stablemaster.duplicate_selection"] = duplicate_present ? "Ready" : "Drag Pet";
            client::ui_fill_theme_vars(riding_view, vars);
            const float s = game_hud.scale();
            const float riding_w = riding_view.window_w() * s;
            const float riding_h = riding_view.window_h() * s;
            riding_host.set_pointer(riding_hit_at(input.mouse_x, input.mouse_y, s),
                                    input.mouse_left);
            client::UiView::draw(*engine,
                                 ui,
                                 riding_view,
                                 riding_window_x,
                                 riding_window_y,
                                 riding_w,
                                 riding_h,
                                 s,
                                 vars,
                                 riding_body_font,
                                 riding_title_font,
                                 &riding_host,
                                 true);
            if (inv_win.drag_slot() >= 0) {
                const auto hit = riding_hit_at(input.mouse_x, input.mouse_y, s);
                const auto* target = riding_view.find_widget(hit.widget_id());
                if (target && client::riding_equipment_slot(target->bind()) >= 0) {
                    const bool accepted = inv_win.tab() == client::Tab::Normal &&
                                          client::riding_equipment_accepts(
                                              *target, riding_host.state(), inv_win.drag_index());
                    engine->draw_ui_rect(riding_window_x + target->x() * s,
                                         riding_window_y + target->y() * s,
                                         target->w() * s,
                                         target->h() * s,
                                         accepted ? 0.3f : 0.9f,
                                         accepted ? 0.8f : 0.2f,
                                         0.2f,
                                         0.35f);
                }
            }
        }
        inv_win.draw_drag_ghost(ui, input.mouse_x, input.mouse_y);
        if (riding_equipment_dragging && riding_pressed_slot >= 0) {
            const std::string icon = item_catalog.icon_filename(
                registered_battle_equipment_indices[static_cast<std::size_t>(riding_pressed_slot)]);
            const float size = 32.0f * game_hud.scale();
            if (!icon.empty() && ui.item_tex(icon)) {
                ui.draw_item_tex(
                    icon, input.mouse_x - size * 0.5f, input.mouse_y - size * 0.5f, size, size);
            }
        }
        party_window.draw(*engine, ui, tip_font.ready() ? tip_font : name_font, input);
        player_options.draw(*engine, ui, tip_font.ready() ? tip_font : name_font);
        game_menu.draw(*engine, ui, tip_font.ready() ? tip_font : name_font, &input);
        settings_window.draw(*engine, ui, tip_font.ready() ? tip_font : name_font, &input);

        // Cursor feedback uses the same targets and precedence as world clicks.
        {
            client::CursorKind kind = client::CursorKind::Default;
            std::uint32_t cursor_target = 0;
            const bool over_ui = world_map_open || menu_pointer_blocked() ||
                                 skill_bar.contains(input.mouse_x, input.mouse_y) ||
                                 game_hud.contains(input.mouse_x, input.mouse_y) ||
                                 game_hud.chat().contains_pointer(input.mouse_x,
                                                                  input.mouse_y,
                                                                  chat_panel.x(),
                                                                  chat_panel.y(),
                                                                  chat_panel.w(),
                                                                  chat_panel.h(),
                                                                  game_hud.scale());
            if (!over_ui && want_net && !cam.fly_mode() && !local_player.dead()) {
                const auto [hit_mob,
                            best,
                            hit_item,
                            best_item,
                            hit_npc,
                            best_npc,
                            hit_player,
                            best_player] = pick_world_targets();
                const bool prefer_player = hit_player != 0 &&
                                           (hit_mob == 0 || best_player <= best) &&
                                           (hit_npc == 0 || best_player <= best_npc) &&
                                           (hit_item == 0 || best_player <= best_item);
                const bool prefer_npc = !prefer_player && hit_npc != 0 &&
                                        (hit_mob == 0 || best_npc <= best) &&
                                        (hit_item == 0 || best_npc <= best_item);
                if (prefer_player && net.castle_enemy(hit_player) &&
                    !net.castle_defeated(hit_player)) {
                    kind = client::CursorKind::Attack;
                } else if (!prefer_player && !prefer_npc) {
                    if (hit_item != 0 && (hit_mob == 0 || best_item <= best)) {
                        kind = client::CursorKind::Item;
                        cursor_target = hit_item;
                    } else if (hit_mob != 0) {
                        kind = client::CursorKind::Attack;
                        const auto hit = net.monsters().find(hit_mob);
                        if (hit != net.monsters().end() &&
                            protocol::MonsterRaceInfo::is_openable(hit->second.race())) {
                            kind = client::CursorKind::Special;
                        }
                    }
                }
            }
            soft_cursor.set(kind, cursor_target);
        }

        {
            const client::FreetypeFont& tip = tip_font.ready() ? tip_font : name_font;
            std::string text = char_info_buff_tip;
            if (text.empty() && hud_buff_tip >= 0 &&
                hud_buff_tip < static_cast<int>(hud_buff_slots.size())) {
                text = client::BuffSlotBar::tooltip_text(
                    hud_buff_slots[static_cast<std::size_t>(hud_buff_tip)], hud_buff_now_ms);
            }
            if (!text.empty() && tip.ready()) {
                std::vector<client::TipLine> lines;
                std::size_t start = 0;
                bool first = true;
                while (start <= text.size()) {
                    const std::size_t nl = text.find('\n', start);
                    client::TipLine line;
                    line.add(text.substr(
                                 start, nl == std::string::npos ? text.size() - start : nl - start),
                             first ? 0.92f : 1.0f,
                             first ? 0.92f : 0.65f,
                             first ? 0.88f : 0.15f);
                    lines.push_back(std::move(line));
                    if (nl == std::string::npos) {
                        break;
                    }
                    start = nl + 1;
                    first = false;
                }
                client::ItemTooltip::draw(ui,
                                          *engine,
                                          tip,
                                          lines,
                                          input.mouse_x,
                                          input.mouse_y,
                                          static_cast<float>(input.width),
                                          static_cast<float>(input.height));
            }
            const auto riding_hover = riding_hit_at(input.mouse_x, input.mouse_y, game_hud.scale());
            if (riding_hover.kind() == client::UiHitKind::None) {
                inv_win.draw_tooltip(ui,
                                     *engine,
                                     player_bag,
                                     tip_font.ready() ? &tip_font : &name_font,
                                     static_cast<float>(input.width),
                                     static_cast<float>(input.height));
                shop_win.draw_tooltip(ui,
                                      *engine,
                                      tip,
                                      item_catalog,
                                      static_cast<float>(input.width),
                                      static_cast<float>(input.height),
                                      inv_win.player_clazz(),
                                      inv_win.player_level());
                storage_win.draw_tooltip(ui,
                                         *engine,
                                         tip,
                                         item_catalog,
                                         static_cast<float>(input.width),
                                         static_cast<float>(input.height),
                                         inv_win.player_clazz(),
                                         inv_win.player_level());
                skill_win.draw_tooltip(
                    *engine, ui, static_cast<float>(input.width), static_cast<float>(input.height));
                transform_win.draw_tooltip(*engine,
                                           ui,
                                           tip,
                                           static_cast<float>(input.width),
                                           static_cast<float>(input.height),
                                           inv_win.player_clazz(),
                                           inv_win.player_level());
                skill_bar.draw_tooltip(
                    ui, tip, static_cast<float>(input.width), static_cast<float>(input.height));
            }
            if (riding_hover.kind() == client::UiHitKind::IconSlot && inv_win.drag_slot() < 0 &&
                !riding_equipment_dragging && !game_menu.open() && !settings_window.open()) {
                const int slot = client::riding_equipment_slot(riding_hover.bind());
                const int item_index =
                    slot < 0 ? player_wear.ride_index()
                    : registered_battle_equipment_iids[static_cast<std::size_t>(slot)] != 0
                        ? registered_battle_equipment_indices[static_cast<std::size_t>(slot)]
                        : 0;
                if (const auto* def = item_catalog.find(item_index)) {
                    std::string markup = client::ItemTooltip::make_tip(
                        *def, item_catalog, 1, inv_win.player_clazz(), inv_win.player_level());
                    if (slot >= 0) {
                        markup += "#n#3Enchant +" +
                                  std::to_string(
                                      registered_battle_equipment_enchants[static_cast<std::size_t>(
                                          slot)]);
                        markup += "#n#0Right-click to remove.";
                    }
                    client::ItemTooltip::draw(ui,
                                              *engine,
                                              tip,
                                              client::ItemTooltip::parse_markup(markup),
                                              input.mouse_x,
                                              input.mouse_y,
                                              static_cast<float>(input.width),
                                              static_cast<float>(input.height));
                }
            }
        }

        if (!doggebi_ritual_overlay.empty() &&
            std::chrono::steady_clock::now() < doggebi_ritual_overlay_until) {
            const client::FreetypeFont& hint_font = tip_font.ready() ? tip_font : name_font;
            if (hint_font.ready()) {
                const float ui_scale = static_cast<float>(input.height) / 600.0f;
                const float text_scale = 0.55f * ui_scale;
                hint_font.draw_centered(*engine,
                                        static_cast<float>(input.width) * 0.5f,
                                        static_cast<float>(input.height) * 0.32f,
                                        doggebi_ritual_overlay,
                                        0.92f,
                                        0.82f,
                                        0.45f,
                                        text_scale,
                                        0.9f);
            }
        }

        if (doggebi_countdown_timer != protocol::MapStateTimer::None &&
            doggebi_countdown_seconds > 0) {
            const auto elapsed_seconds = static_cast<std::uint32_t>(
                std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() -
                                                                 doggebi_countdown_received)
                    .count());
            const std::uint32_t remaining = elapsed_seconds >= doggebi_countdown_seconds
                                                ? 0
                                                : doggebi_countdown_seconds - elapsed_seconds;
            if (remaining > 0) {
                const client::FreetypeFont& timer_font = tip_font.ready() ? tip_font : name_font;
                if (timer_font.ready()) {
                    const std::uint32_t minutes = remaining / 60u;
                    const std::uint32_t seconds = remaining % 60u;
                    const std::string label =
                        doggebi_countdown_timer == protocol::MapStateTimer::Ritual
                            ? "Doggebi Lord arrival: "
                            : "Doggebi Lord retreat: ";
                    const std::string timer = label + std::to_string(minutes) + ":" +
                                              (seconds < 10u ? "0" : "") + std::to_string(seconds);
                    const float ui_scale = static_cast<float>(input.height) / 600.0f;
                    timer_font.draw_centered(*engine,
                                             static_cast<float>(input.width) * 0.5f,
                                             static_cast<float>(input.height) * 0.10f,
                                             timer,
                                             0.96f,
                                             0.83f,
                                             0.42f,
                                             0.58f * ui_scale,
                                             0.95f);
                }
            }
        }

        if (show_frame_stats || show_debug) {
            const auto profile_sample_at = std::chrono::steady_clock::now();
            const auto st = engine->frame_stats();
            if (show_frame_stats) {
                frame_profiler.record_timing(
                    input.dt * 1000.0f,
                    st.busy_ms,
                    std::chrono::duration<float, std::milli>(world_draw_started -
                                                             frame_work_started)
                        .count(),
                    std::chrono::duration<float, std::milli>(ui_draw_started - world_draw_started)
                        .count(),
                    std::chrono::duration<float, std::milli>(profile_sample_at - ui_draw_started)
                        .count(),
                    st.present_ms);
            }
            const std::size_t prop_count =
                props.size() + (indoor_view.loaded() ? indoor_view.props().size() : 0);
            std::array<char, 384> line{};
            if (show_debug) {
                const int chunk_x = coords::patch_index_for_client(local_player.pos().x);
                const int chunk_y = coords::patch_index_for_client(local_player.pos().z);
                const std::string chunk = client::DebugOverlay::patch_name(chunk_x, chunk_y);
                if (show_frame_stats) {
                    std::snprintf(line.data(),
                                  line.size(),
                                  "fps %.0f  %s  busy %.1fms  draws %d (mesh %d terrain "
                                  "%d ui %d other %d)  "
                                  "props %zu/%zu  fx emitters %zu culled %zu quads %zu ribbons %zu "
                                  "batches %zu",
                                  static_cast<double>(st.fps),
                                  chunk.c_str(),
                                  static_cast<double>(st.busy_ms),
                                  st.total(),
                                  st.mesh,
                                  st.terrain,
                                  st.ui,
                                  st.other,
                                  visible_props.size(),
                                  prop_count,
                                  sprite_fx.last_draw_emitters(),
                                  sprite_fx.last_culled_emitters(),
                                  sprite_fx.last_draw_billboards(),
                                  sprite_fx.last_draw_ribbons(),
                                  sprite_fx.last_draw_batches());
                } else {
                    std::snprintf(line.data(),
                                  line.size(),
                                  "fps %.0f  %s",
                                  static_cast<double>(st.fps),
                                  chunk.c_str());
                }
            } else {
                std::snprintf(line.data(),
                              line.size(),
                              "fps %.0f  busy %.1fms  draws %d (mesh %d terrain %d ui "
                              "%d other %d)  props %zu/%zu  fx emitters %zu culled %zu quads %zu "
                              "ribbons %zu batches %zu",
                              static_cast<double>(st.fps),
                              static_cast<double>(st.busy_ms),
                              st.total(),
                              st.mesh,
                              st.terrain,
                              st.ui,
                              st.other,
                              visible_props.size(),
                              prop_count,
                              sprite_fx.last_draw_emitters(),
                              sprite_fx.last_culled_emitters(),
                              sprite_fx.last_draw_billboards(),
                              sprite_fx.last_draw_ribbons(),
                              sprite_fx.last_draw_batches());
            }
            std::vector<std::string> debug_lines{line.data()};
            if (show_debug) {
                const engine::Vec3& pose = local_player.pos();
                const int origin = profile.origin_patch();
                const int server_x = coords::to_server(pose.x, origin);
                const int server_y = coords::to_server(pose.z, origin);
                const int server_z = coords::to_server_z(pose.y);
                float dir_deg = std::fmod(local_player.yaw() * 180.0f / 3.14159265f, 360.0f);
                if (dir_deg < 0.0f) {
                    dir_deg += 360.0f;
                }
                std::array<char, 192> pose_line{};
                std::snprintf(pose_line.data(),
                              pose_line.size(),
                              "server %d %d %d  dir %.1f  client %.1f %.1f %.1f",
                              server_x,
                              server_y,
                              server_z,
                              static_cast<double>(dir_deg),
                              static_cast<double>(pose.x),
                              static_cast<double>(pose.y),
                              static_cast<double>(pose.z));
                debug_lines.emplace_back(pose_line.data());
                // Live chest range snapshot for selected / pending / opening openable.
                std::uint32_t dbg_chest = 0;
                if (opening_chest_id != 0) {
                    dbg_chest = opening_chest_id;
                } else if (pending_chest_id != 0) {
                    dbg_chest = pending_chest_id;
                } else if (selected_mob_id != 0) {
                    dbg_chest = selected_mob_id;
                }
                std::array<char, 320> chest_live{};
                auto cit = net.monsters().find(dbg_chest);
                if (dbg_chest != 0 && cit != net.monsters().end() &&
                    protocol::MonsterRaceInfo::is_openable(cit->second.race())) {
                    const float dx = cit->second.wx() - local_player.pos().x;
                    const float dz = cit->second.wz() - local_player.pos().z;
                    const float client_dist = std::sqrt(dx * dx + dz * dz);
                    const int pose_sx =
                        coords::to_server(local_player.pos().x, profile.origin_patch());
                    const int pose_sy =
                        coords::to_server(local_player.pos().z, profile.origin_patch());
                    const long long pdx = static_cast<long long>(cit->second.x()) - pose_sx;
                    const long long pdy = static_cast<long long>(cit->second.y()) - pose_sy;
                    const double pose_dist = std::sqrt(static_cast<double>(pdx * pdx + pdy * pdy));
                    const long long bdx = static_cast<long long>(cit->second.x()) - net.server_px();
                    const long long bdy = static_cast<long long>(cit->second.y()) - net.server_py();
                    const double base_dist = std::sqrt(static_cast<double>(bdx * bdx + bdy * bdy));
                    std::snprintf(chest_live.data(),
                                  chest_live.size(),
                                  "chest id=%u race=%d hp=%d client=%.1f pose=%.1f "
                                  "base=%.1f range=%d "
                                  "pend=%u open=%u fb=%u",
                                  dbg_chest,
                                  cit->second.race(),
                                  cit->second.hp(),
                                  static_cast<double>(client_dist),
                                  pose_dist,
                                  base_dist,
                                  protocol::kItemMonsterOpenRange,
                                  pending_chest_id,
                                  opening_chest_id,
                                  static_cast<unsigned>(net.last_chest_feedback_msg()));
                    debug_lines.emplace_back(chest_live.data());
                }
                if (!chest_debug_last.empty()) {
                    debug_lines.push_back(chest_debug_last);
                }
            }
            client::DebugOverlay::draw_text(*engine, debug_lines, input.width, input.height);
            frame_profiler.log_if_due(line.data());
        }

        if (world_map_open) {
            const client::FreetypeFont& map_font = tip_font.ready() ? tip_font : name_font;
            client::WorldMapWidget::draw(
                ui, *engine, map_font, frame_w, frame_h, world_map_data, world_map_tiles);
        }

        if (skill_bar.take_auto_hp_dirty()) {
            persist_hud();
        }
        talent_tree.draw(*engine, ui, frame_w, frame_h);
        powerbook.draw(*engine, ui, frame_w, frame_h);
        trading_post.draw(
            *engine, ui, tip_font.ready() ? tip_font : name_font, item_catalog, trading_post_input);
        trade_window.draw(
            *engine, ui, tip_font.ready() ? tip_font : name_font, item_catalog, trade_input);
        if (show_fps) {
            client::DebugOverlay::draw_fps(
                *engine, engine->frame_stats().fps, input.width, input.height);
        }
        soft_cursor.draw(ui, input.mouse_x, input.mouse_y, input.dt, input.key_ctrl);
        engine->end_frame();
    }

    net.close();
    embedded_server.stop();
    if (hud_layout_dirty) {
        persist_hud();
    }
    if (store) {
        store->close();
    }
    exp_event_board.destroy();
    soft_cursor.destroy(*engine);
    click_marker.destroy(*engine);
    range_skill_decal.destroy(*engine);
    quest_marker.destroy(*engine);
    indoor_view.unload(floor);
    select_marker.destroy(*engine);
    drop_item_visual.destroy(*engine);
    world_debug.destroy(*engine);
    client::DebugOverlay::destroy(*engine);
    sky_box.destroy(*engine);
    inv_win.destroy_fonts(*engine);
    talent_tree.destroy(*engine);
    powerbook.destroy(*engine);
    skill_win.destroy_fonts(*engine);
    transform_win.destroy_fonts(*engine);
    riding_body_font.destroy(*engine);
    riding_title_font.destroy(*engine);
    char_info_win.destroy_fonts(*engine);
    npc_talk_win.destroy_fonts(*engine);
    blacksmith_win.destroy_fonts(*engine);
    quest_journal_win.destroy_fonts(*engine);
    guild_window.destroy(*engine);
    player_options.destroy(*engine);
    mail_window.destroy_fonts(*engine);
    game_hud.destroy_fonts(*engine);
    skin_shop_win.destroy(*engine);
    shop_win.destroy_fonts(*engine);
    storage_win.destroy_fonts(*engine);
    ui_resources.shutdown(*engine);
    pet_instance.destroy(*engine);
    riding_instance.destroy(*engine);
    for (auto& [id, inst] : monster_instances) {
        detach_monster_visual(id);
        inst.destroy(*engine);
    }
    for (const std::uint32_t id : npc_fx_live) {
        sprite_fx.detach(id);
    }
    for (const std::uint32_t id : world_prop_fx_live) {
        sprite_fx.detach(id);
    }
    sprite_fx.detach(kWeaponMixFxEntity);
    bof_fx.detach(sprite_fx);
    sprite_fx.destroy(*engine);
    remote_players.destroy(*engine);
    player_actor.destroy(*engine);
    if (water_mesh) {
        engine->destroy_mesh(water_mesh);
    }
    if (water_tex) {
        engine->destroy_texture(water_tex);
    }
    for (auto& [path, model] : model_cache) {
        for (auto& part : model.parts) {
            engine->destroy_mesh(part.mesh);
        }
    }
    for (auto& [k, tex] : prop_tex_cache) {
        engine->destroy_texture(tex);
    }
    for (auto& d : drawn) {
        engine->destroy_mesh(d.mesh);
        if (d.shadow_height_map) {
            engine->destroy_texture(d.shadow_height_map);
        }
        for (std::size_t i = 0; i < d.layers.size(); ++i) {
            if (i == 0 || d.layers[i].color.id != d.layers[0].color.id) {
                engine->destroy_texture(d.layers[i].color);
            }
        }
    }
    for (auto& [name, tex] : detail_cache) {
        engine->destroy_texture(tex);
    }
    if (!select_character_requested) {
        engine->shutdown();
    }
    return select_character_requested ? client::ClientSession::kSelectCharacterExitCode : 0;
}
