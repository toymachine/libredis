from redis import *

import time

class _Timer:
    def __enter__(self):
        self._start = time.time()
        return self

    def __exit__(self, type, value, traceback):
        self._end = time.time()

    def __str__(self):
        self._end - self._start

    def sec(self, n):
        return n / (self._end - self._start)
    
def timer():
    return _Timer()

def test_simple():
    connection = Connection("127.0.0.1:6379")

    N = 10
    for j in range(N):
        print repr(connection.get('blaat'))
        
def test_ketama():
    ketama = Ketama()

    ketama.add_server(("10.0.1.1", 11211), 600)
    ketama.add_server(("10.0.1.2", 11211), 300)
    ketama.add_server(("10.0.1.3", 11211), 200)
    ketama.add_server(("10.0.1.4", 11211), 350)
    ketama.add_server(("10.0.1.5", 11211), 1000)
    ketama.add_server(("10.0.1.6", 11211), 800)
    ketama.add_server(("10.0.1.7", 11211), 950)
    ketama.add_server(("10.0.1.8", 11211), 100)

    ketama.create_continuum()
    #ketama.print_continuum()
    def check(key, addr):
        assert addr == ketama.get_server_address(ketama.get_server_ordinal(key))
        
    check('12936', '10.0.1.7:11211')
    check('27804', '10.0.1.5:11211')
    check('37045', '10.0.1.2:11211')
    check('50829', '10.0.1.1:11211')
    check('65422', '10.0.1.6:11211')
    check('74912', '10.0.1.6:11211')
    
def test_mget():
    ketama = Ketama()
    ketama.add_server(("127.0.0.1", 6379), 300)
    ketama.add_server(("127.0.0.1", 6380), 300)
    
    ketama.create_continuum()

    connection_manager= ConnectionManager()
    
    redis = Redis(ketama, connection_manager)
    
    
#    M = 1000
#    N = 200
    M = 1000
    N = 200
    keys = ['piet%d' % i for i in range(N)]

    for i in range(N):
        redis.set('piet%d' % i, 'blaat%d' % i)
        assert(redis.get('piet%d' % i) == 'blaat%d' % i)
        #print '******', repr(redis.get('piet%d' % i))
    start = time.time()
    for i in range(M):
        redis.mget(*keys)
    end = time.time()
    print (N * M) / (end - start)
    
def profile(f = None):
    from cProfile import Profile
    prof = Profile()
    try:
        prof = prof.runctx("f()", globals(), locals())
    except SystemExit:
        pass

    import pstats
    stats = pstats.Stats(prof)
    stats.strip_dirs()
    stats.sort_stats('time')
    stats.print_stats(20)

if __name__ == '__main__':
    #test_ketama()
    #test_simple()
    #profile(test_mget)
    test_mget()
    
