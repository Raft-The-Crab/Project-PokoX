#include "poko_test_harness.h"

#include <cstddef>
#include <typeindex>

#include "poko_reflection.h"

namespace {

struct SampleComponent {
    int health = 100;
    float speed = 1.0f;
};

const std::string kTypeName = "SampleComponent";
const std::string kMemberName = "health";

const poko::reflect::TypeInfo kSampleInfo{
    kTypeName,
    typeid(SampleComponent),
    sizeof(SampleComponent),
    {{kMemberName, poko::reflect::MemberKind::Int32, offsetof(SampleComponent, health), sizeof(int),
      nullptr},
     {"speed", poko::reflect::MemberKind::Float, offsetof(SampleComponent, speed), sizeof(float),
      nullptr}},
};

} // namespace

POKO_TEST(register_and_lookup) {
    auto& registry = poko::reflect::TypeRegistry::instance();
    registry.register_type(kSampleInfo);

    const auto* by_name = registry.find("SampleComponent");
    POKO_CHECK(by_name != nullptr);
    POKO_CHECK_EQ(by_name->size, sizeof(SampleComponent));
    POKO_CHECK_EQ(by_name->members.size(), 2u);

    const auto* by_type = registry.find(typeid(SampleComponent));
    POKO_CHECK(by_type == by_name);
    POKO_CHECK(registry.find("NoSuchType") == nullptr);
}

POKO_TEST(member_offsets_are_usable) {
    auto& registry = poko::reflect::TypeRegistry::instance();
    registry.register_type(kSampleInfo);
    const auto* info = registry.find("SampleComponent");
    POKO_CHECK(info != nullptr);

    // Walk a real instance through member descriptors.
    SampleComponent comp;
    comp.health = 42;
    for (const auto& member : info->members) {
        if (member.name == "health") {
            const auto* value = reinterpret_cast<const int*>(
                reinterpret_cast<const std::byte*>(&comp) + member.offset);
            POKO_CHECK_EQ(*value, 42);
        }
    }
}

POKO_TEST_MAIN()
