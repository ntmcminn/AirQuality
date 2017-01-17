module.exports = {
    // this query will need adjustment, sensors are sending data in GMT,
    // these queries use local time
    aqstatusquery: {
        "query" : {
            "filtered" : {
                "query" : {
                    "match_all" : {}
                    },
                    "filter": {
                    "range" : {
                        "datetime" : {
                            "gte" : "now-1d/d",
                            "lt" :  "now+1d/d"
                        }
                    }
                }
            }
        },
    
    "aggs" : {
            "avgdensity" : { "avg" : { "field" : "dsdata.density" } },
            "avgtemp" : { "avg" : { "field" : "thdata.temp" } },
            "avghumidity" : { "avg" : { "field" : "thdata.humidity" } },
            "avgmq135" : { "avg" : { "field" : "gsdata.MQ135" } },
            "avgmq131" : { "avg" : { "field" : "gsdata.MQ131" } },
            "avgmq7" : { "avg" : { "field" : "gsdata.MQ7" } },
            "maxdensity" : { "max" : { "field" : "dsdata.density" } },
            "maxtemp" : { "max" : { "field" : "thdata.temp" } },
            "maxhumidity" : { "max" : { "field" : "thdata.humidity" } },
            "maxmq135" : { "max" : { "field" : "gsdata.MQ135" } },
            "maxmq131" : { "max" : { "field" : "gsdata.MQ131" } },
            "maxmq7" : { "max" : { "field" : "gsdata.MQ7" } },
            "mindensity" : { "min" : { "field" : "dsdata.density" } },
            "mintemp" : { "min" : { "field" : "thdata.temp" } },
            "minhumidity" : { "min" : { "field" : "thdata.humidity" } },
            "minmq135" : { "min" : { "field" : "gsdata.MQ135" } },
            "minmq131" : { "min" : { "field" : "gsdata.MQ131" } },
            "minmq7" : { "min" : { "field" : "gsdata.MQ7" } }
        }
    },

    // last reading from a specific node
    lastaqquery: {
        "query": {
            "match_all": {}
        },
        "size": 1,
        "sort": [
            {
            "_timestamp": {
                "order": "desc"
            }
            }
        ]
    },

    // map all nodes
    mapquery: {
        "query": {
            "filtered" : {
                "query" : {
                    "match_all" : {}
                    },
                    "filter": {
                    "range" : {
                        "datetime" : {
                            "gte" : "now-1d/d",
                            "lt" :  "now+1d/d"
                        }
                    }
                }
            }
        }
    }
}   