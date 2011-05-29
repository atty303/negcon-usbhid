from ctypes import *

'''This module provides an interface to libhid that should be more familiar to
Python programmers. It uses the ctypes library introduced in Python 2.5 (but
also available from http://starship.python.net/crew/theller/ctypes/

See the test_hidwrap.py script for an example of how to use this module.
'''

# Load the libhid shared object:
hid = cdll.LoadLibrary("libhid.dylib")

#libc = cdll.LoadLibrary("libc.so.6")

# import all symbolic constants from hid --------------------------------------
### FIXME: can we do this automatically in ctypes?
# for identifier in dir(hid):
#    if identifier.startswith("HID_"):
#        locals()[identifier] = hid.__dict__[identifier]

HID_RET_SUCCESS = 0

# types:
class HIDInterfaceMatcher(Structure):
    _fields_ = [
        ("vendor_id", c_ushort),
        ("product_id", c_ushort), ]
#       ("matcher_fn", 

hid.hid_strerror.restype = c_char_p

# error handling --------------------------------------------------------------
class HIDError(Exception):
    '''The exception class for errors raised by hidwrap.
    ``code`` is the return code (HID_RET_*) and ``description`` is an error
    string from hid.hid_strerror().
    '''
    def __init__(self, code, description):
        self.code = code
        self.description = description
    def __str__(self):
        return repr((self.code, self.description))




def _hid_raise(op, result):
    '''Called on return from a libhid function. It interprets the result code,
    and raises a HIDError exception if the function returned a non-zero error
    code.
    '''
    if isinstance(result, int):
        result_code = result
        retval = None
    elif isinstance(result, (tuple, list)):
        result_code = result[0]
        if len(result) == 2:
            retval = result[1]
        else:
            retval = result[1:]
    else:
        raise ValueError, "result must be either an int or a tuple"

    if result_code != HID_RET_SUCCESS:
        try:
            raise HIDError, (result_code, "%s: %s" % (op, hid.hid_strerror(result_code)))
        except KeyError:
            raise HIDError, (result_code, "%s: Unknown error code" % op)
    else:
        return retval




# debugging -------------------------------------------------------------------
def set_debug(value):
    '''Set the debug level to the bitwise OR of one or more of the following:
     * HID_DEBUG_NONE (0; default)
     * HID_DEBUG_ERRORS 
     * HID_DEBUG_WARNINGS
     * HID_DEBUG_NOTICES
     * HID_DEBUG_TRACES
     * HID_DEBUG_ASSERTS
     * HID_DEBUG_NOTRACES
     * HID_DEBUG_ALL
    '''
    hid.hid_set_debug(value)

def set_debug_stream(stream):
    '''Send debug messages to a specific Python output stream (such as
    sys.stdout or sys.stderr.'''
    stream_file = libc.fdopen(stream.fileno(), 'w')
    hid.hid_set_debug_stream(stream_file)

def set_usb_debug(level):
    '''Set the debug level used by libusb.'''
    hid.hid_set_usb_debug(level)




# init / cleanup --------------------------------------------------------------
IS_INITIALIZED = [False]

def init():
    '''Calls hid_init(), and registers an atexit procedure to clean up at
    termination time.'''
    import atexit

    _hid_raise("init", hid.hid_init())
    IS_INITIALIZED[0] = True
    atexit.register(_finalize_hid)

def is_initialized():
    return IS_INITIALIZED[0]

def cleanup():
    _hid_raise("cleanup", hid.hid_cleanup())
    IS_INITIALIZED[0] = False

def _finalize_hid():
    if is_initialized():
        cleanup()


# interface -------------------------------------------------------------------
class Interface:
    '''A HID Interface object.

    Constructing an interface calls hid_force_open() (or hid_open, if force = False).

    Usage: iface = Interface(vendor_id = 0x1234, product_id = 0x5678)
    
    Raises a HIDError exception if the interface cannot be opened.
    '''
    def __init__(self, vendor_id, product_id, interface_number = 0, 
            force=True, retries=3):
        self.is_open = False

        if not is_initialized():
            init()

        matcher = HIDInterfaceMatcher() # Changed: ctypes
        matcher.vendor_id = vendor_id
        matcher.product_id = product_id

        self.interface = hid.hid_new_HIDInterface()

        if force:
            _hid_raise("force_open", hid.hid_force_open(
                self.interface, interface_number, byref(matcher), retries)) # ctypes: added byref
        else:
            _hid_raise("open", hid.hid_open(self.interface, 0, byref(matcher))) # ctypes: added byref

        self.is_open = True


    def write_identification(self, stream):
        '''Print the vendor, model and serial number strings, as available.'''
        stream_file = libc.fdopen(stream.fileno(), 'w')
        _hid_raise("write_identification", hid.hid_write_identification(
            stream_file, self.interface))

    def dump_tree(self, stream):
        '''Dump the HID tree to a Python output stream.'''
        stream_file = libc.fdopen(stream.fileno(), 'w')
        _hid_raise("dump_tree", hid.hid_dump_tree(
            stream_file, self.interface))

    def __del__(self):
        '''If the interface has been properly opened, close it.'''
        if self.is_open:
            self.close()

    def close(self):
        '''Close the Interface. Called automatically by __del__().'''
        _hid_raise("close", hid.hid_close(self.interface))

    def get_input_report(self, path, size):
        return _hid_raise("get_input_report",
                hid.hid_get_input_report(self.interface, path, size))

    def set_output_report(self, path, bytes):
        _hid_raise("set_output_report",
                hid.hid_set_output_report(self.interface, path, bytes))

    def get_feature_report(self, path, size):
        return _hid_raise("get_feature_report",
                hid.hid_get_feature_report(self.interface, path, size))

    def set_feature_report(self, path, bytes):
        _hid_raise("set_feature_report",
                hid.hid_set_feature_report(self.interface, path, bytes))

    def get_item_value(self, path):
        return _hid_raise("get_item_value",
                hid.hid_get_item_value(self.interface, path))

    def interrupt_read(self, endpoint, size, timeout=0):
        return _hid_raise("interrupt_read",
                hid.hid_interrupt_read(self.interface, endpoint, size, timeout))

    def interrupt_write(self, endpoint, bytes, timeout=0):
        return _hid_raise("interrupt_write",
                hid.hid_interrupt_write(self.interface, endpoint, bytes, timeout))

    def set_idle(self, duration=0, report_id=0):
        '''Set the idle interval for an interface (report_id=0) or a specific report.

        The duration should be 0 (no reports unless something changes) or a
        multiple of 4 up to 1020 (milliseconds).'''
        return _hid_raise("set_idle",
                hid.hid_set_idle(self.interface, duration, report_id))

