#include "demo/aos_sim.h"
#include "demo/config.h"
#include "demo/imgui_raylib.h"
#include "demo/renderer.h"
#include "demo/soa_sim.h"
#include "demo/spatial_grid.h"
#include "imgui.h"
#include "jobs/scheduler.h"
#include "profiler/chrome_trace.h"
#include "profiler/profiler.h"
#include "raylib.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <memory>
#include <thread>
#include <vector>

namespace {

std::uint64_t now_us() {
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
                                          std::chrono::steady_clock::now().time_since_epoch())
                                          .count());
}

struct TimingSample {
    float grid_ms = 0.0f;
    float boids_ms = 0.0f;
    float systems_ms = 0.0f;
    float integrate_ms = 0.0f;
    float recolor_ms = 0.0f;
    float bounce_ms = 0.0f;
    float lifetime_ms = 0.0f;
    float render_ms = 0.0f;
    float frame_ms = 0.0f;
};

float wall_ms(const std::vector<hive::profiler::Event>& events, const char* name,
              std::uint64_t frame_start) {
    std::uint64_t lo = ~std::uint64_t{0};
    std::uint64_t hi = 0;
    for (const hive::profiler::Event& event : events) {
        if (event.name == nullptr || std::strcmp(event.name, name) != 0) {
            continue;
        }
        if (event.end_us < frame_start) {
            continue;
        }
        lo = std::min(lo, event.start_us);
        hi = std::max(hi, event.end_us);
    }
    if (hi <= lo || lo == ~std::uint64_t{0}) {
        return 0.0f;
    }
    return static_cast<float>(hi - lo) / 1000.0f;
}

TimingSample sample_timings(std::uint64_t frame_start) {
    const std::vector<hive::profiler::Event> events = hive::profiler::snapshot_all_threads();
    TimingSample sample;
    sample.grid_ms = wall_ms(events, "grid", frame_start);
    sample.boids_ms = wall_ms(events, "boids", frame_start);
    sample.systems_ms = wall_ms(events, "systems", frame_start);
    sample.integrate_ms = wall_ms(events, "integrate", frame_start);
    sample.recolor_ms = wall_ms(events, "recolor", frame_start);
    sample.bounce_ms = wall_ms(events, "bounce", frame_start);
    sample.lifetime_ms = wall_ms(events, "lifetime", frame_start);
    sample.render_ms = wall_ms(events, "render", frame_start);
    sample.frame_ms = wall_ms(events, "frame", frame_start);
    return sample;
}

Camera2D make_camera() {
    Camera2D camera{};
    camera.offset = {static_cast<float>(GetScreenWidth()) * 0.5f,
                     static_cast<float>(GetScreenHeight()) * 0.5f};
    camera.target = {hive::demo::kWorldWidth * 0.5f, hive::demo::kWorldHeight * 0.5f};
    camera.rotation = 0.0f;
    const float zoom_x = static_cast<float>(GetScreenWidth()) / hive::demo::kWorldWidth;
    const float zoom_y = static_cast<float>(GetScreenHeight()) / hive::demo::kWorldHeight;
    camera.zoom = std::min(zoom_x, zoom_y);
    return camera;
}

} // namespace

