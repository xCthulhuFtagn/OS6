$ sudo docker run -d --name prom --network="host" prom/prometheus
$ sudo docker cp prometheus.yml prom:/etc/prometheus
$ sudo docker restart prom