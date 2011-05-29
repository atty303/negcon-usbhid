from ctypes import *
import ctypes.util

__all__ = [
    'hid_device',
    'hid_device_info',

    'hid_enumerate',
    'hid_free_enumeration',
    'hid_open',
    'hid_open_path',
    'hid_write',
    'hid_read',
    'hid_set_nonblocking',
    'hid_send_feature_report',
    'hid_get_feature_report',
    'hid_close',
    'hid_get_manufacturer_string',
    'hid_get_product_string',
    'hid_get_serial_number_string',
    'hid_get_indexed_string',
    'hid_error',
]

def load():
    dlname = ctypes.util.find_library('hidapi')
    if not dlname:
        raise ImportError("Couldn't find hidapi library")
    return cdll.LoadLibrary(dlname)

lib = load()

class hid_device(Structure): pass

class hid_device_info(Structure): pass
hid_device_info._fields_ = [
    #: Platform-specific device path
    ('path', c_char_p),
    #: Device Vendor ID
    ('vendor_id', c_ushort),
    #: Device Product ID
    ('product_id', c_ushort),
    #: Serial Number
    ('serial_number', c_wchar_p),
    #: Device Release Number in binary-coded decimal,
    #: also known as Device Version Number
    ('release_number', c_ushort),
    #: Manufacturer String
    ('manufacturer_string', c_wchar_p),
    #: Product_string
    ('product_string', c_wchar_p),
    #: Usage Page for this Device/Interface (Windows/Mac only).
    ('usage_page', c_ushort),
    #: Usage for this Device/Interface (Windows/Mac only).
    ('usage', c_ushort),
    #: The USB interface which this logical device
    #: represents (Linux/libusb implementation only).
    ('interface_number', c_int),
    #: Pointer to the next device
    ('next', POINTER(hid_device_info)),
]

def function(ret, name, *args):
    prototype = CFUNCTYPE(ret, *[a[0] for a in args])
    paramflags = []
    for a in args:
        if len(a) == 2:
            paramflags.append((1, a[1]))
        elif len(a) == 3:
            paramflags.append((a[2], a[1]))
        elif len(a) == 4:
            paramflags.append((a[2], a[1], a[3]))
    return prototype((name, lib), tuple(paramflags))

hid_enumerate = \
    function(POINTER(hid_device_info), 'hid_enumerate',
             (c_ushort, 'vendor_id'),
             (c_ushort, 'product_id'))

hid_free_enumeration = \
    function(None, 'hid_free_enumeration',
             (POINTER(hid_device_info), 'devs'))
hid_open = \
    function(POINTER(hid_device), 'hid_open',
             (c_ushort, 'vendor_id'),
             (c_ushort, 'product_id'),
             (c_wchar_p, 'serial_number'))

hid_open_path = \
    function(POINTER(hid_device), 'hid_open_path',
             (c_char_p, 'path'))

hid_write = \
    function(c_int, 'hid_write',
             (POINTER(hid_device), 'device'),
             (POINTER(c_char_p), 'data'),
             (c_size_t, 'length'))

hid_read = \
    function(c_int, 'hid_read',
             (POINTER(hid_device), 'device'),
             (POINTER(c_char_p), 'data'),
             (c_size_t, 'length'))

hid_set_nonblocking = \
    function(c_int, 'hid_set_nonblocking',
             (POINTER(hid_device), 'device'),
             (c_int, 'nonblock'))

hid_send_feature_report = \
    function(c_int, 'hid_send_feature_report',
             (POINTER(hid_device), 'device'),
             (POINTER(c_char_p), 'data'),
             (c_size_t, 'length'))

hid_get_feature_report = \
    function(c_int, 'hid_get_feature_report',
             (POINTER(hid_device), 'device'),
             (POINTER(c_char_p), 'data'),
             (c_size_t, 'length'))

hid_close = \
    function(None, 'hid_close',
             (POINTER(hid_device), 'device'))

hid_get_manufacturer_string = \
    function(c_int, 'hid_get_manufacturer_string',
             (POINTER(hid_device), 'device'),
             (c_wchar_p, 'string'),
             (c_size_t, 'maxlen'))
             
hid_get_product_string = \
    function(c_int, 'hid_get_product_string',
             (POINTER(hid_device), 'device'),
             (c_wchar_p, 'string'),
             (c_size_t, 'maxlen'))

hid_get_serial_number_string = \
    function(c_int, 'hid_get_serial_number_string',
             (POINTER(hid_device), 'device'),
             (c_wchar_p, 'string'),
             (c_size_t, 'maxlen'))

hid_get_indexed_string = \
    function(c_int, 'hid_get_indexed_string',
             (POINTER(hid_device), 'device'),
             (c_wchar_p, 'string'),
             (c_size_t, 'maxlen'))

hid_error = \
    function(c_wchar_p, 'hid_error',
             (POINTER(hid_device), 'device'))

if __name__ == '__main__':
    devices = hid_enumerate(vendor_id=0x16c0, product_id=0x27dc)
    if not devices:
        raise ValueError("No device found")
    devinfo = devices[0]
    print devinfo.path, devinfo.serial_number
    serial = devinfo.serial_number
    hid_free_enumeration(devices)

    dev_p = hid_open(0x16c0, 0x27dc, serial)
    dev = dev_p[0]

    hid_close(dev_p)
