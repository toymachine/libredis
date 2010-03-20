import redis

r = redis.Redis(host='localhost', port=6379, db=0)
for i in range(100000):
	r.get('blaat')
