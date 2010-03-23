from redis import *

def test_simple():
    connection = Connection("127.0.0.1:6379")

    for j in range(10):
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
    print repr(ketama.get_server('12345'))
    
def test_mget():
    ketama = Ketama()
    ketama.add_server(("10.0.1.1", 11211), 300)
    ketama.add_server(("10.0.1.2", 11211), 300)
    ketama.add_server(("10.0.1.3", 11211), 300)
    
    ketama.create_continuum()

    connection_manager= ConnectionManager()
    
    redis = Redis(ketama, connection_manager)
    
    redis.mget(*['piet%d' % i for i in range(10)])
    
if __name__ == '__main__':
    #test_ketama()
    #test_simple()
    test_mget()

