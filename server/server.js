var http = require('http')
var express = require('express');
var bodyParser = require('body-parser');
var morgan = require('morgan');
var mongodb = require('mongodb');
var elasticsearch = require('elasticsearch');
var uuid = require('uuid');

// local requires
var queries = require('./queries.js');

var app = express();
var port = process.env.PORT || 8080;
var mongoport = process.env.MONGOPORT || 27017;
var mongohost = process.env.MONGOHOST || 'localhost';
var eshost = process.env.ESHOST || 'localhost';
var esport = process.env.ESPORT || 9200;
var esindex = process.env.ESINDEX || 'aqindex';

// set up mongodb collection
var mongocollection = 'aq';
var aqdb;

var esinittype = 'initdata';
var esdatatype = 'aqdata';

// use bodyParser so we can get data from POST
app.use(bodyParser.urlencoded({extended: true}));
app.use(bodyParser.json());
app.use(morgan('combined'));

var router = express.Router();

// create MongoDB connection
//mongoclient = mongodb.MongoClient;

// create elasticsearch client
var esclient = new elasticsearch.Client({
  host: eshost + ':' + esport,
  log: 'error'
});

/*mongoclient.connect('mongodb://' + mongohost + ':' + mongoport + '/' + mongocollection, function(err, db) {
  if (err) throw err;
  aqdb = db;
  console.log("Connected to Database");
});*/

// middleware to use for all requests
router.use(function(req, res, next) {
  console.log("We got a reqeust of some kind");
  next();
});

// main page route
router.get('/', function (req, res) {
  res.send('<html><body><h1>Welcome to the AQ API!</h1></body></html>');
});

router.route('/aqstate')

  .get(function(req, res) {

    var aqstate = {};

    // get the last hour averages from ES
    esclient.search({  
      index: esindex,
      type: esdatatype,
      body: queries.aqstatusquery
    },function (error, response, status) {
        if (error){
          console.log("Search error: " + error);
          aqstate.error = error
          res.json(aqstate);
        }
        else {
          for(var i in response.aggregations){
            if(response.aggregations[i].value) {
              aqstate[i] = response.aggregations[i].value.toFixed(2);
            }
          }
          
          // calculate the current state (red/yellow/green)
          aqstate.status = "green";
          res.json(aqstate);
        }
    });
  });

router.route('/lastaqreading')

  .get(function(req, res) {

    var aqstate = {};

    // get the last hour averages from ES
    esclient.search({  
      index: esindex,
      type: esdatatype,
      body: queries.lastaqquery
    },function (error, response, status) {
        if (error){
          console.log("Search error: " + error);
          aqstate.error = error
          res.json(aqstate);
        }
        else {
          console.log(response.hits.hits[0].length);
          if(response.hits.hits.length == 1){
            for(var i in response.hits.hits[0]._source){
              aqstate[i] = response.hits.hits[0]._source[i];
            }
          }
          
          res.json(aqstate);
        }
    });
  });

// route for iteracting with air quality data records
router.route('/aqdata')

  // record a new AQ data package
  .post(function(req, res) {

    var jsonbody = req.body;
    var responseobj = saveData(jsonbody, esdatatype);
    console.log('response object: ' + JSON.stringify(responseobj));
    res.json({message: 'ok'});

    // get 15 minute running averages

    // calculate status value
    
    // update system status based on new values
  });

// route for interacting with device init data
router.route('/initdata')

  // record a new init data package
  .post(function(req, res) {
    
    var jsonbody = req.body;
    //console.log('initdata POST body: ' + JSON.stringify(req.body));
    var responseobj = saveData(jsonbody, esinittype);
    console.log('response object: ' + JSON.stringify(responseobj));
    res.json({message: 'ok'});
  });

function saveData(jsonbody, estype){

  // convert millis time to a real datetime
  var reald = new Date(jsonbody.time * 1000);
  jsonbody.datetime = reald;
  
  console.log("Received data: " + JSON.stringify(jsonbody));
  //insert record
  if(jsonbody){
    /*aqdb.collection(mongocollection).insert(jsonbody, function(err, records) {
      if (err) throw err;
      console.log('Record added as ' + JSON.stringify(records));
      // get the ID
      var rid = records.insertedIds[0];
      console.log('Inserted record id: ' + rid);

      // for some reason the MongoDB save adds a bson ID to the record, ES doesn't like this
      delete jsonbody._id;

      
    });*/

    // create ES index object
    var idxobj = {
      index: esindex,
      type: estype,
      id: uuid.v1(),
      body: jsonbody
    }
    esclient.create(idxobj, function(err, res){
      if(err){
        console.log('Error inserting into Elasticsearch: ' + err);
        return {message: 'Could not save data to Elasticsearch'};
      }else{
        console.log('Response: ' + res);
        return {message: 'Saved data to Elasticsearch'};
      }
    });
  }else {
    return {message: 'Invalid JSON body provided'};
  }
}
// API routes
app.use('/api', router);

// add route for static items
app.use(express.static('public'))

app.listen(port);
console.log('Listening on port: ' + port);

function calculateAQstatus() {
  var aqStatus = 100;
  // use the last 15 minute averages from the ES query

  /**
   * Particlate table (in micrograms as measured by our sensor)
   * Using EPA published Pm10 values
   * Good = x < 54
   * Moderate = 54 < x < 254
   * Poor x > 254
   */

  /**
   * We don't have any good data about MQ135 sensor readings, so these will 
   * need to be refined.  Average in my home during development was < 160 so 
   * let's use that as a baseline for now
   * Good = x < 160
   * Moderate = 160 < x < 200
   * Poor x > 200
   */
  
  /**
   * MQ131 ozone readings are similar to MQ135 readings in that we know that
   * higher is worse, but without good calibration we'll need to treat this as
   * a relative measurement.  Average in home during dev is right around 160.
   * Good = x < 160
   * Moderate = 160 < x < 200
   * Poor x > 200
   */
  
  /**
   * At higher temperatures we know the effect of pollution is worse, so let's apply 
   * a temperature correction factor.  Studies show that high temperature and high
   * humidity make particle pollution worse, and higher temperatures exacerbate other
   * kinds of pollutants as well
   */

  // what combination of values indicates poor air quality 1-100 scale?
  return aqStatus;
}

function updateAQState() {

  // first, check override switches, if system is in yellow, red or green override, change values

  // next, check for a payment based override.  Has somebody paid to switch this thing back on?

  // set pins for lights

  // update WiFi AP state (open, throttled or off)
}