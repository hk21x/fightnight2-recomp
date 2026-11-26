# aptdis.py NAME.apt [filter]: EA APT (Data:6) movies: every action block (frame actions, init actions, clip events,
# defined functions) disassembled, constants from NAME.const resolved. Opcodes: Flash AVM1 plus EA's (OpenSage's names).
import struct, sys

def load(path):
    d = open(path, 'rb').read()
    c = open(path[:-4] + '.const', 'rb').read()
    root, count = struct.unpack('<II', c[0x14:0x1C])
    consts = []
    for i in range(count):
        t, v = struct.unpack('<II', c[0x20 + i * 8:0x28 + i * 8])
        if t == 1:
            e = c.index(b'\0', v); consts.append(c[v:e].decode('latin1'))
        else:
            consts.append(('num', v))
    return d, consts, root

U = lambda d, o: struct.unpack('<I', d[o:o + 4])[0]
S = lambda d, o: d[o:d.index(b'\0', o)].decode('latin1') if o else ''
A4 = lambda o: (o + 3) & ~3

NAMES = {0x00: 'End', 0x04: 'NextFrame', 0x05: 'PrevFrame', 0x06: 'Play', 0x07: 'Stop', 0x0A: 'Add', 0x0B: 'Subtract',
         0x0C: 'Multiply', 0x0D: 'Divide', 0x0E: 'Equals', 0x0F: 'Less', 0x10: 'And', 0x11: 'Or', 0x12: 'Not',
         0x13: 'StringEquals', 0x14: 'StringLength', 0x15: 'StringExtract', 0x17: 'Pop', 0x18: 'ToInteger',
         0x1C: 'GetVariable', 0x1D: 'SetVariable', 0x20: 'SetTarget2', 0x21: 'StringAdd', 0x22: 'GetProperty',
         0x23: 'SetProperty', 0x24: 'CloneSprite', 0x25: 'RemoveSprite', 0x26: 'Trace', 0x27: 'StartDrag',
         0x28: 'EndDrag', 0x29: 'StringLess', 0x30: 'RandomNumber', 0x34: 'GetTime', 0x3A: 'Delete', 0x3B: 'Delete2',
         0x3C: 'DefineLocal', 0x3D: 'CallFunction', 0x3E: 'Return', 0x3F: 'Modulo', 0x40: 'NewObject', 0x41: 'DefineLocal2',
         0x42: 'InitArray', 0x43: 'InitObject', 0x44: 'TypeOf', 0x45: 'TargetPath', 0x46: 'Enumerate', 0x47: 'Add2',
         0x48: 'Less2', 0x49: 'Equals2', 0x4A: 'ToNumber', 0x4B: 'ToString', 0x4C: 'PushDuplicate', 0x4D: 'StackSwap',
         0x4E: 'GetMember', 0x4F: 'SetMember', 0x50: 'Increment', 0x51: 'Decrement', 0x52: 'CallMethod',
         0x53: 'NewMethod', 0x54: 'InstanceOf', 0x55: 'Enumerate2', 0x60: 'BitAnd', 0x61: 'BitOr', 0x62: 'BitXor',
         0x63: 'BitLShift', 0x64: 'BitRShift', 0x65: 'BitURShift', 0x66: 'StrictEquals', 0x67: 'Greater',
         0x68: 'StringGreater', 0x69: 'Extends',
         0x81: 'GotoFrame', 0x83: 'GetURL', 0x87: 'StoreRegister', 0x88: 'ConstantPool', 0x8A: 'WaitForFrame',
         0x8B: 'SetTarget', 0x8C: 'GotoLabel', 0x8D: 'WaitForFrame2', 0x8E: 'DefineFunction2', 0x94: 'With',
         0x96: 'PushData', 0x99: 'BranchAlways', 0x9A: 'GetURL2', 0x9B: 'DefineFunction', 0x9D: 'BranchIfTrue',
         0x9E: 'CallFrame', 0x9F: 'GotoExpression',
         0xA1: 'EA_PushString', 0xA2: 'EA_PushConstantByte', 0xA3: 'EA_PushConstantWord', 0xA4: 'EA_GetStringVar',
         0xA5: 'EA_GetStringMember', 0xA6: 'EA_SetStringVar', 0xA7: 'EA_SetStringMember', 0xA8: 'EA_PushThis',
         0xA9: 'EA_PushGlobal', 0xAA: 'EA_ZeroVar', 0xAB: 'EA_PushTrue', 0xAC: 'EA_PushFalse', 0xAD: 'EA_PushNull',
         0xAE: 'EA_PushUndefined', 0xAF: 'EA_PushZero? (0xAF)',
         0xB0: 'EA_PushZero', 0xB1: 'EA_PushOne', 0xB2: 'EA_CallFunc', 0xB3: 'EA_CallFuncPop', 0xB4: 'EA_CallMethod',
         0xB5: 'EA_CallMethodPop', 0xB6: 'EA_PushThisVar', 0xB7: 'EA_PushGlobalVar', 0xB8: 'EA_ZeroVar2',
         0xB9: 'EA_PushTrue2', 0xBA: 'EA_PushFalse2', 0xBB: 'EA_PushNull2', 0xBC: 'EA_PushUndefined2',
         0xBD: 'EA_PushNamedValue?'}
