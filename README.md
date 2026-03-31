## Remove data from prometheus
```
curl -XPOST http://10.40.0.200:9090/api/v1/admin/tsdb/clean_tombstones
curl -XPOST "http://10.40.0.200:9090/api/v1/admin/tsdb/delete_series?match[]=eventRxManagerA&match[]=eventTxManagerA"
```
