#include "poko/runtime/engine.h"

#include <memory>
#include <utility>

#include "engine_impl.h"

namespace poko::runtime {

Status Engine::Impl::setup_backends() {
    filesystem = std::make_unique<LocalFileSystem>(desc.asset_root);

    physics = std::make_unique<CustomPhysicsWorld>();
    PhysicsWorldDesc physics_desc;
    physics_desc.fixed_delta_seconds = static_cast<float>(desc.fixed_step_seconds);
    if (!physics->initialize(physics_desc)) {
        return Status::fail(ErrorCode::FailedPrecondition, "physics backend failed to initialize");
    }

    assets = std::make_unique<FilesystemAssetProvider>(*filesystem);
    if (!assets->initialize()) {
        return Status::fail(ErrorCode::FailedPrecondition, "asset backend failed to initialize");
    }

    if (desc.enable_audio) {
        audio = std::make_unique<MixerAudioDevice>();
        if (!audio->initialize()) {
            return Status::fail(ErrorCode::FailedPrecondition, "audio backend failed to initialize");
        }
        SoundDesc ambient;
        ambient.kind = SoundKind::Ambient;
        ambient.logical_path = "audio/demo_ambient.pcm";
        ambient.volume = 0.5f;
        ambient.loop = true;
        const auto sound = audio->load_sound(ambient);
        if (!sound.ok()) {
            return Status::fail(sound.error().code, sound.error().message);
        }
        ambient_sound = sound.value();
        if (!audio->play(ambient_sound).ok()) {
            return Status::fail(ErrorCode::Internal, "failed to start the ambient voice");
        }
        audio_buffer.assign(256, 0.0f);
    }

    if (desc.enable_network) {
        network = std::make_unique<ChannelNetworkTransport>();
        const auto listener = network->listen("127.0.0.1", 47001);
        if (!listener.ok()) {
            return Status::fail(listener.error().code, listener.error().message);
        }
        listen_endpoint = listener.value();
        const auto client = network->connect("127.0.0.1", 47001);
        if (!client.ok()) {
            return Status::fail(client.error().code, client.error().message);
        }
        client_endpoint = client.value();
        network->update(); // complete the connect handshake
    }

    return Status::success();
}

Status Engine::Impl::select_render_device() {
    backend_chain = detail::backend_chain_string();

    std::unique_ptr<api::IRenderDevice> device;
    if (injected_device) {
        device = std::move(injected_device);
    } else if (desc.render_backend == RenderBackend::Auto ||
               desc.render_backend == RenderBackend::Null) {
        // No GPU context exists in a headless/offline build, so the preference
        // chain (Vulkan -> OpenGL -> Null) lands on its last available entry.
        auto chosen = std::make_unique<NullRenderDevice>();
        this->null_device = chosen.get();
        device = std::move(chosen);
    } else {
        return Status::fail(ErrorCode::Unsupported,
                            "OpenGL/Vulkan backends need a window/context; inject a device "
                            "via set_render_device()");
    }

    if (!device || !device->available()) {
        return Status::fail(ErrorCode::FailedPrecondition, "render device is unavailable");
    }
    active_backend = ::poko::to_string(device->backend());
    renderer = std::make_unique<Renderer>(std::move(device));
    return Status::success();
}

} // namespace poko::runtime
