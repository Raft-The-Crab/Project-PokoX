#include "poko_test_harness.h"

#include "poko_animation.h"

namespace {

poko::anim::AnimationClip make_clip() {
    poko::anim::AnimationClip clip;
    clip.name = "slide";
    clip.duration = 1.0f;

    poko::anim::AnimTrack move;
    move.target = "hips";
    move.channel = poko::anim::ChannelTarget::Translation;
    move.vec_keys = {{0.0f, {0.0f, 0.0f, 0.0f}}, {1.0f, {0.0f, 1.0f, 0.0f}}};
    clip.tracks.push_back(move);

    poko::anim::AnimTrack spin;
    spin.target = "hips";
    spin.channel = poko::anim::ChannelTarget::Rotation;
    spin.rot_keys = {{0.0f, {0.0f, 0.0f, 0.0f, 1.0f}},
                     {1.0f, {0.0f, 0.70710678f, 0.0f, 0.70710678f}}};
    clip.tracks.push_back(spin);
    return clip;
}

} // namespace

POKO_TEST(clip_validation) {
    const auto clip = make_clip();
    POKO_CHECK(poko::anim::valid(clip));

    auto bad = clip;
    bad.tracks[0].vec_keys[1].time = -1.0f; // unsorted
    POKO_CHECK(!poko::anim::valid(bad));

    bad = clip;
    bad.tracks[0].target.clear();
    POKO_CHECK(!poko::anim::valid(bad));
}

POKO_TEST(vec_track_interpolates) {
    const auto pose = poko::anim::sample(make_clip(), 0.5f);
    POKO_CHECK_EQ(pose.size(), 1u);
    POKO_CHECK_EQ(pose[0].target, "hips");
    POKO_CHECK_EQ(pose[0].transform.position.y, 0.5f);
}

POKO_TEST(time_is_clamped) {
    const auto before = poko::anim::sample(make_clip(), -5.0f);
    POKO_CHECK_EQ(before[0].transform.position.y, 0.0f);
    const auto after = poko::anim::sample(make_clip(), 99.0f);
    POKO_CHECK_EQ(after[0].transform.position.y, 1.0f);
}

POKO_TEST(rotation_nlerps) {
    const auto pose = poko::anim::sample(make_clip(), 1.0f);
    // 90 deg about Y at t=1.
    POKO_CHECK(pose[0].transform.rotation.w > 0.6f);
    POKO_CHECK(pose[0].transform.rotation.y > 0.6f);
}

POKO_TEST_MAIN()
