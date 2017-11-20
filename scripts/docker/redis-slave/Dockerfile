FROM redis
EXPOSE 6379
WORKDIR /opt
COPY redis.conf /opt/redis.conf
CMD [ "redis-server", "/opt/redis.conf" ]