NAMES.update({0x59: 'PushZero', 0x5A: 'PushOne', 0x5B: 'CallFuncPop', 0x5C: 'CallFunc', 0x5D: 'CallMethodPop',
              0x5E: 'CallMethod', 0x70: 'PushThis', 0x71: 'PushGlobal', 0x72: 'ZeroVar', 0x73: 'PushTrue',
              0x74: 'PushFalse', 0x75: 'PushNull', 0x76: 'PushUndefined', 0xA1: 'PushString', 0xA2: 'PushConst',
              0xA3: 'PushConstW', 0xA4: 'GetStringVar', 0xA5: 'GetStringMember', 0xA6: 'SetStringVar',
              0xA7: 'SetStringMember', 0xAE: 'PushValueOfVar', 0xAF: 'GetNamedMember', 0xB0: 'CallNamedFuncPop',
              0xB1: 'CallNamedFunc', 0xB2: 'CallNamedMethodPop', 0xB3: 'CallNamedMethod', 0xB4: 'PushFloat',
              0xB5: 'PushByte', 0xB6: 'PushShort', 0xB7: 'PushLong', 0xB8: 'BranchIfFalse', 0xB9: 'PushRegister'})
BYTE1 = {0xA2, 0xAE, 0xAF, 0xB0, 0xB1, 0xB2, 0xB3, 0xB5, 0xB9, 0x87, 0x9A, 0x9F}
BYTE2 = {0xA3, 0xB6}
PTR = {0xA1, 0xA4, 0xA5, 0xA6, 0xA7, 0x8C, 0x8B}
U32 = {0x81, 0x94, 0xB4, 0xB7}
BR = {0x99, 0x9D, 0xB8}

def dis(d, consts, o, out, depth=0, pool=None, end=None):
    pool = pool or []
    name = lambda i: (repr(consts[pool[i]]) if i < len(pool) and isinstance(consts[pool[i]], str) else f'#{i}')
    pad = '  ' * depth
    while end is None or o < end:
        op = d[o]; at = o; o += 1; arg = ''
        if op in BYTE1: arg = name(d[o]) if op in (0xA2, 0xAE, 0xAF, 0xB0, 0xB1, 0xB2, 0xB3) else str(d[o]); o += 1
        elif op in BYTE2:
            v = struct.unpack('<H', d[o:o+2])[0]; o += 2; arg = name(v) if op == 0xA3 else str(v)
        elif op in PTR: o = A4(o); arg = repr(S(d, U(d, o))); o += 4
        elif op in U32: o = A4(o); arg = hex(U(d, o)); o += 4
        elif op in BR: o = A4(o); off = struct.unpack('<i', d[o:o+4])[0]; o += 4; arg = f'-> {hex(o + off)}'
        elif op == 0x88:
            o = A4(o); n, p = U(d, o), U(d, o + 4); o += 8; pool = [U(d, p + 4 * i) for i in range(n)]; arg = f'{n} entries'
        elif op == 0x96:
            o = A4(o); n, p = U(d, o), U(d, o + 4); o += 8; arg = f'{n} values at {hex(p)}'
        elif op == 0x83:
            o = A4(o); arg = f'{S(d, U(d, o))!r} {S(d, U(d, o + 4))!r}'; o += 8
        elif op == 0x9B:
            o = A4(o); fn, na, ap, size = struct.unpack('<4I', d[o:o+16]); o += 24
            args = [S(d, U(d, ap + 4 * i)) for i in range(na)]
            out.append(f'{pad}{hex(at)} DefineFunction {S(d, fn)!r}({", ".join(args)}) size {size}')
            dis(d, consts, o, out, depth + 1, pool, o + size); o += size; continue
        elif op == 0x8E:
            o = A4(o); fn, na = U(d, o), U(d, o + 4); regs, flags = d[o + 8], struct.unpack('<H', d[o + 9:o + 11])[0]
            ap, size = U(d, o + 12), U(d, o + 16); o += 28
            out.append(f'{pad}{hex(at)} DefineFunction2 {S(d, fn)!r} nargs {na} size {size}')
            dis(d, consts, o, out, depth + 1, pool, o + size); o += size; continue
        elif op not in NAMES:
            out.append(f'{pad}{hex(at)} ?? {op:02x}  (stop)'); return
        out.append(f'{pad}{hex(at)} {NAMES.get(op, hex(op))} {arg}')
        if op == 0 and end is None: return

def blocks(d, root):
    # every action block reachable from the movie: frame actions and init actions of the movie and its sprites
    res = []
    def frames(fc, fp, who):
        for f in range(fc):
            ic, ip = U(d, fp + f * 8), U(d, fp + f * 8 + 4)
            for i in range(ic):
                it = U(d, ip + i * 4); t = U(d, it)
                if t == 1: res.append((f'{who} frame {f} action', U(d, it + 4)))
                if t == 7: res.append((f'{who} frame {f} initaction sprite {U(d, it + 4)}', U(d, it + 8)))
    frames(U(d, root + 8), U(d, root + 12), 'movie')
    cc, cp = U(d, root + 20), U(d, root + 24)
    for i in range(cc):
        ch = U(d, cp + 4 * i)
        if ch and U(d, ch) == 5:  # sprite
            frames(U(d, ch + 8), U(d, ch + 12), f'sprite {i}')
    return res

if __name__ == '__main__':
    d, consts, root = load(sys.argv[1]); flt = sys.argv[2] if len(sys.argv) > 2 else None
    for who, o in blocks(d, root):
        out = []
        try: dis(d, consts, o, out)
        except Exception as e: out.append(f'  error {e}')
        text = '\n'.join(out)
        if not flt or flt in text: print(f'== {who} at {hex(o)}\n{text}')
