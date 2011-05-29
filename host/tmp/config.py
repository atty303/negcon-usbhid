import hidapi

import sys
import time
import usb.core
import usb.util

def bin(x, width):
    return ''.join(str((x>>i)&1) for i in xrange(width-1,-1,-1))

def main():
    device = hidapi.Device(0x16c0, 0x27dc)
    # check serialNumber
    device.set_nonblocking(1)

    while True:
        # ret2 = device.get_feature_report(0, 8)
        ret2 = device.read(8)
        sys.stderr.write(repr(ret2) + '\r')
        ret = []
        for i, r in enumerate(ret2):
            if i >= 2:
                ret.append((~ord(r)) & 0xFF)
            else:
                ret.append(ord(r))
        # sys.stderr.write(' '.join(['%02x' % x for x in ret]) + ' | ' + ' '.join([bin(x, 8) for x in ret]) + '\r')
        # if ret[1] != 0x5A:
        #     print 'error'
        time.sleep(0.05)

import hid_ctypes

def main2():
    iface = hid_ctypes.Interface(vendor_id=0x16c0, product_id=0x27dc)
    # iface.write_identification(sys.stdout)
    # iface.dump_tree(sys.stdout)

if __name__ == '__main__':
    main()
