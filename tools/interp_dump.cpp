// interp_dump FILE [threshold]: reads a smooth-motion dump (F11 in the game, gpu::RequestInterpDump) and runs each
// skinned draw's vertex program on its mesh: at the step before (its matched draw's registers), at its step, and with
// each replay's registers. A vertex whose in-between screen place is more than the threshold (guest pixels, default 6)
// from the straight line between its two step places is reported: a spike. Built by scripts/interp-dump.sh.
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "gpu/state.h"
#include "gpu/vertex_program.h"

namespace {
struct Input {
    int32_t stream;
    uint32_t offset, type;
};
struct Mesh {
    std::vector<uint32_t> program;
    Input inputs[16];
    uint32_t strides[16];
    std::map<uint32_t, std::vector<uint8_t>> streams;
    std::vector<uint8_t> indices;
};
struct Draw {
    uint32_t index, vs, prim, start, count, base;
    uint64_t program, mesh;
    int32_t prev;
    std::vector<float> vsc;  // 192 * 4
};
struct Replay {
    float alpha;
    double t;
    std::vector<std::pair<uint32_t, std::vector<float>>> draws;
};
struct Step {
    uint64_t step;
    double t;
    uint32_t blended;
    std::vector<Draw> draws;
    std::vector<Replay> replays;
};

FILE* f;
uint32_t U32() { uint32_t v = 0; if (std::fread(&v, 4, 1, f) != 1) throw 1; return v; }
uint64_t U64() { uint64_t v = 0; if (std::fread(&v, 8, 1, f) != 1) throw 1; return v; }
double F64() { double v = 0; if (std::fread(&v, 8, 1, f) != 1) throw 1; return v; }
float F32() { float v = 0; if (std::fread(&v, 4, 1, f) != 1) throw 1; return v; }
std::vector<uint8_t> Blob() {
    std::vector<uint8_t> b(U32());
    if (!b.empty() && std::fread(b.data(), 1, b.size(), f) != b.size()) throw 1;
    return b;
}
std::vector<float> Regs() {
    std::vector<float> v(192 * 4);
    if (std::fread(v.data(), 4, v.size(), f) != v.size()) throw 1;
    return v;
}

std::map<uint32_t, int> g_unknown_types;

// one input register of vertex i, by its D3DVSDT type
bool Read(const Mesh& m, int r, uint32_t i, float v[4]) {
    const Input& in = m.inputs[r];
    v[0] = v[1] = v[2] = 0, v[3] = 1;
    if (in.stream < 0) return false;
    auto it = m.streams.find(uint32_t(in.stream));
    if (it == m.streams.end()) return false;
    const auto& b = it->second;
    const size_t at = size_t(i) * m.strides[in.stream] + in.offset;
    const auto need = [&](size_t n) { return at + n <= b.size(); };
    const uint8_t* p = b.data() + at;
    switch (in.type) {
        case 0x12: case 0x22: case 0x32: case 0x42: {
            const int n = int(in.type >> 4);
            if (!need(size_t(n) * 4)) return false;
            std::memcpy(v, p, size_t(n) * 4);
            return true;
        }
        case 0x40: {  // D3DCOLOR: b g r a
            if (!need(4)) return false;
            v[0] = p[2] / 255.f, v[1] = p[1] / 255.f, v[2] = p[0] / 255.f, v[3] = p[3] / 255.f;
            return true;
        }
        case 0x15: case 0x25: case 0x35: case 0x45: case 0x11: case 0x21: case 0x31: case 0x41: {
            const int n = int(in.type >> 4);
            if (!need(size_t(n) * 2)) return false;
            for (int c = 0; c < n; c++) {
                int16_t q;
                std::memcpy(&q, p + c * 2, 2);
                v[c] = (in.type & 0xF) == 5 ? float(q) : std::max(-1.f, q / 32767.f);
            }
            return true;
        }
        case 0x14: case 0x24: case 0x34: case 0x44: {
            const int n = int(in.type >> 4);
            if (!need(size_t(n))) return false;
            for (int c = 0; c < n; c++) v[c] = p[c] / 255.f;
            return true;
        }
        case 0x16: {  // NORMPACKED3: 11 11 10 signed
            if (!need(4)) return false;
            uint32_t w;
            std::memcpy(&w, p, 4);
            const int32_t x = int32_t(w << 21) >> 21, y = int32_t(w << 10) >> 21, z = int32_t(w) >> 22;
            v[0] = x / 1023.f, v[1] = y / 1023.f, v[2] = z / 511.f;
            return true;
        }
        case 0x02: return false;
        default: g_unknown_types[in.type]++; return false;
    }
}

// vertex i's screen place (guest pixels, 640 wide) with these registers; false behind the camera
bool Place(const Mesh& m, const std::vector<float>& regs, uint32_t i, float out[2], gpu::State& s) {
    std::memcpy(s.vsc.data(), regs.data(), regs.size() * 4);
    gpu::vp::VertexProgram p{s};
    for (int r = 0; r < 16; r++) {
        float v[4];
        Read(m, r, i, v);
        p.in[r] = gpu::vp::V(v[0], v[1], v[2], v[3]);
    }
    p.Run(m.program);
    // the program ends with the viewport transform (Xbox D3D appends it: oPos.xyz times 1/w, scaled by c58, plus c59):
    // x and y are guest screen pixels already; w is the depth
    const float w = p.out[0][3];
    if (!(w > 1e-4f)) return false;
    out[0] = p.out[0][0], out[1] = p.out[0][1];
    return std::isfinite(out[0]) && std::isfinite(out[1]);
}
}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "interp_dump FILE [threshold pixels]\n");
        return 2;
    }
    const float threshold = argc > 2 ? float(std::atof(argv[2])) : 6.f;
    f = std::fopen(argv[1], "rb");
    if (!f) return 1;
    char magic[14];
    if (std::fread(magic, 1, 14, f) != 14 || std::memcmp(magic, "FN2INTERPDUMP1", 14)) return 1;
    std::map<uint64_t, Mesh> meshes;
    std::vector<Step> steps;
    try {
        for (uint32_t n = U32(); n--;) {
            const uint64_t h = U64();
            Mesh m;
            auto prog = Blob();
            m.program.resize(prog.size() / 4);
            std::memcpy(m.program.data(), prog.data(), prog.size());
            for (auto& in : m.inputs) in.stream = int32_t(U32()), in.offset = U32(), in.type = U32();
            for (auto& st : m.strides) st = U32();
            for (uint32_t k = U32(); k--;) {
                const uint32_t stream = U32();
                m.streams[stream] = Blob();
            }
            m.indices = Blob();
            meshes[h] = std::move(m);
        }
        for (uint32_t n = U32(); n--;) {
            Step st;
            st.step = U64(), st.t = F64(), st.blended = U32();
            for (uint32_t k = U32(); k--;) {
                Draw d;
                d.index = U32(), d.vs = U32(), d.prim = U32(), d.start = U32(), d.count = U32(), d.base = U32();
                d.program = U64(), d.mesh = U64(), d.prev = int32_t(U32());
                d.vsc = Regs();
                st.draws.push_back(std::move(d));
            }
            for (uint32_t k = U32(); k--;) {
                Replay rp;
                rp.alpha = F32(), rp.t = F64();
                for (uint32_t j = U32(); j--;) {
                    const uint32_t i = U32();
                    rp.draws.push_back({i, Regs()});
                }
                st.replays.push_back(std::move(rp));
            }
            steps.push_back(std::move(st));
        }
    } catch (int) {
        std::fprintf(stderr, "the file ends early\n");
    }
    std::printf("%zu meshes, %zu steps\n", meshes.size(), steps.size());
    if (std::getenv("DUMP_INFO")) {
        for (const Step& st : steps) {
            std::printf("step %llu blended %u draws %zu replays", (unsigned long long)st.step, st.blended, st.draws.size());
            for (const Replay& rp : st.replays) std::printf(" [%.2f %zu]", rp.alpha, rp.draws.size());
            std::printf("\n");
        }
    }
    static gpu::State s;
    uint64_t checked = 0, spikes = 0;
    for (size_t si = 1; si < steps.size(); si++) {
        const Step& st = steps[si];
        const Step& before = steps[si - 1];
        std::map<uint32_t, const Draw*> by_index, before_by_index;
        for (const Draw& d : st.draws) by_index[d.index] = &d;
        for (const Draw& d : before.draws) before_by_index[d.index] = &d;
        for (const Replay& rp : st.replays) {
            for (const auto& [index, regs] : rp.draws) {
                const Draw* d = by_index[index];
                if (!d || d->prev < 0) continue;  // unmatched: the ring ropes, drawn at a step on purpose
                const auto mi = meshes.find(d->mesh);
                if (mi == meshes.end()) continue;
                const Mesh& m = mi->second;
                // the step before's own draw of this mesh (the same vertices): where it really was. Of several (a mesh drawn
                // twice, the boxers' alike pieces), the nearest to its place now; the matched draw is reported beside
                const Draw* matched = d->prev >= 0 ? before_by_index[uint32_t(d->prev)] : nullptr;
                const Draw* p = nullptr;
                {
                    float best = 1e30f;
                    for (const Draw& o : before.draws) {
                        if (o.mesh != d->mesh) continue;
                        float dist = 0;
                        for (int k = 0; k < 16; k++) dist += std::fabs(o.vsc[size_t(k)] - d->vsc[size_t(k)]);
                        if (dist < best) best = dist, p = &o;
                    }
                }
                if (!p) p = matched;
                const bool rematched = p != matched;
                // the vertices the draw uses
                std::vector<uint32_t> verts;
                if (!m.indices.empty()) {
                    for (size_t k = 0; k + 1 < m.indices.size(); k += 2) {
                        uint16_t v;
                        std::memcpy(&v, &m.indices[k], 2);
                        verts.push_back(uint32_t(v) + d->base);
                    }
                } else {
                    for (uint32_t k = 0; k < d->count; k++) verts.push_back(d->start + k);
                }
                std::sort(verts.begin(), verts.end());
                verts.erase(std::unique(verts.begin(), verts.end()), verts.end());
                float worst = 0;
                uint32_t worst_v = 0;
                float wa[2] = {}, wb[2] = {}, wr[2] = {};
                for (uint32_t v : verts) {
                    float a[2], b[2], r[2];
                    // SELFTEST=1: one bone (registers 8-11, the third group) moved 5 units in the replay: must be found
                    static const bool selftest = std::getenv("SELFTEST") != nullptr;
                    std::vector<float> moved;
                    const std::vector<float>* use = &regs;
                    if (selftest) {
                        moved = regs;
                        for (int row = 0; row < 3; row++) moved[size_t((8 + row) * 4 + 3)] += 5;
                        use = &moved;
                    }
                    if (!Place(m, *use, v, r, s) || !Place(m, d->vsc, v, b, s)) continue;
                    if (!p || !Place(m, p->vsc, v, a, s)) continue;
                    // on or near the screen (640x480 layout, 854 wide on a wide frame) in all three
                    const auto near_screen = [](const float q[2]) { return q[0] > -400 && q[0] < 1040 && q[1] > -300 && q[1] < 780; };
                    if (!near_screen(a) || !near_screen(b) || !near_screen(r)) continue;
                    const float ex = a[0] + (b[0] - a[0]) * rp.alpha, ey = a[1] + (b[1] - a[1]) * rp.alpha;
                    const float off = std::hypot(r[0] - ex, r[1] - ey);
                    checked++;
                    static int shown = 0;
                    if (std::getenv("DUMP_INFO") && shown < 5 && off > 0) {
                        shown++;
                        std::printf("  e.g. alpha %.2f was %.2f,%.2f now %.2f,%.2f drawn %.2f,%.2f off %.4f\n", rp.alpha, a[0], a[1], b[0], b[1], r[0], r[1], off);
                    }
                    if (off > worst) worst = off, worst_v = v, std::memcpy(wa, a, 8), std::memcpy(wb, b, 8), std::memcpy(wr, r, 8);
                }
                if (worst > threshold) {
                    spikes++;
                    std::printf("step %llu (t %.3f) replay alpha %.2f: draw %u vs %08X count %u prev %d%s: vertex %u %.1f px off the "
                                "line (was %.0f,%.0f now %.0f,%.0f drawn %.0f,%.0f)\n",
                                (unsigned long long)st.step, st.t - steps[0].t, rp.alpha, index, d->vs, d->count, d->prev,
                                rematched ? " (matched to another draw of the step before)" : "", worst_v, worst, wa[0], wa[1], wb[0], wb[1],
                                wr[0], wr[1]);
                }
            }
        }
    }
    std::printf("%llu vertex places checked, %llu draws with a spike over %.1f px\n", (unsigned long long)checked,
                (unsigned long long)spikes, threshold);
    for (auto [t, n] : g_unknown_types) std::printf("unread input type %02X (%d)\n", t, n);
    return 0;
}
