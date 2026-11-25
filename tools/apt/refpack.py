# tools/apt: EA APT front end movies. refpack.py SOURCE DEST unpacks a RefPack (10FB) file or a BIGF archive of one;
# refpack.py: EA RefPack (10FB) decompression, and BIGF archive listing/extraction
import struct, sys, os
def unrefpack(d):
    p = 0; flags = d[0]
    if d[1] != 0xFB: return d
    p = 2
    size_len = 4 if flags & 0x80 else 3
    if flags & 0x01: p += size_len  # compressed size present
    out_len = int.from_bytes(d[p:p+size_len], 'big'); p += size_len
    o = bytearray()
    while p < len(d):
        b0 = d[p]
        if b0 < 0x80:
            b1 = d[p+1]; p += 2
            lit = b0 & 3; o += d[p:p+lit]; p += lit
            cnt = ((b0 & 0x1C) >> 2) + 3; off = ((b0 & 0x60) << 3) + b1 + 1
        elif b0 < 0xC0:
            b1, b2 = d[p+1], d[p+2]; p += 3
            lit = b1 >> 6; o += d[p:p+lit]; p += lit
            cnt = (b0 & 0x3F) + 4; off = ((b1 & 0x3F) << 8) + b2 + 1
        elif b0 < 0xE0:
            b1, b2, b3 = d[p+1], d[p+2], d[p+3]; p += 4
            lit = b0 & 3; o += d[p:p+lit]; p += lit
            cnt = ((b0 & 0x0C) << 6) + b3 + 5; off = ((b0 & 0x10) << 12) + (b1 << 8) + b2 + 1
        elif b0 < 0xFC:
            lit = ((b0 & 0x1F) << 2) + 4; p += 1; o += d[p:p+lit]; p += lit; continue
        else:
            lit = b0 & 3; p += 1; o += d[p:p+lit]; p += lit; break
        s = len(o) - off
        for i in range(cnt): o.append(o[s + i])
    return bytes(o[:out_len])
def big(d):
    n = struct.unpack('>I', d[8:12])[0]; p = 16; r = []
    for i in range(n):
        off, size = struct.unpack('>II', d[p:p+8]); p += 8
        e = d.index(b'\0', p); r.append((d[p:e].decode('latin1'), d[off:off+size])); p = e + 1
    return r
if __name__ == '__main__':
    src, dst = sys.argv[1], sys.argv[2]
    d = unrefpack(open(src, 'rb').read())
    if d[:4] == b'BIGF':
        for name, data in big(d):
            fn = os.path.join(dst, name); os.makedirs(os.path.dirname(fn) or '.', exist_ok=True); open(fn, 'wb').write(data)
            print(name, len(data))
    else:
        open(dst, 'wb').write(d)
