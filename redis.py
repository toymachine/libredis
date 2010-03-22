import atexit

from ctypes import *

libredis = cdll.LoadLibrary("Debug/libredis.so")
#libredis = cdll.LoadLibrary("Release/libredis.so")

libredis.Module_init()
atexit.register(libredis.Module_free)

class Connection(object):
    def __init__(self, addr, port):
        self._connection = libredis.Connection_new(addr, port)

    def get(self, key):
        batch = Batch()
        batch.write_command("GET %s\r\n", key)
        batch.execute(self)
        reply = batch.next_reply()
        value = reply.value
        reply.free()
        batch.free()
        return value
        
    def free(self):
        libredis.Connection_free(self._connection)
        self._connection = None

class Reply(object):
    def __init__(self, reply):
        self._reply = reply

    def dump(self):
        libredis.Reply_dump(self._reply)

    def free(self):
        libredis.Reply_free(self._reply)
        self._reply = None

    @property
    def value(self):
        return string_at(libredis.Reply_data(self._reply), libredis.Reply_length(self._reply))

class Batch(object):
    def __init__(self):
        self._batch = libredis.Batch_new()

    def write_command(self, format, *args):
        libredis.Batch_write_command(self._batch, format, *args);

    def execute(self, connection):
        libredis.Batch_execute(self._batch, connection._connection)

    def has_reply(self):
        return bool(libredis.Batch_has_reply(self._batch))

    def next_reply(self):
        return Reply(libredis.Batch_next_reply(self._batch))

    def free(self):
        libredis.Batch_free(self._batch)
        self._batch = None

class Ketama(object):
    libredis.Ketama_get_server.restype = c_char_p
    
    def __init__(self):
        self._ketama = libredis.Ketama_new()

    def add_server(self, addr, weight):
        libredis.Ketama_add_server(self._ketama, addr[0], addr[1], weight)

    def create_continuum(self):
        libredis.Ketama_create_continuum(self._ketama)

    def get_server(self, key):
        return libredis.Ketama_get_server(self._ketama, key, len(key))

    def free(self):
        libredis.Ketama_free(self._ketama)
        self._ketama = None

