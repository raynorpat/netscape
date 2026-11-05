"""Check the actual executable's toolbar and icon resources against Gold assets.

Uses Windows resource APIs without loading or executing application code.
No Pillow dependency is added to the native build/test requirements.
"""
import argparse
import ctypes
from ctypes import wintypes
from pathlib import Path
import re
import struct
from check_windows_artwork_pixels import check as check_pixels


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', required=True, type=Path)
    args = parser.parse_args()
    check_pixels()
    root = Path(__file__).resolve().parents[2] / 'cmd/winfe'
    ids = {}
    for header in ('resource.h', 'composer_resource.h'):
        ids.update((name, int(number)) for name, number in
                   re.findall(r'^#define\s+(\w+)\s+(\d+)\s*$', (root / header).read_text(), re.M))
    kernel = ctypes.WinDLL('kernel32', use_last_error=True)
    kernel.LoadLibraryExW.argtypes = [wintypes.LPCWSTR, wintypes.HANDLE, wintypes.DWORD]
    kernel.LoadLibraryExW.restype = wintypes.HMODULE
    kernel.FindResourceW.argtypes = [wintypes.HMODULE, ctypes.c_void_p, ctypes.c_void_p]
    kernel.FindResourceW.restype = ctypes.c_void_p
    kernel.SizeofResource.argtypes = [wintypes.HMODULE, ctypes.c_void_p]
    kernel.SizeofResource.restype = wintypes.DWORD
    kernel.LoadResource.argtypes = [wintypes.HMODULE, ctypes.c_void_p]
    kernel.LoadResource.restype = ctypes.c_void_p
    kernel.LockResource.argtypes = [ctypes.c_void_p]
    kernel.LockResource.restype = ctypes.c_void_p
    kernel.FreeLibrary.argtypes = [wintypes.HMODULE]
    module = kernel.LoadLibraryExW(str(args.executable.resolve()), None, 2)  # AS_DATAFILE
    if not module:
        raise ctypes.WinError(ctypes.get_last_error())

    def resource(kind, number):
        found = kernel.FindResourceW(module, number, kind)
        if not found:
            raise RuntimeError('Missing executable resource {}/{}'.format(kind, number))
        size = kernel.SizeofResource(module, found)
        pointer = kernel.LockResource(kernel.LoadResource(module, found))
        if not pointer or not size:
            raise RuntimeError('Unreadable executable resource')
        return ctypes.string_at(pointer, size)

    checked = 0
    try:
        for name, kind, filename in re.findall(r'^(\w+)\s+(BITMAP|ICON)\s+"([^"]+)"',
                                              (root / 'netscape.rc').read_text(), re.M):
            number = int(name) if name.isdigit() else ids[name]
            expected = (root / filename).read_bytes()
            if kind == 'BITMAP':
                assert resource(2, number) == expected[14:], filename
            else:
                group = resource(14, number)
                count = struct.unpack_from('<H', expected, 4)[0]
                assert struct.unpack_from('<H', group, 4)[0] == count, filename
                for i in range(count):
                    entry = struct.unpack_from('<BBBBHHIH', group, 6 + 14 * i)
                    icon = struct.unpack_from('<BBBBHHII', expected, 6 + 16 * i)
                    assert entry[:7] == icon[:7], filename
                    assert resource(3, entry[7]) == expected[icon[7]:icon[7] + icon[6]], filename
            checked += 1
    finally:
        kernel.FreeLibrary(module)
    assert checked >= 70, checked
    print('Verified', checked, 'compiled Windows Gold bitmap/icon resources')


if __name__ == '__main__':
    main()
