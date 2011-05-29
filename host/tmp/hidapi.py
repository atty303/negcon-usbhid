import ctypes
from hidapi_wrapper import *

__all__ = [
    'HIDError',
    'enumerate',
    'Device',
]

class HIDError(Exception): pass

class _DeviceInfo(object):
    def __init__(self, **kwargs):
        self.__dict__.update(kwargs)

    def __repr__(self):
        return '<HIDDeviceInfo(0x%04x,0x%04x,%s)>' % (self.vendor_id, self.product_id, self.path)

def enumerate(vendor_id=0, product_id=0):
    infos = []
    p_info_first = hid_enumerate(vendor_id, product_id)
    p_info = p_info_first
    try:
        while bool(p_info):
            info = p_info[0]
            kwargs = {}
            for f in hid_device_info._fields_:
                if f[0] != 'next':
                    kwargs[f[0]] = getattr(info, f[0])
            infos.append(_DeviceInfo(**kwargs))
            p_info = info.next
    finally:
        hid_free_enumeration(p_info_first)
    return infos


class Device(object):
    def __init__(self, vendor_id=None, product_id=None, serial_number=None, path=None):
        if path:
            self._device = hid_open_path(path)
        else:
            assert vendor_id is not None and product_id is not None
            self._device = hid_open(vendor_id, product_id, serial_number)
        if not bool(self._device):
            self._handle_error(-1)

    def write(self, data):
        ret = hid_write(self._device, ctypes.byref(data), len(data))
        self._handle_error(ret)
        return ret

    def read(self, length):
        buf = ctypes.create_string_buffer('X'*length, length)
        ret = hid_read(self._device, ctypes.cast(buf, ctypes.c_char_p), length)
        self._handle_error(ret)
        return buf.raw

    def set_nonblocking(self, nonblock):
        ret = hid_set_nonblocking(self._device, 1 if nonblock else 0)
        self._handle_error(ret)

    def send_feature_report(self, data):
        ret = hid_send_feature_report(self._device, ctypes.byref(data), len(data))
        self._handle_error(ret)
        return ret

    def get_feature_report(self, report_id, length):
        buf = ctypes.create_string_buffer('X'*length, length)
        buf[0] = chr(report_id)
        ret = hid_get_feature_report(self._device, ctypes.cast(buf, ctypes.c_char_p), length)
        self._handle_error(ret)
        return buf.raw

    def close(self):
        hid_close(self._device)
        self._device = None

    def get_serial_number_string(self):
        buf = ctypes.create_unicode_buffer('X'*255, 255)
        hid_get_serial_number_string(self._device, ctypes.cast(buf, ctypes.c_wchar_p), 255)
        return buf.value

    def _handle_error(self, ret):
        if ret < 0:
            msg = hid_error(self._device)
            if msg:
                raise HIDError(msg)
            else:
                raise HIDError('Unknown error: %d' % ret)


if __name__ == '__main__':
    for info in enumerate():
        print repr(info)

    device = Device(vendor_id=0x16c0, product_id=0x27dc)
    device.close()