int main() {
    const unsigned hw = std::max(1u, std::thread::hardware_concurrency());

    hive::demo::DemoConfig config;
    config.threads = hw;
    config.entity_count = hive::demo::kDefaultEntities;
    config.grid_on = true;
    config.layout = hive::demo::Layout::SoA;

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
    InitWindow(1280, 720, "Hive Engine — Swarm");
    SetTargetFPS(0);
    SetExitKey(KEY_NULL);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    hive::demo::imgui_raylib_init();

    hive::profiler::init();

    auto sched = std::make_unique<hive::jobs::Scheduler>(config.threads);
    hive::demo::SpatialGrid grid;
    auto soa = std::make_unique<hive::demo::SoaSim>(*sched, config);
    std::unique_ptr<hive::demo::AosSim> aos;
    hive::demo::Renderer renderer;
    std::vector<hive::demo::Instance> instances;
    instances.reserve(static_cast<std::size_t>(config.entity_count));

    constexpr int kPlotN = 120;
    float frame_plot[kPlotN]{};
    int plot_i = 0;
    TimingSample timings{};
    bool dump_trace = false;

    while (!WindowShouldClose()) {
        const std::uint64_t frame_start = now_us();
        hive::demo::imgui_raylib_new_frame();
        ImGui::NewFrame();

        hive::demo::DemoConfig desired = config;
        ImGui::SetNextWindowPos(ImVec2(12.0f, 12.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(360.0f, 420.0f), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Swarm")) {
            ImGui::Text("FPS %d   frame %.2f ms", GetFPS(), timings.frame_ms);
            ImGui::Text("count %u   instancing %s",
                        config.layout == hive::demo::Layout::SoA ? soa->count()
                                                                 : (aos ? aos->count() : 0),
                        renderer.instancing() ? "on" : "fallback");
            ImGui::SliderInt("entities", &desired.entity_count, hive::demo::kMinEntities,
                             hive::demo::kMaxEntities);
            int threads = static_cast<int>(desired.threads);
            ImGui::SliderInt("threads", &threads, 1, static_cast<int>(hw));
            desired.threads = static_cast<unsigned>(threads);
            ImGui::Checkbox("spatial grid", &desired.grid_on);
            if (!desired.grid_on) {
                ImGui::TextWrapped(
                    "Grid off is O(n^2). At 100k this will hitch — that is the lesson.");
            }
            int layout = desired.layout == hive::demo::Layout::SoA ? 0 : 1;
            ImGui::RadioButton("SoA (ECS)", &layout, 0);
            ImGui::SameLine();
            ImGui::RadioButton("AoS (struct)", &layout, 1);
            desired.layout = layout == 0 ? hive::demo::Layout::SoA : hive::demo::Layout::AoS;

            ImGui::Separator();
            ImGui::Text("grid       %6.2f ms", timings.grid_ms);
            ImGui::Text("boids      %6.2f ms", timings.boids_ms);
            ImGui::Text("systems    %6.2f ms", timings.systems_ms);
            ImGui::Text("integrate  %6.2f ms", timings.integrate_ms);
            ImGui::Text("recolor    %6.2f ms", timings.recolor_ms);
            ImGui::Text("bounce     %6.2f ms", timings.bounce_ms);
            ImGui::Text("lifetime   %6.2f ms", timings.lifetime_ms);
            ImGui::Text("render     %6.2f ms", timings.render_ms);
            ImGui::PlotLines("frame ms", frame_plot, kPlotN, plot_i, nullptr, 0.0f, 40.0f,
                             ImVec2(0.0f, 80.0f));
            if (ImGui::Button("Dump trace.json")) {
                dump_trace = true;
            }
        }
        ImGui::End();

        if (desired.threads != config.threads) {
            sched = std::make_unique<hive::jobs::Scheduler>(desired.threads);
            if (soa) {
                soa->set_scheduler(*sched);
            }
            if (aos) {
                aos->set_scheduler(*sched);
            }
        }
        if (desired.layout != config.layout) {
            if (desired.layout == hive::demo::Layout::SoA) {
                aos.reset();
                soa = std::make_unique<hive::demo::SoaSim>(*sched, desired);
            } else {
                soa.reset();
                aos = std::make_unique<hive::demo::AosSim>(*sched, desired);
            }
        }
        config = desired;
        if (soa) {
            soa->set_config(config);
        }
        if (aos) {
            aos->set_config(config);
        }

        float dt = GetFrameTime();
        if (dt <= 0.0f) {
            dt = 1.0f / 60.0f;
        }
        dt = std::min(dt, 1.0f / 20.0f);

        {
            PROFILE_SCOPE("frame");
            if (config.layout == hive::demo::Layout::SoA && soa) {
                soa->update(dt, grid);
                soa->fill_instances(instances);
            } else if (aos) {
                aos->update(dt, grid);
                aos->fill_instances(instances);
            }

            BeginDrawing();
            ClearBackground(Color{12, 14, 22, 255});
            BeginMode2D(make_camera());
            {
                PROFILE_SCOPE("render");
                renderer.upload(instances);
                renderer.draw(hive::demo::kDrawRadius);
            }
            EndMode2D();

            ImGui::Render();
            hive::demo::imgui_raylib_draw();
            EndDrawing();
        }

        timings = sample_timings(frame_start);
        frame_plot[plot_i] = timings.frame_ms;
        plot_i = (plot_i + 1) % kPlotN;

        if (dump_trace) {
            hive::profiler::write_chrome_trace("trace.json");
            dump_trace = false;
        }
    }

    soa.reset();
    aos.reset();
    sched.reset();
    hive::demo::imgui_raylib_shutdown();
    ImGui::DestroyContext();
    hive::profiler::shutdown();
    CloseWindow();
    return 0;
}
