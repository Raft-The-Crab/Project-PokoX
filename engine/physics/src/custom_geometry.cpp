#include <cstddef>

#include "geometry.h"
#include "poko/physics/custom_world.h"

namespace poko {

// Out of line: the geometry type is incomplete in the public header.
CustomPhysicsWorld::CustomPhysicsWorld() = default;
CustomPhysicsWorld::~CustomPhysicsWorld() = default;

namespace {

template <typename Vec>
HandleIndex store(Vec& slots, std::vector<Generation>& generations, std::vector<HandleIndex>& free_list,
                  std::shared_ptr<physics::detail::Geometry> geometry) {
    HandleIndex index;
    if (!free_list.empty()) {
        index = free_list.back();
        free_list.pop_back();
        slots[index] = std::move(geometry);
    } else {
        index = static_cast<HandleIndex>(slots.size());
        slots.push_back(std::move(geometry));
        generations.push_back(0);
    }
    return index;
}

} // namespace

Result<GeometryHandle> CustomPhysicsWorld::create_convex_hull(std::span<const math::Vec3> points) {
    auto built = physics::detail::build_hull(points); // no world state touched: build outside the lock
    if (!built.ok()) return Result<GeometryHandle>::fail(built.error().code, built.error().message);
    std::lock_guard<std::mutex> guard(mutex_);
    const auto index = store(geometries_, geometry_generations_, geometry_free_, built.value());
    return Result<GeometryHandle>::success(make_handle(index, geometry_generations_[index]));
}

Result<GeometryHandle> CustomPhysicsWorld::create_triangle_mesh(std::span<const math::Vec3> vertices,
                                                                std::span<const std::uint32_t> indices) {
    auto built = physics::detail::build_mesh(vertices, indices);
    if (!built.ok()) return Result<GeometryHandle>::fail(built.error().code, built.error().message);
    std::lock_guard<std::mutex> guard(mutex_);
    const auto index = store(geometries_, geometry_generations_, geometry_free_, built.value());
    return Result<GeometryHandle>::success(make_handle(index, geometry_generations_[index]));
}

Status CustomPhysicsWorld::destroy_geometry(GeometryHandle geometry) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!is_valid(geometry) || static_cast<std::size_t>(geometry.index) >= geometries_.size() ||
        geometry_generations_[geometry.index] != geometry.generation || geometries_[geometry.index] == nullptr) {
        return Status::fail(ErrorCode::NotFound, "geometry handle is not live");
    }
    if (geometries_[geometry.index]->users > 0) {
        return Status::fail(ErrorCode::FailedPrecondition, "geometry is still used by a body");
    }
    geometries_[geometry.index].reset();
    ++geometry_generations_[geometry.index];
    geometry_free_.push_back(geometry.index);
    return Status::success();
}

} // namespace poko
