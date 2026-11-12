#include "poko/runtime/engine.h"

#include <cmath>
#include <cstdint>
#include <span>
#include <utility>

#include "engine_impl.h"

namespace poko::runtime {

Engine::Engine() : impl_(std::make_unique<Impl>()) {}
Engine::~Engine() = default;
Engine::Engine(Engine&&) noexcept = default;
Engine& Engine::operator=(Engine&&) noexcept = default;

Status Engine::initialize(const EngineDesc& desc) {
    if (!impl_) {
        return Status::fail(ErrorCode::Internal, "engine implementation is missing");
    }
    if (impl_->initialized) {
        return Status::fail(ErrorCode::AlreadyExists, "engine is already initialized");
    }
    if (!(desc.fixed_step_seconds > 0.0) || !std::isfinite(desc.fixed_step_seconds)) {
        return Status::fail(ErrorCode::InvalidArgument, "fixed_step_seconds must be finite and > 0");
    }
    if (desc.max_steps_per_frame == 0) {
        return Status::fail(ErrorCode::InvalidArgument, "max_steps_per_frame must be > 0");
    }
    impl_->desc = desc;

    // The sample scene ships a real asset so the PRODUCTION filesystem provider
    // decodes it from disk during the frame loop, not from a synthesized MOCK.
    if (desc.load_demo_scene) {
        std::filesystem::path root = desc.asset_root;
        if (root.empty()) {
            root = detail::default_asset_root();
        }
        std::error_code ec;
        std::filesystem::create_directories(root, ec);
        if (ec || !detail::write_demo_asset(root)) {
            return Status::fail(ErrorCode::Internal, "failed to prepare the demo asset root");
        }
        impl_->desc.asset_root = root;
    }

    Status status = impl_->setup_backends();
    if (!status.ok()) {
        (void)shutdown();
        return status;
    }
    status = impl_->select_render_device();
    if (!status.ok()) {
        (void)shutdown();
        return status;
    }
    status = impl_->upload_resources();
    if (!status.ok()) {
        (void)shutdown();
        return status;
    }

    RuntimeDesc runtime_desc;
    runtime_desc.fixed_step_seconds = desc.fixed_step_seconds;
    runtime_desc.max_steps_per_frame = desc.max_steps_per_frame;
    impl_->runtime =
        std::make_unique<GameRuntime>(*impl_->physics, *impl_->assets, runtime_desc);

    if (desc.load_demo_scene) {
        status = build_demo_scene(*impl_->runtime);
        if (!status.ok()) {
            (void)shutdown();
            return status;
        }
    }
    impl_->runtime->set_render_fn(
        [impl = impl_.get()](const FrameContext& ctx) -> Status {
            return impl->render_frame(ctx);
        });

    impl_->initialized = true;
    return Status::success();
}

Status Engine::tick(double frame_seconds) {
    if (!impl_ || !impl_->initialized) {
        return Status::fail(ErrorCode::FailedPrecondition, "engine is not initialized");
    }

    if (impl_->audio && !impl_->audio_buffer.empty()) {
        (void)impl_->audio->render(impl_->audio_buffer, 48000u);
    }
    if (impl_->network) {
        (void)impl_->network->send(
            impl_->client_endpoint, ChannelReliableOrdered,
            std::span<const std::byte>(impl_->state_packet.data(), impl_->state_packet.size()));
        impl_->network->update();
        (void)impl_->network->receive(
            [](const PacketDesc&, std::span<const std::byte>) {});
    }

    return impl_->runtime->tick(frame_seconds);
}

Status Engine::run(std::uint32_t frames, double frame_seconds) {
    for (std::uint32_t i = 0; i < frames; ++i) {
        Status status = tick(frame_seconds);
        if (!status.ok()) {
            return status;
        }
    }
    return Status::success();
}

Status Engine::shutdown() {
    if (!impl_) {
        return Status::success();
    }
    // Tear down in reverse owning order; release GPU handles while the device
    // is still alive so a re-initialize starts from a clean device.
    impl_->runtime.reset();
    if (impl_->renderer) {
        for (const auto handle : impl_->material_handles) {
            (void)impl_->materials->destroy(handle);
        }
        for (const auto handle : impl_->mesh_handles) {
            (void)impl_->meshes->destroy(handle);
        }
    }
    impl_->draw_builder.reset();
    impl_->materials.reset();
    impl_->meshes.reset();
    impl_->renderer.reset();
    impl_->null_device = nullptr;
    impl_->network.reset();
    impl_->audio.reset();
    impl_->assets.reset();
    impl_->physics.reset();
    impl_->filesystem.reset();
    impl_->injected_device.reset();
    impl_->mesh_handles.clear();
    impl_->material_handles.clear();
    impl_->audio_buffer.clear();
    impl_->initialized = false;
    return Status::success();
}

bool Engine::initialized() const noexcept {
    return impl_ && impl_->initialized;
}

EngineStats Engine::stats() const {
    EngineStats out;
    if (!impl_) {
        return out;
    }
    out.active_backend = impl_->active_backend;
    out.backend_chain = impl_->backend_chain;
    out.device_recoveries = impl_->device_recoveries;
    out.resource_failures = impl_->resource_failures;
    out.skipped_draw_items = impl_->skipped_draw_items;
    if (!impl_->initialized) {
        return out;
    }
    out.frames = impl_->runtime->frame_index();
    out.simulation_steps = impl_->runtime->simulation_step_count();
    out.draw_calls = impl_->renderer->stats().draw_calls();
    out.triangles = impl_->renderer->stats().triangles();
    out.asset_cache_size = impl_->assets->cache_size();
    out.physics_bodies = impl_->physics->body_count();
    if (impl_->audio) {
        out.audio_active_voices = impl_->audio->stats().active_voices;
    }
    if (impl_->network) {
        out.transport_framed = impl_->network->framed_count();
    }
    return out;
}

const char* Engine::active_backend_name() const noexcept {
    return impl_ ? impl_->active_backend.c_str() : "unknown";
}

void Engine::set_render_device(std::unique_ptr<api::IRenderDevice> device) noexcept {
    if (impl_) {
        impl_->injected_device = std::move(device);
    }
}

void Engine::simulate_device_loss() noexcept {
    if (impl_ && impl_->null_device != nullptr) {
        impl_->null_device->simulate_device_loss();
    }
}

Status Engine::recover_device() noexcept {
    if (!impl_ || !impl_->renderer) {
        return Status::fail(ErrorCode::FailedPrecondition, "engine is not initialized");
    }
    api::IRenderDevice& gpu = impl_->renderer->device();
    if (!gpu.device_lost()) {
        return Status::fail(ErrorCode::FailedPrecondition, "render device is not lost");
    }
    Status status = gpu.recover_from_device_loss();
    if (status.ok()) {
        ++impl_->device_recoveries;
    }
    return status;
}

CustomPhysicsWorld& Engine::physics() noexcept {
    return *impl_->physics;
}
FilesystemAssetProvider& Engine::assets() noexcept {
    return *impl_->assets;
}
MixerAudioDevice& Engine::audio() noexcept {
    return *impl_->audio;
}
ChannelNetworkTransport& Engine::network() noexcept {
    return *impl_->network;
}
LocalFileSystem& Engine::filesystem() noexcept {
    return *impl_->filesystem;
}
Renderer& Engine::renderer() noexcept {
    return *impl_->renderer;
}
graphics::MeshLibrary& Engine::meshes() noexcept {
    return *impl_->meshes;
}
graphics::MaterialLibrary& Engine::materials() noexcept {
    return *impl_->materials;
}
graphics::DrawCallBuilder& Engine::draw_calls() noexcept {
    return *impl_->draw_builder;
}
GameRuntime& Engine::runtime() noexcept {
    return *impl_->runtime;
}
api::IRenderDevice& Engine::render_device() noexcept {
    return impl_->renderer->device();
}

} // namespace poko::runtime
