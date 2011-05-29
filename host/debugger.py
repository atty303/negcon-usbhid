import sys
import time
import usb.core
import usb.util

def bin(x, width):
    return ''.join(str((x>>i)&1) for i in xrange(width-1,-1,-1))

def hexdump(v):
    return ' '.join(['%02x' % x for x in v])

def bindump(v):
    return ' '.join([bin(x, 8) for x in v])

class Device(object):
    def __init__(self):
        device = usb.core.find(custom_match=lambda d: d.idVendor == 0x16c0 and d.idProduct == 0x27dc)
        # check serialNumber
        if not device:
            raise ValueError('device was not found')
        device.set_configuration()
        self._device = device

    def recv_vendor_request(self, cmd, value, index, length):
        bRequestType = (usb.util.CTRL_TYPE_VENDOR |
                        usb.util.CTRL_RECIPIENT_DEVICE |
                        usb.util.ENDPOINT_IN)
        return self._device.ctrl_transfer(bRequestType, cmd, value, index, length)

    def hid_get_feature(self, report_id=0, index=0, length=128):
        bRequestType = (usb.util.CTRL_TYPE_CLASS |
                        usb.util.CTRL_RECIPIENT_DEVICE |
                        usb.util.ENDPOINT_IN)
        return self._device.ctrl_transfer(bRequestType, 0x01, 0x0300 | report_id,
                                          index, length)

    def hid_set_feature(self, data, report_id=0, index=0):
        bRequestType = (usb.util.CTRL_TYPE_CLASS |
                        usb.util.CTRL_RECIPIENT_DEVICE |
                        usb.util.ENDPOINT_OUT)
        return self._device.ctrl_transfer(bRequestType, 0x09, 0x0300 | report_id,
                                          index, data)

import struct

class Setting(object):
    def __init__(self):
        self.map = {}
        self.calib = {}

    def pack(self):
        d = struct.pack('<11B6B',
                        self.map['start'],
                        self.map['a'],
                        self.map['b'],
                        self.map['r'],
                        self.map['left'],
                        self.map['down'],
                        self.map['right'],
                        self.map['up'],
                        self.map['i'],
                        self.map['ii'],
                        self.map['l'],
                        self.calib['i'][0], self.calib['i'][1],
                        self.calib['ii'][0], self.calib['ii'][1],
                        self.calib['l'][0], self.calib['l'][1])
        return d

def write_setting(device):
    s = Setting()
    s.map['a'] = 0
    s.map['b'] = 1
    s.map['r'] = 2
    s.map['start'] = 3
    s.map['left'] = 4
    s.map['down'] = 5
    s.map['right'] = 6
    s.map['up'] = 7
    s.map['i'] = 8
    s.map['ii'] = 9
    s.map['l'] = 10
    s.calib['i'] = (40, 255)
    s.calib['ii'] = (150, 255)
    s.calib['l'] = (20, 200)

    data = s.pack()
    print 'Write:', hexdump([ord(d) for d in data])
    print 'Len: ', device.hid_set_feature(data)

def read_setting(device):
    v = device.hid_get_feature()
    print 'Readed:', hexdump(v)
    print 'Length:', len(v)

def pool(device):
    while True:
        ret = device.hid_get_feature()
        # ret = []
        # for i, r in enumerate(ret2):
        #     if i >= 2:
        #         ret.append((~r) & 0xFF)
        #     else:
        #         ret.append(r)
        sys.stderr.write(hexdump(ret) + ' | ' + bindump(ret) + '\r')
        if ret[1] != 0x5A:
            sys.stderr.write('\nERROR\n')
        time.sleep(0.05)

def main():
    device = Device()
    write_setting(device)
    read_setting(device)

if __name__ == '__main__':
    main()
