"""Extract the exact production glow pass for a standalone real-GL regression.

Only emulator configuration, shader-compatibility and context infrastructure are
provided by the test. Geometry, shader bodies, settings sizing and draw order
come verbatim from the selected production xr_gun.cpp.
"""
import argparse
import hashlib
from pathlib import Path


def function(source, signature):
    start = source.index(signature)
    opening = source.index('{', start)
    depth = 1
    for at in range(opening + 1, len(source)):
        depth += (source[at] == '{') - (source[at] == '}')
        if not depth:
            return source[start:at + 1]
    raise ValueError('Unclosed function: ' + signature)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--header', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    source = args.source.read_text()
    functions = [function(source, name) for name in (
        'bool initGlow()', 'void drawGlow(', 'void billboard(', 'void flame(',
        'void drawGlowShape(', 'static void muzzleNow(', 'void customReticle(',
        'void drawGunGlow(')]
    handles = source.index('GLuint glowVbo')
    globals_ = source[handles:source.index('\n', handles)] + '\n' + source[source.index('struct GlowVertex'):source.index('bool initModel()')]
    signature = functions[-1].split('{', 1)[0]
    has_occlusion = 'occludeCustomReticle' in signature
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(
        '// Exact extracted xr_gun.cpp SHA256: ' + hashlib.sha256(args.source.read_bytes()).hexdigest() + '\n'
        '#define SS_TEST_GLOW_HAS_OCCLUSION ' + str(int(has_occlusion)) + '\n'
        'namespace vr::xr {\n' + globals_ + '\n'.join(functions) + '\n}\n', newline='\n')
    (args.output.parent / 'xr_gun.h').write_bytes(args.header.read_bytes())


if __name__ == '__main__':
    main()
