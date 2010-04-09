import atexit

from ctypes import *

import sys
if '--debug' in sys.argv:
    libredis = cdll.LoadLibrary("Debug/libredis.so")
elif '--release' in sys.argv:
    libredis = cdll.LoadLibrary("Release/libredis.so")
else:
    libredis = cdll.LoadLibrary("lib/libredis.so")

g_module = libredis.Module_new()
libredis.Module_init(g_module)
def g_Module_free():
    libredis.Module_free(g_module)
atexit.register(g_Module_free)

DEFAULT_TIMEOUT_MS = 3000

class RedisError(Exception):
    pass
    
class Executor(object):
    def __init__(self):
        self._executor = libredis.Executor_new()
    
    def add(self, connection, batch):
        libredis.Executor_add(self._executor, connection._connection, batch._batch)
                    
    def execute(self, timeout_ms = DEFAULT_TIMEOUT_MS):
        libredis.Executor_execute(self._executor, timeout_ms)

    def free(self):
        libredis.Executor_free(self._executor)
        self._executor = None

    def __del__(self):
        if self._executor is not None:
            self.free()
            
class Connection(object):
    def __init__(self, addr):
        self._connection = libredis.Connection_new(addr)

    def get(self, key, timeout_ms = DEFAULT_TIMEOUT_MS):
        batch = Batch()
        batch.write("GET %s\r\n" % key, 1)
        return self._execute_simple(batch, timeout_ms)
    
    def _execute_simple(self, batch, timeout_ms):
        executor = Executor()
        executor.add(self, batch)
        executor.execute(timeout_ms)
        return Reply.from_next(batch).value
       
    def free(self):
        libredis.Connection_free(self._connection)
        self._connection = None

    def __del__(self):
        if self._connection is not None:
            self.free()

class ConnectionManager(object):
    def __init__(self):
        self._connections = {}
            
    def get_connection(self, addr):
        if not addr in self._connections:
            self._connections[addr] = Connection(addr)
        return self._connections[addr]
        
class Reply(object):
    RT_ERROR = -1
    RT_NONE = 0
    RT_OK = 1
    RT_BULK_NIL = 2
    RT_BULK = 3
    RT_MULTIBULK_NIL = 4
    RT_MULTIBULK = 5
    RT_INTEGER = 6

    def __init__(self, type, value):
        self.type = type
        self.value = value
        
    def is_multibulk(self):
        return self.type == self.RT_MULTIBULK
    
    @classmethod
    def from_next(cls, batch, raise_exception_on_error = True):
        data = c_char_p()
        rt = c_int()
        len = c_int()
        libredis.Batch_next_reply(batch._batch, byref(rt),byref(data), byref(len))
        type = rt.value
        #print repr(type)
        if type in [cls.RT_OK, cls.RT_ERROR, cls.RT_BULK]:
            value = string_at(data, len)
            if type == cls.RT_ERROR and raise_exception_on_error:
                raise RedisError(value)
        elif type in [cls.RT_MULTIBULK]:
            value = len
        else:
            assert False
        return Reply(type, value)
            

class Buffer(object):
    def __init__(self, buffer):
        self._buffer = buffer
        
    def dump(self, limit = 64):
        libredis.Buffer_dump(self._buffer, limit)
        
class Batch(object):
    def __init__(self, cmd = '', nr_commands = 0):
        self._batch = libredis.Batch_new()
        if cmd or nr_commands:
            self.write(cmd, nr_commands)
            
    def write(self, cmd = '', nr_commands = 0):
        libredis.Batch_write(self._batch, cmd, len(cmd), nr_commands)
        return self
    
    def get(self, key): 
        return self.write("GET %s\r\n" % key, 1)

    def set(self, key, value):
        return self.write("SET %s %d\r\n%s\r\n" % (key, len(value), value), 1)
    
    def next_reply(self):
        return Reply.from_next(self)

    @property
    def write_buffer(self):
        return Buffer(libredis.Batch_write_buffer(self._batch))
        
    def free(self):
        libredis.Batch_free(self._batch)
        self._batch = None

    def __del__(self):
        if self._batch is not None:
            self.free()

class Ketama(object):
    libredis.Ketama_get_server_address.restype = c_char_p
    
    def __init__(self):
        self._ketama = libredis.Ketama_new()

    def add_server(self, addr, weight):
        libredis.Ketama_add_server(self._ketama, addr[0], addr[1], weight)

    def create_continuum(self):
        libredis.Ketama_create_continuum(self._ketama)

    def print_continuum(self):
        libredis.Ketama_print_continuum(self._ketama)

    def get_server_ordinal(self, key):
        return libredis.Ketama_get_server_ordinal(self._ketama, key, len(key))

    def get_server_address(self, ordinal):
        return libredis.Ketama_get_server_address(self._ketama, ordinal)

    def free(self):
        libredis.Ketama_free(self._ketama)
        self._ketama = None

    def __del__(self):
        if self._ketama is not None:
            self.free()
            
class Redis(object):
    def __init__(self, server_hash, connection_manager):
        self.server_hash = server_hash
        self.connection_manager = connection_manager

    def _execute_simple(self, batch, server_key, timeout_ms = DEFAULT_TIMEOUT_MS):
        server_addr = self.server_hash.get_server_address(self.server_hash.get_server_ordinal(server_key))
        connection = self.connection_manager.get_connection(server_addr)
        return connection._execute_simple(batch, timeout_ms)
        
    def set(self, key, value, server_key = None, timeout_ms = DEFAULT_TIMEOUT_MS):
        if server_key is None: server_key = key
        batch = Batch().set(key, value)
        return self._execute_simple(batch, server_key)
    
    def get(self, key, server_key = None, timeout_ms = DEFAULT_TIMEOUT_MS):
        if server_key is None: server_key = key
        batch = Batch().get(key)
        return self._execute_simple(batch, server_key)
    
    def mget(self, *keys, **kwargs):
        timeout_ms = kwargs.get('timeout_ms', DEFAULT_TIMEOUT_MS)
        batches = {}
        #add all keys to batches
        for key in keys:
            server_ip = self.server_hash.get_server_address(self.server_hash.get_server_ordinal(key))
            batch = batches.get(server_ip, None)
            if batch is None: #new batch
                batch = Batch("MGET")
                batch.keys = []
                batches[server_ip] = batch
            batch.write(" %s" % key)
            batch.keys.append(key)
        #finalize batches, and start executing
        executor = Executor()
        for server_ip, batch in batches.items():
            batch.write("\r\n", 1)
            connection = self.connection_manager.get_connection(server_ip)
            executor.add(connection, batch)
        #handle events until all complete
        executor.execute(timeout_ms)
        #build up results
        results = {}
        for batch in batches.values():
            #only expect 1 (multibulk) reply per batch
            reply = batch.next_reply()
            assert reply.is_multibulk()
            for key in batch.keys:
                child = batch.next_reply()
                value = child.value
                results[key] = value
        return results
    
