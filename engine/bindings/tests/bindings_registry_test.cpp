#include "poko_test_harness.h"

#include "poko/bindings/registry.h"

POKO_TEST(register_find_call) {
    poko::bindings::BindingRegistry registry;
    const auto status = registry.register_function(
        "math.add",
        [](const std::vector<poko::bindings::Value>& args) -> poko::Result<poko::bindings::Value> {
            if (args.size() != 2) {
                return poko::Result<poko::bindings::Value>::fail(poko::ErrorCode::InvalidArgument,
                                                                 "expected 2 args");
            }
            poko::bindings::Value out;
            out.kind = poko::bindings::ValueKind::Float;
            out.f = args[0].f + args[1].f;
            return poko::Result<poko::bindings::Value>::success(out);
        });
    POKO_CHECK(status.ok());
    POKO_CHECK_EQ(registry.count(), 1u);

    const auto fn = registry.find("math.add");
    POKO_CHECK(static_cast<bool>(fn));
    const auto result =
        fn({poko::bindings::Value{poko::bindings::ValueKind::Float, false, 0, 2.0, {}, 0},
            poko::bindings::Value{poko::bindings::ValueKind::Float, false, 0, 3.0, {}, 0}});
    POKO_CHECK(result.ok());
    POKO_CHECK_EQ(result.value().f, 5.0);
    POKO_CHECK(!registry.find("math.sub"));
}

POKO_TEST(invalid_registration_rejected) {
    poko::bindings::BindingRegistry registry;
    POKO_CHECK(!registry
                    .register_function("",
                                       [](const std::vector<poko::bindings::Value>&) {
                                           return poko::Result<poko::bindings::Value>::success({});
                                       })
                    .ok());
    POKO_CHECK(!registry.register_function("x.y", {}).ok());
}

POKO_TEST(names_sorted) {
    auto noop =
        [](const std::vector<poko::bindings::Value>&) -> poko::Result<poko::bindings::Value> {
        return poko::Result<poko::bindings::Value>::success({});
    };
    poko::bindings::BindingRegistry registry;
    registry.register_function("b.fn", noop);
    registry.register_function("a.fn", noop);
    const auto names = registry.names();
    POKO_CHECK_EQ(names.size(), 2u);
    POKO_CHECK_EQ(names[0], "a.fn");
}

POKO_TEST_MAIN()
