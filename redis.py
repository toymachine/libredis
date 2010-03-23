import atexit

from ctypes import *

libredis = cdll.LoadLibrary("Debug/libredis.so")
#libredis = cdll.LoadLibrary("Release/libredis.so")

libredis.Module_init()
atexit.register(libredis.Module_free)

class Connection(object):
    def __init__(self, addr):
        self._connection = libredis.Connection_new(addr)

    def get(self, key):
        batch = Batch()
        batch.write("GET %s\r\n", key)
        batch.add_command()
        batch.execute(self)
        reply = batch.next_reply()
        return reply.value
        
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
    def __init__(self, reply):
        self._reply = reply

    @property
    def type(self):
        return libredis.Reply_type(self._reply)
    
    def dump(self):
        libredis.Reply_dump(self._reply)

    def free(self):
        libredis.Reply_free(self._reply)
        self._reply = None

    def __del__(self):
        if self._reply is not None:
            self.free()

    @property
    def value(self):
        return string_at(libredis.Reply_data(self._reply), libredis.Reply_length(self._reply))

class Buffer(object):
    def __init__(self, buffer):
        self._buffer = buffer
        
    def dump(self, limit = 64):
        libredis.Buffer_dump(self._buffer, limit)
        
class Batch(object):
    def __init__(self):
        self._batch = libredis.Batch_new()

    def write(self, format, *args):
        libredis.Batch_write(self._batch, format, *args)

    def add_command(self):
        libredis.Batch_add_command(self._batch)
    
    def execute(self, connection):
        libredis.Batch_execute(self._batch, connection._connection)

    def has_reply(self):
        return bool(libredis.Batch_has_reply(self._batch))

    def next_reply(self):
        return Reply(libredis.Batch_next_reply(self._batch))

    @property
    def write_buffer(self):
        return Buffer(libredis.Batch_write_buffer(self._batch))
        
    def free(self):
        libredis.Batch_free(self._batch)
        self._batch = None

    def __del__(self):
        if self._batch is not None:
            self.free()

class Executor(object):
    def __init__(self):
        self._executor = libredis.Executor_new()
    
    def add_batch_connection(self, batch, connection):
        libredis.Executor_add_batch_connection(self._executor, batch._batch, connection._connection)
    
    def has_batch(self):
        libredis.Executor_has_batch(self._executor)

    def pop_batch(self):
        return Batch(libredis.Executor_pop_batch(self._executor))
        
    def execute(self):
        libredis.Executor_execute(self._executor)

    def free(self):
        libredis.Executor_free(self._executor)
        self._executor = None

    def __del__(self):
        if self._executor is not None:
            self.free()
        
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

    def __del__(self):
        if self._ketama is not None:
            self.free()
            
class Redis(object):
    def __init__(self, server_hash, connection_manager):
        self.server_hash = server_hash
        self.connection_manager = connection_manager
        
    def mget(self, *keys):
        batches = {}
        for key in keys:
            server_ip = self.server_hash.get_server(key)
            batch = batches.get(server_ip, None)
            if batch is None:
                batch = Batch()
                batch.write("MGET")
                batches[server_ip] = batch
            batch.write(" %s", key)
        executor = Executor()
        for server_ip, batch in batches.items():
            batch.write("\r\n")
            #batch.write_buffer.dump()
            batch.add_command()
            connection = self.connection_manager.get_connection(server_ip)
            executor.add_batch_connection(batch, connection)
        executor.execute()
        while executor.has_batch():
            batch = executor.pop_batch()
            while batch.has_reply():
                reply = batch.next_reply()
                print reply.type
        
    
