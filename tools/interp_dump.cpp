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

// vertex i's oPos (x y z w) with these registers
bool Pos(const Mesh& m, const std::vector<float>& regs, uint32_t i, float out[4], gpu::State& s) {
    std::memcpy(s.vsc.data(), regs.data(), regs.size() * 4);
    gpu::vp::VertexProgram p{s};
    for (int r = 0; r < 16; r++) {
        float v[4];
        Read(m, r, i, v);
        p.in[r] = gpu::vp::V(v[0], v[1], v[2], v[3]);
    }
    p.Run(m.program);
    for (int c = 0; c < 4; c++) out[c] = p.out[0][c];
    return std::isfinite(out[0]) && std::isfinite(out[1]) && std::isfinite(out[2]) && out[3] > 1e-4f;
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
    // OVERLAY=handle-suffix (hex, e.g. 2FB1): its vertices that sit on another skinned draw's vertex in the step's own
    // drawing (screen place within 0.05 px, depth within 1e-5 of the depth range), and how far apart each pair is in
    // the replays: a pass laid over the skin by depth that parts from it flickers
    // COLOURS=1: every output but the position (oD0 oD1 oFog oT0..3) for each skinned vertex, at its step and in each
    // replay: values not finite, or a colour outside 0..1 in a replay where the step's own was inside, or a colour more
    // than 0.25 from the straight line between the two steps
    // NONFINITE=1: registers not finite in the step's own drawing and in the replays, by register; and positions
    // LAYERS=1: draws of one program and count with equal registers in the step's own drawing (a piece drawn twice,
    // a layer over it by depth): are their registers still equal in each replay?
    if (std::getenv("LAYERS")) {
        uint64_t pairs = 0, parted = 0, groups_parted = 0;
        std::map<std::string, uint64_t> where;
        for (const Step& st : steps) {
            for (size_t i = 0; i < st.draws.size(); i++)
                for (size_t j = i + 1; j < st.draws.size(); j++) {
                    const Draw& a = st.draws[i];
                    const Draw& b = st.draws[j];
                    if (a.vs != b.vs || a.count != b.count || a.vsc != b.vsc) continue;
                    for (const Replay& rp : st.replays) {
                        const std::vector<float>* ra = nullptr; const std::vector<float>* rb = nullptr;
                        for (const auto& [k, regs] : rp.draws) { if (k == a.index) ra = &regs; if (k == b.index) rb = &regs; }
                        if (!ra || !rb) continue;
                        pairs++;
                        if (*ra != *rb) {
                            parted++;
                            for (int g = 0; g < 48; g++)
                                if (std::memcmp(&(*ra)[size_t(g) * 16], &(*rb)[size_t(g) * 16], 64)) {
                                    groups_parted++;
                                    char w[96];
                                    std::snprintf(w, sizeof w, "vs ..%04X count %u group %d (registers %d-%d), prev %d/%d", a.vs & 0xFFFF, a.count, g, g * 4, g * 4 + 3, a.prev, b.prev);
                                    where[w]++;
                                }
                        }
                    }
                }
        }
        std::printf("layer pairs (equal at their step) in replays: %llu, with different registers: %llu (%llu groups)\n",
                    (unsigned long long)pairs, (unsigned long long)parted, (unsigned long long)groups_parted);
        int shown = 0;
        for (auto& [w, n] : where) if (shown++ < 25) std::printf("  %s: %llu\n", w.c_str(), (unsigned long long)n);
        return 0;
    }
    if (std::getenv("NONFINITE")) {
        std::map<int, uint64_t> own, rep;
        std::map<int, std::pair<float, float>> example;
        for (const Step& st : steps) {
            std::map<uint32_t, const Draw*> by_index;
            for (const Draw& d : st.draws) {
                by_index[d.index] = &d;
                for (int k = 0; k < 192 * 4; k++) if (!std::isfinite(d.vsc[size_t(k)])) own[k]++;
            }
            for (const Replay& rp : st.replays)
                for (const auto& [i, regs] : rp.draws) {
                    const Draw* d = by_index[i];
                    for (int k = 0; k < 192 * 4; k++)
                        if (!std::isfinite(regs[size_t(k)])) {
                            rep[k]++;
                            if (d && !example.count(k)) example[k] = {d->vsc[size_t(k)], regs[size_t(k)]};
                        }
                }
        }
        std::printf("registers not finite in the steps' own drawing: %zu kinds\n", own.size());
        for (auto [k, n] : own) std::printf("  c%d.%c: %llu\n", k / 4 - 96, "xyzw"[k % 4], (unsigned long long)n);
        std::printf("registers not finite in the replays: %zu kinds\n", rep.size());
        for (auto [k, n] : rep) std::printf("  c%d.%c (register %d): %llu, e.g. step's %g, replay's %g\n", k / 4 - 96, "xyzw"[k % 4], k / 4, (unsigned long long)n, example[k].first, example[k].second);
        return 0;
    }
    if (std::getenv("COLOURS")) {
        static gpu::State sc;
        const char* names[16] = {"oPos", "", "", "oD0", "oD1", "oFog", "oPts", "oB0", "oB1", "oT0", "oT1", "oT2", "oT3", "", "", ""};
        std::map<std::string, uint64_t> bad;
        uint64_t checked = 0;
        float worst = 0;
        std::string worst_where;
        const auto run = [&](const Mesh& m, const std::vector<float>& regs, uint32_t v, gpu::vp::Vec out[16]) {
            std::memcpy(sc.vsc.data(), regs.data(), regs.size() * 4);
            gpu::vp::VertexProgram p{sc};
            for (int r = 0; r < 16; r++) {
                float x[4];
                Read(m, r, v, x);
                p.in[r] = gpu::vp::V(x[0], x[1], x[2], x[3]);
            }
            p.Run(m.program);
            for (int o = 0; o < 16; o++) out[o] = p.out[o];
        };
        for (size_t si = 1; si < steps.size(); si++) {
            const Step& st = steps[si];
            const Step& before = steps[si - 1];
            std::map<uint32_t, const Draw*> by_index;
            for (const Draw& d : st.draws) by_index[d.index] = &d;
            for (const Replay& rp : st.replays)
                for (const auto& [index, regs] : rp.draws) {
                    const Draw* d = by_index[index];
                    if (!d || d->prev < 0) continue;
                    const Mesh& m = meshes[d->mesh];
                    const Draw* p = nullptr;
                    for (const Draw& o : before.draws) if (o.mesh == d->mesh) { p = &o; break; }
                    if (!p) continue;
                    std::vector<uint32_t> verts;
                    for (size_t k = 0; k + 1 < m.indices.size(); k += 2) {
                        uint16_t v;
                        std::memcpy(&v, &m.indices[k], 2);
                        verts.push_back(uint32_t(v) + d->base);
                    }
                    std::sort(verts.begin(), verts.end());
                    verts.erase(std::unique(verts.begin(), verts.end()), verts.end());
                    for (uint32_t v : verts) {
                        gpu::vp::Vec a[16], b[16], r[16];
                        run(m, p->vsc, v, a), run(m, d->vsc, v, b), run(m, regs, v, r);
                        for (int o : {3, 4, 5, 9, 10, 11, 12}) {
                            for (int c = 0; c < 4; c++) {
                                checked++;
                                const float x = r[o][c], xa = a[o][c], xb = b[o][c];
                                if (!std::isfinite(x)) { bad[std::string(names[o]) + " not finite"]++; continue; }
                                if (o == 3 || o == 4) {
                                    const bool in_a = xa >= 0 && xa <= 1, in_b = xb >= 0 && xb <= 1;
                                    if (in_a && in_b && (x < -0.01f || x > 1.01f)) bad[std::string(names[o]) + " out of 0..1"]++;
                                }
                                const float line = xa + (xb - xa) * rp.alpha, off = std::fabs(x - line);
                                const float scale = std::max({1.0f, std::fabs(xa), std::fabs(xb)});
                                if (off / scale > 0.25f) {
                                    bad[std::string(names[o]) + " off the line by over 25%"]++;
                                    if (off / scale > worst) {
                                        worst = off / scale;
                                        char w[256];
                                        std::snprintf(w, sizeof w, "step %llu alpha %.2f vs %08X count %u vertex %u %s.%c: was %g now %g drawn %g",
                                                      (unsigned long long)st.step, rp.alpha, d->vs, d->count, v, names[o], "xyzw"[c], xa, xb, x);
                                        worst_where = w;
                                    }
                                }
                            }
                        }
                    }
                }
        }
        std::printf("%llu output values checked\n", (unsigned long long)checked);
        for (auto& [k, n] : bad) std::printf("  %s: %llu\n", k.c_str(), (unsigned long long)n);
        if (!worst_where.empty()) std::printf("  the worst: %s\n", worst_where.c_str());
        return 0;
    }
    if (const char* ov = std::getenv("OVERLAY")) {
        const uint32_t want = uint32_t(std::strtoul(ov, nullptr, 16));
        static gpu::State st0;
        uint64_t pairs = 0, parted = 0;
        float most_z = 0, most_xy = 0;
        std::map<int, uint64_t> by_z;  // log10 of the depth gap
        for (const Step& st : steps) {
            for (const Draw& o : st.draws) {
                if ((o.vs & 0xFFFF) != want) continue;
                const Mesh& om = meshes[o.mesh];
                // the overlay's vertices now
                std::vector<std::pair<uint32_t, std::array<float, 4>>> ov_now;
                std::vector<uint32_t> verts;
                for (size_t k = 0; k + 1 < om.indices.size(); k += 2) {
                    uint16_t v;
                    std::memcpy(&v, &om.indices[k], 2);
                    verts.push_back(uint32_t(v) + o.base);
                }
                std::sort(verts.begin(), verts.end());
                verts.erase(std::unique(verts.begin(), verts.end()), verts.end());
                for (uint32_t v : verts) {
                    float q[4];
                    if (Pos(om, o.vsc, v, q, st0)) ov_now.push_back({v, {q[0], q[1], q[2], q[3]}});
                }
                // the other skinned draws' vertices now, the nearest to each overlay vertex
                for (const Draw& b : st.draws) {
                    if (&b == &o || (b.vs & 0xFFFF) == want) continue;
                    const Mesh& bm = meshes[b.mesh];
                    std::vector<uint32_t> bv;
                    for (size_t k = 0; k + 1 < bm.indices.size(); k += 2) {
                        uint16_t v;
                        std::memcpy(&v, &bm.indices[k], 2);
                        bv.push_back(uint32_t(v) + b.base);
                    }
                    std::sort(bv.begin(), bv.end());
                    bv.erase(std::unique(bv.begin(), bv.end()), bv.end());
                    for (uint32_t v : bv) {
                        float q[4];
                        if (!Pos(bm, b.vsc, v, q, st0)) continue;
                        for (const auto& [ovv, oq] : ov_now) {
                            if (std::fabs(oq[0] - q[0]) > 0.05f || std::fabs(oq[1] - q[1]) > 0.05f || std::fabs(oq[2] - q[2]) > 1e-5f * std::max(1.0f, std::fabs(q[2]))) continue;
                            // a pair: in each replay, both with that replay's registers
                            for (const Replay& rp : st.replays) {
                                const std::vector<float>* ro = nullptr; const std::vector<float>* rb = nullptr;
                                for (const auto& [i, regs] : rp.draws) { if (i == o.index) ro = &regs; if (i == b.index) rb = &regs; }
                                if (!ro || !rb) continue;
                                float a[4], c[4];
                                if (!Pos(om, *ro, ovv, a, st0) || !Pos(bm, *rb, v, c, st0)) continue;
                                pairs++;
                                const float dz = std::fabs(a[2] - c[2]) / std::max(1.0f, std::fabs(c[2]));
                                const float dxy = std::hypot(a[0] - c[0], a[1] - c[1]);
                                most_z = std::max(most_z, dz), most_xy = std::max(most_xy, dxy);
                                by_z[dz == 0 ? -99 : int(std::floor(std::log10(dz)))]++;
                                if (dxy > 0.05f || dz > 1e-5f) parted++;
                            }
                        }
                    }
                }
            }
        }
        std::printf("overlay ..%04X: %llu coincident pairs in replays, %llu parted (screen > 0.05 px or depth > 1e-5), the most: %.3f px, depth %.2e\n",
                    want, (unsigned long long)pairs, (unsigned long long)parted, most_xy, most_z);
        for (auto [e, n] : by_z) std::printf("  depth gap 1e%d: %llu\n", e, (unsigned long long)n);
        return 0;
    }
    if (std::getenv("DUMP_PROGRAMS") && !steps.empty()) {
        std::map<uint32_t, int> n;
        for (const Draw& d : steps.back().draws) n[d.vs & 0xFFFF]++;
        for (auto [vs, k] : n) std::printf("program ..%04X: %d skinned draws\n", vs, k);
    }
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
                    {
                        const bool okr = Place(m, *use, v, r, s), okb = Place(m, d->vsc, v, b, s);
                        static uint64_t lost = 0;
                        if (okb && !okr && lost++ < 20)
                            std::printf("  lost in a replay: step %llu alpha %.2f vs %08X count %u vertex %u (fine at its step)\n",
                                        (unsigned long long)st.step, rp.alpha, d->vs, d->count, v);
                        if (!okr || !okb) continue;
                    }
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
